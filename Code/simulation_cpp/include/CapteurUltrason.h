#ifndef CAPTEUR_ULTRASON_H
#define CAPTEUR_ULTRASON_H

#include <string>

// ============================================================
//  CAPTEUR À ULTRASONS HC-SR04 (simulation)
// ============================================================
class CapteurUltrason {
public:
    // etiquette : "Gauche" ou "Droit"
    explicit CapteurUltrason(const std::string& etiquette);

    // Définit la distance réelle au mur (cm). En simulation, on injecte
    // directement la distance ; sur Arduino, ce serait pulseIn().
    void setDistance(float distanceCm);

    // Lit la distance mesurée (cm). Retourne -1 si hors portée ou mesure invalide.
    float mesurer() const;

    // Nom du capteur (pour l'affichage)
    const std::string& etiquette() const { return etiquette_; }

private:
    std::string etiquette_;
    float distance_ = -1.0f;  // -1 = pas de mesure valide
};

#endif // CAPTEUR_ULTRASON_H
