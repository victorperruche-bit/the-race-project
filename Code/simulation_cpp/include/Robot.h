#ifndef ROBOT_H
#define ROBOT_H

#include "CapteurUltrason.h"
#include "CorrecteurPD.h"

// ============================================================
//  ROBOT SUIVEUR DE COULOIR (simulation)
// ============================================================
class Robot {
public:
    Robot();

    // Met à jour les distances mesurées par les capteurs (cm).
    void setDistances(float distanceGauche, float distanceDroite);

    // Exécute un pas de régulation (une itération de la boucle).
    // Retourne true si le robot est en mode "rotation d'urgence".
    bool etapeRegulation();

    // --- Getters pour l'affichage / debug ---
    int   pwmGauche()      const { return pwmGauche_; }
    int   pwmDroit()       const { return pwmDroit_; }
    float erreur()         const { return erreur_; }
    bool  enRotation()     const { return enRotation_; }
    float positionSimulee() const { return positionSimulee_; }

private:
    CapteurUltrason capteurGauche_;
    CapteurUltrason capteurDroit_;
    CorrecteurPD    correcteur_;

    // État interne
    int   pwmGauche_       = 0;
    int   pwmDroit_        = 0;
    float erreur_          = 0.0f;
    bool  enRotation_      = false;

    // Simulation de la position dans le couloir (0 = mur gauche, largeur = mur droit)
    float positionSimulee_ = 0.0f;

    // Applique la correction PD aux vitesses des moteurs
    void appliquerCorrection();

    // Applique le ralentissement progressif selon l'écart
    int  appliquerRalentissement(int vitesseBase) const;

    // Rotation d'urgence à droite (quand un mur est perdu)
    void rotationDroite();
};

#endif // ROBOT_H
