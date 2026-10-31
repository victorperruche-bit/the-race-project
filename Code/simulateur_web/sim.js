/* =====================================================================
   SIM.JS — Logique du simulateur The Race Project
   Portage direct du composant React (aucune dépendance, aucun DOM).
   Exposé en variables globales : ROUTES, CUSTOM_STARTS, makeSim, tick, ...
   ===================================================================== */

var VMAX = 1.5;        // (obsolète : utiliser P.vmax = VMAX_M du robot reel)
var TRACKW = 0.2;      // voie entre les roues (m)
var T = 0.35;          // épaisseur des murs extérieurs
var REARM = 1.5;       // réarmement : les 2 murs vus à moins de 1,5 m
var CAP = 2.0;         // au-delà de 2 m, un écho n'est plus "le mur du couloir"
var STREAK_MAX = 0.3;  // mur surveillé invisible depuis plus de 0,3 s → vraiment parti
var MIN_RUN = 2.5;     // distance mini depuis la dernière étape (anti faux déclenchement)
var TRIG_CAP = 1.45;   // écho surveillé > 1,45 m hors couloir = le mur est parti
var SUM_TOL = 0.4;     // |dG + dD − 2| < 0,4 : signature d'un vrai couloir de 2 m

var BLOC_HAUT = { x: 0, y: 0, w: 20, h: 5 };
var BLOC_GAUCHE = { x: 0, y: 7, w: 9, h: 5 };
var BLOC_DROIT = { x: 11, y: 7, w: 9, h: 5 };
var WALLS = [
  BLOC_HAUT,
  BLOC_GAUCHE,
  BLOC_DROIT,
  { x: -2 - T, y: -2 - T, w: 24 + 2 * T, h: T }, // mur haut
  { x: -2 - T, y: 14, w: 24 + 2 * T, h: T },     // mur bas
  { x: -2 - T, y: -2, w: T, h: 16 },             // mur gauche
  { x: 22, y: -2, w: T, h: 16 },                 // mur droit
];

/* ---------- LES 7 PARCOURS ----------
   start : [x, y, cap]   fin : {axis, pos, dir, lo, hi, th}
   étapes : {c: "D"|"G"|"B" (capteur(s) surveillé(s)), t: "R"|"L"|"S"}
   R = virage à droite · L = virage à gauche · S = tout droit ---------- */
var ROUTES = [
  {
    id: "bg-h", nom: "Bloc gauche — horaire",
    start: [-1, 9.5, (3 * Math.PI) / 2],
    fin: { axis: "y", pos: 9.5, dir: -1, lo: -2, hi: 0, th: (3 * Math.PI) / 2 },
    steps: [{ c: "D", t: "R" }, { c: "D", t: "R" }, { c: "D", t: "R" }, { c: "D", t: "R" }],
  },
  {
    id: "bg-a", nom: "Bloc gauche — anti-horaire",
    start: [-1, 9.5, Math.PI / 2],
    fin: { axis: "y", pos: 9.5, dir: 1, lo: -2, hi: 0, th: Math.PI / 2 },
    steps: [{ c: "G", t: "L" }, { c: "G", t: "L" }, { c: "G", t: "L" }, { c: "G", t: "L" }],
  },
  {
    id: "bd-h", nom: "Bloc droit — horaire",
    start: [15, 6, 0],
    fin: { axis: "x", pos: 15, dir: 1, lo: 5, hi: 7, th: 0 },
    steps: [{ c: "D", t: "R" }, { c: "D", t: "R" }, { c: "D", t: "R" }, { c: "D", t: "R" }],
  },
  {
    id: "bd-a", nom: "Bloc droit — anti-horaire",
    start: [15, 6, Math.PI],
    fin: { axis: "x", pos: 15, dir: -1, lo: 5, hi: 7, th: Math.PI },
    steps: [{ c: "G", t: "L" }, { c: "G", t: "L" }, { c: "G", t: "L" }, { c: "G", t: "L" }],
  },
  {
    id: "gt-h", nom: "Grand tour — horaire",
    start: [-1, 9.5, (3 * Math.PI) / 2],
    fin: { axis: "y", pos: 9.5, dir: -1, lo: -2, hi: 0, th: (3 * Math.PI) / 2 },
    steps: [
      { c: "D", t: "R" }, { c: "D", t: "S" }, { c: "D", t: "R" },
      { c: "D", t: "R" }, { c: "D", t: "S" }, { c: "D", t: "R" },
    ],
  },
  {
    id: "gt-a", nom: "Grand tour — anti-horaire",
    start: [-1, 9.5, Math.PI / 2],
    fin: { axis: "y", pos: 9.5, dir: 1, lo: -2, hi: 0, th: Math.PI / 2 },
    steps: [
      { c: "G", t: "L" }, { c: "G", t: "S" }, { c: "G", t: "L" },
      { c: "G", t: "L" }, { c: "G", t: "S" }, { c: "G", t: "L" },
    ],
  },
  {
    id: "bh-h", nom: "Bloc haut — horaire",
    start: [15, -1, 0],
    fin: { axis: "x", pos: 15, dir: 1, lo: -2, hi: 0, th: 0 },
    steps: [
      { c: "D", t: "R" }, { c: "D", t: "R" }, { c: "G", t: "S" },
      { c: "D", t: "R" }, { c: "D", t: "R" },
    ],
  },
];

