# Robot suiveur de couloir (Arduino)

Robot a **2 moteurs DC** (pont en H L298N) et **2 capteurs a ultrasons HC-SR04**
(un a gauche, un a droite). Le programme maintient le robot **au milieu du
couloir** : il cherche en permanence a avoir la meme distance au mur gauche et
au mur droit. Si une des deux distances depasse la portee maximale du capteur
(plus de mur detectable de ce cote), le robot **tourne a droite** jusqu'a
retrouver les deux murs.

Fichier principal : [robot_suiveur_couloir.ino](robot_suiveur_couloir.ino)

## 1. Cablage

### Capteurs a ultrasons (HC-SR04)

| Capteur | Broche HC-SR04 | Broche Arduino |
|---|---|---|
| Gauche | VCC | 5V |
| Gauche | GND | GND |
| Gauche | TRIG | D2 |
| Gauche | ECHO | D3 |
| Droit | VCC | 5V |
| Droit | GND | GND |
| Droit | TRIG | D4 |
| Droit | ECHO | D5 |

### Pont en H L298N

| L298N | Vers |
|---|---|
| ENA | D9 (PWM) - vitesse moteur gauche |
| IN1 | D8 - sens moteur gauche |
| IN2 | D12 - sens moteur gauche |
| ENB | D10 (PWM) - vitesse moteur droit |
| IN3 | D11 - sens moteur droit |
| IN4 | D13 - sens moteur droit |
| OUT1 / OUT2 | Moteur gauche |
| OUT3 / OUT4 | Moteur droit |
| 12V | Batterie (ex. 6 x piles AA, 7,2 V a 9 V) |
| GND | GND batterie **et** GND Arduino (masse commune obligatoire) |
| 5V | *Ne rien brancher* si le cavalier 5V-EN est en place |

> Les moteurs sont alimentes par la batterie, **pas** par l'Arduino.
> Le cavalier (jumper) 5V-EN du L298N reste en place : la carte fournit le 5 V.
> Si vous alimentez le L298N en 5 V, retirez ce cavalier et alimentez la logique
> en 5 V depuis l'Arduino.

Reglages utiles dans le code (section 2) :
`DISTANCE_MAX_CM = 200` (portee au-dela de laquelle on considere qu'il n'y a
plus de mur), `VITESSE_BASE = 110`, `KP = 1.2`, `KD = 0.35`.

## 2. Fonctionnement du programme

1. **Mesure** : chaque capteur est lu 3 fois, les mesures aberrantes sont
   ignorees, puis on fait la moyenne (filtrage du bruit des ultrasons).
2. **Recentrage** : `erreur = distanceDroite - distanceGauche`.
   - `erreur > 0` : le mur droit est plus loin, le robot est trop a gauche
     -> la roue gauche accelere, la roue droite ralentit (il pivote a droite).
   - `erreur < 0` : situation inverse.
   La correction est un **correcteur PD** : `KP * erreur + KD * (erreur - erreurPrecedente)`.
   Le terme derive (`KD`) amortit les oscillations.
3. **Ralentissement** : au-dela de 15 cm puis 30 cm d'ecart, la vitesse de base
   est reduite (70 % puis 45 %) pour eviter de foncer dans un mur.
4. **Rotation a droite** : des qu'une distance est nulle (pas d'echo) ou
   superieure a `DISTANCE_MAX_CM`, le robot pivote a droite sur place
   (roue gauche avant, roue droite arriere) pendant au moins 220 ms, puis
   reprend le suivi des que les deux murs sont revenus.
5. **Securite** : si la rotation dure plus de 3 s, le robot s'arrete et le
   programme se bloque (il faut reinitialiser la carte).

La boucle n'utilise pas `delay()` pour la regulation : elle tourne toutes les
50 ms (`PERIODE_MS`), ce qui rend le comportement fluide et regulier.

## 3. Mise en route

1. Ouvrir `robot_suiveur_couloir.ino` dans l'Arduino IDE.
2. Carte : **Arduino Uno** (ou Nano), port serie correspondant.
3. Televerser, puis ouvrir le **moniteur serie a 115200 bauds**.
4. Poser le robot au milieu du couloir : apres 1 seconde, il demarre.
5. Verifier sur le moniteur les lignes `G=... D=... err=... PWM G=... D=...`.

## 4. Reglages (si le comportement n'est pas bon)

| Symptome | Correction |
|---|---|
| Le robot zigzague / oscille | Diminuer `KP` (1.2 -> 0.8) ou augmenter `KD` (0.35 -> 0.6) |
| Correction trop molle, il derive vers un mur | Augmenter `KP` (1.2 -> 1.8) |
| Il tourne a gauche au lieu de droite | Echanger les signes dans `rotationDroite()` (ou inverser les fils du moteur droit) |
| Les roues vont a l'envers en marche avant | Inverser IN1/IN2 (moteur gauche) ou IN3/IN4 (moteur droit) |
| Il ne bouge pas, les moteurs sifflent | Augmenter `VITESSE_BASE` ou `PWM_MINI` |
| Il tourne a droite dans un couloir large | Augmenter `DISTANCE_MAX_CM` (200 -> 300), mais rester sous la portee reelle du HC-SR04 |
| Un capteur lit n'importe quoi | Eviter les murs molle/tres inclines ; eloigner les deux capteurs l'un de l'autre pour eviter la diaphonie (ou augmenter le `delay(6)` entre les mesures) |

## 5. Remarques techniques

- Vitesse du son utilisee : 343 m/s (`duree * 0.0343 / 2`), soit ~1 cm par 58 us.
- `pulseIn(..., 25000)` : au-dela de 25 ms (environ 4 m), la mesure est
  declaree sans echo et renvoie -1.
- Les deux capteurs sont lus l'un apres l'autre, jamais en meme temps : deux
  HC-SR04 qui emettent simultanement se perturbent mutuellement.
- L298N : les sorties PWM D9 et D10 utilisent les timers 1 et 2, ce qui laisse
  la liaison serie et `millis()` parfaitement fonctionnels.
