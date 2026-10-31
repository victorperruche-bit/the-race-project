/* =====================================================================
   THE RACE PROJECT - Version 3 : plan de course embarque
   Auteur  : Victor PERRUCHE - Lycee Gustave Eiffel Dijon - TSI1
   Date    : 31 octobre 2026
   =====================================================================
   Le robot ne devine pas le circuit : il suit un PLAN DE COURSE, comme
   un copilote qui lirait sa feuille de route.

     A chaque ETAPE, un capteur surveille un mur.
     Quand ce mur disparait -> action de l'etape :
       'R' = virage a droite  (capteur DROIT surveille)
       'L' = virage a gauche (capteur GAUCHE surveille)
       'S' = tout droit : on franchit la jonction
             (PLAN_CAPTEUR = 'B' : les DEUX capteurs surveillent)

   Regles (validees en simulation : 7 parcours x 5 configs, 35/35 FIN) :
   1. CENTRAGE PD : uniquement si les 2 murs se font face a 2 m
      (signature de couloir |dG + dD - 2| < 0,4 m). Sinon -> jonction.
   2. VRAI COULOIR seulement : un echo > 2 m n'est pas le mur du couloir.
   3. JONCTION : si un seul mur reste visible, on le tient a ~1 m
      (tenue monopalpe) pour ne pas deriver en aveugle.
   4. ANTI-FAUX-VIRAGE : une etape ne se declenche qu'apres au moins
      2,5 m parcourus depuis la precedente (odometrie par le temps).
   ===================================================================== */

// ============ BROCHES (identiques a robot_suiveur_couloir) ============
const int TRIG_G = 2, ECHO_G = 3;       // capteur ultrasons GAUCHE
const int TRIG_D = 4, ECHO_D = 5;       // capteur ultrasons DROIT
const int ENA = 9,  IN1 = 8, IN2 = 12;  // moteur GAUCHE (L298N, PWM D9)
const int ENB = 10, IN3 = 11, IN4 = 13; // moteur DROIT  (L298N, PWM D10)

// ============ REGLAGES ============
const int   VITESSE    = 160;   // PWM de croisiere (0-255)
const int   VIRAGE_PWM = 120;   // PWM pendant le quart de tour
const float KP         = 60.0;  // gain de centrage (PWM par metre d'ecart)
const float KD         = 30.0;  // amortissement (PWM.s/m) -- 0 = ca oscille !
const float PORTEE     = 4.0;   // portee capteur (m) : au-dela = plus d'echo
const float VMAX_M     = 1.5;   // vitesse reelle a PWM 255 (m/s) -- A CALIBRER
const int   TEMPS_VIRAGE_90_MS = 420; // duree d'un quart de tour sur place -- A CALIBRER
const unsigned long PERIODE_MS = 50;  // cycle de regulation (50 ms comme le simulateur)

// ---- seuils valides en simulation : ne pas modifier a la legere ----
const float CAP_MUR    = 2.0;   // un echo > 2 m n'est PAS le mur du couloir
const float REARM_M    = 1.5;   // rearmement : les 2 murs revus a moins de 1,5 m
const float TRIG_CAP   = 1.45;  // echo surveille > 1,45 m hors couloir = mur parti
const float SUM_TOL    = 0.4;   // |dG + dD - 2| < 0,4 m : signature d'un couloir
const float ZONE_MORTE = 0.02;  // on ne corrige pas a moins de 2 cm
const float MIN_RUN_M  = 2.5;   // distance mini entre 2 etapes (anti faux virage)
const unsigned long STREAK_MS = 300; // invisible depuis > 300 ms = vraiment parti
const float ERR_MAX    = 0.6;   // erreur de centrage clampee
const float MONO_M     = 1.0;   // tenue monopalpe : garder ~1 m du mur qui reste
const float MONO_MAX   = 0.3;   // correction monopalpe clampee
const float DERIVEE_MAX = 0.5;  // derivee clampee
const int   CMD_MAX    = 150;   // correction PWM max

// ============ PLAN DE COURSE ============
// Exemple : parcours "bloc gauche, horaire" (4 virages a droite).
// Les 7 parcours valides sont dans README.md.
const char PLAN_CAPTEUR[] = {'D', 'D', 'D', 'D'};
const char PLAN_ACTION[]  = {'R', 'R', 'R', 'R'};
const int  N_ETAPES = 4;

