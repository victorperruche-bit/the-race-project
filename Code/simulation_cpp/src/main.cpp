// ============================================================
//  THE RACE PROJECT - Simulation C++ du robot suiveur de couloir
//  ============================================================
//  Simulation sur PC fidele au robot reel :
//    - 2 moteurs CC (gauche / droit) pilotes par L298N
//    - 2 capteurs a ultrasons HC-SR04 (gauche / droit)
//    - Correcteur PD avec zone morte et saturation
//
//  Scenarios disponibles :
//    1. Centrage simple (robot decale dans un couloir droit)
//    2. Perte de mur (couloir qui s'ouvre a droite)
//    3. Couloir en L (virage a 90 degres a droite)
//    4. Bruit capteurs (mesures bruitees, test robustesse PD)
//    5. Parcours complet (plusieurs segments)
// ============================================================

#include "Robot.h"
#include "Config.h"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <limits>

// ============================================================
//  AFFICHAGE
// ============================================================

// Affiche le couloir en vue de dessus (ASCII art)
// position = distance au mur gauche (cm), largeur = largeur couloir
void afficherCouloirDessus(float position, float largeur, float avance)
{
    constexpr int LARGEUR_BARRE = 50;
    int posBarre = static_cast<int>((position / largeur) * (LARGEUR_BARRE - 1));
    posBarre = std::clamp(posBarre, 0, LARGEUR_BARRE - 1);
    int centreBarre = (LARGEUR_BARRE - 1) / 2;

    // Mur du haut
    std::cout << "  +" << std::string(LARGEUR_BARRE, '=') << "+\n";
    std::cout << "  |";
    for (int i = 0; i < LARGEUR_BARRE; ++i) {
        if (i == posBarre)      std::cout << "R";  // Robot
        else if (i == centreBarre) std::cout << ":";  // Centre
        else                     std::cout << " ";
    }
    std::cout << "|  dG=" << std::fixed << std::setprecision(1) << position
              << "  dD=" << std::setprecision(1) << (largeur - position) << "\n";
    // Mur du bas
    std::cout << "  +" << std::string(LARGEUR_BARRE, '=') << "+";
    std::cout << "  avance=" << std::setprecision(0) << avance << " cm\n";
}

// Affiche une ligne de donnees de la simulation
void afficherLigne(int iter, float dG, float dD, float erreur,
                   int pwmG, int pwmD, bool enRotation, float avance)
{
    std::cout << std::left
              << std::setw(5)  << iter
              << std::setw(8)  << std::fixed << std::setprecision(1) << dG
              << std::setw(8)  << std::fixed << std::setprecision(1) << dD
              << std::setw(8)  << std::fixed << std::setprecision(2) << erreur
              << std::setw(7)  << pwmG
              << std::setw(7)  << pwmD
              << "  ";
    if (enRotation) std::cout << "[ROTATION ->]";
    else            std::cout << "           ";
    std::cout << "  av=" << std::setprecision(0) << avance << "cm\n";
}

void afficherEntete()
{
    std::cout << std::left
              << std::setw(5)  << "Iter"
              << std::setw(8)  << "dG(cm)"
              << std::setw(8)  << "dD(cm)"
              << std::setw(8)  << "err"
              << std::setw(7)  << "PWM_G"
              << std::setw(7)  << "PWM_D"
              << "  Etat          Avance\n";
    std::cout << std::string(70, '-') << "\n";
}

// ============================================================
//  STATISTIQUES DE PERFORMANCE
// ============================================================

struct Stats {
    float erreurMaxAbs = 0.0f;
    float erreurMoyenne = 0.0f;
    int   nbDepassements = 0;      // fois ou |erreur| a augmente par rapport a l'iter precedente
    float erreurPrecedente = 0.0f;
    int   nbIterations = 0;
    int   nbRotations = 0;
    float distanceParcourue = 0.0f;
    float positionInitiale = 0.0f;
    float positionFinale = 0.0f;
    float erreurFinale = 0.0f;

