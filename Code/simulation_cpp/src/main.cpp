// ============================================================
//  THE RACE PROJECT - Simulation C++ du robot suiveur de couloir
//  ============================================================
//  Simulation sur PC de l'algorithme de suivi de couloir.
//  Le robot est placé dans un couloir virtuel et on observe
//  comment il se recentre entre les deux murs.
// ============================================================

#include "Robot.h"
#include "Config.h"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <algorithm>

// Affiche une barre visuelle de la position du robot dans le couloir
void afficherCouloir(float position, float largeur)
{
    constexpr int LARGEUR_BARRE = 60;
    int posBarre = static_cast<int>((position / largeur) * LARGEUR_BARRE);
    posBarre = std::clamp(posBarre, 0, LARGEUR_BARRE - 1);

    std::cout << "|";
    for (int i = 0; i < LARGEUR_BARRE; ++i) {
        if (i == posBarre) {
            std::cout << "R";  // Robot
        } else if (i == LARGEUR_BARRE / 2) {
            std::cout << ":";  // Centre du couloir
        } else {
            std::cout << " ";
        }
    }
    std::cout << "|  pos=" << std::fixed << std::setprecision(1)
              << position << " cm\n";
}

// Calcule les distances aux murs à partir de la position simulée
// dans le couloir (0 = mur gauche, largeur = mur droit)
void calculerDistances(float position, float largeur,
                       float& distGauche, float& distDroite)
{
    distGauche = position;
    distDroite = largeur - position;
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "  THE RACE PROJECT - Simulation C++\n";
    std::cout << "  Robot suiveur de couloir\n";
    std::cout << "========================================\n\n";

    std::cout << "Configuration :\n";
    std::cout << "  Largeur couloir : " << config::LARGEUR_COULOIR_CM << " cm\n";
    std::cout << "  Position depart : " << config::POSITION_DEPART_CM << " cm\n";
    std::cout << "  KP = " << config::KP << "  KD = " << config::KD << "\n";
    std::cout << "  Vitesse base    : " << config::VITESSE_BASE << "\n";
    std::cout << "  Iterations      : " << config::NB_ITERATIONS << "\n\n";

    Robot robot;

    // --- Scénario 1 : robot décalé à gauche, couloir normal ---
    std::cout << "--- Scenario 1 : robot decale a GAUCHE (20 cm) ---\n\n";

    float position = 20.0f;  // robot à 20 cm du mur gauche (centre = 40)
    float distG, distD;

    std::cout << std::left
              << std::setw(6)  << "Iter"
              << std::setw(10) << "dGauche"
              << std::setw(10) << "dDroit"
              << std::setw(10) << "erreur"
              << std::setw(10) << "PWM_G"
              << std::setw(10) << "PWM_D"
              << "  Couloir\n";
    std::cout << std::string(76, '-') << "\n";

    for (int i = 0; i < config::NB_ITERATIONS; ++i) {
        calculerDistances(position, config::LARGEUR_COULOIR_CM, distG, distD);
        robot.setDistances(distG, distD);
        robot.etapeRegulation();

        // Affichage toutes les 10 itérations
        if (i % 10 == 0 || i == config::NB_ITERATIONS - 1) {
            std::cout << std::left
                      << std::setw(6)  << i
                      << std::setw(10) << std::fixed << std::setprecision(1) << distG
                      << std::setw(10) << std::fixed << std::setprecision(1) << distD
                      << std::setw(10) << std::fixed << std::setprecision(2) << robot.erreur()
                      << std::setw(10) << robot.pwmGauche()
                      << std::setw(10) << robot.pwmDroit()
                      << "  ";
            afficherCouloir(position, config::LARGEUR_COULOIR_CM);
        }

        // Mise à jour de la position simulée
        position = robot.positionSimulee();
    }

    // --- Scénario 2 : perte du mur droit (couloir qui s'ouvre) ---
    std::cout << "\n--- Scenario 2 : PERTE DU MUR DROIT ---\n\n";

    position = 40.0f;  // retour au centre
    robot = Robot();   // réinitialise le robot

    for (int i = 0; i < 30; ++i) {
        if (i < 10) {
            // Couloir normal
            calculerDistances(position, config::LARGEUR_COULOIR_CM, distG, distD);
        } else {
            // Mur droit disparaît (distance > portée max)
            distG = position;
            distD = 999.0f;  // hors portée
        }

        robot.setDistances(distG, distD);
        bool enRot = robot.etapeRegulation();

        if (i % 5 == 0 || enRot) {
            std::cout << std::left
                      << std::setw(6)  << i
                      << std::setw(10) << std::fixed << std::setprecision(1) << distG
                      << std::setw(10) << (distD > 500 ? std::string("---") : std::to_string(distD).substr(0,5))
                      << std::setw(10) << std::fixed << std::setprecision(2) << robot.erreur()
                      << std::setw(10) << robot.pwmGauche()
                      << std::setw(10) << robot.pwmDroit()
                      << "  " << (enRot ? "[ROTATION DROITE]" : "")
                      << "\n";
        }

        if (i >= 10) {
            // En rotation, on revient progressivement vers le centre
            position = robot.positionSimulee();
        } else {
            position = robot.positionSimulee();
        }
    }

    // --- Résumé ---
    std::cout << "\n========================================\n";
    std::cout << "  Simulation terminee.\n";
    std::cout << "========================================\n";

    return 0;
}
