/* =====================================================================
   THE RACE PROJECT - Robot autonome avec camera
   Auteur  : Victor PERRUCHE - Lycee Eiffel Dijon - TSI1
   Version : 2.0 (correcteur PD corrige, 3 capteurs, ESP32-CAM)
   ===================================================================== */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ============ CONFIGURATION ============
const char* WIFI_SSID     = "TON_RESEAU_WIFI";
const char* WIFI_PASSWORD = "TON_MOT_DE_PASSE";
const char* SERVER_URL    = "http://192.168.1.100:5000/upload";

// Broches MOTEURS (L298N)
const uint8_t PIN_ENA = 13; const uint8_t PIN_IN1 = 12; const uint8_t PIN_IN2 = 14;
const uint8_t PIN_ENB = 15; const uint8_t PIN_IN3 = 2;  const uint8_t PIN_IN4 = 4;

// Broches CAPTEURS HC-SR04 (AVANT, GAUCHE, DROIT)
const uint8_t PIN_TRIG_AVANT=1; const uint8_t PIN_ECHO_AVANT=3;
const uint8_t PIN_TRIG_GAUCHE=16; const uint8_t PIN_ECHO_GAUCHE=0;
const uint8_t PIN_TRIG_DROIT=33; const uint8_t PIN_ECHO_DROIT=32;

// Encodeurs + LED
const uint8_t PIN_ENC_G=35; const uint8_t PIN_ENC_D=34; const uint8_t PIN_LED=33;

// ============ CONSTANTES PHYSIQUES ============
const float RAYON_ROUE_MM=49.7; const float CIRCONFERENCE_CM=31.23;
const int TICKS_PAR_TOUR=20; const int TAILLE_CARREAU_CM=30;
const float DISTANCE_MAX_CM=200.0; const float DISTANCE_MIN_CM=3.0;

// Vitesses PWM
const int VITESSE_BASE=150; const int VITESSE_VIRAGE=100; const int VITESSE_ROTATION=130;
const int PWM_MINI=60; const int PWM_MAX=255;

// ============ CORRECTEUR PD CORRIGE ============
const float KP=1.2; const float KD=0.0175; const float DT_S=0.05;
const float CORRECTION_MAX=255.0; const float ZONE_MORTE_CM=2.0;

// Ralentissement progressif
const float SEUIL_RALENTI_1=15.0; const float SEUIL_RALENTI_2=30.0;
const float COEF_RALENTI_1=0.70; const float COEF_RALENTI_2=0.45;

// Temporisation
const unsigned long PERIODE_MS=50; const unsigned long INTERVALLE_PHOTO=1000;
const unsigned long TEMPS_VIRAGE_MS=400; const unsigned long TEMPS_DEMI_TOUR=1500;
const unsigned long TEMPS_SECURITE_MS=3000; const unsigned long DEMARRAGE_MS=1000;

// Seuils
const float SEUIL_MUR_AVANT_CM=25.0; const float SEUIL_VIRAGE_CM=20.0;
const bool DEBUG_SERIAL=true; const unsigned long BAUD_RATE=115200;

// ============ ETATS ============
enum EtatRobot { INIT, AVANCE, VIRE_DROITE, VIRE_GAUCHE, DEMI_TOUR, ARRET_FIN, ERREUR };
EtatRobot etat = INIT;

// ============ VARIABLES GLOBALES ============
unsigned long dernierCycle=0, dernierePhoto=0, debutManouvre=0;
float erreurPrecedente=0.0, derivee=0.0; bool premierCycle=true;
volatile unsigned long ticksGauche=0, ticksDroit=0;
float distanceParcourueCm=0.0; unsigned int nombreTours=0;
int pwmGauche=0, pwmDroit=0; bool cameraInitialisee=false;

