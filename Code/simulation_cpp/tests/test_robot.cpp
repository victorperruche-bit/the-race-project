/* =====================================================================
   TESTS AUTOMATIQUES - Robot suiveur de couloir
   ---------------------------------------------------------------------
   Objectifs verifies (cf. robot_suiveur_couloir.ino) :
     1. RECENTRAGE : le robot corrige sa position pour se placer au
        centre du couloir (dG == dD -> tout droit, sinon virage du bon cote).
     2. ROTATION A DROITE : si une distance est hors portee
        (> DISTANCE_MAX_CM ou mesure invalide), le robot tourne a droite
        (roue gauche en avant, roue droite en arriere).

   Compilation / execution (depuis la racine du projet) :
     g++ -std=c++17 -Wall -Wextra -I Code/simulation_cpp/include \
         Code/simulation_cpp/tests/test_robot.cpp \
         Code/simulation_cpp/src/Robot.cpp \
         Code/simulation_cpp/src/CapteurUltrason.cpp \
         Code/simulation_cpp/src/CorrecteurPD.cpp \
         -o Code/simulation_cpp/build/test_robot.exe
     ./Code/simulation_cpp/build/test_robot.exe

   Le programme renvoie 0 si tous les tests passent, 1 sinon.
   ===================================================================== */

#include "Robot.h"
#include "CapteurUltrason.h"
#include "CorrecteurPD.h"
#include "Config.h"

#include <cmath>
#include <cstdio>
#include <string>

/* ------------------- Mini framework d'assertions ------------------- */
static int testsReussis = 0;
static int testsEchoues = 0;
static std::string testCourant;

#define TEST(nom)                                        \
    do {                                                 \
        testCourant = (nom);                             \
        std::printf("  [TEST] %-45s ", (nom));           \
    } while (0)

static void verifier(bool condition, const char* message)
{
    if (condition) {
        std::printf("OK\n");
        ++testsReussis;
    } else {
        std::printf("ECHEC  <-- %s\n", message);
        ++testsEchoues;
    }
}

/* Tolérance de comparaison pour les flottants */
static bool presqueEgal(double a, double b, double tol = 1e-6)
{
    return std::fabs(a - b) <= tol;
}

/* =================================================================== */
/* 1. CAPTEUR : validation des distances hors portee                    */
/* =================================================================== */
static void testCapteurHorsPortee()
{
    TEST("Capteur : distance valide mesuree telle quelle");
    CapteurUltrason capteur("Test");
    capteur.setDistance(42.0f);
    verifier(presqueEgal(capteur.mesurer(), 42.0f),
             "Une distance dans [3;200] cm doit etre renvoyee telle quelle");

    TEST("Capteur : distance > 200 cm => -1 (hors portee)");
    capteur.setDistance(250.0f);
    verifier(presqueEgal(capteur.mesurer(), -1.0f),
             "Au-dela de DISTANCE_MAX_CM, le capteur doit renvoyer -1");

    TEST("Capteur : distance < 3 cm => -1 (mesure aberrante)");
    capteur.setDistance(2.0f);
    verifier(presqueEgal(capteur.mesurer(), -1.0f),
             "En dessous de DISTANCE_MIN_CM, le capteur doit renvoyer -1");
}

/* =================================================================== */
/* 2. RECENTRAGE : le robot doit se placer au centre du couloir         */
/* =================================================================== */
static void testRecentrage()
{
    Robot robot;

    TEST("Recentrage : dG == dD => tout droit (PWM egaux)");
    robot.setDistances(40.0f, 40.0f);   // exactement au centre
    bool rotation = robot.etapeRegulation();
    verifier(!rotation && robot.pwmGauche() == robot.pwmDroit(),
             "Au centre, les deux moteurs doivent avoir la meme vitesse");

    TEST("Recentrage : robot trop a gauche => tourne a droite");
    robot.setDistances(10.0f, 70.0f);   // mur droit plus loin => trop a gauche
    rotation = robot.etapeRegulation();
    verifier(!rotation && robot.pwmGauche() > robot.pwmDroit(),
             "Trop a gauche, la roue gauche doit accelerer (virage a droite)");

    TEST("Recentrage : robot trop a droite => tourne a gauche");
    robot.setDistances(70.0f, 10.0f);   // mur gauche plus loin => trop a droite
    rotation = robot.etapeRegulation();
    verifier(!rotation && robot.pwmDroit() > robot.pwmGauche(),
             "Trop a droite, la roue droite doit accelerer (virage a gauche)");

    TEST("Recentrage : erreur tres grande => PWM satures (securite)");
    robot.setDistances(3.0f, 200.0f);   // ecart maximal
    rotation = robot.etapeRegulation();
    verifier(!rotation
                 && robot.pwmGauche() == config::PWM_MAX
                 && robot.pwmDroit()  == config::PWM_MINI,
             "Avec l'ecart maximal, la correction doit etre saturee");
}

/* Convergence : le robot desaxe doit revenir vers le centre.
   On simule la physique du couloir (largeur 80 cm) :
     - position x (0 = mur gauche, 80 = mur droit)
     - dG = x et dD = 80 - x
     - un ecart de vitesse fait deriver x vers le mur oppose a la roue rapide.
   On verifie qu'apres N iterations le robot est revenu pres du centre. */
