# Serveur de reception photos - THE RACE PROJECT

Serveur Flask pour recevoir les photos envoyees par l"ESP32-CAM du robot.

## Installation

```bash
pip install -r requirements.txt
```

## Lancement

```bash
python serveur_photos.py
```

## Utilisation

| URL | Description |
|-----|-------------|
| http://localhost:5000 | Galerie photos (interface web) |
| http://localhost:5000/upload | Endpoint POST pour recevoir les photos |
| http://localhost:5000/api/stats | Stats du robot en JSON |
| http://localhost:5000/api/photos | Liste des photos en JSON |
| http://localhost:5000/dernier | Derniere photo recue |

## Format d"envoi (depuis l"ESP32)

```
POST /upload HTTP/1.1
Content-Type: image/jpeg
X-Distance: 123.4
X-Etat: 1
X-Tour: 0

<donnees JPEG brutes>
```

## Codes d"etat (X-Etat)

| Code | Etat |
|------|------|
| 0 | INIT |
| 1 | AVANCE |
| 2 | VIRE_DROITE |
| 3 | VIRE_GAUCHE |
| 4 | DEMI_TOUR |
| 5 | ARRET_FIN |
| 6 | ERREUR |

## Structure des dossiers

```
serveur_photos/
  serveur_photos.py    # Serveur Flask
  requirements.txt     # Dependances Python
  README.md            # Cette documentation
  photos/              # Dossier des photos recues (cree auto)
```
