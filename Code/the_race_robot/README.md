# The Race Project - Robot autonome ESP32-CAM

## Fichier principal
-  - Code complet pour ESP32-CAM

## Materiel requis
| Composant | Quantite | Description |
|-----------|----------|-------------|
| ESP32-CAM | 1 | Carte ESP32 + camera OV2640 + WiFi |
| L298N | 1 | Pont en H pour 2 moteurs DC |
| Moteurs DC | 2 | Avec encodeurs optiques |
| HC-SR04 | 3 | Capteurs ultrasons (AVANT, GAUCHE, DROIT) |
| Batterie | 1 | LiPo 7.4V ou 6xAA (9V) |

## Cablage

### Moteurs (L298N)
| L298N | ESP32-CAM GPIO |
|-------|-----------------|
| ENA   | 13 (PWM) |
| IN1   | 12 |
| IN2   | 14 |
| ENB   | 15 (PWM) |
| IN3   | 2  |
| IN4   | 4  |

### Capteurs HC-SR04
| Capteur | TRIG | ECHO |
|---------|------|------|
| AVANT   | GPIO 1 (TX) | GPIO 3 (RX) |
| GAUCHE  | GPIO 16 | GPIO 0 (pull-up 10k) |
| DROIT   | GPIO 33 | GPIO 32 |

### Encodeurs
| Roue | GPIO |
|------|------|
| Gauche | 35 (interruption) |
| Droite | 34 (interruption) |

## Configuration a modifier


## Fonctionnalites
1. **Navigation autonome** - Tour complet du couloir
2. **Correcteur PD corrige** - dt, zone morte, saturation
3. **Camera** - Photo toutes les 1s, envoi WiFi HTTP POST
4. **Odometrie** - Distance parcourue via encodeurs
5. **Machine a etats** - AVANCE, VIRE_DROITE, VIRE_GAUCHE, DEMI_TOUR

## Compilation
1. Installer le support ESP32 dans Arduino IDE
2. Selectionner "AI Thinker ESP32-CAM"
3. Televerser avec un programmateur FTDI

## Serveur de reception (Python/Flask exemple)

