#include "Robot.h"
#include "Config.h"
#include <algorithm>
#include <cmath>

Robot::Robot()
    : capteurGauche_("Gauche")
    , capteurDroit_("Droit")
    , correcteur_(config::KP, config::KD)
    , positionSimulee_(config::POSITION_DEPART_CM)
{
}

void Robot::setDistances(float distanceGauche, float distanceDroite)
{
    capteurGauche_.setDistance(distanceGauche);
    capteurDroit_.setDistance(distanceDroite);
}

bool Robot::etapeRegulation()
{
    float dG = capteurGauche_.mesurer();
    float dD = capteurDroit_.mesurer();

    // --- Détection perte de mur ---
    bool murPerdu = (dG < 0.0f) || (dD < 0.0f);

    if (murPerdu) {
        rotationDroite();
        return true;
    }

    // --- Calcul de l'erreur : dD - dG ---
    // erreur > 0  => mur droit plus loin => robot trop à gauche => tourner à droite
    // erreur < 0  => mur gauche plus loin => robot trop à droite => tourner à gauche
    erreur_ = dD - dG;

    // --- Correction PD ---
    double correction = correcteur_.calculer(erreur_);

    // --- Application aux moteurs ---
    int vitesseBase = config::VITESSE_BASE;
    vitesseBase = appliquerRalentissement(vitesseBase);

    // La correction ajuste différentiellement les deux moteurs
    int pwmG = vitesseBase + static_cast<int>(correction);
    int pwmD = vitesseBase - static_cast<int>(correction);

    // Saturation
    pwmGauche_ = std::clamp(pwmG, config::PWM_MINI, config::PWM_MAX);
    pwmDroit_  = std::clamp(pwmD, config::PWM_MINI, config::PWM_MAX);

    enRotation_ = false;

    // --- Mise à jour de la position simulée ---
    // On simule un déplacement proportionnel à la différence de vitesse
    float diffVitesse = static_cast<float>(pwmDroit_ - pwmGauche_) / config::PWM_MAX;
    // Facteur de déplacement (cm par itération)
    constexpr float DEPLACEMENT_MAX_CM = 2.0f;
    positionSimulee_ += diffVitesse * DEPLACEMENT_MAX_CM;

    // Limites du couloir
    positionSimulee_ = std::clamp(positionSimulee_, 0.0f,
                                   static_cast<float>(config::LARGEUR_COULOIR_CM));

    return false;
}

int Robot::appliquerRalentissement(int vitesseBase) const
{
    float ecart = std::abs(erreur_);

    if (ecart > config::SEUIL_RALENTI_2) {
        return static_cast<int>(vitesseBase * config::COEF_RALENTI_2);
    }
    if (ecart > config::SEUIL_RALENTI_1) {
        return static_cast<int>(vitesseBase * config::COEF_RALENTI_1);
    }
    return vitesseBase;
}

void Robot::rotationDroite()
{
    // Rotation sur place : roue gauche avance, roue droite recule
    pwmGauche_ = config::VITESSE_BASE;
    pwmDroit_  = -config::VITESSE_BASE;
    enRotation_ = true;

    // En simulation, on fait tourner le robot vers la droite
    positionSimulee_ += 3.0f;
    positionSimulee_ = std::clamp(positionSimulee_, 0.0f,
                                   static_cast<float>(config::LARGEUR_COULOIR_CM));
}