// ============ ETAT ============
int   etape = 0;                 // etape courante du plan
char  surveille;                 // capteur surveille pour cette etape
bool  detectionArmee = true;     // prete a detecter la perte du mur
float dernierEcho = 1.0;         // dernier echo proche du cote surveille (m)
unsigned long tSansEcho = 0;     // temps sans echo du cote surveille (ms)
float distDepuisEtape = 0.0;     // odometrie depuis la derniere etape (m)
float filtreG = 1.0, filtreD = 1.0;  // distances filtrees (anti-bruit)
float erreur = 0.0, erreurPrec = 0.0, derivee = 0.0;
bool  premierCycleFiable = true;    // pour reinitialiser le filtre apres un blanc
unsigned long dernierCycle = 0;

// ============ MOTEURS (L298N) ============
void moteurGauche(int v) {
  v = constrain(v, -255, 255);
  if (v > 0)      { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  }
  else if (v < 0) { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); }
  else            { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);   }
  analogWrite(ENA, abs(v));
}
void moteurDroit(int v) {
  v = constrain(v, -255, 255);
  if (v > 0)      { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  }
  else if (v < 0) { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
  else            { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);   }
  analogWrite(ENB, abs(v));
}
void arreter() { moteurGauche(0); moteurDroit(0); }

// Quart de tour sur place. sens +1 = droite, -1 = gauche.
// (virage a droite = roue gauche AVANT, roue droite ARRIERE)
void virer(int sens) {
  unsigned long t0 = millis();
  while (millis() - t0 < (unsigned long)TEMPS_VIRAGE_90_MS) {
    moteurGauche(sens * VIRAGE_PWM);
    moteurDroit(-sens * VIRAGE_PWM);
  }
  arreter();
}

// ============ CAPTEURS ============
// Moyenne de 3 mesures. Renvoie -1 si aucun echo fiable.
float lireDistanceM(int trig, int echo) {
  float somme = 0.0; int n = 0;
  for (int i = 0; i < 3; i++) {
    digitalWrite(trig, LOW);  delayMicroseconds(2);
    digitalWrite(trig, HIGH); delayMicroseconds(10);
    digitalWrite(trig, LOW);
    unsigned long duree = pulseIn(echo, HIGH, 25000UL); // > 25 ms = pas d'echo
    if (duree > 0) {
      float m = duree * 0.0001715;   // vitesse du son, aller-retour
      if (m >= 0.02 && m <= PORTEE) { somme += m; n++; }
    }
    delay(3);
  }
  return (n == 0) ? -1.0 : somme / n;
}

// ============ AIDE ============
// Mur perdu : plus d'echo, hors portee, ou echo invraisemblable
// (> 1,45 m alors qu'on n'est pas dans un couloir : le rayon a glisse
// sur un mur au fond d'une jonction).
bool murPerdu(float d, bool dansCouloir) {
  if (d < 0.0)      return true;
  if (d > PORTEE)   return true;
  if (d > TRIG_CAP && !dansCouloir) return true;
  return false;
}

// ============ SETUP ============
void setup() {
  pinMode(TRIG_G, OUTPUT); pinMode(ECHO_G, INPUT);
  pinMode(TRIG_D, OUTPUT); pinMode(ECHO_D, INPUT);
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  Serial.begin(115200);
  arreter();
  Serial.println("=== THE RACE PROJECT v3 -- plan de course ===");
  Serial.print("Plan : ");
  for (int i = 0; i < N_ETAPES; i++) {
    Serial.print(PLAN_CAPTEUR[i]); Serial.print(PLAN_ACTION[i]); Serial.print(' ');
  }
  Serial.println();
  Serial.println("Posez le robot au depart... (1 s)");
  delay(1000);
  dernierCycle = millis();
}

