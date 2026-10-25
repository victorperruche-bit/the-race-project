#ifndef CORRECTEUR_PD_H
#define CORRECTEUR_PD_H

// ============================================================
//  CORRECTEUR PD (Proportionnel - Dérivé)
// ============================================================
class CorrecteurPD {
public:
    CorrecteurPD(double kp, double kd);

    // Calcule la correction à partir de l'erreur actuelle.
    // erreur = distanceDroite - distanceGauche
    // Retourne une valeur de correction (positive = tourner à droite).
    double calculer(double erreur);

    // Réinitialise l'état interne (utile entre deux phases).
    void reinitialiser();

private:
    double kp_;
    double kd_;
    double erreurPrecedente_ = 0.0;
    bool   premierCalcul_    = true;
};

#endif // CORRECTEUR_PD_H
