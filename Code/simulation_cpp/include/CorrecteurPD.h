#ifndef CORRECTEUR_PD_H
#define CORRECTEUR_PD_H

// ============================================================
//  CORRECTEUR PD (Proportionnel - Dérivé)
//  Avec pas de temps, saturation et zone morte
// ============================================================
class CorrecteurPD {
public:
    // kp        : gain proportionnel (PWM/cm)
    // kd        : gain dérivé (PWM·s/cm)
    // dt        : pas de temps entre deux appels de calculer() (secondes)
    // sortieMin : saturation minimale de la correction (typiquement -255)
    // sortieMax : saturation maximale de la correction (typiquement +255)
    // zoneMorte : |erreur| < zoneMorte → correction = 0 (évite les oscillations)
    CorrecteurPD(double kp, double kd, double dt = 0.05,
                 double sortieMin = -255.0, double sortieMax = 255.0,
                 double zoneMorte = 0.0);

    // Calcule la correction à partir de l'erreur actuelle (cm).
    // erreur = distanceDroite - distanceGauche
    // Retourne la correction saturée (positive = tourner à droite).
    double calculer(double erreur);

    // Réinitialise l'état interne (utile entre deux phases).
    void reinitialiser();

    // --- Getters pour debug ---
    double erreurPrecedente() const { return erreurPrecedente_; }
    double derivee()          const { return derivee_; }

private:
    double kp_;
    double kd_;
    double dt_;            // Pas de temps (s) — évite la dépendance à la fréquence
    double sortieMin_;     // Saturation basse
    double sortieMax_;     // Saturation haute
    double zoneMorte_;     // Zone morte (cm)
    double erreurPrecedente_ = 0.0;
    double derivee_          = 0.0;
    bool   premierCalcul_    = true;
};

#endif // CORRECTEUR_PD_H
