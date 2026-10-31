# 🏁 Simulateur web — The Race Project

**Version en ligne :**
<https://victorperruche-bit.github.io/the-race-project/Code/simulateur_web/>

Mini-application **100 % autonome** (aucune installation, aucun serveur, aucune
dépendance) qui simule la machine de course dans le bâtiment : **double-clic sur
`index.html`** et c'est parti.

## Lancer

Ouvrir `index.html` dans un navigateur (Chrome, Edge, Firefox…).
Raccourcis : `Espace` = départ/pause, `R` = réinitialiser.

## Ce que ça fait

- **Le bâtiment complet** (blocs de salles, couloirs, portes, ligne d'arrivée)
  rendu en Canvas, avec la machine, sa trace, et les rayons des deux capteurs
  ultrasons (vert = gauche, rouge = droite, tirets = plus d'écho).
- **7 parcours prédéfinis** : bloc gauche/droit (horaire et anti-horaire),
  grand tour (horaire et anti-horaire), bloc haut. Objectif : **3 tours**
  sans toucher les murs.
- **Composeur de parcours libre** : choisis le départ (5 couloirs), empile tes
  étapes (↱ droite / ↰ gauche / ↑ tout droit à la jonction), lance.
- **Réglages en direct** : vitesse, KP, KD, portée capteurs, vitesse de virage,
  bruit des capteurs (± cm), et les **2 constantes de calibration du robot
  réel** (cf. `the_race_plan.ino` v3) :
  - `VMAX_M` : vitesse réelle à PWM 255 (m/s) — à mesurer sur ton robot ;
  - `TEMPS_VIRAGE_90_MS` : durée d'un quart de tour. Sur le robot réel c'est
    un pivot sur place (~420 ms) ; dans la simulation le virage est un **arc**
    (la machine balaie le coin), la valeur par défaut **2100 ms** reproduit la
    géométrie validée.
- **Télémétrie** : distances, écart au centre, vitesse, PWM moteurs, dernier
  tour, record, collisions.
- **Panneau « Code Arduino simulé »** : la logique affichée avec les constantes
  et le plan de course **mis à jour en direct** par les curseurs.

## La logique simulée (identique à la stratégie réelle)

1. **Rester au milieu** : centrage uniquement si les deux murs se font face à
   2 m (signature du couloir : `|dG + dD − 2| < 0,4`). Erreur corrigée en PD.
2. **Plan de course embarqué** : à chaque étape, un capteur surveille un mur ;
   s'il le perd (plus d'écho, ou écho invraisemblable > 1,45 m hors couloir)
   après ≥ 2,5 m parcourus → étape suivante : virage droite / gauche / tout droit.
3. **Jonctions** : tenue monopalpe à ~1 m du mur restant pour ne pas dériver.

Records au réglage par défaut : **≈ 38,5 s** le tour d'un bloc,
**61,9 s** le grand tour.

## Fichiers

| Fichier | Rôle |
|---------|------|
| `index.html` | structure de la page |
| `style.css` | thème sombre, mise en page |
| `sim.js` | logique de simulation (géométrie, capteurs, physique, parcours) — pur ES5, testable |
| `app.js` | rendu Canvas, contrôles, télémétrie |

## Tests

La logique de `sim.js` a été validée en exécution headless (moteur JS
Duktape/QuickJS) : les 7 parcours bouclent leurs 3 tours sans collision au
réglage par défaut (~95 % des courses avec bruit ±2 cm — les rares collisions
viennent du bruit aléatoire des capteurs, c'est le jeu !).