// ============ MOTEURS (L298N) ============
void moteurGauche(int v) {
  v = constrain(v, -PWM_MAX, PWM_MAX);
  if (v > 0) { digitalWrite(PIN_IN1,HIGH); digitalWrite(PIN_IN2,LOW); }
  else if (v < 0) { digitalWrite(PIN_IN1,LOW); digitalWrite(PIN_IN2,HIGH); }
  else { digitalWrite(PIN_IN1,LOW); digitalWrite(PIN_IN2,LOW); }
  analogWrite(PIN_ENA, abs(v));
}
void moteurDroit(int v) {
  v = constrain(v, -PWM_MAX, PWM_MAX);
  if (v > 0) { digitalWrite(PIN_IN3,HIGH); digitalWrite(PIN_IN4,LOW); }
  else if (v < 0) { digitalWrite(PIN_IN3,LOW); digitalWrite(PIN_IN4,HIGH); }
  else { digitalWrite(PIN_IN3,LOW); digitalWrite(PIN_IN4,LOW); }
  analogWrite(PIN_ENB, abs(v));
}
void arreter() { moteurGauche(0); moteurDroit(0); }
int compenserZoneMorte(int v) {
  if (v > 0 && v < PWM_MINI) return PWM_MINI;
  if (v < 0 && v > -PWM_MINI) return -PWM_MINI;
  return v;
}

// ============ ENCODEURS (odometrie) ============
void IRAM_ATTR compteurGauche() { ticksGauche++; }
void IRAM_ATTR compteurDroit() { ticksDroit++; }
void mettreAJourOdometrie() {
  unsigned long t = (ticksGauche + ticksDroit) / 2;
  distanceParcourueCm = (float)t * CIRCONFERENCE_CM / TICKS_PAR_TOUR;
}
void resetOdometrie() { ticksGauche=0; ticksDroit=0; distanceParcourueCm=0.0; }

// ============ CAPTEURS HC-SR04 ============
float mesurerBrut(uint8_t trig, uint8_t echo) {
  digitalWrite(trig,LOW); delayMicroseconds(2);
  digitalWrite(trig,HIGH); delayMicroseconds(10);
  digitalWrite(trig,LOW);
  unsigned long d = pulseIn(echo,HIGH,25000UL);
  if (d == 0) return -1.0;
  return d * 0.0343 / 2.0;
}
float mesurerDistance(uint8_t trig, uint8_t echo) {
  float s=0; int n=0;
  for (int i=0;i<3;i++) {
    float d=mesurerBrut(trig,echo);
    if (d>=DISTANCE_MIN_CM && d<=400.0) { s+=d; n++; }
    delay(3);
  }
  return n==0 ? -1.0 : s/n;
}
bool murDetecte(float d) { return (d>0 && d<=DISTANCE_MAX_CM); }

// ============ CORRECTEUR PD CORRIGE ============
// erreur = dDroit - dGauche | >0=trop a gauche | <0=trop a droite
float calculerCorrectionPD(float erreur) {
  if (fabs(erreur) < ZONE_MORTE_CM) { erreurPrecedente=erreur; derivee=0.0; return 0.0; }
  float termeP = KP * erreur;
  float termeD = 0.0;
  if (!premierCycle) { derivee = (erreur - erreurPrecedente) / DT_S; termeD = KD * derivee; }
  else { derivee = 0.0; }
  erreurPrecedente = erreur; premierCycle = false;
  float correction = termeP + termeD;
  return constrain(correction, -CORRECTION_MAX, CORRECTION_MAX);
}
void reinitialiserPD() { erreurPrecedente=0.0; derivee=0.0; premierCycle=true; }

// ============ RALENTISSEMENT ============
int appliquerRalentissement(int v, float err) {
  float e = fabs(err);
  if (e > SEUIL_RALENTI_2) return (int)(v * COEF_RALENTI_2);
  if (e > SEUIL_RALENTI_1) return (int)(v * COEF_RALENTI_1);
  return v;
}

// ============ MANOEUVRES ============
void rotationDroite(int v) { moteurGauche(compenserZoneMorte(v)); moteurDroit(compenserZoneMorte(-v)); }
void rotationGauche(int v) { moteurGauche(compenserZoneMorte(-v)); moteurDroit(compenserZoneMorte(v)); }

// ============ CAMERA ESP32-CAM (OV2640) ============
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

