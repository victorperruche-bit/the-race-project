#include "CorrecteurPD.h"

CorrecteurPD::CorrecteurPD(double kp, double kd)
    : kp_(kp), kd_(kd)
{
}

double CorrecteurPD::calculer(double erreur)
{
    double termeP = kp_ * erreur;

    double termeD = 0.0;
    if (!premierCalcul_) {
        termeD = kd_ * (erreur - erreurPrecedente_);
    }

    erreurPrecedente_ = erreur;
    premierCalcul_    = false;

    return termeP + termeD;
}

void CorrecteurPD::reinitialiser()
{
    erreurPrecedente_ = 0.0;
    premierCalcul_    = true;
}
