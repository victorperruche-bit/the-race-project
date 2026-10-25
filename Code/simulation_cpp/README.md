# The Race Project — Simulation C++

Simulation sur PC de l'algorithme du **robot suiveur de couloir**.
Cette version C++ reproduit la logique du code Arduino (`robot_suiveur_couloir.ino`)
dans un environnement de bureau, avec un couloir virtuel.

## Structure du projet

```
simulation_cpp/
├── CMakeLists.txt          # Fichier de build CMake
├── include/
│   ├── Config.h            # Constantes de configuration
│   ├── CapteurUltrason.h   # Classe capteur HC-SR04 (simulé)
│   ├── CorrecteurPD.h      # Correcteur Proportionnel-Dérivé
│   └── Robot.h             # Classe principale du robot
├── src/
│   ├── main.cpp            # Point d'entrée + scénarios de simulation
│   ├── CapteurUltrason.cpp
│   ├── CorrecteurPD.cpp
│   └── Robot.cpp
└── build/                  # Dossier de compilation (généré)
```

## Compilation

### Avec CMake (recommandé)

```bash
cd simulation_cpp
mkdir build && cd build
cmake ..
cmake --build .
```

### Avec g++ directement

```bash
g++ -std=c++17 -Iinclude src/*.cpp -o the_race.exe
```

### Avec Visual Studio Code

1. Installer l'extension **C/C++** (Microsoft)
2. Installer **CMake Tools**
3. Ouvrir le dossier `simulation_cpp`
4. `Ctrl+Shift+P` → `CMake: Configure` → `CMake: Build`

## Exécution

```bash
# Windows
build\the_race.exe
# ou
build\Debug\the_race.exe

# Linux / macOS
./build/the_race
```

## Scénarios de simulation

| Scénario | Description |
|----------|-------------|
| **1** | Robot décalé à gauche (20 cm) → observe le recentrage progressif vers le centre (40 cm) |
| **2** | Perte du mur droit → le robot passe en rotation d'urgence à droite |

## Paramètres réglables

Tous les paramètres sont dans `include/Config.h` :

| Paramètre | Défaut | Description |
|-----------|--------|-------------|
| `KP` | 1.2 | Gain proportionnel (réactivité) |
| `KD` | 0.35 | Gain dérivé (amortissement) |
| `VITESSE_BASE` | 110 | Vitesse de base des moteurs (0-255) |
| `DISTANCE_MAX_CM` | 200 | Portée max des capteurs |
| `LARGEUR_COULOIR_CM` | 80 | Largeur du couloir simulé |