static void testConvergenceCentre()
{
    TEST("Convergence : le robot desaxe revient au centre");
    Robot robot;
    const double largeur = 80.0;
    double x = 5.0;                       // robot tres desaxe (pres du mur gauche)
    bool erreurRotation = false;

    for (int i = 0; i < 300; ++i) {
        robot.setDistances(static_cast<float>(x),
                           static_cast<float>(largeur - x));
        if (robot.etapeRegulation()) {    // ne doit JAMAIS tourner ici
            erreurRotation = true;
            break;
        }
        // Modele physique simple : l'ecart de PWM fait glisser le robot
        double ecartVitesse = (robot.pwmGauche() - robot.pwmDroit())
                              / static_cast<double>(config::PWM_MAX);
        x += ecartVitesse * 2.0;           // ~2 cm d'effet lateral max par pas
        if (x < 0.0) x = 0.0;
        if (x > largeur) x = largeur;
    }

    const double ecartFinal = std::fabs(x - largeur / 2.0);
    verifier(!erreurRotation && ecartFinal <= 1.5,
             "Le robot doit revenir a moins de 1,5 cm du centre sans jamais "
             "passer en rotation");
}

/* =================================================================== */
/* 3. ROTATION A DROITE : distance hors portee                          */
/* =================================================================== */
static void testRotationDroite()
{
    Robot robot;

    TEST("Rotation : mur DROIT hors portee => rotation a droite");
    robot.setDistances(40.0f, 250.0f);    // ouverture a droite (> 200 cm)
    bool rotation = robot.etapeRegulation();
    verifier(rotation
                 && robot.enRotation()
                 && robot.pwmGauche() > 0
                 && robot.pwmDroit()  < 0,
             "Mur droit perdu : roue gauche en AVANT, roue droite en ARRIERE");

    TEST("Rotation : mur GAUCHE hors portee => rotation a droite");
    robot.setDistances(250.0f, 40.0f);    // ouverture a gauche (> 200 cm)
    rotation = robot.etapeRegulation();
    verifier(rotation
                 && robot.pwmGauche() > 0
                 && robot.pwmDroit()  < 0,
             "Mur gauche perdu : le robot doit aussi tourner a droite");

    TEST("Rotation : les DEUX murs hors portee => rotation a droite");
    robot.setDistances(-1.0f, -1.0f);     // plus aucun echo
    rotation = robot.etapeRegulation();
    verifier(rotation && robot.pwmGauche() > 0 && robot.pwmDroit() < 0,
             "Aucun mur detecte : le robot tourne a droite");

    TEST("Retour au suivi : murs retrouves => sortie de la rotation");
    robot.setDistances(35.0f, 45.0f);     // les deux murs reviennent
    rotation = robot.etapeRegulation();
    verifier(!rotation
                 && !robot.enRotation()
                 && robot.pwmGauche() > 0
                 && robot.pwmDroit()  > 0,
             "Murs retrouves : les deux roues repartent en avant");
}

/* =================================================================== */
/* 4. CORRECTEUR PD : zone morte et reinitialisation                    */
/* =================================================================== */
static void testCorrecteur()
{
    TEST("Correcteur : erreur dans la zone morte => correction nulle");
    CorrecteurPD correcteur(config::KP, config::KD, config::DT_S,
                            -config::CORRECTION_MAX, config::CORRECTION_MAX,
                            config::ZONE_MORTE_CM);
    verifier(presqueEgal(correcteur.calculer(1.0), 0.0),
             "Une erreur de 1 cm (< zone morte de 2 cm) ne doit rien corriger");

    TEST("Correcteur : erreur reelle => correction du bon signe");
    correcteur.reinitialiser();
    double c = correcteur.calculer(10.0);
    verifier(c > 0.0,
             "Erreur positive (trop a gauche) => correction positive (droite)");
    c = correcteur.calculer(-10.0);
    verifier(c < 0.0,
             "Erreur negative (trop a droite) => correction negative (gauche)");

    TEST("Correcteur : saturation de la correction");
    correcteur.reinitialiser();
    double saturation = correcteur.calculer(10000.0);
    verifier(presqueEgal(saturation, config::CORRECTION_MAX),
             "Une erreur enorme doit etre saturee a CORRECTION_MAX");
}

/* =================================================================== */
int main()
{
    std::printf("=====================================================\n");
    std::printf("  TESTS - Robot suiveur de couloir\n");
    std::printf("  (recentrage au centre + rotation hors portee)\n");
    std::printf("=====================================================\n\n");

    testCapteurHorsPortee();
    std::printf("\n");
    testRecentrage();
    std::printf("\n");
    testConvergenceCentre();
    std::printf("\n");
    testRotationDroite();
    std::printf("\n");
    testCorrecteur();

    std::printf("\n=====================================================\n");
    std::printf("  Resultat : %d reussis, %d echoues, %d au total\n",
                testsReussis, testsEchoues, testsReussis + testsEchoues);
    std::printf("=====================================================\n");

    return (testsEchoues == 0) ? 0 : 1;
}
