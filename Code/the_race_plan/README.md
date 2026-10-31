# The Race Project v3 - Plan de course embarque (Arduino)

Robot a **2 moteurs DC** (L298N) et **2 capteurs HC-SR04** (gauche / droite).
Cette version ne suit plus seulement un couloir : elle embarque un
**PLAN DE COURSE** (7 parcours valides, ou le votre) pour naviguer dans le
batiment du plan du lycee, **avec des virages a droite ET a gauche**, et le
franchissement de jonctions.

Fichier principal : [the_race_plan.ino](the_race_plan.ino)

## 1. Principe

A chaque **etape** du plan, un capteur surveille un mur :

| PLAN_CAPTEUR | Capteur surveille | PLAN_ACTION | Action quand le mur disparait |
|---|---|---|---|
| `'D'` | droit | `'R'` | quart de tour a droite |
| `'G'` | gauche | `'L'` | quart de tour a gauche |
| `'B'` | les deux | `'S'` | tout droit (franchir la jonction) |

Le plan se repete en boucle : le robot enchaine les tours de circuit.

Autres regles (identiques au simulateur interactif) :

1. **Centrage PD** : le robot ne se cale que si les 2 murs se font face a 2 m
   (signature de couloir `|dG + dD - 2| < 0,4`). Erreur = dG - dD, corrigee
   en PD : KP accroche, KD amortit.
2. **Anti faux virage** : une etape ne se declenche qu'apres au moins **2,5 m**
   parcourus depuis la precedente (odometrie par le temps : a remplacer par
   des encodeurs de roues des que possible).
3. **Anti echo parasite** : un echo > 2 m n'est pas le mur du couloir ; un echo
   surveille > 1,45 m hors couloir = le mur est parti (sinon un rayon qui
   glisse derriere un coin declenche un virage fantome).
4. **Jonction (tenue monopalpe)** : si un seul mur reste visible, le robot le
   tient a environ 1 m pour ne pas deriver en aveugle, jusqu'a retrouver les
   deux murs du couloir suivant (rearmement < 1,5 m).

## 2. Les 7 parcours valides (simulation 35/35)

| # | Parcours | Depart (croisement, cap) | PLAN_CAPTEUR | PLAN_ACTION |
|---|---|---|---|---|
| 1 | Bloc gauche - horaire | Couloir ouest (x=-1, y=9,5), vers le nord | D D D D | R R R R |
| 2 | Bloc gauche - anti-horaire | Couloir ouest, vers le sud | G G G G | L L L L |
| 3 | Bloc droit - horaire | Couloir de liaison (x=15, y=6), vers l'est | D D D D | R R R R |
| 4 | Bloc droit - anti-horaire | Couloir de liaison, vers l'ouest | G G G G | L L L L |
| 5 | Grand tour - horaire | comme n.1 | D D D D D D | R S R R S R |
| 6 | Grand tour - anti-horaire | comme n.2 | G G G G G G | L S L L S L |
| 7 | Bloc haut - horaire | Couloir nord (x=15, y=-1), vers l'est | D D B D D | R R S R R |

Pour changer de parcours : recopier les 2 tableaux et `N_ETAPES` en haut du
`.ino`. Astuce : une etape `'S'` peut toujours utiliser `'B'` (les deux
capteurs) -- peu importe de quel cote la jonction s'ouvre.

**Parcours libre** : choisir un depart parmi les 5 ci-dessus et empiler ses
propres etapes (R / L / S). Attention : si le plan demande un virage vers un
mur qui ne disparait jamais, le robot file tout droit jusqu'a l'obstacle --
c'est la lecon numero 1 de la conception d'un plan.

## 3. Cablage

Identique a robot_suiveur_couloir :

| Capteur | Broche Arduino |
|---|---|
| TRIG gauche / droit | D2 / D4 |
| ECHO gauche / droit | D3 / D5 |

| L298N | Broche Arduino |
|---|---|
| ENA / ENB (PWM) | D9 / D10 |
| IN1 / IN2 | D8 / D12 |
| IN3 / IN4 | D11 / D13 |

## 4. Mise en route

1. Ouvrir `the_race_plan.ino` dans l'IDE Arduino (Uno / Nano).
2. Televerser, ouvrir le moniteur serie a 115200 bauds.
3. Poser le robot au point de depart du parcours choisi, axes perpendiculaires
   aux murs. Apres 1 s il demarre.
4. Le moniteur affiche l'etape courante, le capteur surveille, les distances,
   et les virages declenches.

## 5. Calibration (2 valeurs a mesurer sur VOTRE robot)

| Constante | Methode |
|---|---|
| `VMAX_M` | faire rouler le robot 2 m a fond (255), chronometrer, v = 2/t |
| `TEMPS_VIRAGE_90_MS` | ajuster jusqu'a ce que le quart de tour tombe pile a 90 degres |

Reglages PD :

| Symptome | Correction |
|---|---|
| Le robot zigzague | baisser KP (60 -> 40) ou monter KD |
| Correction trop molle | monter KP |
| Virage rate / trop court | monter TEMPS_VIRAGE_90_MS ou VIRAGE_PWM |
| Faux virage dans une jonction | verifier TRIG_CAP (1,45 m) et MIN_RUN_M (2,5 m) |

## 6. Resultats de la simulation (validation)

- 7 parcours x 5 configurations (defaut, 255 PWM, lent 90 PWM, bruit +/-8 cm,
  KP 120 + bruit 4) : **35/35 courses terminees sans collision**.
- Reglages par defaut (160 PWM) : ~38,5 s le tour autour d'un bloc,
  ~61,9 s en grand tour. A 255 PWM : 27,4 s / 42,1 s (records).
- La **tenue monopalpe** et le seuil **MIN_RUN = 2,5 m** sont indispensables :
  sans eux, le coin sud-est du batiment rearmait la detection 9 m trop tot.

## 7. Ameliorations futures

- **Encodeurs de roues** pour une odometrie fiable (remplacer
  `distDepuisEtape` basee sur le temps).
- Ligne d'arrivee (capteur sol ou bande magnetique) pour compter les tours.
- Bluetooth/serial pour changer de parcours sans re-televerser.
