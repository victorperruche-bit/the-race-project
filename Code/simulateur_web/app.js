/* ============================================================
   APP.JS — Interface du simulateur The Race Project
   Rendu Canvas du bâtiment + contrôles + télémétrie.
   Utilise la logique de sim.js (makeSim, tick, ROUTES, ...).
   ============================================================ */
(function () {
"use strict";

/* ---------------- état de l'interface ---------------- */
var PARAM_DEFS = [
  { key: "vitesse", label: "🚗 Vitesse de croisière", min: 80, max: 255, step: 5,
    fmt: function (v) { return v + " PWM · ≈ " + ((v / 255) * VMAX).toFixed(2) + " m/s"; } },
  { key: "kp", label: "🎯 Gain de centrage (KP)", min: 20, max: 400, step: 10,
    fmt: function (v) { return v + " PWM/m d'écart"; } },
  { key: "kd", label: "🧲 Amortissement (KD)", min: 0, max: 120, step: 5,
    fmt: function (v) { return v + " PWM·s/m — 0 = ça oscille !"; } },
  { key: "portee", label: "📡 Portée des capteurs", min: 1.5, max: 4, step: 0.25,
    fmt: function (v) { return v.toFixed(2) + " m (HC-SR04)"; } },
  { key: "vmax", label: "🏃 Vitesse max réelle (VMAX_M)", min: 0.5, max: 3, step: 0.05,
    fmt: function (v) { return v.toFixed(2) + " m/s à PWM 255 — à calibrer"; } },
  { key: "virage", label: "↩️ Vitesse du virage", min: 40, max: 200, step: 5,
    fmt: function (v) { return v + " PWM"; } },
  { key: "tpsVirage", label: "⏱️ Durée virage 90° (TEMPS_VIRAGE_90_MS)", min: 600, max: 3000, step: 50,
    fmt: function (v) { return v + " ms (robot réel : pivot ≈ 420 ms)"; } },
  { key: "bruit", label: "🎲 Bruit des capteurs", min: 0, max: 8, step: 1,
    fmt: function (v) { return "± " + v + " cm"; } },
];

var params = { vitesse: 160, kp: 60, kd: 30, portee: 4, vmax: 1.5, virage: 120, tpsVirage: 2100, bruit: 2 };
var routeId = "bg-h";
var customSteps = [];      // "R" | "L" | "S"
var customStartId = "w-n";
var running = false;
var simSpeed = 1;
var beams = true;

var sim = makeSim(ROUTES[0]);

function $(id) { return document.getElementById(id); }

/* ---------------- parcours (prédéfinis + composé) ---------------- */
function customRoute() {
  if (!customSteps.length) return null;
  var cs = CUSTOM_STARTS[0];
  for (var i = 0; i < CUSTOM_STARTS.length; i++) {
    if (CUSTOM_STARTS[i].id === customStartId) cs = CUSTOM_STARTS[i];
  }
  return {
    id: "custom", nom: "Mon parcours",
    start: cs.start, fin: cs.fin,
    steps: customSteps.map(function (t) {
      return { c: t === "R" ? "D" : t === "L" ? "G" : "B", t: t };
    }),
  };
}
function allRoutes() {
  var cr = customRoute();
  return cr ? ROUTES.concat([cr]) : ROUTES;
}
function routeById(id) {
  var all = allRoutes();
  for (var i = 0; i < all.length; i++) if (all[i].id === id) return all[i];
  return ROUTES[0];
}

function reset() {
  sim = makeSim(routeById(routeId));
  running = false;
}
function chooseRoute(id) {
  routeId = id;
  sim = makeSim(routeById(id));
  running = false;
  buildRouteList();
  renderCode();
}
function toggleRun() {
  if (sim.mode === "CRASH" || sim.mode === "FIN") return;
  running = !running;
}

/* ---------------- canvas ---------------- */
var canvas = $("track");
var ctx = canvas.getContext("2d");
var X0 = -2.95, Y0 = -2.95, VW = 25.9, VH = 17.9;

function resize() {
  var cssW = canvas.parentElement.clientWidth || 900;
  var dpr = window.devicePixelRatio || 1;
  canvas.style.width = cssW + "px";
  canvas.style.height = (cssW * VH / VW) + "px";
  canvas.width = Math.round(cssW * dpr);
  canvas.height = Math.round(cssW * VH / VW * dpr);
}

function rr(x, y, w, h, r) { // rectangle arrondi (en mètres)
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

var ROOMS = [
  ["A006", 3.33, 2.5], ["A008", 10, 2.5], ["A010", 16.67, 2.5],
  ["A012", 2.25, 9.5], ["A014", 6.75, 9.5],
  ["A016", 12.75, 9.5], ["A018", 17.25, 9.5],
];
var DOORS = [
  [2.55, 4.9, 0.9, 0.1], [9.55, 4.9, 0.9, 0.1], [16.55, 4.9, 0.9, 0.1],
  [2.55, -0.05, 0.9, 0.1], [9.55, -0.05, 0.9, 0.1], [16.55, -0.05, 0.9, 0.1],
  [1.55, 6.95, 0.9, 0.1], [6.05, 6.95, 0.9, 0.1],
  [8.9, 8.3, 0.1, 0.9], [8.9, 10.3, 0.1, 0.9],
  [2.05, 11.9, 0.9, 0.1], [6.05, 11.9, 0.9, 0.1],
  [11, 8.3, 0.1, 0.9], [11, 10.3, 0.1, 0.9],
  [14.05, 11.9, 0.9, 0.1], [18.05, 11.9, 0.9, 0.1],
];

function draw() {
  var s = canvas.width / VW;
  ctx.setTransform(s, 0, 0, s, -X0 * s, -Y0 * s);
  ctx.clearRect(X0, Y0, VW, VH);

  /* ---- sol des couloirs + grille ---- */
  ctx.fillStyle = "#14335a";
  ctx.fillRect(-2, -2, 24, 16);
  ctx.strokeStyle = "rgba(148,197,255,0.10)";
  ctx.lineWidth = 0.02;
  ctx.beginPath();
  var gx, gy;
  for (gx = -2; gx <= 22; gx += 1) { ctx.moveTo(gx, -2); ctx.lineTo(gx, 14); }
  for (gy = -2; gy <= 14; gy += 1) { ctx.moveTo(-2, gy); ctx.lineTo(22, gy); }
  ctx.stroke();

  /* ---- blocs de salles ---- */
  var blocks = [BLOC_HAUT, BLOC_GAUCHE, BLOC_DROIT];
  for (var i = 0; i < blocks.length; i++) {
    var b = blocks[i];
    ctx.fillStyle = "#0e2542";
    ctx.fillRect(b.x, b.y, b.w, b.h);
    ctx.strokeStyle = "#dbeafe";
    ctx.lineWidth = 0.12;
    ctx.strokeRect(b.x, b.y, b.w, b.h);
  }

  /* ---- cloisons intérieures ---- */
  ctx.strokeStyle = "#7fa8d4";
  ctx.lineWidth = 0.06;
  ctx.beginPath();
  ctx.moveTo(6.667, 0); ctx.lineTo(6.667, 5);
  ctx.moveTo(13.333, 0); ctx.lineTo(13.333, 5);
  ctx.moveTo(4.5, 7); ctx.lineTo(4.5, 12);
  ctx.moveTo(15.5, 7); ctx.lineTo(15.5, 12);
  ctx.stroke();

  /* ---- portes (décor) ---- */
  ctx.fillStyle = "#f8fafc";
  ctx.globalAlpha = 0.9;
  for (var d = 0; d < DOORS.length; d++) {
    ctx.fillRect(DOORS[d][0], DOORS[d][1], DOORS[d][2], DOORS[d][3]);
  }
  ctx.globalAlpha = 1;

  /* ---- murs extérieurs ---- */
  ctx.fillStyle = "#dbeafe";
  for (var w = 3; w < WALLS.length; w++) {
    ctx.fillRect(WALLS[w].x, WALLS[w].y, WALLS[w].w, WALLS[w].h);
  }

  /* ---- noms des salles ---- */
  ctx.textAlign = "center";
  for (var r = 0; r < ROOMS.length; r++) {
    ctx.fillStyle = "#a8c8ec";
    ctx.font = "600 0.85px 'Segoe UI', sans-serif";
    ctx.fillText(ROOMS[r][0], ROOMS[r][1], ROOMS[r][2] + 0.25);
    ctx.fillStyle = "#5f83b0";
    ctx.font = "0.38px 'Segoe UI', sans-serif";
    ctx.fillText("salle de cours", ROOMS[r][1], ROOMS[r][2] + 0.85);
  }

  /* ---- étiquettes des couloirs ---- */
  function labelRot(txt, x, y, size, ang, fill) {
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(ang);
    ctx.font = size + "px 'Segoe UI', sans-serif";
    ctx.fillStyle = fill;
    ctx.textAlign = "center";
    ctx.fillText(txt, 0, 0);
    ctx.restore();
  }
  ctx.font = "0.6px 'Segoe UI', sans-serif";
  ctx.fillStyle = "rgba(255,255,255,0.35)";
  ctx.fillText("couloir nord", 10, -1.05);
  labelRot("couloir est", 21, 6, 0.55, Math.PI / 2, "rgba(255,255,255,0.35)");
  ctx.font = "0.4px 'Segoe UI', sans-serif";
  ctx.fillStyle = "rgba(255,255,255,0.30)";
  ctx.fillText("couloir de liaison", 4.4, 6.62);
  labelRot("couloir central", 10, 9.6, 0.4, Math.PI / 2, "rgba(255,255,255,0.30)");
  ctx.fillStyle = "rgba(255,255,255,0.30)";
  ctx.fillText("couloir sud", 11.6, 13.55);
  labelRot("couloir ouest", -1.6, 11.2, 0.4, -Math.PI / 2, "rgba(255,255,255,0.30)");

  /* ---- ligne d'arrivée (bande damier) ---- */
  var route = routeById(routeId);
  var fin = route.fin;
  ctx.fillStyle = "#f8fafc";
  var nCells = Math.round((fin.hi - fin.lo) / 0.25);
  for (var ci = 0; ci < nCells; ci++) {
    for (var cj = 0; cj < 2; cj++) {
      if ((ci + cj) % 2 !== 0) continue;
      if (fin.axis === "y") {
        ctx.fillRect(fin.lo + ci * 0.25, fin.pos - 0.25 + cj * 0.25, 0.25, 0.25);
      } else {
        ctx.fillRect(fin.pos - 0.25 + cj * 0.25, fin.lo + ci * 0.25, 0.25, 0.25);
      }
    }
  }
  ctx.fillStyle = "#bbf7d0";
  ctx.font = "0.4px 'Segoe UI', sans-serif";
  var finTx = fin.axis === "y"
    ? { x: (fin.lo + fin.hi) / 2, y: fin.pos + (fin.dir === -1 ? 0.95 : -0.6) }
    : { x: fin.pos + (fin.dir === 1 ? -1.0 : 0.65), y: (fin.lo + fin.hi) / 2 };
  ctx.fillText("🏁 arrivée", finTx.x, finTx.y);

  /* ---- marqueur de départ ---- */
  ctx.strokeStyle = "#4ade80";
  ctx.lineWidth = 0.04;
  ctx.setLineDash([0.08, 0.06]);
  ctx.beginPath();
  ctx.arc(route.start[0], route.start[1], 0.18, 0, 2 * Math.PI);
  ctx.stroke();
  ctx.setLineDash([]);

  /* ---- trajet déjà parcouru ---- */
  if (sim.trail.length > 1) {
    ctx.strokeStyle = "#f59e0b";
    ctx.lineWidth = 0.09;
    ctx.globalAlpha = 0.45;
    ctx.beginPath();
    ctx.moveTo(sim.trail[0][0], sim.trail[0][1]);
    for (var t = 1; t < sim.trail.length; t++) ctx.lineTo(sim.trail[t][0], sim.trail[t][1]);
    ctx.stroke();
    ctx.globalAlpha = 1;
  }

  /* ---- rayons capteurs ---- */
  var lgx = sim.x + Math.sin(sim.th) * sim.dGraw;
  var lgy = sim.y - Math.cos(sim.th) * sim.dGraw;
  var rgx = sim.x - Math.sin(sim.th) * sim.dDraw;
  var rgy = sim.y + Math.cos(sim.th) * sim.dDraw;
  var lgEx = sim.x + Math.sin(sim.th) * params.portee;
  var lgEy = sim.y - Math.cos(sim.th) * params.portee;
  var rgEx = sim.x - Math.sin(sim.th) * params.portee;
  var rgEy = sim.y + Math.cos(sim.th) * params.portee;

  if (beams) {
    ctx.lineWidth = 0.05;
    // gauche (vert)
    ctx.strokeStyle = "#34d399";
    if (sim.dGno) {
      ctx.setLineDash([0.18, 0.14]);
      ctx.beginPath(); ctx.moveTo(sim.x, sim.y); ctx.lineTo(lgEx, lgEy); ctx.stroke();
      ctx.setLineDash([]);
    } else {
      ctx.beginPath(); ctx.moveTo(sim.x, sim.y); ctx.lineTo(lgx, lgy); ctx.stroke();
      ctx.fillStyle = "#34d399";
      ctx.beginPath(); ctx.arc(lgx, lgy, 0.08, 0, 2 * Math.PI); ctx.fill();
    }
    // droite (rouge)
    ctx.strokeStyle = "#f87171";
    if (sim.dDno) {
      ctx.setLineDash([0.18, 0.14]);
      ctx.beginPath(); ctx.moveTo(sim.x, sim.y); ctx.lineTo(rgEx, rgEy); ctx.stroke();
      ctx.setLineDash([]);
    } else {
      ctx.beginPath(); ctx.moveTo(sim.x, sim.y); ctx.lineTo(rgx, rgy); ctx.stroke();
      ctx.fillStyle = "#f87171";
      ctx.beginPath(); ctx.arc(rgx, rgy, 0.08, 0, 2 * Math.PI); ctx.fill();
    }
    // étiquettes des distances
    ctx.fillStyle = "rgba(11,26,46,0.9)";
    ctx.fillRect(lgx - 0.95, lgy - 0.55 - 0.42, 1.9, 0.5);
    ctx.fillStyle = "#6ee7b7";
    ctx.font = "0.4px 'Segoe UI', sans-serif";
    ctx.fillText("G : " + (sim.dGno ? "aucun écho" : sim.dG.toFixed(2) + " m"), lgx, lgy - 0.55 - 0.05);
    ctx.fillStyle = "rgba(11,26,46,0.9)";
    ctx.fillRect(rgx - 0.95, rgy + 0.75 - 0.08, 1.9, 0.5);
    ctx.fillStyle = "#fca5a5";
    ctx.fillText("D : " + (sim.dDno ? "aucun écho" : sim.dD.toFixed(2) + " m"), rgx, rgy + 0.75 + 0.3);
  }

  /* ---- la machine ---- */
  ctx.save();
  ctx.translate(sim.x, sim.y);
  ctx.rotate(sim.th);
  // roues
  ctx.fillStyle = "#1e293b";
  ctx.strokeStyle = "#94a3b8";
  ctx.lineWidth = 0.02;
  rr(-0.06, -0.17, 0.12, 0.07, 0.02); ctx.fill(); ctx.stroke();
  rr(-0.06, 0.10, 0.12, 0.07, 0.02); ctx.fill(); ctx.stroke();
  // caisse
  ctx.fillStyle = "#fb923c";
  ctx.strokeStyle = "#7c2d12";
  ctx.lineWidth = 0.03;
  rr(-0.15, -0.10, 0.30, 0.20, 0.06); ctx.fill(); ctx.stroke();
  // nez
  ctx.fillStyle = "#fde68a";
  ctx.beginPath();
  ctx.moveTo(0.10, -0.055); ctx.lineTo(0.10, 0.055); ctx.lineTo(0.185, 0);
  ctx.closePath(); ctx.fill();
  // capteurs
  ctx.fillStyle = "#fde047";
  ctx.strokeStyle = "#0f172a";
  ctx.lineWidth = 0.025;
  ctx.beginPath(); ctx.arc(0, -0.115, 0.05, 0, 2 * Math.PI); ctx.fill(); ctx.stroke();
  ctx.beginPath(); ctx.arc(0, 0.115, 0.05, 0, 2 * Math.PI); ctx.fill(); ctx.stroke();
  ctx.restore();

  ctx.setTransform(1, 0, 0, 1, 0, 0);
}

/* ---------------- panneau latéral (DOM) ---------------- */
function buildRouteList() {
  var box = $("route-list");
  box.innerHTML = "";
  var all = allRoutes();
  for (var i = 0; i < all.length; i++) {
    (function (r) {
      var btn = document.createElement("button");
      btn.className = "route-btn" + (routeId === r.id ? " selected" : "");
      var plan = "";
      for (var j = 0; j < r.steps.length; j++) {
        var st = r.steps[j];
        plan += st.c + (st.t === "R" ? "↱" : st.t === "L" ? "↰" : "↑") + " ";
      }
      btn.innerHTML = "<span>" + r.nom + "</span><span class='plan'>" + plan + "</span>";
      btn.onclick = function () { chooseRoute(r.id); };
      box.appendChild(btn);
    })(all[i]);
  }
}

function buildStartChips() {
  var box = $("start-chips");
  box.innerHTML = "";
  for (var i = 0; i < CUSTOM_STARTS.length; i++) {
    (function (st) {
      var c = document.createElement("button");
      c.className = "chip" + (customStartId === st.id ? " selected" : "");
      c.textContent = st.label;
      c.onclick = function () {
        customStartId = st.id;
        buildStartChips();
      };
      box.appendChild(c);
    })(CUSTOM_STARTS[i]);
  }
}

function buildStepsChips() {
  var box = $("steps-chips");
  box.innerHTML = "";
  if (!customSteps.length) return;
  var lbl = document.createElement("span");
  lbl.className = "chip-label";
  lbl.textContent = "Plan :";
  box.appendChild(lbl);
  for (var i = 0; i < customSteps.length; i++) {
    (function (idx) {
      var c = document.createElement("button");
      c.className = "chip step";
      c.title = "Retirer cette étape";
      c.textContent = (idx + 1) + " " + (customSteps[idx] === "R" ? "↱" : customSteps[idx] === "L" ? "↰" : "↑");
      c.onclick = function () {
        customSteps.splice(idx, 1);
        refreshCustom();
      };
      box.appendChild(c);
    })(i);
  }
}

function refreshCustom() {
  buildStepsChips();
  buildRouteList();
  var btn = $("use-custom");
  btn.disabled = !customSteps.length;
  btn.textContent = "▶ Utiliser mon parcours (" + customSteps.length + " étape" +
    (customSteps.length > 1 ? "s" : "") + ")";
  // si le parcours custom affiché n'existe plus, revenir au parcours par défaut
  if (routeId === "custom" && !customSteps.length) {
    routeId = "bg-h";
    sim = makeSim(routeById(routeId));
    running = false;
    buildRouteList();
    renderCode();
  }
}

function buildParams() {
  var box = $("params-list");
  box.innerHTML = "";
  for (var i = 0; i < PARAM_DEFS.length; i++) {
    (function (def) {
      var wrap = document.createElement("div");
      wrap.className = "param";
      var row = document.createElement("div");
      row.className = "row";
      var lab = document.createElement("span");
      lab.textContent = def.label;
      var val = document.createElement("span");
      val.className = "val";
      val.id = "val-" + def.key;
      val.textContent = def.fmt(params[def.key]);
      row.appendChild(lab); row.appendChild(val);
      var slider = document.createElement("input");
      slider.type = "range";
      slider.min = def.min; slider.max = def.max; slider.step = def.step;
      slider.value = params[def.key];
      slider.oninput = function () {
        params[def.key] = parseFloat(slider.value);
        val.textContent = def.fmt(params[def.key]);
        renderCode();
      };
      wrap.appendChild(row); wrap.appendChild(slider);
      box.appendChild(wrap);
    })(PARAM_DEFS[i]);
  }
}

/* ---------------- code Arduino simulé ---------------- */
function esc(t) { return t.replace(/&/g, "&amp;").replace(/</g, "&lt;"); }
function hl(v) { return '<span class="hl">' + v + "</span>"; }

/* code Arduino affiché : une ligne = une chaine, valeurs live en {{...}} */
var CODE_LINES = [
  "/* ─────────────────────────────────────",
  "   THE RACE PROJECT — logique de la machine",
  "   Le plan de course est embarqué",
  "   ───────────────────────────────────── */",
  "const int TRIG_G = 2, ECHO_G = 3;   // capteur ultrasons gauche",
  "const int TRIG_D = 4, ECHO_D = 5;   // capteur ultrasons droit",
  "const int MOT_G  = 9, MOT_D  = 10;  // moteurs DC (PWM)",
  "",
  "/* ── Réglages pilotés par les curseurs ── */",
  "const int   VITESSE    = {{vitesse}};   // PWM de croisière (0–255)",
  "const float KP         = {{kp}};     // gain de centrage (PWM par m)",
  "const float KD         = {{kd}};     // amortissement (PWM·s/m)",
  "const float PORTEE     = {{portee}};  // portée capteur HC-SR04 (m)",
  "const float VMAX_M     = {{vmax}};   // vitesse réelle à PWM 255 (m/s) — À CALIBRER",
  "const int   VIRAGE_PWM = {{virage}};   // PWM pendant le virage",
  "const int   TEMPS_VIRAGE_90_MS = {{tpsVirage}}; // durée d'un quart de tour — À CALIBRER",
  "",
  "/* ── Plan de course : une action par étape ──",
  "   PLAN_CAPTEUR : capteur surveillé ('D' droite, 'G' gauche,",
  "                  'B' les deux — étapes « tout droit »)",
  "   PLAN_ACTION  : 'R' virage à droite · 'L' virage à gauche",
  "                  'S' tout droit (franchir la jonction) */",
  "const char PLAN_CAPTEUR[] = {{caps}};",
  "const char PLAN_ACTION[]  = {{acts}};",
  "const int  N_ETAPES = {{n}};",
  "",
  "int  etape = 0;                     // étape courante du plan",
  "char surveille = PLAN_CAPTEUR[0];   // capteur surveillé pour cette étape",
  "",
  "bool  detectionArmee = true;      // prête à détecter la perte du mur",
  "float dernierEcho    = 1.0;       // dernier écho proche du capteur surveillé (m)",
  "float filtreG = 1, filtreD = 1;   // distances filtrées (anti-bruit)",
  "float erreurPrec = 0;             // erreur du cycle précédent (pour la dérivée)",
  "unsigned long tSansEcho = 0;      // temps sans écho du côté surveillé (ms)",
  "unsigned long tDerniereEtape = 0; // pour la règle des 2,5 m (anti faux virage)",
  "",
  "void loop() {",
  "  float dG = lireDistance(TRIG_G, ECHO_G);  // mur gauche (m)",
  "  float dD = lireDistance(TRIG_D, ECHO_D);  // mur droite (m)",
  "  float dSurv = (surveille == 'D') ? dD : dG;  // côté surveillé",
  "",
  "  /* Signature d'un couloir : les deux murs se font face à 2 m.",
  "     Si ce n'est pas le cas, on est dans une jonction → on ne se cale pas. */",
  "  bool dansCouloir = (dG < 2.0 && dD < 2.0 && fabs(dG + dD - 2.0) < 0.4);",
  "",
  "  /* Mur surveillé perdu : plus d'écho, ou un écho invraisemblable",
  "     (> 1,45 m alors qu'on n'est pas dans un couloir : le rayon",
  "     a glissé sur un mur au fond d'une jonction). */",
  "  bool murPerdu;",
  "  if (surveille == 'B')  /* étape « tout droit » : les deux capteurs */",
  "    murPerdu = (dD > PORTEE || (dD > 1.45 && !dansCouloir))",
  "            || (dG > PORTEE || (dG > 1.45 && !dansCouloir));",
  "  else",
  "    murPerdu = (dSurv > PORTEE) || (dSurv > 1.45 && !dansCouloir);",
  "  if (murPerdu) tSansEcho += 50; else { tSansEcho = 0; dernierEcho = dSurv; }",
  "",
  "  /* Une étape ne peut pas arriver avant ≈ 2,5 m parcourus depuis la",
  "     précédente (odométrie : millis() × vitesse — ou mieux, des",
  "     encodeurs de roues pour mesurer la vraie distance). */",
  "  bool assezLoin = (millis() - tDerniereEtape > 2500);",
  "",
  "  if (detectionArmee && murPerdu && assezLoin",
  "      && (dernierEcho < 1.5 || tSansEcho > 300)) {",
  "    char action = PLAN_ACTION[etape % N_ETAPES];",
  "    if      (action == 'R') virer(+90);   // quart de tour à droite",
  "    else if (action == 'L') virer(-90);   // quart de tour à gauche",
  "    /* action == 'S' : tout droit, on franchit juste la jonction */",
  "    etape++;",
  "    surveille = PLAN_CAPTEUR[etape % N_ETAPES];",
  "    detectionArmee = false;",
  "    tDerniereEtape = millis();",
  "    tSansEcho = 0; dernierEcho = 99;",
  "  }",
  "",
  "  /* Les deux murs du nouveau couloir sont revus à moins de 1,5 m",
  "     → on réarme la détection */",
  "  if (!detectionArmee && dansCouloir && filtreG < 1.5 && filtreD < 1.5) {",
  "    detectionArmee = true;",
  "    filtreG = dG; filtreD = dD;  // reset du filtre après le blank",
  "    erreurPrec = 0;",
  "  }",
  "",
  "  /* Rester au milieu : commande PD sur l'erreur filtrée */",
  "  filtreG = 0.7 * filtreG + 0.3 * dG;",
  "  filtreD = 0.7 * filtreD + 0.3 * dD;",
  "",
  "  float erreur = filtreG - filtreD;         // > 0 : trop près du mur droit",
  "  if (!dansCouloir) {",
  "    /* Jonction : si un seul mur reste visible, on le tient à ~1 m",
  "       (tenue monopalpe) pour ne pas dériver en aveugle. */",
  "    if      (dG > 2.0 && dD < 2.0) erreur = 1.0 - filtreD;",
  "    else if (dD > 2.0 && dG < 2.0) erreur = filtreG - 1.0;",
  "    else                           erreur = 0;",
  "    erreur = constrain(erreur, -0.3, 0.3);",
  "    erreurPrec = 0;",
  "  }",
  "  if (fabs(erreur) < 0.02) erreur = 0;      // zone morte de 2 cm",
  "  erreur = constrain(erreur, -0.6, 0.6);",
  "",
  "  float derivee = (erreur - erreurPrec) / 0.05;   // vitesse de l'écart",
  "  derivee = constrain(derivee, -0.5, 0.5);",
  "  erreurPrec = erreur;",
  "",
  "  int cmd = constrain(KP * erreur + KD * derivee, -150, 150);",
  "",
  "  analogWrite(MOT_G, VITESSE + cmd);  // trop à droite → tourne à gauche",
  "  analogWrite(MOT_D, VITESSE - cmd);",
  "}",
];

function renderCode() {
  var route = routeById(routeId);
  var caps = [], acts = [];
  for (var i = 0; i < route.steps.length; i++) {
    caps.push("'" + route.steps[i].c + "'");
    acts.push("'" + route.steps[i].t + "'");
  }
  var live = {
    vitesse: params.vitesse,
    kp: params.kp,
    kd: params.kd,
    portee: params.portee.toFixed(2),
    vmax: params.vmax.toFixed(2),
    virage: params.virage,
    tpsVirage: params.tpsVirage,
    caps: caps.join(", "),
    acts: acts.join(", "),
    n: route.steps.length,
  };
  var html = "";
  for (var i = 0; i < CODE_LINES.length; i++) {
    html += esc(CODE_LINES[i]).replace(/\{\{(\w+)\}\}/g, function (m, k) {
      return live[k] !== undefined ? hl(live[k]) : m;
    });
    if (i < CODE_LINES.length - 1) html += "\n";
  }
  $("code-block").innerHTML = html;
}


/* ---------------- mise à jour du DOM (chaque frame) ---------------- */
function statusOf(s) {
  if (s.mode === "TURN")
    return { label: s.turnDir > 0 ? "↱ Virage à droite" : "↰ Virage à gauche", cls: "turn" };
  if (s.mode === "CRASH") return { label: "💥 Collision", cls: "crash" };
  if (s.mode === "FIN") return { label: "🏁 Arrivée", cls: "fin" };
  if (!s.arme) return { label: "Recentrage… tout droit", cls: "recentre" };
  if (s.corridor) return { label: "Centrage actif", cls: "align" };
  return { label: "Jonction — tout droit", cls: "transit" };
}

function updateDOM() {
  var route = routeById(routeId);
  var nSteps = route.steps.length;
  var step = route.steps[sim.stepIdx % nSteps];

  $("badge-route").textContent = route.nom;
  $("badge-laps").textContent = "Tour " + sim.laps + "/3";
  $("badge-time").textContent = "⏱ " + fmtTime(sim.time);
  $("badge-best").textContent = "🏆 " + fmtTime(sim.best);

  var st = statusOf(sim);
  var badge = $("status-badge");
  badge.textContent = st.label;
  badge.className = "status " + st.cls;

  var cLabel = step.c === "D" ? '<b class="d">le capteur droit</b>'
    : step.c === "G" ? '<b class="g">le capteur gauche</b>'
    : '<b class="b">les deux capteurs</b>';
  var action = step.t === "R" ? "→ virage droite"
    : step.t === "L" ? "→ virage gauche" : "→ tout droit";
  $("step-info").innerHTML = (sim.stepIdx % nSteps) + 1 + "/" + nSteps +
    " — surveille " + cLabel + " " + action;

  $("txt-g").textContent = sim.dGno ? "aucun écho" : sim.dG.toFixed(2) + " m";
  $("bar-g").style.width = Math.min(100, (sim.dG / params.portee) * 100) + "%";
  $("txt-d").textContent = sim.dDno ? "aucun écho" : sim.dD.toFixed(2) + " m";
  $("bar-d").style.width = Math.min(100, (sim.dD / params.portee) * 100) + "%";

  $("txt-err").textContent = (sim.err === 0 && sim.mode !== "TURN")
    ? "—" : (sim.err * 100).toFixed(0) + " cm";
  $("txt-v").textContent = sim.v.toFixed(2) + " m/s";
  $("txt-pg").textContent = String(Math.round(sim.pwmG));
  $("bar-pg").style.width = (Math.abs(sim.pwmG) / 255) * 100 + "%";
  $("txt-pd").textContent = String(Math.round(sim.pwmD));
  $("bar-pd").style.width = (Math.abs(sim.pwmD) / 255) * 100 + "%";

  $("txt-lastlap").textContent = fmtTime(sim.lastLap);
  $("txt-record").textContent = fmtTime(sim.best);
  $("txt-crash").textContent = String(sim.crashes);

  var btnRun = $("btn-run");
  btnRun.textContent = running ? "⏸ Pause" : "▶ Démarrer";
  btnRun.disabled = (sim.mode === "CRASH" || sim.mode === "FIN");

  // overlay collision / arrivée
  var ov = $("overlay");
  if (sim.mode === "CRASH") {
    ov.className = "overlay crash";
    $("overlay-title").textContent = "💥 Collision !";
    $("overlay-text").textContent =
      "La machine a touché un mur. Essaie : réduire la vitesse, remettre KD autour de 30 " +
      "(l'amortissement), ou baisser KP si la machine zigzague.";
  } else if (sim.mode === "FIN") {
    ov.className = "overlay fin";
    $("overlay-title").textContent = "🏁 Arrivée !";
    $("overlay-text").innerHTML =
      "3 tours en <b>" + fmtTime(sim.time) + "</b> — meilleur tour : <b>" +
      fmtTime(sim.best) + "</b>";
  } else {
    ov.className = "overlay hidden";
  }
}

/* ---------------- boucle principale ---------------- */
var lastT = window.performance.now();
function loop(now) {
  var dtR = Math.min(0.05, (now - lastT) / 1000);
  lastT = now;
  if (running) {
    if (sim.mode === "CRASH" || sim.mode === "FIN") {
      running = false;
    } else {
      var acc = dtR * simSpeed;
      var h = 1 / 240;
      while (acc > 0 && sim.mode !== "CRASH" && sim.mode !== "FIN") {
        var dt = Math.min(h, acc);
        tick(sim, params, dt);
        acc -= dt;
      }
      if (sim.mode === "CRASH" || sim.mode === "FIN") running = false;
    }
  }
  draw();
  updateDOM();
  requestAnimationFrame(loop);
}

/* ---------------- initialisation ---------------- */
$("btn-run").onclick = toggleRun;
$("btn-reset").onclick = reset;
$("overlay-btn").onclick = reset;
$("chk-beams").onchange = function () { beams = $("chk-beams").checked; };

var speedBtns = document.querySelectorAll(".speed-btn");
for (var sb = 0; sb < speedBtns.length; sb++) {
  (function (btn) {
    btn.onclick = function () {
      simSpeed = parseFloat(btn.getAttribute("data-s"));
      for (var k = 0; k < speedBtns.length; k++) speedBtns[k].classList.remove("active");
      btn.classList.add("active");
    };
  })(speedBtns[sb]);
}

$("add-r").onclick = function () { customSteps.push("R"); refreshCustom(); };
$("add-s").onclick = function () { customSteps.push("S"); refreshCustom(); };
$("add-l").onclick = function () { customSteps.push("L"); refreshCustom(); };
$("clear-steps").onclick = function () { customSteps = []; refreshCustom(); };
$("use-custom").onclick = function () {
  if (!customSteps.length) return;
  chooseRoute("custom");
};

document.addEventListener("keydown", function (e) {
  var tag = e.target && e.target.tagName;
  if (tag === "INPUT" || tag === "BUTTON" || tag === "SUMMARY" || tag === "A") return;
  if (e.code === "Space") { e.preventDefault(); toggleRun(); }
  else if (e.code === "KeyR") { reset(); }
});

window.addEventListener("resize", function () { resize(); });

buildParams();
buildStartChips();
refreshCustom();
buildRouteList();
renderCode();
resize();
reset();
requestAnimationFrame(loop);
})();