void configurerCamera() {
  camera_config_t c;
  c.ledc_channel=LEDC_CHANNEL_0; c.ledc_timer=LEDC_TIMER_0;
  c.pin_d0=CAM_PIN_D0; c.pin_d1=CAM_PIN_D1; c.pin_d2=CAM_PIN_D2; c.pin_d3=CAM_PIN_D3;
  c.pin_d4=CAM_PIN_D4; c.pin_d5=CAM_PIN_D5; c.pin_d6=CAM_PIN_D6; c.pin_d7=CAM_PIN_D7;
  c.pin_xclk=CAM_PIN_XCLK; c.pin_pclk=CAM_PIN_PCLK; c.pin_vsync=CAM_PIN_VSYNC;
  c.pin_href=CAM_PIN_HREF; c.pin_sscb_sda=CAM_PIN_SIOD; c.pin_sscb_scl=CAM_PIN_SIOC;
  c.pin_pwdn=CAM_PIN_PWDN; c.pin_reset=CAM_PIN_RESET; c.xclk_freq_hz=20000000;
  c.pixel_format=PIXFORMAT_JPEG; c.frame_size=FRAMESIZE_VGA;
  c.jpeg_quality=12; c.fb_count=1;
  esp_err_t err = esp_camera_init(&c);
  cameraInitialisee = (err == ESP_OK);
  if (DEBUG_SERIAL) Serial.printf(cameraInitialisee ? "Camera OK
" : "Camera ERREUR: 0x%x
", err);
}

void prendrePhotoEtEnvoyer() {
  if (!cameraInitialisee) return;
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { if (DEBUG_SERIAL) Serial.println("Echec capture"); return; }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Distance", String(distanceParcourueCm, 1));
    http.addHeader("X-Etat", String((int)etat));
    int code = http.POST(fb->buf, fb->len);
    if (DEBUG_SERIAL) Serial.printf("Photo: %d octets, HTTP %d
", fb->len, code);
    http.end();
  }
  esp_camera_fb_return(fb);
}

// ============ SETUP ============
void setup() {
  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, HIGH);
  Serial.begin(BAUD_RATE); delay(100);
  if (DEBUG_SERIAL) {
    Serial.println("
========================================");
    Serial.println("  THE RACE PROJECT v2.0");
    Serial.println("  Robot autonome + camera ESP32-CAM");
    Serial.println("========================================");
  }
  // WiFi
  if (DEBUG_SERIAL) Serial.print("WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 20) {
    delay(500); if (DEBUG_SERIAL) Serial.print(".");
    t++; digitalWrite(PIN_LED, !digitalRead(PIN_LED));
  }
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_LED, LOW);
    if (DEBUG_SERIAL) Serial.printf("
WiFi OK: %s
", WiFi.localIP().toString().c_str());
  } else if (DEBUG_SERIAL) Serial.println("
WiFi ECHEC");
  configurerCamera();
  // Broches moteurs
  pinMode(PIN_ENA,OUTPUT); pinMode(PIN_IN1,OUTPUT); pinMode(PIN_IN2,OUTPUT);
  pinMode(PIN_ENB,OUTPUT); pinMode(PIN_IN3,OUTPUT); pinMode(PIN_IN4,OUTPUT);
  // Broches capteurs
  pinMode(PIN_TRIG_AVANT,OUTPUT); pinMode(PIN_ECHO_AVANT,INPUT);
  pinMode(PIN_TRIG_GAUCHE,OUTPUT); pinMode(PIN_ECHO_GAUCHE,INPUT_PULLUP);
  pinMode(PIN_TRIG_DROIT,OUTPUT); pinMode(PIN_ECHO_DROIT,INPUT);
  // Encodeurs
  pinMode(PIN_ENC_G,INPUT_PULLUP); pinMode(PIN_ENC_D,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_G), compteurGauche, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_D), compteurDroit, RISING);
  arreter();
  if (DEBUG_SERIAL) Serial.println("Positionnez le robot au depart...");
  delay(DEMARRAGE_MS);
  resetOdometrie(); reinitialiserPD();
  etat = AVANCE;
  if (DEBUG_SERIAL) {
    Serial.println("=== DEMARRAGE - Tour du couloir ===");
    Serial.printf("KP=%.2f KD=%.4f DT=%.3f ZoneMorte=%.1f
", KP, KD, DT_S, ZONE_MORTE_CM);
  }
}

