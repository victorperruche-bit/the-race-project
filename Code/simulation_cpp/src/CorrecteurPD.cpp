#include "CorrecteurPD.h"
#include <algorithm>  // std::clamp
#include <cmath>      // std::fabs

CorrecteurPD::CorrecteurPD(double kp, double kd, double dt,
                           double sortieMin, double sortieMax,
                           double zoneMorte)
    : kp_(kp)
    , kd_(kd)
    , dt_(dt > 0.0 ? dt : 0.05)           // Protection contre dt = 0
    , sortieMin_(sortieMin)
    , sortieMax_(sortieMax)
    , zoneMorte_(zoneMorte >= 0.0 ? zoneMorte : 0.0)  // Zone morte positive
{
}

double CorrecteurPD::calculer(double erreur)
{
    // --- Zone morte : évite les oscillations autour de 0 ---
    if (std::fabs(erreur) < zoneMorte_) {
        erreurPrecedente_ = erreur;
        derivee_ = 0.0;
        return 0.0;
    }

    // --- Terme proportionnel ---
    double termeP = kp_ * erreur;

    // --- Terme dérivé (avec division par dt) ---
    // Indépendant de la fréquence d'échantillonnage
    double termeD = 0.0;
    if (!premierCalcul_) {
        derivee_ = (erreur - erreurPrecedente_) / dt_;
        termeD = kd_ * derivee_;
    } else {
        derivee_ = 0.0;  // Pas de dérivée au premier calcul (évite l'à-coup)
    }

    erreurPrecedente_ = erreur;
    premierCalcul_    = false;

    // --- Somme + saturation ---
    double correction = termeP + termeD;
    return std::clamp(correction, sortieMin_, sortieMax_);
}

void CorrecteurPD::reinitialiser()
{
    erreurPrecedente_ = 0.0;
    derivee_          = 0.0;
    premierCalcul_    = true;
}