    void enregistrer(float erreur, float /*position*/, float avance, bool enRotation)
    {
        float absErr = std::fabs(erreur);
        erreurMaxAbs = std::max(erreurMaxAbs, absErr);
        erreurMoyenne = (erreurMoyenne * nbIterations + absErr) / (nbIterations + 1);
        if (absErr > std::fabs(erreurPrecedente) && nbIterations > 0) {
            nbDepassements++;
        }
        erreurPrecedente = erreur;
        if (enRotation) nbRotations++;
        distanceParcourue += avance;
        nbIterations++;
    }

    void afficher() const
    {
        std::cout << "\n--- Statistiques de performance ---\n";
        std::cout << "  Iterations totales   : " << nbIterations << "\n";
        std::cout << "  Erreur max absolue   : " << std::fixed << std::setprecision(2)
                  << erreurMaxAbs << " cm\n";
        std::cout << "  Erreur moyenne       : " << std::setprecision(2)
                  << erreurMoyenne << " cm\n";
        std::cout << "  Depassements (osc.)  : " << nbDepassements << "\n";
        std::cout << "  Rotations d'urgence  : " << nbRotations << "\n";
        std::cout << "  Distance parcourue   : " << std::setprecision(1)
                  << distanceParcourue << " cm\n";
        std::cout << "  Position initiale    : " << std::setprecision(1)
                  << positionInitiale << " cm\n";
        std::cout << "  Position finale      : " << std::setprecision(1)
                  << positionFinale << " cm\n";
        std::cout << "  Erreur finale        : " << std::setprecision(2)
                  << erreurFinale << " cm\n";
        // Critere de reussite : erreur finale < 3 cm (dans la zone morte + marge)
        float centre = config::LARGEUR_COULOIR_CM / 2.0f;
        float ecartFinal = std::fabs(positionFinale - centre);
        std::cout << "  Ecart au centre      : " << std::setprecision(2)
                  << ecartFinal << " cm  ";
        if (ecartFinal < 3.0f) {
            std::cout << "[OK - centre atteint]\n";
        } else if (ecartFinal < 8.0f) {
            std::cout << "[MOYEN - proche du centre]\n";
        } else {
            std::cout << "[ECHEC - trop eloigne du centre]\n";
        }
    }
};

// ============================================================
//  MODELES DE COULOIR
// ============================================================

// Couloir droit : retourne les distances aux murs gauche et droit
void couloirDroit(float position, float largeur, float& dG, float& dD)
{
    dG = position;
    dD = largeur - position;
}

// Couloir en L : apres un certain avancement, le mur droit disparait
void couloirEnL(float position, float largeur, float avance,
                float debutVirage, float& dG, float& dD)
{
    couloirDroit(position, largeur, dG, dD);
    if (avance > debutVirage) {
        // Dans le virage : le mur droit disparait progressivement
        // -> la distance au mur droit augmente jusqu'a depasser la portee max
        float progressionVirage = (avance - debutVirage) / 80.0f;
        if (progressionVirage > 1.0f) progressionVirage = 1.0f;
        dD = largeur - position + progressionVirage * 250.0f;  // mur droit s'eloigne fortement
        if (dD > config::DISTANCE_MAX_CM) dD = -1.0f;  // hors portee -> perte detectee
    }
}

// ============================================================
//  GENERATEUR DE BRUIT (pour scenario 4)
// ============================================================

std::mt19937 generateurBruit(42);  // seed fixe pour reproductibilite

float ajouterBruit(float valeur, float amplitudeCm)
{
    std::normal_distribution<float> dist(0.0f, amplitudeCm);
    float bruite = valeur + dist(generateurBruit);
    // Un capteur reel ne mesure jamais une distance negative :
    // on borne a 0 (mur tres proche) ou on laisse le capteur gerer
    return std::max(0.0f, bruite);
}