/* ---------- départs possibles du parcours libre ---------- */
var CUSTOM_STARTS = [
  { id: "w-n", label: "Couloir ouest ⬆", start: [-1, 9.5, (3 * Math.PI) / 2],
    fin: { axis: "y", pos: 9.5, dir: -1, lo: -2, hi: 0, th: (3 * Math.PI) / 2 } },
  { id: "w-s", label: "Couloir ouest ⬇", start: [-1, 9.5, Math.PI / 2],
    fin: { axis: "y", pos: 9.5, dir: 1, lo: -2, hi: 0, th: Math.PI / 2 } },
  { id: "m-e", label: "Couloir de liaison →", start: [15, 6, 0],
    fin: { axis: "x", pos: 15, dir: 1, lo: 5, hi: 7, th: 0 } },
  { id: "m-w", label: "Couloir de liaison ←", start: [15, 6, Math.PI],
    fin: { axis: "x", pos: 15, dir: -1, lo: 5, hi: 7, th: Math.PI } },
  { id: "n-e", label: "Couloir nord →", start: [15, -1, 0],
    fin: { axis: "x", pos: 15, dir: 1, lo: -2, hi: 0, th: 0 } },
];

function clamp(v, a, b) { return Math.max(a, Math.min(b, v)); }

/* ---------- lancer de rayon (capteur ultrasons) ---------- */
function raySeg(px, py, dx, dy, x1, y1, x2, y2) {
  var ex = x2 - x1, ey = y2 - y1;
  var D = dx * ey - ex * dy;
  if (Math.abs(D) < 1e-12) return null;
  var rx = x1 - px, ry = y1 - py;
  var t = (rx * ey - ex * ry) / D;
  var u = (dy * rx - dx * ry) / D;
  if (t > 1e-6 && u >= -1e-9 && u <= 1 + 1e-9) return t;
  return null;
}

function castRay(px, py, dx, dy) {
  var best = Infinity;
  for (var i = 0; i < WALLS.length; i++) {
    var w = WALLS[i];
    var segs = [
      [w.x, w.y, w.x + w.w, w.y],
      [w.x + w.w, w.y, w.x + w.w, w.y + w.h],
      [w.x + w.w, w.y + w.h, w.x, w.y + w.h],
      [w.x, w.y + w.h, w.x, w.y],
    ];
    for (var j = 0; j < segs.length; j++) {
      var s = segs[j];
      var t = raySeg(px, py, dx, dy, s[0], s[1], s[2], s[3]);
      if (t !== null && t < best) best = t;
    }
  }
  return best;
}

/* ---------- collision : points du pourtour de la machine ---------- */
var BODY_PTS = [
  [0.14, 0.09], [0.14, -0.09], [-0.14, 0.09], [-0.14, -0.09],
  [0.14, 0], [-0.14, 0], [0, 0.09], [0, -0.09],
];
function hitWall(s) {
  var c = Math.cos(s.th), si = Math.sin(s.th);
  for (var i = 0; i < BODY_PTS.length; i++) {
    var p = BODY_PTS[i];
    var wx = s.x + p[0] * c - p[1] * si;
    var wy = s.y + p[0] * si + p[1] * c;
    for (var j = 0; j < WALLS.length; j++) {
      var w = WALLS[j];
      if (wx > w.x && wx < w.x + w.w && wy > w.y && wy < w.y + w.h) return true;
    }
  }
  return false;
}

/* ---------- état de la simulation ---------- */
function makeSim(route) {
  return {
    route: route, stepIdx: 0,
    x: route.start[0], y: route.start[1], th: route.start[2],
    prevAx: route.fin.axis === "y" ? route.start[1] : route.start[0],
    mode: "ALIGN", turned: 0, turnDir: 1, arme: true, dist: 0,
    time: 0, sensorAcc: 1,
    dG: 1, dD: 1, dGno: false, dDno: false, dGraw: 1, dDraw: 1,
    gPrevEcho: 1.0, dPrevEcho: 1.0, gStreak: 0, dStreak: 0,
    fG: 1, fD: 1, err: 0, errRate: 0, prevRaw: null,
    corridor: true,
    laps: 0, lapStart: 0, lastLap: null, best: null,
    crashes: 0, trail: [[route.start[0], route.start[1]]],
    pwmG: 0, pwmD: 0, v: 0,
  };
}

