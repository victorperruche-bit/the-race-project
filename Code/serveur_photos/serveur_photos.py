#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os, datetime
from flask import Flask, request, jsonify, render_template_string, send_from_directory, redirect

# ============ CONFIGURATION ============

UPLOAD_FOLDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "photos")
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# ============ ETAT GLOBAL ============

class RobotState:
    def __init__(self):
        self.distance_cm = 0.0
        self.etat = "INIT"
        self.nombre_tours = 0
        self.derniere_maj = None
        self.historique = []

    def update(self, distance, etat, tour):
        self.distance_cm = float(distance) if distance else 0.0
        self.etat = etat
        self.nombre_tours = int(tour) if tour else 0
        self.derniere_maj = datetime.datetime.now()
        self.historique.append({
            "timestamp": self.derniere_maj.isoformat(),
            "distance": self.distance_cm,
            "etat": self.etat,
            "tour": self.nombre_tours
        })
        if len(self.historique) > 1000:
            self.historique = self.historique[-1000:]

    def to_dict(self):
        return {
            "distance_cm": self.distance_cm,
            "etat": self.etat,
            "nombre_tours": self.nombre_tours,
            "derniere_maj": self.derniere_maj.isoformat() if self.derniere_maj else None,
            "nb_photos": len(os.listdir(UPLOAD_FOLDER)) if os.path.exists(UPLOAD_FOLDER) else 0
        }

state = RobotState()
app = Flask(__name__)
app.config["UPLOAD_FOLDER"] = UPLOAD_FOLDER


# ============ TEMPLATE HTML ============

HTML = """<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<title>The Race Project - Galerie</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;padding:20px}
h1{color:#e94560;text-align:center}
.stats{display:flex;justify-content:center;gap:20px;flex-wrap:wrap;margin-bottom:30px}
.box{background:#16213e;padding:20px;border-radius:10px;text-align:center;min-width:120px}
.val{font-size:2em;color:#e94560;font-weight:bold}
.lbl{color:#aaa;font-size:.9em}
.gal{display:grid;grid-template-columns:repeat(auto-fill,minmax(250px,1fr));gap:15px}
.card{background:#16213e;border-radius:10px;overflow:hidden}
.card img{width:100%;height:180px;object-fit:cover}
.card .info{padding:10px;font-size:.85em;color:#aaa}
.btn{background:#e94560;color:#fff;border:none;padding:10px 30px;border-radius:5px;cursor:pointer}
.empty{text-align:center;color:#666;padding:50px}
.badge{padding:3px 10px;border-radius:15px;font-size:.8em;font-weight:bold}
.b-avance{background:#27ae60}.b-vd{background:#f39c12}.b-vg{background:#3498db}
.b-dt{background:#e74c3c}.b-fin{background:#9b59b6}.b-err{background:#c0392b}
</style>
</head>
<body>
<h1>The Race Project - Galerie Photos</h1>
<div class="stats">
<div class="box"><div class="val">{{stats.distance_cm|round(1)}}</div><div class="lbl">Distance (cm)</div></div>
<div class="box"><div class="val">{{stats.etat}}</div><div class="lbl">Etat</div></div>
<div class="box"><div class="val">{{stats.nombre_tours}}</div><div class="lbl">Tours</div></div>
<div class="box"><div class="val">{{stats.nb_photos}}</div><div class="lbl">Photos</div></div>
</div>
<div style="text-align:center;margin-bottom:20px"><button class="btn" onclick="location.reload()">Rafraichir</button></div>
{% if photos %}
<div class="gal">
{% for p in photos %}
<div class="card"><img src="/photos/{{p.nom}}"><div class="info"><strong>{{p.heure}}</strong><br>{{p.distance}} cm - {{p.etat}}</div></div>
{% endfor %}
</div>
{% else %}
<div class="empty">Aucune photo recue. En attente du robot...</div>
{% endif %}
</body>
</html>"""


# ============ ROUTES ============

@app.route("/upload", methods=["POST"])
def upload():
    data = request.data
    if not data: return jsonify({"error": "no data"}), 400
    dist = request.headers.get("X-Distance", "0")
    etat = request.headers.get("X-Etat", "0")
    tour = request.headers.get("X-Tour", "0")
    etats = {0:"INIT",1:"AVANCE",2:"VIRE_DROITE",3:"VIRE_GAUCHE",4:"DEMI_TOUR",5:"ARRET_FIN",6:"ERREUR"}
    en = etats.get(int(etat), "?")
    state.update(dist, en, tour)
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
    fn = f"photo_{ts}_d{int(float(dist))}_{en}.jpg"
    with open(os.path.join(UPLOAD_FOLDER, fn), "wb") as f: f.write(data)
    print(f"[PHOTO] {fn} ({len(data)}o) dist={dist} etat={en}")
    return jsonify({"status":"ok","file":fn,"size":len(data)}), 200

@app.route("/")
def index():
    ph = []
    if os.path.exists(UPLOAD_FOLDER):
        for f in sorted(os.listdir(UPLOAD_FOLDER), reverse=True):
            if f.endswith(".jpg"):
                st = os.stat(os.path.join(UPLOAD_FOLDER, f))
                h = datetime.datetime.fromtimestamp(st.st_mtime).strftime("%H:%M:%S")
                parts = f.replace("photo_","").replace(".jpg","").split("_")
                d, e = "?", "INIT"
                if len(parts) >= 4:
                    d = parts[2].replace("d","")
                    e = parts[3]
                ph.append({"nom":f,"heure":h,"distance":d,"etat":e})
    return render_template_string(HTML, photos=ph, stats=state.to_dict())

@app.route("/api/photos")
def api_ph():
    ph = [{"nom":f,"url":f"/photos/{f}"} for f in sorted(os.listdir(UPLOAD_FOLDER), reverse=True) if f.endswith(".jpg")] if os.path.exists(UPLOAD_FOLDER) else []
    return jsonify(ph)

@app.route("/api/stats")
def api_st(): return jsonify(state.to_dict())

@app.route("/api/historique")
def api_h(): return jsonify(state.historique)

@app.route("/photos/<fn>")
def get_ph(fn): return send_from_directory(UPLOAD_FOLDER, fn)

@app.route("/dernier")
def last():
    if os.path.exists(UPLOAD_FOLDER):
        p = sorted([f for f in os.listdir(UPLOAD_FOLDER) if f.endswith(".jpg")])
        if p: return redirect(f"/photos/{p[-1]}")
    return "Aucune photo", 404

@app.route("/reset", methods=["POST"])
def reset():
    global state
    state = RobotState()
    return jsonify({"status":"reset"}), 200

@app.route("/clear", methods=["POST"])
def clear():
    if os.path.exists(UPLOAD_FOLDER):
        for f in os.listdir(UPLOAD_FOLDER): os.remove(os.path.join(UPLOAD_FOLDER, f))
    global state
    state = RobotState()
    return jsonify({"status":"cleared"}), 200


# ============ MAIN ============

if __name__ == "__main__":
    print("=" * 50)
    print("  THE RACE PROJECT - Serveur de reception")
    print("=" * 50)
    print(f"Photos: {UPLOAD_FOLDER}")
    print("URL: http://0.0.0.0:5000")
    print("Upload: http://0.0.0.0:5000/upload")
    print("=" * 50)
    print("En attente du robot...")
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
