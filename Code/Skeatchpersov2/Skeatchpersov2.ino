/* =====================================================================
   ROBOT SUIVEUR DE COULOIR - recentrage entre deux murs
   ---------------------------------------------------------------------
   Materiel :
     - 1 carte Arduino UNO (ATmega328P, 5 V)
     - 2 moteurs CC (un par roue) pilotes par un pont en H L298N
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


// Commande 1 moteur CC Gauche

// -------Déclaration des broches E/S--------

const int vitmotA=3; // constante de vitesse moteur A pour la broche 3
const int sensmotA=12; // constante de sens moteur A pour la broche 12
const int freinmotA=9; // constante freinage moteur A pour la broche 9
const int intensitemotA=A0; // constante lecture intensité moteur A broche A0

// Commande 2 moteur CC Droit 

const int vitmotA=3; // constante de vitesse moteur A pour la broche 3
const int sensmotA=12; // constante de sens moteur A pour la broche 12
const int freinmotA=9; // constante freinage moteur A pour la broche 9
const int intensitemotA=A0; // constante lecture intensité moteur A broche A0

// Comande Capteur a Ultrason 1 Gauche

int trig =12; // signal émis sur broche 12
int echo=11; // signal reçu sur broche 11
int lecture; // 
float cm;

// Comande Capteur a Ultrason 2 Droit 

int trig =12; // signal émis sur broche 12
int echo=11; // signal reçu sur broche 11
int lecture; // 
float cm;

/* ------------------------- 2. REGLAGES ------------------------------ */
// --- Capteurs ---
// Portee maximale prise en compte (cm). Au-dela, on considere qu'il n'y a
// PAS de mur : c'est ce seuil qui declenche la rotation a droite.
// Distance MAX credible (cm) :
const int DISTANCE_MAX_CM = 200;

// Distance min credible (cm) :
const int DISTANCE_MIN_CM = 3;







