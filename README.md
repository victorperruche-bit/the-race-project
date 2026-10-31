# The Race Project

Robot de course autonome - Projet SI, TSI1, Lycee Gustave Eiffel (Dijon).

Le robot navigue dans les couloirs du batiment : 2 moteurs DC (pont en H
L298N) + 2 capteurs a ultrasons HC-SR04 (gauche / droit), correcteur PD.

## Contenu du dossier Code/

| Dossier | Contenu |
|---|---|
| [robot_suiveur_couloir](Code/robot_suiveur_couloir) | v1 : suivi de couloir simple (tourner a droite quand un mur disparait) |
| [the_race_robot](Code/the_race_robot) | v2 : robot autonome + camera ESP32-CAM + WiFi |
| [the_race_plan](Code/the_race_plan) | v3 : **plan de course embarque** - 7 parcours valides (virages a droite et a gauche, jonctions), ou parcours libre |
| [simulation_cpp](Code/simulation_cpp) | simulation PC du suivi de couloir (menu de scenarios + tests) |
| [simulateur_web](Code/simulateur_web) | mini-app web autonome : simulateur interactif de course (7 parcours, parcours libre, reglages en direct) |
| [serveur_photos](Code/serveur_photos) | serveur de reception des photos (v2, camera) |
| [Skeatchpersov2](Code/Skeatchpersov2) | esquisses personnelles |