// ============================================================
//  MOTEUR DE SIMULATION
// ============================================================

struct ResultatSimulation {
    Stats stats;
    std::vector<float> historiqueErreur;
    std::vector<float> historiquePosition;
};

// Execute une simulation avec un modele de couloir donne
// modeleCouloir : 0 = droit, 1 = en L
// bruitCm : amplitude du bruit ajoute aux capteurs (0 = pas de bruit)
// dureeMax : nombre max d'iterations
// verbose : affiche chaque N iterations
ResultatSimulation executerSimulation(Robot& robot, float positionInitiale,
                                       int modeleCouloir, float bruitCm,
                                       int dureeMax, int verboseEvery,
                                       bool modePasAPas)
{
    ResultatSimulation resultat;
    float position = positionInitiale;
    float avance = 0.0f;
    float largeur = config::LARGEUR_COULOIR_CM;
    resultat.stats.positionInitiale = positionInitiale;

    afficherEntete();

    for (int i = 0; i < dureeMax; ++i) {
        // --- Calcul des distances selon le modele de couloir ---
        float dG, dD;
        if (modeleCouloir == 1) {
            couloirEnL(position, largeur, avance, 200.0f, dG, dD);
        } else {
            couloirDroit(position, largeur, dG, dD);
        }

        // --- Ajout de bruit si demande ---
        if (bruitCm > 0.0f) {
            dG = ajouterBruit(dG, bruitCm);
            dD = ajouterBruit(dD, bruitCm);
        }

        // --- Execution d'un pas de regulation ---
        robot.setDistances(dG, dD);
        bool enRotation = robot.etapeRegulation();

        // --- Mise a jour de la simulation ---
        position = robot.positionSimulee();

        // Avance = composante longitudinale (approximation)
        int pwmG = robot.pwmGauche();
        int pwmD = robot.pwmDroit();
        float vitesseMoyenne = (std::abs(pwmG) + std::abs(pwmD)) / 2.0f;
        float avanceIter = (vitesseMoyenne / config::PWM_MAX) * 2.0f;  // ~2 cm max par iter
        if (enRotation) avanceIter *= 0.3f;  // moins d'avance en rotation
        avance += avanceIter;

        // --- Enregistrement des stats ---
        resultat.stats.enregistrer(robot.erreur(), position, avanceIter, enRotation);
        resultat.historiqueErreur.push_back(robot.erreur());
        resultat.historiquePosition.push_back(position);

        // --- Affichage ---
        bool afficher = (verboseEvery > 0 && (i % verboseEvery == 0 || i == dureeMax - 1))
                        || enRotation;
        if (afficher) {
            float dGaff = (dG < 0) ? -1.0f : dG;
            float dDaff = (dD < 0) ? -1.0f : dD;
            afficherLigne(i, dGaff, dDaff, robot.erreur(), pwmG, pwmD, enRotation, avance);
        }

        // --- Mode pas a pas : attend l'appui sur Entree ---
        if (modePasAPas && i < dureeMax - 1) {
            std::cout << "  [Entree = continuer, q = quitter] ";
            std::string saisie;
            std::getline(std::cin, saisie);
            if (saisie == "q" || saisie == "Q") {
                std::cout << "  Simulation interrompue par l'utilisateur.\n";
                break;
            }
        }
    }

    resultat.stats.positionFinale = position;
    resultat.stats.erreurFinale = robot.erreur();
    return resultat;
}

// ============================================================
//  SCENARIOS
// ============================================================

void scenario1_centrageSimple()
{
    std::cout << "\n========================================\n";
    std::cout << "  SCENARIO 1 : Centrage simple\n";
    std::cout << "  Robot place a 20 cm du mur gauche (centre = 40 cm)\n";
    std::cout << "  Objectif : se recentrer au milieu du couloir\n";
    std::cout << "========================================\n\n";

    Robot robot;
    auto resultat = executerSimulation(robot, 20.0f, 0, 0.0f, 150, 10, false);
    resultat.stats.afficher();
}

