/* =====================================================================
   ROBOT SUIVEUR DE COULOIR - recentrage entre deux murs
   ---------------------------------------------------------------------
   Materiel :
     - 1 carte Arduino UNO / NANO (ATmega328P, 5 V)
     - 2 moteurs DC (un par roue) pilotes par un pont en H L298N
     - 2 capteurs a ultrasons HC-SR04 : un a GAUCHE, un a DROITE,
       qui mesurent la distance aux deux murs du couloir

   Principe de fonctionnement :
     1) Le robot compare la distance au mur GAUCHE (dG) et au mur DROIT (dD).
     2) Objectif : dG == dD  =>  le robot est exactement au milieu.
        L'ecart (dD - dG) est corrige par un correcteur PD qui ajuste la
        vitesse des deux moteurs (l'un accelere, l'autre ralentit).
     3) EXCEPTION : si une des deux distances depasse la portee maximale
        du capteur (donc plus de mur detectable de ce cote), le robot ne
        peut plus se centrer : il effectue une ROTATION A DROITE jusqu'a
        retrouver les deux murs.

   Cablage, reglages et depannage : voir README.md
   ===================================================================== */

/* ------------------------- 1. BROCHES ------------------------------- */
// Capteur ultrasons GAUCHE
const uint8_t TRIG_GAUCHE = 2;
const uint8_t ECHO_GAUCHE = 3;

// Capteur ultrasons DROIT
const uint8_t TRIG_DROIT = 4;
const uint8_t ECHO_DROIT = 5;

// Pont en H L298N - moteur GAUCHE
const uint8_t ENA = 9;    // PWM vitesse moteur gauche
const uint8_t IN1 = 8;    // sens 1
const uint8_t IN2 = 12;   // sens 2

// Pont en H L298N - moteur DROIT
const uint8_t ENB = 10;   // PWM vitesse moteur droit
const uint8_t IN3 = 11;   // sens 1
const uint8_t IN4 = 13;   // sens 2

/* ------------------------- 2. REGLAGES ------------------------------ */
// --- Capteurs ---
// Portee maximale prise en compte (cm). Au-dela, on considere qu'il n'y a
// PAS de mur : c'est ce seuil qui declenche la rotation a droite.
// HC-SR04 : portee pratique fiable jusqu'a ~200 cm.
const int DISTANCE_MAX_CM = 200;

// Distance minimale credible (cm) : en dessous, mesure aberrante ignoree.
const int DISTANCE_MIN_CM = 3;

// --- Deplacement ---
const int VITESSE_BASE = 110;     // vitesse de croisiere (0-255)
const int VITESSE_ROTATION = 130; // vitesse de la rotation a droite (0-255)
const int PWM_MINI = 45;          // sous cette valeur, les moteurs ne tournent pas

// --- Correcteur PD ---
const float KP = 1.2;   // gain proportionnel (trop grand => oscillations)
const float KD = 0.35;  // gain derive (amortit les oscillations), 0 = inactif

// Ralentissement quand le robot est tres desaxe (securite anti-collision).
const float ECART_RALENTI_1 = 15.0;  // cm
const float ECART_RALENTI_2 = 30.0;  // cm

// --- Temporisation ---
const unsigned long PERIODE_MS = 50;         // periode de la regulation (20 Hz)
const unsigned long ROTATION_MINI_MS = 220;  // duree mini d'une rotation
const unsigned long ROTATION_MAXI_MS = 3000; // securite : arret si bloque
const unsigned long DEMARRAGE_MS = 1000;     // temps pour poser le robot

// Afficher les mesures dans le moniteur serie (115200 bauds).
const bool DEBUG = true;

/* ------------------------- 3. ETAT INTERNE -------------------------- */
enum Etat { SUIVI, ROTATION_DROITE };
Etat etat = SUIVI;

unsigned long dernierCycle = 0;
unsigned long debutRotation = 0;
float erreurPrecedente = 0;
bool premierCycle = true;

/* ------------------------- 4. MOTEURS ------------------------------- */
// Vitesse signee : >0 avant, <0 arriere, 0 arret.
void moteurGauche(int vitesse) {
  vitesse = constrain(vitesse, -255, 255);
  if (vitesse > 0) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  } else if (vitesse < 0) {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  } else {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
  }
  analogWrite(ENA, abs(vitesse));
}

void moteurDroit(int vitesse) {
  vitesse = constrain(vitesse, -255, 255);
  if (vitesse > 0) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else if (vitesse < 0) {
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
  }
  analogWrite(ENB, abs(vitesse));
}

void arreter() {
  moteurGauche(0);
  moteurDroit(0);
}

// Compense la zone morte : une petite consigne devient PWM_MINI.
int compenserZoneMorte(int vitesse) {
  if (vitesse > 0 && vitesse < PWM_MINI) return PWM_MINI;
  if (vitesse < 0 && vitesse > -PWM_MINI) return -PWM_MINI;
  return vitesse;
}

// Rotation a DROITE sur place : roue gauche en avant, roue droite en arriere.
// Si le robot tourne a gauche chez vous, inversez les deux signes.
void rotationDroite() {
  moteurGauche(compenserZoneMorte(VITESSE_ROTATION));
  moteurDroit(compenserZoneMorte(-VITESSE_ROTATION));
}