/* ---------- lecture capteurs (toutes les 50 ms, comme le vrai code) ---------- */
function sample(s, P) {
  var lg = castRay(s.x, s.y, Math.sin(s.th), -Math.cos(s.th)); // gauche
  var rg = castRay(s.x, s.y, -Math.sin(s.th), Math.cos(s.th)); // droite
  s.dGraw = lg; s.dDraw = rg;
  var nz = function () { return (Math.random() * 2 - 1) * (P.bruit / 100); };
  // pour le centrage, un mur vu à plus de 2 m n'est pas "le mur du couloir"
  var gE = lg <= CAP - 1e-9, dE = rg <= CAP - 1e-9;
  s.dG = gE ? Math.max(0.02, lg + nz()) : CAP;
  s.dD = dE ? Math.max(0.02, rg + nz()) : CAP;
  // signature d'un couloir : les deux murs se font face à 2 m
  var bothEcho = gE && dE;
  var sumOK = bothEcho && Math.abs(s.dG + s.dD - 2) < SUM_TOL;
  // mur perdu : plus d'écho, ou un écho invraisemblable (> 1,45 m)
  // alors qu'on n'est pas dans un couloir
  var curGno = lg > P.portee - 1e-9 || (lg > TRIG_CAP && !sumOK);
  var curDno = rg > P.portee - 1e-9 || (rg > TRIG_CAP && !sumOK);
  if (curGno) s.gStreak += 0.05; else { s.gStreak = 0; s.gPrevEcho = lg; }
  if (curDno) s.dStreak += 0.05; else { s.dStreak = 0; s.dPrevEcho = rg; }
  s.dGno = curGno; s.dDno = curDno;

  if (sumOK) {
    if (s.prevRaw === null) {
      // 1re mesure fiable après un blanc : on réinitialise le filtre
      s.fG = s.dG; s.fD = s.dD; s.errRate = 0;
    } else {
      // filtre exponentiel : lisse le bruit des capteurs
      s.fG = 0.7 * s.fG + 0.3 * s.dG;
      s.fD = 0.7 * s.fD + 0.3 * s.dD;
      var rawRate = ((s.fG - s.fD) - s.prevRaw) / 0.05;
      s.errRate = clamp(0.5 * s.errRate + 0.5 * rawRate, -0.5, 0.5);
    }
    s.prevRaw = s.fG - s.fD;
    var e = clamp(s.fG - s.fD, -0.6, 0.6);
    if (Math.abs(e) < 0.02) e = 0; // zone morte : on ne réagit pas à 2 cm près
    s.err = e;
  } else {
    /* Jonction : un seul mur visible → tenue monopalpe douce.
       On garde ~1 m du mur qui reste, pour ne pas dériver en aveugle. */
    if (!gE && dE) {
      var e1 = clamp(1.0 - s.dD, -0.3, 0.3);
      if (Math.abs(e1) < 0.02) e1 = 0;
      s.err = e1; s.errRate = 0;
    } else if (!dE && gE) {
      var e2 = clamp(s.dG - 1.0, -0.3, 0.3);
      if (Math.abs(e2) < 0.02) e2 = 0;
      s.err = e2; s.errRate = 0;
    } else { s.err = 0; s.errRate = 0; }
    s.prevRaw = null;
  }
  s.corridor = sumOK && s.fG < REARM && s.fD < REARM;
}