void scenario2_perteMur()
{
    std::cout << "\n========================================\n";
    std::cout << "  SCENARIO 2 : Perte du mur droit\n";
    std::cout << "  Le couloir s'ouvre a droite -> rotation d'urgence\n";
    std::cout << "========================================\n\n";

    Robot robot;
    float position = 40.0f;
    float largeur = config::LARGEUR_COULOIR_CM;

    afficherEntete();

    Stats stats;
    stats.positionInitiale = position;
    float avance = 0.0f;

    for (int i = 0; i < 80; ++i) {
        float dG, dD;
        if (i < 20) {
            // Couloir normal
            couloirDroit(position, largeur, dG, dD);
        } else {
            // Mur droit perdu
            dG = position;
            dD = -1.0f;  // hors portee
        }

        robot.setDistances(dG, dD);
        bool enRotation = robot.etapeRegulation();
        position = robot.positionSimulee();

        int pwmG = robot.pwmGauche();
        int pwmD = robot.pwmDroit();
        float vitesseMoy = (std::abs(pwmG) + std::abs(pwmD)) / 2.0f;
        float avanceIter = (vitesseMoy / config::PWM_MAX) * 2.0f * (enRotation ? 0.3f : 1.0f);
        avance += avanceIter;

        stats.enregistrer(robot.erreur(), position, avanceIter, enRotation);

        if (i % 5 == 0 || enRotation || i == 79) {
            float dGaff = (dG < 0) ? -1.0f : dG;
            float dDaff = (dD < 0) ? -1.0f : dD;
            afficherLigne(i, dGaff, dDaff, robot.erreur(), pwmG, pwmD, enRotation, avance);
        }
    }

    stats.positionFinale = position;
    stats.erreurFinale = robot.erreur();
    stats.afficher();
}

void scenario3_couloirEnL()
{
    std::cout << "\n========================================\n";
    std::cout << "  SCENARIO 3 : Couloir en L (virage a droite)\n";
    std::cout << "  Apres 200 cm d'avance, le mur droit disparait\n";
    std::cout << "  Le robot doit detecter la perte et tourner\n";
    std::cout << "========================================\n\n";

    Robot robot;
    auto resultat = executerSimulation(robot, 40.0f, 1, 0.0f, 300, 15, false);
    resultat.stats.afficher();
}

void scenario4_bruitCapteurs()
{
    std::cout << "\n========================================\n";
    std::cout << "  SCENARIO 4 : Bruit sur les capteurs\n";
    std::cout << "  Bruit gaussien de +/-3 cm sur les mesures\n";
    std::cout << "  Test de robustesse du correcteur PD\n";
    std::cout << "========================================\n\n";

    Robot robot;
    auto resultat = executerSimulation(robot, 25.0f, 0, 3.0f, 200, 10, false);
    resultat.stats.afficher();
}

