# Simulation C++ - The Race Project

Simulation sur PC du robot suiveur de couloir, fidèle au matériel réel :
- **2 moteurs CC** (gauche / droit) pilotés par L298N
- **2 capteurs à ultrasons HC-SR04** (gauche / droit)
- Correcteur PD avec zone morte et saturation

## Compilation

```bash
# Depuis la racine du projet
g++ -std=c++17 -Wall -Wextra \
    -ICode/simulation_cpp/include \
    Code/simulation_cpp/src/main.cpp \
    Code/simulation_cpp/src/Robot.cpp \
    Code/simulation_cpp/src/CapteurUltrason.cpp \
    Code/simulation_cpp/src/CorrecteurPD.cpp \
    -o Code/simulation_cpp/build/the_race.exe
```

## Exécution

```bash
Code/simulation_cpp/build/the_race.exe
```

Un menu interactif permet de choisir le scénario.

## Scénarios disponibles

| # | Nom | Description |
|---|-----|-------------|
| 1 | **Centrage simple** | Robot décalé à 20 cm, doit se recentrer au milieu (40 cm) |
| 2 | **Perte du mur droit** | Le couloir s'ouvre → rotation d'urgence à droite |
| 3 | **Couloir en L** | Virage à 90° : le mur droit disparaît après 200 cm |
| 4 | **Bruit capteurs** | Bruit gaussien ±3 cm sur les mesures (test robustesse) |
| 5 | **Parcours complet** | 3 segments : droit → virage L → droit (avec bruit léger) |
| 6 | **Tous les scénarios** | Exécute les 5 scénarios à la suite |

## Architecture des fichiers

```
simulation_cpp/
├── include/
│   ├── Config.h           # Paramètres (KP, KD, vitesses, seuils...)
│   ├── CapteurUltrason.h  # Classe capteur US (simulation)
│   ├── CorrecteurPD.h     # Correcteur PD (dt, saturation, zone morte)
│   └── Robot.h            # Classe Robot (2 moteurs + 2 capteurs)
├── src/
│   ├── main.cpp           # Menu + scénarios + statistiques
│   ├── Robot.cpp          # Logique de régulation
│   ├── CapteurUltrason.cpp
│   └── CorrecteurPD.cpp
└── build/                 # Exécutable compilé
```

## Paramètres clés (Config.h)

| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| `KP` | 1.2 | Gain proportionnel (PWM/cm) |
| `KD` | 0.0175 | Gain dérivé (ajusté pour dt=50ms) |
| `DT_S` | 0.05 | Pas de temps (50 ms) |
| `VITESSE_BASE` | 110 | PWM de base (0-255) |
| `ZONE_MORTE_CM` | 2.0 | Zone morte anti-oscillation |
| `LARGEUR_COULOIR_CM` | 80.0 | Largeur du couloir simulé |

## Statistiques affichées

Après chaque scénario, le programme affiche :
- Erreur max absolue et moyenne
- Nombre de dépassements (oscillations)
- Rotations d'urgence déclenchées
- Distance parcourue
- Écart final au centre avec évaluation OK/MOYEN/ÉCHEC

## Correspondance avec le code Arduino

| Simulation C++ | Arduino (the_race_robot.ino) |
|----------------|------------------------------|
| `Robot::etapeRegulation()` | `loop()` + `reguler()` |
| `CorrecteurPD::calculer()` | `calculerCorrectionPD()` |
| `CapteurUltrason::mesurer()` | `mesurerDistanceUS()` |
| `Config.h` | Constantes en haut du `.ino` |

> **Note** : Le code Arduino ESP32-CAM utilise 3 capteurs (AVANT/GAUCHE/DROITE)
> pour la détection d'obstacles et les virages. La simulation PC se concentre
> sur le suivi de couloir pur (2 capteurs G/D), identique au robot de base.

## Tests automatiques (`tests/test_robot.cpp`)

Suite de tests qui vérifie les deux comportements clés du robot :

1. **Recentrage au centre du couloir** : au centre → tout droit ; trop à
   gauche → tourne à droite ; trop à droite → tourne à gauche ; convergence
   vers le centre (à moins de 1,5 cm) sur un couloir simulé de 80 cm.
2. **Rotation à droite hors portée** : une distance > 200 cm (ou invalide)
   déclenche la rotation sur place à droite (roue gauche en avant, roue
   droite en arrière), et le robot reprend le suivi dès que les deux murs
   réapparaissent.

Lancement depuis la racine du projet (ou tâche VS Code « Build & Run Tests ») :

```bash
g++ -std=c++17 -Wall -Wextra -I Code/simulation_cpp/include \
    Code/simulation_cpp/tests/test_robot.cpp \
    Code/simulation_cpp/src/Robot.cpp \
    Code/simulation_cpp/src/CapteurUltrason.cpp \
    Code/simulation_cpp/src/CorrecteurPD.cpp \
    -o Code/simulation_cpp/build/test_robot.exe
./Code/simulation_cpp/build/test_robot.exe
```

Avec CMake : `cmake --build build && cd build && ctest`