// ============ LOOP - Machine a etats ============
void loop() {
  unsigned long maintenant = millis();
  if (maintenant - dernierCycle < PERIODE_MS) return;
  dernierCycle = maintenant;
  mettreAJourOdometrie();

  // Photo periodique
  if (maintenant - dernierePhoto >= INTERVALLE_PHOTO) {
    dernierePhoto = maintenant;
    prendrePhotoEtEnvoyer();
  }

  // Mesure 3 capteurs
  float dAvant  = mesurerDistance(PIN_TRIG_AVANT, PIN_ECHO_AVANT);
  float dGauche = mesurerDistance(PIN_TRIG_GAUCHE, PIN_ECHO_GAUCHE);
  float dDroit  = mesurerDistance(PIN_TRIG_DROIT, PIN_ECHO_DROIT);
  bool murAvant  = murDetecte(dAvant);
  bool murGauche = murDetecte(dGauche);
  bool murDroit  = murDetecte(dDroit);

  switch (etat) {

  case AVANCE: {
    // 1. Mur avant -> decision virage
    if (murAvant && dAvant < SEUIL_MUR_AVANT_CM) {
      if (!murGauche && murDroit) { etat=VIRE_DROITE; debutManouvre=maintenant; if(DEBUG_SERIAL) Serial.println("-> Virage D"); }
      else if (murGauche && !murDroit) { etat=VIRE_GAUCHE; debutManouvre=maintenant; if(DEBUG_SERIAL) Serial.println("-> Virage G"); }
      else { etat=DEMI_TOUR; debutManouvre=maintenant; if(DEBUG_SERIAL) Serial.println("-> Demi-tour"); }
      break;
    }
    // 2. Perte mur lateral
    if (!murGauche && murDroit) { etat=VIRE_DROITE; debutManouvre=maintenant; break; }
    if (murGauche && !murDroit) { etat=VIRE_GAUCHE; debutManouvre=maintenant; break; }
    if (!murGauche && !murDroit) { etat=DEMI_TOUR; debutManouvre=maintenant; break; }
    // 3. Centrage PD normal
    float erreur = dDroit - dGauche;
    float correction = calculerCorrectionPD(erreur);
    int base = appliquerRalentissement(VITESSE_BASE, erreur);
    pwmGauche = base + (int)correction;
    pwmDroit  = base - (int)correction;
    moteurGauche(compenserZoneMorte(pwmGauche));
    moteurDroit(compenserZoneMorte(pwmDroit));
    break;
  }

  case VIRE_DROITE:
    rotationDroite(VITESSE_VIRAGE);
    if (murGauche && murDroit && dGauche>SEUIL_VIRAGE_CM && dDroit>SEUIL_VIRAGE_CM && (maintenant-debutManouvre>TEMPS_VIRAGE_MS)) {
      etat=AVANCE; reinitialiserPD();
    }
    if (maintenant-debutManouvre > TEMPS_SECURITE_MS) { etat=DEMI_TOUR; debutManouvre=maintenant; }
    break;

  case VIRE_GAUCHE:
    rotationGauche(VITESSE_VIRAGE);
    if (murGauche && murDroit && dGauche>SEUIL_VIRAGE_CM && dDroit>SEUIL_VIRAGE_CM && (maintenant-debutManouvre>TEMPS_VIRAGE_MS)) {
      etat=AVANCE; reinitialiserPD();
    }
    if (maintenant-debutManouvre > TEMPS_SECURITE_MS) { etat=DEMI_TOUR; debutManouvre=maintenant; }
    break;

  case DEMI_TOUR:
    rotationDroite(VITESSE_ROTATION);
    if (maintenant-debutManouvre > TEMPS_DEMI_TOUR) { etat=AVANCE; reinitialiserPD(); }
    break;

  case ARRET_FIN:
    arreter();
    if (DEBUG_SERIAL) Serial.printf("=== TOUR TERMINE: %.1f cm ===
", distanceParcourueCm);
    prendrePhotoEtEnvoyer();
    while (true) { digitalWrite(PIN_LED,!digitalRead(PIN_LED)); delay(500); }
    break;

  case ERREUR:
    arreter();
    while (true) { digitalWrite(PIN_LED,!digitalRead(PIN_LED)); delay(100); }
    break;

  case INIT:
    etat = AVANCE;
    break;
  }

  // Debug
  if (DEBUG_SERIAL) {
    Serial.printf("[%d] dA=%.1f dG=%.1f dD=%.1f | G=%d D=%d | %.1fcm
",
      (int)etat, dAvant, dGauche, dDroit, pwmGauche, pwmDroit, distanceParcourueCm);
  }
}