// ============ LOOP ============
void loop() {
  unsigned long maintenant = millis();
  if (maintenant - dernierCycle < PERIODE_MS) return;
  float dt = (maintenant - dernierCycle) / 1000.0;
  dernierCycle = maintenant;

  // ---- 1. mesures (jamais les 2 capteurs en meme temps) ----
  float dG = lireDistanceM(TRIG_G, ECHO_G);
  float dD = lireDistanceM(TRIG_D, ECHO_D);
  surveille = PLAN_CAPTEUR[etape % N_ETAPES];

  // ---- 2. signature de couloir : les 2 murs se font face a 2 m ----
  bool dansCouloir = (dG > 0.0 && dD > 0.0 && dG < CAP_MUR && dD < CAP_MUR
                      && fabs(dG + dD - 2.0) < SUM_TOL);

  // ---- 3. le mur surveille est-il perdu ? ----
  bool perdu;
  if (surveille == 'B') {
    perdu = murPerdu(dD, dansCouloir) || murPerdu(dG, dansCouloir);
  } else {
    float dSurv = (surveille == 'D') ? dD : dG;
    perdu = murPerdu(dSurv, dansCouloir);
  }
  if (perdu) {
    tSansEcho += PERIODE_MS;
  } else {
    tSansEcho = 0;
    if      (surveille == 'D') dernierEcho = dD;
    else if (surveille == 'G') dernierEcho = dG;
    else                       dernierEcho = (dG < dD) ? dG : dD;
  }

  // ---- 4. odometrie par le temps (a remplacer par des encodeurs si possible) ----
  distDepuisEtape += ((float)VITESSE / 255.0) * VMAX_M * dt;

  // ---- 5. declenchement de l'etape suivante ----
  if (detectionArmee && perdu && distDepuisEtape > MIN_RUN_M
      && (dernierEcho < REARM_M || tSansEcho > STREAK_MS)) {
    char action = PLAN_ACTION[etape % N_ETAPES];
    etape++;
    detectionArmee = false;
    distDepuisEtape = 0.0;
    tSansEcho = 0;
    dernierEcho = 99.0;
    premierCycleFiable = true;   // filtre a reinitialiser au retour en couloir
    if      (action == 'R') { Serial.println("-> virage DROITE");  virer(+1); }
    else if (action == 'L') { Serial.println("-> virage GAUCHE"); virer(-1); }
    else                    { Serial.println("-> jonction : TOUT DROIT"); }
    dernierCycle = millis();  // on ne compte pas le temps du virage dans l'odometrie
    return;
  }

  // ---- 6. filtre + rearmement ----
  if (dansCouloir) {
    if (premierCycleFiable) {
      filtreG = dG; filtreD = dD;
      erreurPrec = dG - dD;   // la derivee repart proprement apres le blank
      derivee = 0.0;
      premierCycleFiable = false;
    } else {
      filtreG = 0.7 * filtreG + 0.3 * dG;
      filtreD = 0.7 * filtreD + 0.3 * dD;
    }
    if (!detectionArmee && filtreG < REARM_M && filtreD < REARM_M) {
      detectionArmee = true;
      Serial.print("rearmement -- etape ");
      Serial.println(etape % N_ETAPES + 1);
    }
  } else {
    premierCycleFiable = true;   // hors couloir : filtre a refaire au retour
  }

  // ---- 7. erreur de centrage ----
  if (dansCouloir) {
    erreur = constrain(filtreG - filtreD, -ERR_MAX, ERR_MAX);
    float deriveeBrute = (erreur - erreurPrec) / (PERIODE_MS / 1000.0);
    derivee = 0.5 * derivee + 0.5 * constrain(deriveeBrute, -DERIVEE_MAX, DERIVEE_MAX);
  } else {
    // jonction : tenue monopalpe sur le mur qui reste visible
    bool gOK = (dG > 0.0 && dG < CAP_MUR);
    bool dOK = (dD > 0.0 && dD < CAP_MUR);
    if      (!gOK && dOK) erreur = constrain(MONO_M - dD, -MONO_MAX, MONO_MAX);
    else if (!dOK && gOK) erreur = constrain(dG - MONO_M, -MONO_MAX, MONO_MAX);
    else                  erreur = 0.0;   // aucun mur fiable : on file tout droit
    derivee = 0.0;
  }
  if (fabs(erreur) < ZONE_MORTE) erreur = 0.0;   // zone morte de 2 cm
  erreurPrec = erreur;

  // ---- 8. commande PD ----
  int cmd = (int)constrain(KP * erreur + KD * derivee, -CMD_MAX, CMD_MAX);
  moteurGauche(VITESSE + cmd);   // trop pres du mur droit -> tourne a gauche
  moteurDroit(VITESSE - cmd);

  // ---- 9. debug ----
  Serial.print("etape "); Serial.print(etape % N_ETAPES + 1);
  Serial.print('/'); Serial.print(N_ETAPES);
  Serial.print(" sur="); Serial.print(surveille);
  Serial.print(" dG="); Serial.print(dG);
  Serial.print(" dD="); Serial.print(dD);
  Serial.print(" couloir="); Serial.print(dansCouloir ? 1 : 0);
  Serial.print(" err="); Serial.print(erreur, 3);
  Serial.print(" cmd="); Serial.println(cmd);
}
