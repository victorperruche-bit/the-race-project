#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  CONFIGURATION DU ROBOT SUIVEUR DE COULOIR
//  (Version simulation C++ - miroir du code Arduino)
// ============================================================

namespace config {

// --- Capteurs à ultrasons ---
// Portée maximale prise en compte (cm). Au-delà, on considère
// qu'il n'y a pas de mur : déclenche la rotation à droite.
constexpr int DISTANCE_MAX_CM = 200;

// Distance minimale crédible (cm) : en dessous, mesure aberrante ignorée.
constexpr int DISTANCE_MIN_CM = 3;

// --- Moteurs ---
constexpr int VITESSE_BASE   = 110;  // PWM de base (0-255)
constexpr int PWM_MINI       = 60;   // PWM minimum pour démarrer les moteurs
constexpr int PWM_MAX        = 255;  // PWM maximum

// --- Correcteur PD ---
constexpr double KP = 1.2;              // gain proportionnel (PWM/cm)
constexpr double KD = 0.0175;           // gain dérivé (PWM·s/cm) — ajusté pour dt=50ms
constexpr double DT_S = 0.05;           // pas de temps = 50 ms (période boucle)
constexpr double CORRECTION_MAX = 255.0; // saturation PWM du correcteur
constexpr double ZONE_MORTE_CM = 2.0;    // zone morte anti-oscillation (cm)

// --- Ralentissement progressif ---
constexpr int SEUIL_RALENTI_1 = 15;  // écart (cm) -> vitesse x 0.70
constexpr int SEUIL_RALENTI_2 = 30;  // écart (cm) -> vitesse x 0.45
constexpr double COEF_RALENTI_1 = 0.70;
constexpr double COEF_RALENTI_2 = 0.45;

// --- Rotation d'urgence ---
constexpr int TEMPS_ROTATION_MS = 220;  // durée minimale de rotation (ms)
constexpr int TEMPS_SECURITE_MS = 3000; // arrêt d'urgence après 3 s

// --- Boucle de régulation ---
constexpr int PERIODE_MS = 50;  // période de la boucle principale

// --- Simulation ---
constexpr double LARGEUR_COULOIR_CM = 80.0;  // largeur du couloir simulé
constexpr double POSITION_DEPART_CM  = 40.0;  // position initiale (centre)
constexpr int    NB_ITERATIONS       = 200;   // nombre d'itérations de simu

} // namespace config

#endif // CONFIG_H