/* ------------------------- 5. CAPTEURS ------------------------------ */
// Une mesure brute en cm. Renvoie -1 si aucun echo (rien devant le capteur).
float mesurerBrut(uint8_t trig, uint8_t echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(3);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  // Timeout 25 ms (~4 m) ; pulseIn renvoie 0 en cas d'absence d'echo.
  unsigned long duree = pulseIn(echo, HIGH, 25000UL);
  if (duree == 0) return -1.0;
  return duree * 0.0343 / 2.0;   // 343 m/s : distance = t * v / 2
}

// Moyenne de 3 mesures valides (filtre le bruit des ultrasons).
// Renvoie -1 si aucune mesure credible.
float mesurerDistance(uint8_t trig, uint8_t echo) {
  float somme = 0;
  int valides = 0;
  for (int i = 0; i < 3; i++) {
    float d = mesurerBrut(trig, echo);
    if (d >= DISTANCE_MIN_CM && d <= 400.0) {
      somme += d;
      valides++;
    }
    delay(6);
  }
  if (valides == 0) return -1.0;
  return somme / valides;
}

// Vrai si la mesure correspond a un mur exploitable.
bool murDetecte(float d) {
  return (d > 0 && d <= (float)DISTANCE_MAX_CM);
}

/* ------------------------- 6. SETUP --------------------------------- */
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_GAUCHE, OUTPUT); pinMode(ECHO_GAUCHE, INPUT);
  pinMode(TRIG_DROIT, OUTPUT);  pinMode(ECHO_DROIT, INPUT);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  arreter();
  delay(DEMARRAGE_MS);
  if (DEBUG) {
    Serial.println(F("=== Robot suiveur de couloir : pret ==="));
    Serial.print(F("Portee max consideree : "));
    Serial.print(DISTANCE_MAX_CM);
    Serial.println(F(" cm (au-dela : rotation a droite)"));
  }
}

/* ------------------------- 7. BOUCLE -------------------------------- */
void loop() {
  unsigned long maintenant = millis();
  if (maintenant - dernierCycle < PERIODE_MS) return;
  dernierCycle = maintenant;

  float dGauche = mesurerDistance(TRIG_GAUCHE, ECHO_GAUCHE);
  float dDroit  = mesurerDistance(TRIG_DROIT,  ECHO_DROIT);

  bool murGauche = murDetecte(dGauche);
  bool murDroit  = murDetecte(dDroit);

  /* --- 7.1 Priorite : une distance > portee max => rotation a droite --- */
  // Cas couverts : ouverture dans le mur, fin de couloir d'un cote,
  // virage, capteur qui ne voit plus rien.
  bool perteMur = (!murGauche || !murDroit);

  if (perteMur) {
    if (etat != ROTATION_DROITE) {        // on entre en rotation
      etat = ROTATION_DROITE;
      debutRotation = maintenant;
    }
    rotationDroite();

    if (DEBUG) {
      Serial.print(F("ROTATION DROITE  G="));
      Serial.print(dGauche, 1);
      Serial.print(F("  D="));
      Serial.println(dDroit, 1);
    }

    // Securite : si on tourne trop longtemps, on s'arrete.
    if (maintenant - debutRotation >= ROTATION_MAXI_MS) {
      arreter();
      if (DEBUG) Serial.println(F("ARRET : rotation trop longue (robot bloque ?)"));
      while (true) { delay(1000); }      // reinitialiser la carte pour repartir
    }

    // On sort de la rotation des que les DEUX murs sont revenus,
    // apres une duree minimale (anti-rebond).
    if (maintenant - debutRotation >= ROTATION_MINI_MS) {
      etat = SUIVI;
      premierCycle = true;
      erreurPrecedente = 0;
    }
    return;
  }

  /* --- 7.2 Recentrage entre les deux murs ---------------------------- */
  // erreur > 0 : le mur droit est plus loin => le robot est trop a gauche
  //              => il doit tourner a droite (roue gauche plus vite).
  float erreur = dDroit - dGauche;

  float derivee = premierCycle ? 0.0 : (erreur - erreurPrecedente);
  erreurPrecedente = erreur;
  premierCycle = false;

  float correction = KP * erreur + KD * derivee;

  // Ralentissement progressif quand l'ecart devient important.
  float facteur = 1.0;
  float ecart = fabs(erreur);
  if (ecart > ECART_RALENTI_1) facteur = 0.70;
  if (ecart > ECART_RALENTI_2) facteur = 0.45;

  int base = (int)(VITESSE_BASE * facteur);
  int vGauche = base + (int)correction;
  int vDroit  = base - (int)correction;

  moteurGauche(compenserZoneMorte(vGauche));
  moteurDroit(compenserZoneMorte(vDroit));

  /* --- 7.3 Debug ---------------------------------------------------- */
  if (DEBUG) {
    Serial.print(F("G=")); Serial.print(dGauche, 1);
    Serial.print(F("  D=")); Serial.print(dDroit, 1);
    Serial.print(F("  err=")); Serial.print(erreur, 1);
    Serial.print(F("  corr=")); Serial.print(correction, 1);
    Serial.print(F("  PWM G=")); Serial.print(vGauche);
    Serial.print(F(" D=")); Serial.println(vDroit);
  }
}