void scenario5_parcoursComplet()
{
    std::cout << "\n========================================\n";
    std::cout << "  SCENARIO 5 : Parcours complet\n";
    std::cout << "  Segment 1 : couloir droit (150 iter)\n";
    std::cout << "  Segment 2 : couloir en L (200 iter)\n";
    std::cout << "  Segment 3 : retour couloir droit (150 iter, leger bruit)\n";
    std::cout << "========================================\n\n";

    Robot robot;
    Stats statsGlobales;
    float position = 30.0f;
    float avanceTotale = 0.0f;

    // Segment 1 : couloir droit
    std::cout << "--- Segment 1 : Couloir droit ---\n";
    auto res1 = executerSimulation(robot, position, 0, 0.0f, 150, 25, false);
    statsGlobales.nbIterations += res1.stats.nbIterations;
    statsGlobales.erreurMaxAbs = std::max(statsGlobales.erreurMaxAbs, res1.stats.erreurMaxAbs);
    statsGlobales.nbRotations += res1.stats.nbRotations;
    position = res1.stats.positionFinale;
    avanceTotale += res1.stats.distanceParcourue;

    // Segment 2 : couloir en L (on repart du centre)
    std::cout << "\n--- Segment 2 : Virage (couloir en L) ---\n";
    Robot robot2;
    auto res2 = executerSimulation(robot2, 40.0f, 1, 0.0f, 200, 25, false);
    statsGlobales.nbIterations += res2.stats.nbIterations;
    statsGlobales.erreurMaxAbs = std::max(statsGlobales.erreurMaxAbs, res2.stats.erreurMaxAbs);
    statsGlobales.nbRotations += res2.stats.nbRotations;
    position = res2.stats.positionFinale;
    avanceTotale += res2.stats.distanceParcourue;

    // Segment 3 : retour couloir droit
    std::cout << "\n--- Segment 3 : Couloir droit (retour) ---\n";
    Robot robot3;
    auto res3 = executerSimulation(robot3, position, 0, 1.0f, 150, 25, false);
    statsGlobales.nbIterations += res3.stats.nbIterations;
    statsGlobales.erreurMaxAbs = std::max(statsGlobales.erreurMaxAbs, res3.stats.erreurMaxAbs);
    statsGlobales.nbRotations += res3.stats.nbRotations;
    statsGlobales.distanceParcourue = avanceTotale + res3.stats.distanceParcourue;
    statsGlobales.positionInitiale = 30.0f;
    statsGlobales.positionFinale = res3.stats.positionFinale;
    statsGlobales.erreurFinale = res3.stats.erreurFinale;

    statsGlobales.afficher();
}

// ============================================================
//  MENU PRINCIPAL
// ============================================================

void afficherMenu()
{
    std::cout << "\n========================================\n";
    std::cout << "  THE RACE PROJECT - Simulation PC\n";
    std::cout << "  Robot suiveur de couloir\n";
    std::cout << "  2 moteurs (G/D) + 2 capteurs US (G/D)\n";
    std::cout << "========================================\n\n";
    std::cout << "  Configuration :\n";
    std::cout << "    Largeur couloir : " << config::LARGEUR_COULOIR_CM << " cm\n";
    std::cout << "    KP = " << config::KP << "   KD = " << config::KD
              << "   dt = " << config::DT_S << " s\n";
    std::cout << "    Vitesse base    : " << config::VITESSE_BASE << "/255\n";
    std::cout << "    Zone morte      : " << config::ZONE_MORTE_CM << " cm\n\n";
    std::cout << "  Scenarios :\n";
    std::cout << "    1. Centrage simple (robot decale a 20 cm)\n";
    std::cout << "    2. Perte du mur droit (rotation d'urgence)\n";
    std::cout << "    3. Couloir en L (virage a 90 degres)\n";
    std::cout << "    4. Bruit capteurs (+/-3 cm, test robustesse)\n";
    std::cout << "    5. Parcours complet (3 segments)\n";
    std::cout << "    6. Tous les scenarios\n";
    std::cout << "    0. Quitter\n\n";
    std::cout << "  Choix : ";
}

int main()
{
    int choix = -1;
    while (choix != 0) {
        afficherMenu();
        std::cin >> choix;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choix) {
            case 1: scenario1_centrageSimple(); break;
            case 2: scenario2_perteMur(); break;
            case 3: scenario3_couloirEnL(); break;
            case 4: scenario4_bruitCapteurs(); break;
            case 5: scenario5_parcoursComplet(); break;
            case 6:
                scenario1_centrageSimple();
                scenario2_perteMur();
                scenario3_couloirEnL();
                scenario4_bruitCapteurs();
                scenario5_parcoursComplet();
                break;
            case 0:
                std::cout << "\nAu revoir !\n";
                break;
            default:
                std::cout << "\nChoix invalide.\n";
                break;
        }
    }

    return 0;
}