/* ---------- un pas de physique (logique Arduino simulée) ---------- */
function tick(s, P, dt) {
  if (s.mode === "CRASH" || s.mode === "FIN") return;
  s.time += dt;
  s.sensorAcc += dt;
  if (s.sensorAcc >= 0.05) { s.sensorAcc -= 0.05; sample(s, P); }

  var route = s.route;
  var step = route.steps[s.stepIdx % route.steps.length];
  var c = step.c; // "D" capteur droit · "G" gauche · "B" les deux
  var lost = c === "D" ? s.dDno : c === "G" ? s.dGno : (s.dDno || s.dGno);
  var prevEcho = c === "D" ? s.dPrevEcho : c === "G" ? s.gPrevEcho : Math.min(s.dPrevEcho, s.gPrevEcho);
  var streak = c === "D" ? s.dStreak : c === "G" ? s.gStreak : Math.max(s.dStreak, s.gStreak);

  if (s.mode === "TURN") {
    var dir = s.turnDir;
    // translation pendant le virage (arc), comme le modele valide
    var vT = (P.virage / 255) * P.vmax;
    // duree du quart de tour = TEMPS_VIRAGE_90_MS du .ino (P.tpsVirage en ms).
    // Robot reel : pivot sur place (~420 ms). Simulation : arc, ~2100 ms par
    // defaut (defaut valide, la machine balaie le coin pendant le virage).
    var om = (Math.PI / 2) / (P.tpsVirage / 1000);
    var dth = dir * om * dt;
    // le virage se termine à l'angle exact (pas de dépassement)
    if (Math.abs(s.turned + dth) >= Math.PI / 2) dth = dir * (Math.PI / 2 - s.turned);
    s.th += dth;
    s.turned += Math.abs(dth);
    s.x += vT * Math.cos(s.th) * dt;
    s.y += vT * Math.sin(s.th) * dt;
    s.pwmG = -dir * P.virage; s.pwmD = dir * P.virage; s.v = 0;
    if (s.turned >= Math.PI / 2 - 1e-9) { s.mode = "RECENTRE"; s.turned = 0; }
  } else {
    // étape suivante si le mur surveillé vient de disparaître (il était à
    // moins de 1,5 m) ou il est invisible depuis plus de 0,3 s — et à
    // condition d'avoir roulé au moins 2,5 m depuis la dernière étape
    if (s.arme && lost && s.dist > MIN_RUN &&
        (prevEcho < 1.5 || streak > STREAK_MAX)) {
      if (c === "D") { s.dPrevEcho = 99; s.dStreak = 0; }
      else if (c === "G") { s.gPrevEcho = 99; s.gStreak = 0; }
      else { s.dPrevEcho = 99; s.dStreak = 0; s.gPrevEcho = 99; s.gStreak = 0; }
      s.arme = false; s.dist = 0;
      s.stepIdx += 1;
      if (step.t === "S") {
        s.mode = "TRANSIT"; // jonction : on continue tout droit
      } else {
        s.mode = "TURN"; s.turned = 0;
        s.turnDir = step.t === "R" ? 1 : -1;
      }
    } else {
      // réarmement quand les 2 murs du couloir sont revus à moins de 1,5 m
      var seuil = Math.min(P.portee - 0.25, REARM);
      if (!s.arme && s.corridor && s.fG < seuil && s.fD < seuil) s.arme = true;
      s.mode = !s.arme ? "RECENTRE" : (s.corridor ? "ALIGN" : "TRANSIT");
      // commande PD : proportionnelle + dérivée (amortissement)
      var cmd = clamp(P.kp * s.err + P.kd * s.errRate, -150, 150);
      s.pwmG = clamp(P.vitesse + cmd, 0, 255);
      s.pwmD = clamp(P.vitesse - cmd, 0, 255);
      var vG = (s.pwmG / 255) * P.vmax, vD = (s.pwmD / 255) * P.vmax;
      var v = (vG + vD) / 2;
      s.x += v * Math.cos(s.th) * dt;
      s.y += v * Math.sin(s.th) * dt;
      s.th += ((vD - vG) / TRACKW) * dt;
      s.v = v;
      s.dist += v * dt; // odométrie depuis la dernière étape
    }
  }
  s.th = ((s.th % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);

  // passage de la ligne d'arrivée (on ignore la 1re seconde : au départ,
  // la machine est posée SUR la ligne)
  var fin = route.fin;
  var cur = fin.axis === "y" ? s.y : s.x;
  var crossed = fin.dir === -1
    ? (s.prevAx > fin.pos && cur <= fin.pos)
    : (s.prevAx < fin.pos && cur >= fin.pos);
  var inRange = fin.axis === "y"
    ? (s.x > fin.lo && s.x < fin.hi)
    : (s.y > fin.lo && s.y < fin.hi);
  var dthFin = Math.abs(
    (((s.th - fin.th + Math.PI) % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI) - Math.PI
  );
  if (s.time > 1 && crossed && inRange && dthFin < 0.8) {
    s.laps += 1;
    var lt = s.time - s.lapStart;
    s.lastLap = lt; s.lapStart = s.time;
    if (s.best === null || lt < s.best) s.best = lt;
    if (s.laps >= 3) s.mode = "FIN";
  }
  s.prevAx = cur;

  // trace du trajet
  var last = s.trail[s.trail.length - 1];
  var d2 = (s.x - last[0]) * (s.x - last[0]) + (s.y - last[1]) * (s.y - last[1]);
  if (d2 > 0.09) {
    s.trail.push([s.x, s.y]);
    if (s.trail.length > 500) s.trail.shift();
  }

  if (s.mode !== "FIN" && hitWall(s)) { s.mode = "CRASH"; s.crashes += 1; }
}

function fmtTime(t) {
  if (t === null || t === undefined) return "—";
  var m = Math.floor(t / 60);
  var sec = t - m * 60;
  var s = sec.toFixed(1);
  while (s.length < 4) s = "0" + s;
  return m + ":" + s;
}
