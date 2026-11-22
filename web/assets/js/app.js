/* STI2D-OS V2 — éléments originaux : Copyright © 2026 Yamii (cjnw) et Derim (irationnel).
 * Tous droits réservés. Voir /LICENSE et web/LEGAL.html. Les composants tiers gardent
 * leurs licences propres. Contact : https://discord.gg/sti2d
 */
/* STI2D-OS portal — application.
 *
 * Hash routes, one renderer per page, delegated events. Preferences (small strings) live in
 * localStorage; imported files and wallpapers live in IndexedDB (store.js). Nothing is sent
 * anywhere: the portal works offline once loaded.
 */
(function () {
  "use strict";

  var GAMES = Array.isArray(window.STI2D_GAMES) ? window.STI2D_GAMES : [];
  var AIS = Array.isArray(window.STI2D_AI) ? window.STI2D_AI : [];
  var Store = window.STIStore, Nwa = window.STINwa, Img = window.STIImage;
  var DISCORD = "https://discord.gg/sti2d";
  var VERSION = "2.2.0";

  /* ---------------------------------------------------------------- helpers */
  function $(sel, root) {
    return (root || document).querySelector(sel);
  }
  function $$(sel, root) {
    return Array.prototype.slice.call((root || document).querySelectorAll(sel));
  }
  function esc(v) {
    return String(v == null ? "" : v).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  }
  function icon(id, cls) {
    return '<svg class="' + (cls || "") + '" aria-hidden="true"><use href="#i-' + id + '"/></svg>';
  }
  function size(n) {
    if (n == null) return "—";
    if (n === 0) return "0 Ko";
    if (n >= 1073741824) return (n / 1073741824).toFixed(1).replace(".", ",") + " Go";
    if (n >= 1048576) return (n / 1048576).toFixed(1).replace(".", ",") + " Mo";
    return Math.max(1, Math.round(n / 1024)) + " Ko";
  }
  function date(t) {
    try {
      return new Date(t).toLocaleDateString("fr-FR", { day: "numeric", month: "short", year: "numeric" });
    } catch (e) {
      return "";
    }
  }
  function plural(n, one, many) {
    return n + " " + (n > 1 ? many : one);
  }
  function fileUrl(path) {
    /* the manifest keeps real names ("SubwaySurfers (1).nwa"): encode the name once */
    var i = path.lastIndexOf("/");
    return path.slice(0, i + 1) + encodeURIComponent(path.slice(i + 1));
  }
  var isIOS = /iP(hone|ad|od)/.test(navigator.userAgent) || (navigator.platform === "MacIntel" && navigator.maxTouchPoints > 1);
  var isAndroid = /Android/i.test(navigator.userAgent);
  var isFile = location.protocol === "file:";

  /* ---------------------------------------------------------------- preferences */
  var ACCENTS = [
    { id: 0, name: "Thème", hex: null },
    { id: 1, name: "Orange", hex: "#ff9800" },
    { id: 2, name: "Rose", hex: "#ff5c93" },
    { id: 3, name: "Vert", hex: "#4cc86e" },
    { id: 4, name: "Cyan", hex: "#22c7e0" },
    { id: 5, name: "Bleu", hex: "#4c7dff" },
    { id: 6, name: "Violet", hex: "#a66cff" },
    { id: 7, name: "Rouge", hex: "#f0503f" },
    { id: 8, name: "Or", hex: "#f2c14e" },
  ];
  var SKINS = [
    { id: "sti2d", name: "STI2D-OS", note: "Signature", sw: ["#0b0c0f", "#1a1d23", "#ffa42b"] },
    { id: "classic", name: "STI2D Classic", note: "Carrousel V1", sw: ["#12141a", "#ffbe28", "#f57c00"] },
    { id: "3ds", name: "Nintendo 3DS", note: "Clair et arrondi", sw: ["#eef0f3", "#ffffff", "#e60012"] },
    { id: "ps5", name: "PlayStation 5", note: "Nuit bleue", sw: ["#050816", "#13235a", "#2f7dff"] },
    { id: "ios", name: "iOS", note: "Noir et cartes", sw: ["#000000", "#1c1c1e", "#0a84ff"] },
    { id: "windows", name: "Windows", note: "Fluent", sw: ["#1c1c1c", "#2b2b2b", "#60cdff"] },
    { id: "xbox", name: "Xbox", note: "Noir et vert", sw: ["#0b0b0b", "#171717", "#3ad100"] },
    { id: "linux", name: "Linux / GNOME", note: "Bureau GNOME", sw: ["#302747", "#5b4a77", "#c7a8ff"] },
    { id: "android", name: "Android", note: "Material You", sw: ["#202b43", "#3b4f79", "#b8c8ff"] },
    { id: "macos", name: "macOS", note: "Bureau macOS", sw: ["#182b43", "#668eb8", "#9bcaff"] },
  ];
  var MODELS = {
    n0110: { name: "N0110", cpu: "216 MHz", note: "Epsilon officiel (userland)" },
    n0115: { name: "N0115", cpu: "216 MHz", note: "Epsilon officiel (userland)" },
    n0120: { name: "N0120", cpu: "550 MHz", note: "Epsilon officiel (userland)" },
  };
  var DEFAULTS = {
    skin: "sti2d",
    accent: 0,
    motion: true,
    calcEnv: 0, /* 0 STI2D-OS, 1 STI2D Classic */
    calcAnim: true,
    wallpaper: "", /* id of a wallpaper in IndexedDB */
    portalWall: false,
    model: "",
    libTab: "included",
    filter: "Tout",
  };
  var prefs = loadPrefs();

  function loadPrefs() {
    var p = Object.assign({}, DEFAULTS);
    try {
      var raw = localStorage.getItem("sti2d.prefs.v3");
      if (raw) Object.assign(p, sanitize(JSON.parse(raw)));
    } catch (e) {
      /* private mode or corrupt value: defaults */
    }
    return p;
  }
  function savePrefs() {
    try {
      localStorage.setItem("sti2d.prefs.v3", JSON.stringify(prefs));
    } catch (e) {}
  }
  /* keeps only known keys with valid values (used for imports too) */
  function sanitize(o) {
    var out = {};
    if (!o || typeof o !== "object") return out;
    if (SKINS.some(function (s) { return s.id === o.skin; })) out.skin = o.skin;
    if (Number.isInteger(o.accent) && o.accent >= 0 && o.accent < ACCENTS.length) out.accent = o.accent;
    if (typeof o.motion === "boolean") out.motion = o.motion;
    if (o.calcEnv === 0 || o.calcEnv === 1) out.calcEnv = o.calcEnv;
    if (typeof o.calcAnim === "boolean") out.calcAnim = o.calcAnim;
    if (typeof o.wallpaper === "string" && o.wallpaper.length < 80) out.wallpaper = o.wallpaper;
    if (typeof o.portalWall === "boolean") out.portalWall = o.portalWall;
    if (typeof o.model === "string" && (o.model === "" || MODELS[o.model])) out.model = o.model;
    if (o.libTab === "included" || o.libTab === "personal" || o.libTab === "ai") out.libTab = o.libTab;
    if (typeof o.filter === "string" && o.filter.length < 20) out.filter = o.filter;
    return out;
  }

  function applyAppearance() {
    var root = document.documentElement;
    root.setAttribute("data-skin", prefs.skin);
    var acc = ACCENTS[prefs.accent] && ACCENTS[prefs.accent].hex;
    if (acc) {
      root.style.setProperty("--accent", acc);
      root.style.setProperty("--accent-soft", hexA(acc, 0.13));
      root.style.setProperty("--accent-line", hexA(acc, 0.4));
      root.style.setProperty("--accent-ink", luminance(acc) > 0.45 ? "#15120c" : "#ffffff");
    } else {
      ["--accent", "--accent-soft", "--accent-line", "--accent-ink"].forEach(function (k) {
        root.style.removeProperty(k);
      });
    }
    root.classList.toggle("reduce-motion", !prefs.motion);
    var meta = $('meta[name="theme-color"]');
    if (meta) meta.setAttribute("content", getComputedStyle(root).getPropertyValue("--bg").trim() || "#0b0c0f");
    applyPortalWallpaper();
  }
  function hexA(hex, a) {
    var n = parseInt(hex.slice(1), 16);
    return "rgba(" + (n >> 16) + "," + ((n >> 8) & 255) + "," + (n & 255) + "," + a + ")";
  }
  function luminance(hex) {
    var n = parseInt(hex.slice(1), 16);
    return (0.299 * (n >> 16) + 0.587 * ((n >> 8) & 255) + 0.114 * (n & 255)) / 255;
  }

  /* ---------------------------------------------------------------- personal data cache */
  var cache = { nwa: [], walls: [], ready: false, error: null, urls: {} };

  function refreshCache() {
    return Promise.all([Store.list("nwa"), Store.list("wallpaper")])
      .then(function (r) {
        cache.nwa = r[0];
        cache.walls = r[1];
        cache.ready = true;
        cache.error = null;
        return loadCovers(r[0].concat(r[1]));
      })
      .catch(function (e) {
        cache.ready = true;
        cache.error = e.message || String(e);
      });
  }
  /* object URLs for covers and wallpapers, made once per id */
  function loadCovers(items) {
    return Promise.all(
      items.map(function (it) {
        var key = it.kind === "wallpaper" ? it.id : it.id + ":cover";
        if (cache.urls[key] !== undefined) return null;
        return Store.getBlob(key).then(function (b) {
          cache.urls[key] = b ? URL.createObjectURL(b) : null;
        });
      })
    );
  }
  function dropUrl(key) {
    if (cache.urls[key]) URL.revokeObjectURL(cache.urls[key]);
    delete cache.urls[key];
  }
  function wallUrl(id) {
    return id ? cache.urls[id] || null : null;
  }
  function currentWall() {
    return cache.walls.filter(function (w) {
      return w.id === prefs.wallpaper;
    })[0];
  }
  function applyPortalWallpaper() {
    var url = prefs.portalWall && wallUrl(prefs.wallpaper);
    document.body.style.backgroundImage = url
      ? "linear-gradient(color-mix(in srgb, var(--bg) 88%, transparent), var(--bg) 70%), url(\"" + url + "\")"
      : "";
    document.body.style.backgroundSize = url ? "cover" : "";
    document.body.style.backgroundAttachment = url ? "fixed" : "";
  }

  /* ---------------------------------------------------------------- navigation */
  var ROUTES = {
    accueil: { title: "Accueil", icon: "home", render: viewHome },
    bibliotheque: { title: "Bibliothèque", icon: "library", render: viewLibrary },
    ia: { title: "Intelligences", icon: "ai", render: viewAI },
    studio: { title: "Personnalisation", icon: "brush", render: viewStudio },
    systemes: { title: "Systèmes", icon: "calc", render: viewSystems },
    documents: { title: "Lecteur de fichiers", icon: "file", render: viewDocuments },
    profils: { title: "Profils", icon: "profile", render: viewProfiles },
    calculatrice: { title: "Ma calculatrice", icon: "calc", render: viewDevice },
    installation: { title: "Installation", icon: "install", render: viewInstall },
    parametres: { title: "Paramètres", icon: "settings", render: viewSettings },
  };
  var NAV = [
    { label: "Espace", items: ["accueil", "bibliotheque", "ia", "systemes", "documents"] },
    { label: "Personnaliser", items: ["studio", "profils"] },
    { label: "Appareil", items: ["calculatrice", "installation"] },
    { label: "", items: ["parametres"] },
  ];
  var TABS = ["accueil", "bibliotheque", "studio", "installation"];
  var route = "accueil";

  function renderNav() {
    var html = "";
    NAV.forEach(function (g) {
      if (g.label) html += '<div class="nav-label">' + esc(g.label) + "</div>";
      else html += '<div class="nav-label" aria-hidden="true"></div>';
      g.items.forEach(function (id) {
        var r = ROUTES[id], count = "";
        if (id === "bibliotheque") count = '<span class="nav-count">' + (GAMES.length + cache.nwa.length) + "</span>";
        if (id === "ia") count = '<span class="nav-count">' + AIS.length + "</span>";
        html += '<a class="nav-item" href="#' + id + '"' + (route === id ? ' aria-current="page"' : "") + ">" + icon(r.icon) + "<span>" + esc(r.title) + "</span>" + count + "</a>";
      });
    });
    $("#nav").innerHTML = html;
    $("#tabbar").innerHTML =
      TABS.map(function (id) {
        var r = ROUTES[id];
        var short = { accueil: "Accueil", bibliotheque: "Jeux", studio: "Studio", installation: "Installer" }[id];
        return '<a class="tab" href="#' + id + '"' + (route === id ? ' aria-current="page"' : "") + ">" + icon(r.icon) + "<span>" + short + "</span></a>";
      }).join("") +
      '<button class="tab" type="button" data-act="menu" aria-label="Toutes les pages">' + icon("more") + "<span>Plus</span></button>";
    var personalBytes = cache.nwa.reduce(function (s, i) { return s + (i.size || 0); }, 0);
    $("#sidebar-foot").innerHTML =
      '<div class="row"><span>Ma bibliothèque</span><b>' + plural(cache.nwa.length, "fichier", "fichiers") + "</b></div>" +
      '<div class="row"><span>Stockage local</span><b>' + size(personalBytes) + "</b></div>" +
      '<div class="row"><span>Portail</span><b>V' + VERSION + "</b></div>";
  }

  function go() {
    var id = (location.hash || "").replace(/^#\/?/, "").split("?")[0];
    var changed = route !== (ROUTES[id] ? id : "accueil");
    route = ROUTES[id] ? id : "accueil";
    closeMenu();
    render();
    if (changed) window.scrollTo(0, 0);
  }

  function render() {
    var r = ROUTES[route];
    document.title = (route === "accueil" ? "" : r.title + " · ") + "STI2D-OS";
    renderNav();
    var page = $("#page");
    page.innerHTML = fileModeNote() + r.render();
    if (r.after) r.after();
    afterRender[route] && afterRender[route]();
  }
  var afterRender = {};

  function fileModeNote() {
    if (!isFile || sessionStorage.getItem("sti2d.fileNote")) return "";
    return (
      '<div class="note warn" style="margin-bottom:24px">' + icon("alert") +
      '<div style="flex:1"><strong style="color:var(--text)">Portail ouvert comme fichier local.</strong> La navigation et l\'import fonctionnent ; certains navigateurs bloquent les téléchargements de la bibliothèque et le stockage dans ce mode. ' +
      "Pour tout utiliser (et pour un téléphone), lance <code>start-server.sh</code> ou <code>start-server.bat</code> puis ouvre l'adresse affichée.</div>" +
      '<button class="icon-btn" type="button" data-act="dismiss-file-note" aria-label="Masquer">' + icon("close") + "</button></div>"
    );
  }

  function openMenu() {
    $("#sidebar").classList.add("open");
    $("#scrim").hidden = false;
    $("#menu-open").setAttribute("aria-expanded", "true");
    var first = $("#sidebar .nav-item");
    if (first) first.focus();
  }
  function closeMenu() {
    $("#sidebar").classList.remove("open");
    $("#scrim").hidden = true;
    $("#menu-open").setAttribute("aria-expanded", "false");
  }

  /* ---------------------------------------------------------------- toasts & sheet */
  function toast(msg, kind) {
    var el = document.createElement("div");
    el.className = "toast" + (kind === "error" ? " error" : "");
    el.innerHTML = icon(kind === "error" ? "alert" : "check") + "<span>" + esc(msg) + "</span>";
    $("#toasts").appendChild(el);
    setTimeout(function () {
      el.remove();
    }, kind === "error" ? 6000 : 3200);
  }

  var sheetOnClose = null;
  function openSheet(title, body, foot, onClose) {
    var d = $("#sheet");
    d.innerHTML =
      '<div class="sheet-head"><h2 id="sheet-title">' + esc(title) + '</h2><button class="icon-btn" type="button" data-act="sheet-close" aria-label="Fermer">' + icon("close") + "</button></div>" +
      '<div class="sheet-body">' + body + "</div>" + (foot ? '<div class="sheet-foot">' + foot + "</div>" : "");
    sheetOnClose = onClose || null;
    if (typeof d.showModal === "function") {
      if (!d.open) d.showModal();
    } else {
      d.setAttribute("open", "");
    }
    return d;
  }
  function closeSheet() {
    var d = $("#sheet");
    if (typeof d.close === "function" && d.open) d.close();
    else d.removeAttribute("open");
  }
  function confirmSheet(title, text, okLabel, danger) {
    return new Promise(function (resolve) {
      var done = false;
      openSheet(
        title,
        "<p class=\"muted\">" + text + "</p>",
        '<button class="btn btn-ghost" type="button" data-act="sheet-close">Annuler</button><button class="btn ' + (danger ? "btn-danger" : "btn-primary") + '" type="button" id="confirm-ok">' + esc(okLabel) + "</button>",
        function () {
          if (!done) resolve(false);
        }
      );
      $("#confirm-ok").addEventListener("click", function () {
        done = true;
        closeSheet();
        resolve(true);
      });
    });
  }

  function download(blob, name) {
    var url = URL.createObjectURL(blob), a = document.createElement("a");
    a.href = url;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function () {
      URL.revokeObjectURL(url);
    }, 4000);
  }

  function pickFiles(accept, multiple) {
    return new Promise(function (resolve) {
      var input = document.createElement("input");
      input.type = "file";
      /* iOS and Android pickers gray out files whose type they do not know (.nwa): no filter there,
         the extension is checked after the choice */
      if (accept && !((isIOS || isAndroid) && /\.nwa/.test(accept))) input.accept = accept;
      if (multiple) input.multiple = true;
      input.style.display = "none";
      document.body.appendChild(input);
      input.addEventListener("change", function () {
        resolve(Array.prototype.slice.call(input.files || []));
        input.remove();
      });
      input.click();
    });
  }

  /* ---------------------------------------------------------------- shared pieces */
  function pageHead(eyebrow, title, lead, actions) {
    return (
      '<header class="page-head"><div><span class="eyebrow">' + esc(eyebrow) + "</span><h1>" + title + "</h1>" +
      (lead ? '<p class="lead">' + lead + "</p>" : "") + "</div>" + (actions ? '<div class="page-actions">' + actions + "</div>" : "") + "</header>"
    );
  }

  function calcPreview(opts) {
    opts = opts || {};
    var acc = (ACCENTS[prefs.accent] && ACCENTS[prefs.accent].hex) || "#ff9800";
    var wall = wallUrl(prefs.wallpaper);
    var classic = prefs.calcEnv === 1;
    var screen;
    if (classic) {
      screen =
        '<div class="calc-bar" style="background:' + acc + ';color:#1b1204;border-bottom-color:' + acc + '"><span>STI2D-Play</span><span style="font-weight:600;opacity:.75">' + GAMES.length + " jeux</span></div>" +
        '<div style="position:absolute;inset:16% 0 24%;display:flex;align-items:center;justify-content:center;gap:4cqw">' +
        '<div style="width:16cqw;height:24cqw;border-radius:3cqw;background:linear-gradient(' + acc + ',#7a4300);opacity:.45"></div>' +
        '<div style="width:40cqw;height:30cqw;border-radius:4cqw;background:linear-gradient(#ffbe28,' + acc + ');display:grid;place-items:center;color:#fff">' + icon("gamepad") + "</div>" +
        '<div style="width:16cqw;height:24cqw;border-radius:3cqw;background:linear-gradient(' + acc + ',#7a4300);opacity:.45"></div></div>' +
        '<div class="calc-label" style="bottom:14%">Jeux NumWorks</div><div class="calc-foot">&lt; &gt; choisir · OK ouvrir</div>';
    } else {
      /* the userland's home screen (firmware/v2/src/home_os.c, STI_USERLAND): 4-3-4 cells */
      var cells = [
        [19, -3], [19, -1], [19, 1], [19, 3],
        [50, -2], [50, 0], [50, 2],
        [81, -3], [81, -1], [81, 1], [81, 3],
      ];
      var glyphs = ["calc", "gamepad", "library", "ai", "apps", "home", "file", "image", "copy", "settings", "info"];
      screen =
        '<div class="calc-bar"><span>STI2D-OS</span><span class="calc-batt"></span></div><div class="honey">' +
        cells.map(function (c, i) {
          return '<div class="hex' + (i === 0 ? " on" : "") + '" style="left:' + (50 + c[1] * 8.75) + "%;top:" + c[0] + '%">' + icon(glyphs[i]) + "</div>";
        }).join("") +
        '</div><div class="calc-label">Calculatrice</div><div class="calc-foot">Calculs, Grapheur, Python...</div>';
    }
    return (
      '<div class="calc" style="--calc-acc:' + acc + '"><div class="calc-screen">' +
      (wall && !classic ? '<div class="calc-wall" style="background-image:url(&quot;' + wall + '&quot;)"></div>' : "") + screen +
      "</div></div>" + (opts.caption === false ? "" : '<p class="calc-caption">Aperçu indicatif · écran 320 × 240</p>')
    );
  }

  function coverHTML(item, kind) {
    var url = kind === "personal" ? cache.urls[item.id + ":cover"] : item.cover;
    var badge = kind === "personal" ? '<span class="badge">' + (item.type === "app" ? "App" : "Jeu") + "</span>" : "";
    if (url) {
      return '<div class="cover">' + badge + '<div class="cover-blur" style="background-image:url(&quot;' + esc(url) + '&quot;)"></div><img class="cover-icon" src="' + esc(url) + '" alt="" loading="lazy" decoding="async"></div>';
    }
    var glyph = item.format === "GB" ? "cartridge" : kind === "ai" ? "ai" : item.type === "app" ? "apps" : "gamepad";
    return '<div class="cover">' + badge + '<span class="cover-glyph">' + icon(glyph) + "</span></div>";
  }

  /* ================================================================ pages */

  /* ---------------------------------------------------------------- home */
  function viewHome() {
    var personalBytes = cache.nwa.reduce(function (s, i) { return s + (i.size || 0); }, 0);
    var recent = cache.nwa.slice(0, 4);
    return (
      '<section class="hero"><div><span class="eyebrow">STI2D-OS V2</span>' +
      "<h1>Ta NumWorks,<br><span>à ta façon.</span></h1>" +
      '<p class="lead">Ta bibliothèque de jeux et d\'applications, ton apparence et tes installations, au même endroit. Tout reste dans ce navigateur.</p>' +
      '<div class="page-actions"><button class="btn btn-primary" type="button" data-act="import">' + icon("plus") + "Ajouter un jeu ou une application</button>" +
      '<a class="btn" href="#studio">' + icon("brush") + "Personnaliser</a></div></div>" +
      "<div>" + calcPreview() + "</div></section>" +
      '<div class="stats">' +
      stat("gamepad", "Jeux inclus", GAMES.length, "80 applications NumWorks .nwa") +
      stat("ai", "Intelligences", AIS.length, "Aelyx IA · Matrices V4 · STMG IA") +
      stat("library", "Ma bibliothèque", cache.nwa.length, cache.nwa.length ? size(personalBytes) + " dans ce navigateur" : "Importe tes propres .nwa") +
      stat("image", "Fonds d'écran", cache.walls.length, cache.walls.length ? "Images et GIF importés" : "Aucun pour l'instant") +
      "</div>" +
      '<div class="grid-2 section">' +
      '<div class="card"><div class="card-head"><h3>Accès rapide</h3></div><div class="actions-list">' +
      action("#bibliotheque", "library", "Bibliothèque", "Les 80 jeux inclus et tes imports") +
      action("#studio", "brush", "Personnalisation", "Environnement, couleur, fond d'écran") +
      action("#profils", "profile", "Profils", "Exporter, importer, partager un code") +
      action("#installation", "install", "Installation", "Mettre STI2D-OS et tes apps sur la calculatrice") +
      "</div></div>" +
      '<div class="card"><div class="card-head"><h3>Récemment importés</h3><a class="btn btn-ghost btn-sm" href="#bibliotheque" data-tab="personal">Tout voir</a></div>' +
      (recent.length
        ? '<div class="actions-list">' +
          recent.map(function (it) {
            var c = cache.urls[it.id + ":cover"];
            return '<a class="action" href="#bibliotheque" data-tab="personal">' +
              (c ? '<img class="action-icon" src="' + esc(c) + '" alt="" style="padding:0;object-fit:cover">' : '<span class="action-icon">' + icon(it.type === "app" ? "apps" : "gamepad") + "</span>") +
              '<span class="action-text"><strong>' + esc(it.name) + "</strong><small>" + esc(size(it.size)) + " · importé le " + esc(date(it.importedAt)) + "</small></span>" + icon("chevron") + "</a>";
          }).join("") + "</div>"
        : '<p class="muted small">Rien pour l\'instant. Ajoute un fichier <code>.nwa</code> depuis ton téléphone ou ton ordinateur : il reste disponible après fermeture du navigateur.</p>' +
          '<div style="margin-top:16px"><button class="btn btn-sm" type="button" data-act="import">' + icon("plus") + "Ajouter un fichier .nwa</button></div>") +
      "</div></div>" +
      '<div class="section card"><div class="card-head"><h3>État du projet</h3><span class="badge">Transparence</span></div><div class="rows">' +
      statusRow("Firmware STI2D-OS 2.1.0 (N0110 · N0115 · N0120)", "Compilé et lié, testé dans l'émulateur ARM du projet", "ok", "Compilé") +
      statusRow("Tests sur calculatrice physique", "Indispensables avant une diffusion large", "warn", "À faire") +
      statusRow("Bibliothèque d'origine", "80 jeux .nwa et 3 applications IA", "ok", "OK") +
      statusRow("Installation directe d'un .nwa depuis ce site", "Non disponible : téléchargement + procédure guidée", "warn", "Guidée") +
      "</div></div>"
    );
  }
  function stat(ic, label, value, note) {
    return '<div class="card stat"><div class="stat-label">' + icon(ic) + esc(label) + '</div><div class="stat-value">' + esc(value) + '</div><div class="stat-note">' + esc(note) + "</div></div>";
  }
  function action(href, ic, title, sub) {
    return '<a class="action" href="' + href + '"><span class="action-icon">' + icon(ic) + '</span><span class="action-text"><strong>' + esc(title) + "</strong><small>" + esc(sub) + "</small></span>" + icon("chevron") + "</a>";
  }
  function statusRow(title, desc, level, label) {
    return '<div class="row-item"><span><span style="color:var(--text)">' + esc(title) + '</span><span class="desc">' + esc(desc) + '</span></span><span class="badge ' + level + '">' + esc(label) + "</span></div>";
  }

  /* ---------------------------------------------------------------- library */
  var CATEGORIES = ["Tout", "Arcade", "Aventure", "Puzzle", "Stratégie", "Classique"];
  var PERSONAL_CATEGORIES = ["Arcade", "Aventure", "Puzzle", "Stratégie", "Classique", "Outils", "Révisions", "IA", "Autre"];
  var query = "";

  function viewLibrary() {
    var tab = prefs.libTab;
    var seg =
      '<div class="segmented" role="tablist" aria-label="Bibliothèque">' +
      segBtn("included", "Incluse", GAMES.length, tab) + segBtn("personal", "Personnelle", cache.nwa.length, tab) + segBtn("ai", "IA", AIS.length, tab) +
      "</div>";
    return (
      pageHead("Bibliothèque", "Jeux et applications",
        "Les 80 jeux NumWorks d'origine, les trois IA séparées, et ta bibliothèque personnelle de fichiers <code>.nwa</code> importés.",
        prefs.libTab === "personal" ? "" : '<button class="btn btn-primary" type="button" data-act="import">' + icon("plus") + "Ajouter un jeu ou une application</button>") +
      '<div class="toolbar">' + seg + '<div class="spacer"></div>' +
      '<label class="search"><span class="visually-hidden">Rechercher</span>' + icon("search") +
      '<input class="input" id="lib-search" type="search" placeholder="Rechercher" value="' + esc(query) + '" autocomplete="off"></label></div>' +
      (tab === "included" ? '<div class="chips" style="margin-bottom:18px">' + CATEGORIES.map(function (c) {
        return '<button class="chip" type="button" data-filter="' + esc(c) + '" aria-pressed="' + (prefs.filter === c) + '">' + esc(c) + "</button>";
      }).join("") + "</div>" : "") +
      (tab === "personal" ? personalHeader() : "") +
      '<div id="lib-results">' + libResults() + "</div>"
    );
  }
  function segBtn(id, label, count, tab) {
    return '<button type="button" role="tab" data-tab="' + id + '" aria-selected="' + (tab === id) + '">' + esc(label) + ' <span class="count">' + count + "</span></button>";
  }
  function match(text) {
    return !query || text.toLowerCase().indexOf(query.toLowerCase()) >= 0;
  }
  function libResults() {
    var tab = prefs.libTab, list, html;
    if (tab === "included") {
      list = GAMES.filter(function (g) {
        return (prefs.filter === "Tout" || g.category === prefs.filter) && match(g.title + " " + g.filename + " " + (g.appName || "") + " " + g.category);
      });
      html = list.map(function (g) {
        return tile(g, "included", g.category + " · " + g.sizeLabel);
      }).join("");
    } else if (tab === "ai") {
      list = AIS.filter(function (a) { return match(a.title + " " + a.filename); });
      html = list.map(function (a) { return tile(a, "ai", "IA · " + a.sizeLabel); }).join("");
    } else {
      if (cache.error) return '<div class="note danger">' + icon("alert") + "<span>" + esc(cache.error) + "</span></div>";
      list = cache.nwa.filter(function (it) {
        return match(it.name + " " + it.fileName + " " + (it.category || "") + " " + (it.description || ""));
      });
      if (!cache.nwa.length) return "";
      html = list.map(function (it) {
        return tile(it, "personal", (it.type === "app" ? "Application" : "Jeu") + " · " + size(it.size) + " · " + date(it.importedAt));
      }).join("");
    }
    return '<p class="result-count" style="margin-bottom:12px">' + plural(list.length, "résultat", "résultats") + "</p>" +
      (list.length ? '<div class="lib-grid">' + html + "</div>" : '<div class="empty"><span class="cover-glyph">' + icon("search") + "</span><strong>Aucun résultat</strong>Essaie un autre mot ou une autre catégorie.</div>");
  }
  function tile(item, kind, meta) {
    var id = esc(item.id), name = kind === "personal" ? item.name : item.title;
    var actions =
      '<button class="btn" type="button" data-act="download" data-kind="' + kind + '" data-id="' + id + '">' + icon("download") + "<span>Télécharger</span></button>" +
      '<button class="icon-btn" type="button" data-act="info" data-kind="' + kind + '" data-id="' + id + '" aria-label="Informations sur ' + esc(name) + '">' + icon("info") + "</button>" +
      (kind === "personal" ? '<button class="icon-btn" type="button" data-act="delete" data-id="' + id + '" aria-label="Supprimer ' + esc(name) + '">' + icon("trash") + "</button>" : "");
    return '<article class="tile">' + coverHTML(item, kind) + '<div class="tile-body"><div class="tile-title" title="' + esc(name) + '">' + esc(name) + '</div><div class="tile-meta">' + esc(meta) + '</div></div><div class="tile-actions">' + actions + "</div></article>";
  }
  function personalHeader() {
    var bytes = cache.nwa.reduce(function (s, i) { return s + (i.size || 0); }, 0);
    return (
      '<div class="dropzone" id="dropzone"><span class="action-icon">' + icon("upload") + "</span>" +
      '<div class="dropzone-text"><strong>' + (cache.nwa.length ? "Ajoute d'autres fichiers .nwa" : "Ta bibliothèque personnelle est vide") + "</strong>" +
      '<span class="small">Choisis un ou plusieurs fichiers' + (isIOS || isAndroid ? " depuis ton téléphone" : ", ou glisse-les ici") + ". Ils restent dans ce navigateur (" + size(bytes) + ' utilisés) <span id="quota-note"></span>.</span></div>' +
      '<button class="btn btn-primary" type="button" data-act="import">' + icon("plus") + "Ajouter un jeu ou une application</button></div>"
    );
  }
  afterRender.bibliotheque = function () {
    var dz = $("#dropzone");
    if (dz) {
      ["dragenter", "dragover"].forEach(function (ev) {
        dz.addEventListener(ev, function (e) {
          e.preventDefault();
          dz.classList.add("drag");
        });
      });
      ["dragleave", "drop"].forEach(function (ev) {
        dz.addEventListener(ev, function (e) {
          e.preventDefault();
          dz.classList.remove("drag");
        });
      });
      dz.addEventListener("drop", function (e) {
        var files = Array.prototype.slice.call((e.dataTransfer && e.dataTransfer.files) || []);
        if (files.length) importFiles(files);
      });
      Store.estimate().then(function (est) {
        var n = $("#quota-note");
        if (n && est && est.quota) n.textContent = "· environ " + size(est.quota - est.usage) + " encore disponibles pour ce site";
      });
    }
    var s = $("#lib-search");
    if (s)
      s.addEventListener("input", function () {
        query = s.value;
        $("#lib-results").innerHTML = libResults();
      });
  };

  function findItem(kind, id) {
    var list = kind === "ai" ? AIS : kind === "personal" ? cache.nwa : GAMES;
    return list.filter(function (x) { return x.id === id; })[0];
  }

  function downloadItem(kind, id) {
    var it = findItem(kind, id);
    if (!it) return;
    if (kind === "personal") {
      Store.getBlob(id).then(function (b) {
        if (!b) return toast("Fichier introuvable dans le stockage local.", "error");
        download(b, it.fileName || it.name + ".nwa");
        toast("Téléchargement de « " + it.name + " ».");
      }).catch(function (e) {
        toast(e.message, "error");
      });
      return;
    }
    var a = document.createElement("a");
    a.href = fileUrl(it.path);
    a.download = it.downloadName || it.filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
  }

  /* ---------------------------------------------------------------- info sheet */
  function infoSheet(kind, id) {
    var it = findItem(kind, id);
    if (!it) return;
    var personal = kind === "personal";
    var name = personal ? it.name : it.title;
    var rows = [];
    function row(k, v) {
      if (v !== undefined && v !== null && v !== "") rows.push('<div class="row-item"><span>' + esc(k) + "</span><strong>" + esc(v) + "</strong></div>");
    }
    var e = personal ? it.elf || {} : it;
    row("Type", personal ? (it.type === "app" ? "Application" : "Jeu") : kind === "ai" ? "Intelligence (application)" : it.kind);
    row("Catégorie", it.category);
    if (personal) row("Version", it.version);
    row("Nom sur la calculatrice", e.appName);
    row("Fichier", personal ? it.fileName : it.filename);
    row("Taille du fichier", size(it.size));
    if (e.flash) row("Taille installée (estimée)", size(e.flash));
    if (e.ram) row("RAM demandée (estimée)", size(e.ram));
    if (e.apiLevel !== undefined) row("API EADK", "niveau " + e.apiLevel);
    if (personal) row("Importé le", date(it.importedAt));
    row("Origine", personal ? "Bibliothèque personnelle (ce navigateur)" : "Bibliothèque d'origine STI2D-OS");
    var cover = personal ? cache.urls[it.id + ":cover"] : it.cover;
    var body =
      '<div class="file-chip">' + (cover ? '<img src="' + esc(cover) + '" alt="">' : '<span class="cover-glyph">' + icon(it.format === "GB" ? "cartridge" : "gamepad") + "</span>") +
      '<div style="min-width:0"><strong>' + esc(name) + "</strong><small>" + esc(personal ? it.description || "Sans description" : it.description || it.category || "") + "</small></div></div>" +
      '<div class="rows">' + rows.join("") + "</div>" +
      installHelp(it, personal);
    var foot =
      (personal ? '<button class="btn btn-ghost" type="button" data-act="edit" data-id="' + esc(it.id) + '">Modifier</button>' : "") +
      '<button class="btn btn-primary" type="button" data-act="download" data-kind="' + kind + '" data-id="' + esc(it.id) + '">' + icon("download") + "Télécharger</button>";
    openSheet(name, body, foot);
  }

  function installHelp(it, personal) {
    return (
      '<div class="card" style="padding:16px"><div class="card-head" style="margin-bottom:10px"><h3>Installer sur la calculatrice</h3></div>' +
      '<p class="muted small" style="margin-bottom:12px">Ce site ne peut pas écrire directement un .nwa dans la calculatrice. ' +
      "Télécharge le fichier puis installe-le avec l'une de ces méthodes :</p>" +
      '<ol class="steps"><li><span><strong>my.numworks.com/apps</strong> (Chrome ou Edge sur ordinateur, Epsilon 23.2+) : connecte la calculatrice, ajoute le fichier, Installer.</span></li>' +
      '<li><span><strong>nwlink</strong> en ligne de commande (Epsilon ou Upsilon) :</span></li></ol>' +
      '<div class="cmd" style="margin-top:8px"><code>npx nwlink install-nwa "' + esc(personal ? it.fileName : it.downloadName || it.filename) + '"</code>' +
      '<button class="icon-btn" type="button" data-act="copy" data-text="' + esc('npx nwlink install-nwa "' + (personal ? it.fileName : it.downloadName || it.filename) + '"') + '" aria-label="Copier la commande">' + icon("copy") + "</button></div>" +
      '<p class="faint small" style="margin-top:12px">Un format valide ne garantit pas la compatibilité : RAM, espace libre et logiciel de la calculatrice comptent. Installer des apps remplace celles installées avant : ajoute-les ensemble.</p></div>'
    );
  }

  /* ---------------------------------------------------------------- import */
  function importFiles(files) {
    files = files.filter(Boolean);
    if (!files.length) return;
    var queue = files.slice(), added = 0, total = files.length;
    function next() {
      var f = queue.shift();
      if (!f) {
        closeSheet();
        if (added) {
          prefs.libTab = "personal";
          savePrefs();
          refreshCache().then(function () {
            if (route === "bibliotheque" || route === "accueil") render();
            else renderNav();
          });
          toast(added === 1 ? "Ajouté à ta bibliothèque personnelle." : added + " fichiers ajoutés à ta bibliothèque.");
        }
        return;
      }
      var pos = total > 1 ? " (" + (total - queue.length) + "/" + total + ")" : "";
      openSheet("Vérification" + pos, '<div class="file-chip"><span class="cover-glyph">' + icon("file") + '</span><div style="min-width:0"><strong>' + esc(f.name) + "</strong><small>" + size(f.size) + " · lecture…</small></div></div>");
      Nwa.inspect(f).then(function (res) {
        showImportForm(f, res, pos, function (saved) {
          if (saved) added++;
          next();
        });
      }).catch(function () {
        showImportForm(f, { ok: false, checks: [{ level: "fail", title: "Lecture impossible", detail: "Le fichier n'a pas pu être lu." }], info: {} }, pos, function () {
          next();
        });
      });
    }
    next();
  }

  function checksHTML(checks) {
    return '<div class="checks">' + checks.map(function (c) {
      var ic = c.level === "ok" ? "check" : c.level === "warn" ? "alert" : "close";
      return '<div class="check ' + c.level + '">' + icon(ic) + "<div><span>" + esc(c.title) + "</span>" + (c.detail ? "<small>" + esc(c.detail) + "</small>" : "") + "</div></div>";
    }).join("") + "</div>";
  }

  function guessType(name) {
    return /ia\b|matrices|cheat|calc|note|formul|tool|outil|stmg|aelyx/i.test(name) ? "app" : "game";
  }

  function showImportForm(file, res, pos, done) {
    var finished = false;
    function finish(saved) {
      if (finished) return;
      finished = true;
      done(saved);
    }
    var iconUrl = res.icon ? URL.createObjectURL(res.icon) : null;
    var head =
      '<div class="file-chip">' + (iconUrl ? '<img src="' + iconUrl + '" alt="">' : '<span class="cover-glyph">' + icon("file") + "</span>") +
      '<div style="min-width:0"><strong>' + esc(file.name) + "</strong><small>" + size(file.size) + (res.info.appName ? " · « " + esc(res.info.appName) + " »" : "") + "</small></div></div>";
    if (!res.ok) {
      openSheet("Fichier refusé" + pos, head + checksHTML(res.checks) +
        '<div class="note danger">' + icon("alert") + "<span>Ce fichier n'a pas été ajouté. Vérifie qu'il s'agit bien d'une app NumWorks (.nwa) complète.</span></div>",
        '<button class="btn btn-primary" type="button" data-act="sheet-close">' + (pos ? "Continuer" : "Fermer") + "</button>",
        function () {
          if (iconUrl) URL.revokeObjectURL(iconUrl);
          finish(false);
        });
      return;
    }
    var dup = cache.nwa.filter(function (x) { return x.fileName === file.name && x.size === file.size; })[0];
    var defName = res.info.appName || file.name.replace(/\.nwa$/i, "").replace(/[_-]+/g, " ").trim();
    var body =
      head + checksHTML(res.checks) +
      (dup ? '<div class="note warn">' + icon("alert") + "<span>Un fichier identique (« " + esc(dup.name) + " ») est déjà dans ta bibliothèque.</span></div>" : "") +
      '<div class="form-grid">' +
      '<label class="field full"><span>Nom</span><input class="input" id="f-name" maxlength="60" required value="' + esc(defName) + '"></label>' +
      '<label class="field full"><span>Description</span><textarea class="textarea" id="f-desc" maxlength="400" placeholder="Facultatif"></textarea></label>' +
      '<label class="field"><span>Catégorie</span><select class="select" id="f-cat">' + PERSONAL_CATEGORIES.map(function (c) { return "<option>" + esc(c) + "</option>"; }).join("") + "</select></label>" +
      '<div class="field"><span>Type</span><div class="segmented" role="radiogroup" id="f-type">' +
      '<button type="button" role="radio" data-type="game" aria-selected="' + (guessType(defName) === "game") + '">' + icon("gamepad") + "Jeu</button>" +
      '<button type="button" role="radio" data-type="app" aria-selected="' + (guessType(defName) === "app") + '">' + icon("apps") + "Application</button></div></div>" +
      '<label class="field"><span>Version</span><input class="input" id="f-version" maxlength="20" placeholder="Facultatif"></label>' +
      '<div class="field"><span>Image</span><div style="display:flex;gap:8px;align-items:center;min-height:44px"><span class="small faint" id="f-cover-label">' + (res.icon ? "Icône du fichier" : "Aucune") + '</span><button class="btn btn-sm" type="button" id="f-cover">' + icon("image") + "Choisir</button></div></div>" +
      "</div>";
    var foot =
      '<button class="btn btn-ghost" type="button" data-act="sheet-close">' + (pos ? "Ignorer ce fichier" : "Annuler") + "</button>" +
      '<button class="btn btn-primary" type="button" id="f-save">' + icon("plus") + "Ajouter à ma bibliothèque</button>";
    openSheet("Ajouter à ma bibliothèque" + pos, body, foot, function () {
      if (iconUrl) URL.revokeObjectURL(iconUrl);
      finish(false);
    });
    var cover = res.icon || null;
    $$("#f-type button").forEach(function (b) {
      b.addEventListener("click", function () {
        $$("#f-type button").forEach(function (x) { x.setAttribute("aria-selected", String(x === b)); });
      });
    });
    $("#f-cover").addEventListener("click", function () {
      pickFiles("image/*", false).then(function (fs) {
        var f = fs[0];
        if (!f) return;
        Img.check(f).then(function () {
          if (f.size > 3 * 1024 * 1024) throw new Error("Image trop lourde pour une couverture (3 Mo au plus).");
          cover = f;
          $("#f-cover-label").textContent = f.name;
        }).catch(function (e) {
          toast(e.message, "error");
        });
      });
    });
    $("#f-save").addEventListener("click", function () {
      var name = $("#f-name").value.trim();
      if (!name) {
        $("#f-name").focus();
        return toast("Donne un nom à ce fichier.", "error");
      }
      var sel = $('#f-type [aria-selected="true"]');
      var item = {
        id: Store.newId("nwa"),
        kind: "nwa",
        name: name,
        description: $("#f-desc").value.trim(),
        category: $("#f-cat").value,
        type: sel ? sel.getAttribute("data-type") : "game",
        version: $("#f-version").value.trim(),
        fileName: file.name,
        size: file.size,
        importedAt: Date.now(),
        elf: { appName: res.info.appName || "", apiLevel: res.info.apiLevel, flash: res.info.flash, ram: res.info.ram },
      };
      var btn = $("#f-save");
      btn.disabled = true;
      btn.textContent = "Enregistrement…";
      Store.persist();
      Store.put(item, file, cover).then(function () {
        finished = false;
        if (iconUrl) URL.revokeObjectURL(iconUrl);
        finish(true);
      }).catch(function (e) {
        btn.disabled = false;
        btn.innerHTML = icon("plus") + "Ajouter à ma bibliothèque";
        toast(e.message || "Enregistrement impossible.", "error");
      });
    });
  }

  function editSheet(id) {
    var it = findItem("personal", id);
    if (!it) return;
    var body =
      '<div class="form-grid">' +
      '<label class="field full"><span>Nom</span><input class="input" id="e-name" maxlength="60" value="' + esc(it.name) + '"></label>' +
      '<label class="field full"><span>Description</span><textarea class="textarea" id="e-desc" maxlength="400">' + esc(it.description || "") + "</textarea></label>" +
      '<label class="field"><span>Catégorie</span><select class="select" id="e-cat">' + PERSONAL_CATEGORIES.map(function (c) { return "<option" + (c === it.category ? " selected" : "") + ">" + esc(c) + "</option>"; }).join("") + "</select></label>" +
      '<label class="field"><span>Type</span><select class="select" id="e-type"><option value="game"' + (it.type !== "app" ? " selected" : "") + '>Jeu</option><option value="app"' + (it.type === "app" ? " selected" : "") + ">Application</option></select></label>" +
      '<label class="field"><span>Version</span><input class="input" id="e-version" maxlength="20" value="' + esc(it.version || "") + '"></label>' +
      '<div class="field"><span>Image</span><div style="display:flex;gap:8px;min-height:44px;align-items:center"><button class="btn btn-sm" type="button" id="e-cover">' + icon("image") + 'Remplacer</button></div></div></div>';
    openSheet("Modifier « " + it.name + " »", body,
      '<button class="btn btn-ghost" type="button" data-act="sheet-close">Annuler</button><button class="btn btn-primary" type="button" id="e-save">Enregistrer</button>');
    var newCover = null;
    $("#e-cover").addEventListener("click", function () {
      pickFiles("image/*", false).then(function (fs) {
        if (!fs[0]) return;
        Img.check(fs[0]).then(function () {
          newCover = fs[0];
          $("#e-cover").textContent = fs[0].name;
        }).catch(function (e) { toast(e.message, "error"); });
      });
    });
    $("#e-save").addEventListener("click", function () {
      var name = $("#e-name").value.trim();
      if (!name) return toast("Le nom ne peut pas être vide.", "error");
      var upd = Object.assign({}, it, { name: name, description: $("#e-desc").value.trim(), category: $("#e-cat").value, type: $("#e-type").value, version: $("#e-version").value.trim() });
      Store.update(upd).then(function () {
        return newCover ? Store.setCover(it.id, newCover).then(function () { dropUrl(it.id + ":cover"); }) : null;
      }).then(function () {
        closeSheet();
        return refreshCache();
      }).then(function () {
        render();
        toast("Modifications enregistrées.");
      }).catch(function (e) {
        toast(e.message, "error");
      });
    });
  }

  function deleteItem(id) {
    var it = findItem("personal", id);
    if (!it) return;
    confirmSheet("Supprimer « " + esc(it.name) + " » ?", "Le fichier (" + size(it.size) + ") est retiré de ta bibliothèque personnelle et de ce navigateur. La bibliothèque d'origine n'est pas touchée.", "Supprimer", true)
      .then(function (ok) {
        if (!ok) return;
        return Store.remove(id).then(function () {
          dropUrl(id + ":cover");
          return refreshCache();
        }).then(function () {
          render();
          toast("« " + it.name + " » supprimé, espace libéré.");
        });
      }).catch(function (e) {
        toast(e.message, "error");
      });
  }

  /* ---------------------------------------------------------------- AI */
  function viewAI() {
    return (
      pageHead("Intelligences", "Les trois IA", "Trois applications NumWorks fournies séparément. Choisis Aelyx IA, Matrices V4 ou STMG IA ; elles disposent de leur propre entrée dans STI2D-OS et ne sont ni des jeux ni des applications personnelles.") +
      '<div class="lib-grid ai-grid">' +
      AIS.map(function (a) {
        return '<article class="tile">' + coverHTML(a, "ai") + '<div class="tile-body"><div class="tile-title">' + esc(a.title) + '</div><div class="tile-meta" style="white-space:normal">' + esc(a.description || "") + "</div>" +
          '<div class="tile-meta">' + esc(a.sizeLabel) + (a.ram ? " · RAM ≈ " + size(a.ram) : "") + "</div></div>" +
          '<div class="tile-actions"><button class="btn btn-primary" type="button" data-act="download" data-kind="ai" data-id="' + esc(a.id) + '">' + icon("download") + "<span>Télécharger</span></button>" +
          '<button class="icon-btn" type="button" data-act="info" data-kind="ai" data-id="' + esc(a.id) + '" aria-label="Informations">' + icon("info") + "</button></div></article>";
      }).join("") +
      '</div><div class="note section">' + icon("info") + "<span>Le portail ne fait pas tourner ces IA dans le navigateur : ce sont des programmes pour le processeur ARM de la calculatrice.</span></div>"
    );
  }


  /* ---------------------------------------------------------------- systems */
  function systemPreview(id) {
    var apps = '<i>▦</i><i>◉</i><i>✉</i><i>⚙</i><i>▣</i><i>♫</i><i>◷</i><i>⌕</i>';
    var layouts = {
      sti2d: '<div class="mock-watch"><div class="mock-watch-head"><span>STI2D-OS</span><span>● 100%</span></div><div class="mock-honey">' + apps + '</div><small>CONTROL CENTER</small></div>',
      classic: '<div class="mock-classic"><header>NumWorks <span>●</span></header><div class="mock-classic-grid">' + apps + '</div><footer>Calculs · Fonctions · Suites</footer></div>',
      "3ds": '<div class="mock-3ds"><div class="mock-3ds-top"><div class="mock-3ds-grid">' + apps + '</div></div><div class="mock-3ds-hinge"></div><div class="mock-3ds-bottom"><div>●　●　●</div><span>Nintendo 3DS</span></div></div>',
      ps5: '<div class="mock-console mock-ps5"><header>Bonjour, joueur <span>◉</span></header><div class="mock-console-tiles"><i>🎮</i><i>🎧</i><i>🌐</i><i>⚙</i><i>▣</i></div><h3>Explorer</h3><div class="mock-wide-tiles"><b>Bibliothèque</b><b>PlayStation Store</b></div></div>',
      ios: '<div class="mock-phone"><header>9:41 <span>●●●</span></header><div class="mock-ios-date">Vendredi 20<br><small>Novembre</small></div><div class="mock-ios-apps">' + apps + '</div><div class="mock-ios-dock"><i>☎</i><i>✉</i><i>◎</i><i>♫</i></div></div>',
      windows: '<div class="mock-desktop mock-windows"><header>☁ Espace de travail</header><div class="mock-desktop-icons">' + apps + '</div><div class="mock-taskbar">⊞　⌕　▣　▤　◉</div></div>',
      xbox: '<div class="mock-console mock-xbox"><header>Accueil <span>Profil ●</span></header><div class="mock-xbox-feature">Reprendre votre jeu</div><div class="mock-console-tiles"><i>🎮</i><i>▣</i><i>☁</i><i>⚙</i></div><h3>Mes jeux et applications</h3></div>',
      linux: '<div class="mock-desktop mock-linux"><header>Activités　20 nov. <span>●</span></header><div class="mock-linux-window"><b>Fichiers</b><div>⌂　Personnel<br>▣　Documents<br>▧　Images<br>⬇　Téléchargements</div></div><div class="mock-linux-dock">' + apps + '</div></div>',
      android: '<div class="mock-phone mock-android"><header>9:41 <span>◉</span></header><div class="mock-android-widget">20<br><small>Ven. novembre</small></div><div class="mock-android-apps">' + apps + '</div><div class="mock-android-search">⌕　Rechercher</div></div>',
      macos: '<div class="mock-desktop mock-macos"><header>●　●　● <span>Fichier　Édition　Présentation</span></header><div class="mock-macos-window"><b>Macintosh HD</b><div>Applications　　Documents<br>Bureau　　　　Téléchargements</div></div><div class="mock-macos-dock">' + apps + '</div></div>'
    };
    return layouts[id] || layouts.sti2d;
  }
  function viewSystems() {
    return pageHead("Systèmes", "Choisis ton univers.", "Dix environnements visuels sont disponibles dans le portail. La sélection modifie l'apparence web et reste enregistrée dans ce navigateur.") +
      '<div class="systems-grid">' + SKINS.map(function (env) {
        var selected = prefs.skin === env.id;
        return '<article class="system-choice' + (selected ? ' selected' : '') + '"><div class="system-choice-preview preview-env-' + env.id + '">' + systemPreview(env.id) + '</div><div class="system-choice-info"><div><h3>' + esc(env.name) + '</h3><p>' + esc(env.note) + '</p></div><button class="btn ' + (selected ? 'btn-primary' : '') + ' btn-sm env" type="button" data-skin="' + env.id + '" aria-pressed="' + selected + '">' + (selected ? 'Sélectionné' : 'Choisir') + '</button></div></article>';
      }).join('') + '</div><div class="note section">' + icon("info") + '<span>Ce sont des aperçus web inspirés de ces systèmes. Seul STI2D-OS est le Userland ciblé pour NumWorks ; les autres environnements ne sont pas des firmwares installables sur la calculatrice.</span></div>';
  }

  function viewDocuments() {
    return pageHead("Lecteur de fichiers", "Emporte tes documents sur la calculatrice.",
      "Prépare tes documents sur l'ordinateur, puis transfère un script de lecture sur la NumWorks. Le contenu importé reste du texte : il n'est jamais exécuté.",
      '<button class="btn btn-primary" type="button" data-act="doc-import">' + icon("upload") + "Importer TXT / MD / CSV" + "</button>") +
      '<div class="grid-2"><section class="card"><div class="card-head"><h3>Documents texte</h3><span class="badge">TXT · MD · CSV</span></div>' +
      '<p class="muted">Les fichiers texte sont convertis dans le navigateur en scripts <code>stidoc_*.py</code> composés uniquement de commentaires. Transfère-les avec le gestionnaire Python NumWorks ou ton outil habituel.</p>' +
      '<div class="note section">' + icon("info") + '<span>Limite : 16 Ko par document exporté. Les lignes longues peuvent être reformatées à l’écran.</span></div></section>' +
      '<section class="card"><div class="card-head"><h3>Documents PDF</h3><span class="badge">PDF → texte</span></div>' +
      '<p class="muted">Le PDF est converti sur l’ordinateur en texte paginé, puis intégré au même format sûr que les documents TXT. Le lecteur de la calculatrice n’embarque pas de moteur PDF graphique.</p>' +
      '<ol class="steps"><li>Installe la dépendance : <code>python -m pip install pypdf</code>.</li><li>Télécharge le convertisseur <a href="../tools/pdf_to_stidoc.py" download style="color:var(--accent)">pdf_to_stidoc.py</a>.</li><li>Lance <code>python pdf_to_stidoc.py mon-cours.pdf</code>.</li><li>Transfère le <code>stidoc_*.py</code> généré sur la NumWorks.</li></ol>' +
      '<p class="faint small">Tu peux sélectionner certaines pages : <code>--pages 1-3,5</code>. Le résultat doit rester sous 16 Ko ; réduis la sélection si nécessaire.</p></section></div>' +
      '<section class="card section"><div class="card-head"><h3>Sur la calculatrice</h3></div><ol class="steps"><li>Transfère le script <code>stidoc_*.py</code> comme un script Python.</li><li>Ouvre l’application <strong>Fichiers</strong> dans STI2D-OS.</li><li>Choisis le document, puis fais défiler les pages avec les flèches.</li></ol></section>' +
      '<div class="note section">' + icon("shield") + '<span>Le lecteur analyse uniquement les commentaires <code>#|</code> du format STI2D-DOC-V1. Le code Python importé n’est jamais exécuté. Les PDF scannés sans couche texte doivent être traités par OCR sur ordinateur avant conversion.</span></div>';
  }

  function exportTextDocument(file) {
    if (!file) return;
    if (!/\.(txt|md|csv)$/i.test(file.name)) return toast("Choisis un fichier .txt, .md ou .csv.", "error");
    if (file.size > 16 * 1024) return toast("Le document dépasse la limite de 16 Ko.", "error");
    file.text().then(function (text) {
      text = text.replace(/\u0000/g, "").replace(/\r\n?/g, "\n");
      if (text.length === 0) return toast("Le document est vide.", "error");
      var bytes = window.TextEncoder ? new TextEncoder().encode(text).length : text.length;
      if (bytes > 16 * 1024) return toast("Le document dépasse 16 Ko une fois encodé.", "error");
      var title = file.name.replace(/\.(txt|md|csv)$/i, "").replace(/[\r\n]/g, " ").slice(0, 30) || "Document";
      var slug = title.normalize ? title.normalize("NFD").replace(/[\u0300-\u036f]/g, "") : title;
      slug = slug.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "").slice(0, 24) || "document";
      var script = "# coding: utf-8\n# STI2D-DOC-V1\n# TITLE: " + title + "\n" + text.split("\n").map(function (line) {
        return "#|" + line;
      }).join("\n") + "\n";
      download(new Blob([script], { type: "text/x-python;charset=utf-8" }), "stidoc_" + slug + ".py");
      toast("Document exporté. Transfère le script sur ta NumWorks.");
    }).catch(function () { toast("Impossible de lire ce document.", "error"); });
  }

  /* ---------------------------------------------------------------- studio */
  var conv = { colors: 16, dither: false, small: false, fit: "cover", result: null, busy: false };

  function viewStudio() {
    var wall = currentWall();
    return (
      pageHead("Personnalisation", "Ton apparence", "L'environnement change le portail ; la couleur, les animations et le fond se retrouvent aussi sur la calculatrice grâce au profil exporté.") +
      '<div class="studio-layout">' +
      "<div>" +
      '<section class="card"><div class="card-head"><h3>Environnement du portail</h3><span class="badge">' + esc(skinName(prefs.skin)) + "</span></div>" +
      '<div class="page-actions" style="margin-bottom:12px"><a class="btn btn-sm" href="#systemes">' + icon("calc") + "Voir les 10 environnements" + "</a></div><div class=\"env-grid\">" + SKINS.map(function (s) {
        return '<button class="env" type="button" data-skin="' + s.id + '" aria-pressed="' + (prefs.skin === s.id) + '"><div class="env-swatch" style="background:' + s.sw[0] + '"><i style="width:40%;background:' + s.sw[1] + '"></i><i style="width:22%;background:' + s.sw[2] + '"></i></div><div class="env-name">' + esc(s.name) + "<small>" + esc(s.note) + "</small></div></button>";
      }).join("") + '</div><p class="faint small" style="margin-top:12px">Ces environnements habillent le portail web. Sur la calculatrice, il existe deux interfaces : STI2D-OS et STI2D Classic.</p></section>' +
      '<section class="card section"><div class="card-head"><h3>Couleur d\'accentuation</h3><span class="badge">' + esc(ACCENTS[prefs.accent].name) + "</span></div>" +
      '<div class="accents">' + ACCENTS.map(function (a) {
        return '<button class="accent-dot' + (a.hex ? "" : " auto") + '" type="button" data-accent="' + a.id + '" aria-pressed="' + (prefs.accent === a.id) + '" aria-label="' + esc(a.name) + '" title="' + esc(a.name) + '" style="--dot:' + (a.hex || "#888") + '"></button>';
      }).join("") + "</div></section>" +
      '<section class="card section"><div class="card-head"><h3>Calculatrice</h3></div><div class="rows">' +
      '<div class="row-item"><span>Interface au démarrage<span class="desc">Double π sur l\'accueil pour passer de l\'une à l\'autre</span></span>' +
      '<div class="segmented"><button type="button" data-calcenv="0" aria-selected="' + (prefs.calcEnv === 0) + '">STI2D-OS</button><button type="button" data-calcenv="1" aria-selected="' + (prefs.calcEnv === 1) + '">Classic</button></div></div>' +
      '<div class="row-item"><span>Animations<span class="desc">Portail et calculatrice</span></span><button class="switch" type="button" role="switch" data-act="toggle-motion" aria-checked="' + prefs.motion + '" aria-label="Animations"></button></div>' +
      "</div></section>" +
      '<section class="card section"><div class="card-head"><h3>Fonds d\'écran</h3><button class="btn btn-sm" type="button" data-act="wall-import">' + icon("plus") + "Importer</button></div>" +
      (cache.walls.length
        ? '<div class="wall-grid">' + cache.walls.map(function (w) {
            return '<button class="wall" type="button" data-wall="' + esc(w.id) + '" aria-pressed="' + (prefs.wallpaper === w.id) + '" aria-label="' + esc(w.name) + '"><img src="' + esc(wallUrl(w.id) || "") + '" alt="" loading="lazy"><span class="badge">' + esc(w.format) + "</span></button>";
          }).join("") + "</div>"
        : '<p class="muted small">PNG, JPEG, GIF ou WebP, 10 Mo au plus. Le portail les garde dans ce navigateur ; un GIF reste animé ici.</p>') +
      (wall ? '<div class="row-item" style="margin-top:12px"><span>' + esc(wall.name) + '<span class="desc">' + esc(wall.format + " · " + (wall.width || "?") + " × " + (wall.height || "?") + " · " + size(wall.size)) + '</span></span><span style="display:flex;gap:6px"><button class="btn btn-sm btn-ghost" type="button" data-act="wall-none">Retirer</button><button class="btn btn-sm btn-danger" type="button" data-act="wall-delete" data-id="' + esc(wall.id) + '">' + icon("trash") + "</button></span></div>" : "") +
      "</section>" +
      (wall ? convertCard(wall) : "") +
      "</div>" +
      '<div class="studio-preview">' + calcPreview() + "</div></div>"
    );
  }
  function skinName(id) {
    return (SKINS.filter(function (s) { return s.id === id; })[0] || SKINS[0]).name;
  }
  function convertCard(wall) {
    var gif = wall.format === "GIF";
    return (
      '<section class="card section" id="convert-card"><div class="card-head"><h3>Exporter pour la calculatrice</h3><span class="badge accent">Galerie STI2D-OS</span></div>' +
      '<p class="muted small" style="margin-bottom:14px">La calculatrice ne lit ni PNG ni JPEG ni GIF, et garde environ 42 Ko de fichiers en tout. L\'image est convertie en ' + (conv.small ? "80 × 60" : "160 × 120") + " pixels et " + conv.colors + " couleurs au plus, dans un script Python que la Galerie et le fond d'écran de STI2D-OS savent lire." + (gif ? " <strong style=\"color:var(--warn)\">GIF : seule la première image est exportée</strong> (pas d'animation sur la calculatrice)." : "") + "</p>" +
      '<div class="convert"><figure><img src="' + esc(wallUrl(wall.id) || "") + '" alt="Original"><figcaption>Original</figcaption></figure><figure><canvas id="conv-canvas" width="160" height="120" aria-label="Aperçu calculatrice"></canvas><figcaption id="conv-info">Calcul…</figcaption></figure></div>' +
      '<div class="form-grid" style="margin-top:14px"><label class="field"><span>Couleurs : <b id="conv-colors-v">' + conv.colors + '</b></span><input class="range" type="range" min="2" max="16" value="' + conv.colors + '" id="conv-colors"></label>' +
      '<label class="field"><span>Taille</span><select class="select" id="conv-size"><option value="0"' + (!conv.small ? " selected" : "") + '>160 × 120 (plein écran)</option><option value="1"' + (conv.small ? " selected" : "") + ">80 × 60 (plus léger)</option></select></label>" +
      '<label class="field"><span>Cadrage</span><select class="select" id="conv-fit"><option value="cover"' + (conv.fit === "cover" ? " selected" : "") + '>Remplir l\'écran</option><option value="contain"' + (conv.fit === "contain" ? " selected" : "") + ">Image entière</option></select></label>" +
      '<div class="field"><span>Tramage</span><div style="display:flex;align-items:center;gap:10px;min-height:44px"><button class="switch" type="button" role="switch" id="conv-dither" aria-checked="' + conv.dither + '" aria-label="Tramage"></button><span class="small faint">Plus fidèle, plus lourd</span></div></div></div>' +
      '<div class="page-actions" style="margin-top:16px"><button class="btn btn-primary" type="button" id="conv-download" disabled>' + icon("download") + "Télécharger pour la calculatrice</button></div>" +
      '<p class="faint small" style="margin-top:12px">Envoi : sur <strong>my.numworks.com</strong>, crée un script Python et colles-y le contenu du fichier en gardant son nom (stimg_….py), ou utilise <code>npx nwlink</code>. Puis Galerie &gt; Shift pour en faire le fond d\'écran.</p></section>'
    );
  }
  afterRender.studio = function () {
    var wall = currentWall();
    if (!wall) return;
    var run = function () {
      var info = $("#conv-info"), btn = $("#conv-download");
      if (!info) return;
      info.textContent = "Calcul…";
      btn.disabled = true;
      Store.getBlob(wall.id).then(function (blob) {
        if (!blob) throw new Error("Image introuvable.");
        return Img.convert(blob, { colors: conv.colors, dither: conv.dither, small: conv.small, fit: conv.fit, name: wall.name });
      }).then(function (r) {
        conv.result = r;
        var c = $("#conv-canvas");
        if (!c) return;
        c.width = r.width;
        c.height = r.height;
        c.getContext("2d").drawImage(r.canvas, 0, 0);
        var share = Math.round((r.bytes / (42 * 1024)) * 100);
        info.textContent = "Calculatrice · " + r.colors + " couleurs · " + size(r.bytes) + " (≈ " + share + " % de la mémoire de fichiers)" +
          (share > 40 ? " — lourd : réduis les couleurs, la taille ou le tramage" : "");
        btn.disabled = false;
      }).catch(function (e) {
        info.textContent = e.message || "Conversion impossible.";
      });
    };
    var t = null;
    var later = function () {
      clearTimeout(t);
      t = setTimeout(run, 120);
    };
    $("#conv-colors").addEventListener("input", function (e) {
      conv.colors = +e.target.value;
      $("#conv-colors-v").textContent = conv.colors;
      later();
    });
    $("#conv-size").addEventListener("change", function (e) {
      conv.small = e.target.value === "1";
      later();
    });
    $("#conv-fit").addEventListener("change", function (e) {
      conv.fit = e.target.value;
      later();
    });
    $("#conv-dither").addEventListener("click", function (e) {
      conv.dither = !conv.dither;
      e.currentTarget.setAttribute("aria-checked", String(conv.dither));
      later();
    });
    $("#conv-download").addEventListener("click", function () {
      if (!conv.result) return;
      download(new Blob([conv.result.text], { type: "text/x-python" }), conv.result.filename);
      toast(conv.result.filename + " téléchargé.");
    });
    run();
  };

  function importWallpapers() {
    return pickFiles("image/png,image/jpeg,image/gif,image/webp", true).then(importWallpaperFiles);
  }
  function importWallpaperFiles(files) {
    return Promise.resolve(files).then(function (files) {
      if (!files.length) return;
      var ok = 0, lastId = "";
      return files.reduce(function (p, f) {
        return p.then(function () {
          return Img.check(f).then(function (t) {
            return Img.decode(f).then(function (bmp) {
              var item = { id: Store.newId("wall"), kind: "wallpaper", name: f.name.replace(/\.[^.]+$/, ""), fileName: f.name, size: f.size, format: t.label, width: bmp.width, height: bmp.height, importedAt: Date.now() };
              if (bmp.close) bmp.close();
              return Store.put(item, f, null).then(function () {
                ok++;
                lastId = item.id;
              });
            });
          }).catch(function (e) {
            toast(f.name + " : " + (e.message || "refusé"), "error");
          });
        });
      }, Promise.resolve()).then(function () {
        if (!ok) return;
        if (lastId) prefs.wallpaper = lastId;
        savePrefs();
        return refreshCache().then(function () {
          applyPortalWallpaper();
          render();
          toast(ok === 1 ? "Fond d'écran importé." : ok + " fonds d'écran importés.");
        });
      });
    });
  }

  /* ---------------------------------------------------------------- profiles */
  function profile() {
    var w = currentWall();
    return {
      format: "sti2d-profile",
      version: 3,
      portal: { skin: prefs.skin, accent: prefs.accent, motion: prefs.motion },
      calculator: { env: prefs.calcEnv, accent: prefs.accent, animations: prefs.calcAnim, wallpaper: w ? "image" : "gradient", wallpaperFile: w && conv.result ? conv.result.filename : "" },
    };
  }
  function b64url(str) {
    return btoa(unescape(encodeURIComponent(str))).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  }
  function unb64url(s) {
    s = s.replace(/-/g, "+").replace(/_/g, "/");
    while (s.length % 4) s += "=";
    return decodeURIComponent(escape(atob(s)));
  }
  function shareCode() {
    var p = profile();
    return "STI2D3-" + b64url(JSON.stringify({ s: p.portal.skin, a: p.portal.accent, m: p.portal.motion ? 1 : 0, e: p.calculator.env, n: p.calculator.animations ? 1 : 0 }));
  }
  /* accepts a JSON profile (v2 or v3) or a share code (STI2D3- or the old STI2D2:); never a PIN */
  function applyProfileData(d) {
    var next = {};
    if (d && d.format === "sti2d-profile" && d.version === 3) {
      Object.assign(next, sanitize({ skin: d.portal && d.portal.skin, accent: d.portal && d.portal.accent, motion: d.portal && d.portal.motion }));
      if (d.calculator) Object.assign(next, sanitize({ calcEnv: d.calculator.env, calcAnim: d.calculator.animations }));
    } else if (d && d.format === "sti2d-profile" && d.version === 2) {
      var old = { lichen: "sti2d", obsidienne: "sti2d", glacier: "ios", neon: "xbox" };
      Object.assign(next, sanitize({ skin: d.skin === "sti2d" ? "sti2d" : d.skin, motion: d.reduceMotion === undefined ? undefined : !d.reduceMotion }));
      if (!next.skin && d.theme) next.skin = old[String(d.theme).toLowerCase()] || "sti2d";
    } else if (d && d.s !== undefined) {
      Object.assign(next, sanitize({ skin: d.s, accent: d.a, motion: d.m === 1, calcEnv: d.e, calcAnim: d.n === 1 }));
    } else {
      throw new Error("format");
    }
    if (!Object.keys(next).length) throw new Error("vide");
    Object.assign(prefs, next);
    savePrefs();
    applyAppearance();
  }
  function parseCode(code) {
    code = (code || "").trim();
    if (/^STI2D3-/.test(code)) return JSON.parse(unb64url(code.slice(7)));
    if (/^STI2D2:/.test(code)) return JSON.parse(decodeURIComponent(escape(atob(code.slice(7)))));
    throw new Error("prefix");
  }
  /* the preferences file the firmware reads (firmware/v2/src/config.c): no PIN, no date */
  function calcPrefsFile() {
    var w = currentWall();
    var file = w && conv.result ? conv.result.filename : "";
    return "# STI2D-OS V2 : reglages (ne pas modifier a la main)\n" +
      "E=" + prefs.calcEnv + "\nA=" + prefs.accent + "\nN=" + (prefs.calcAnim ? 1 : 0) + "\n" +
      "W=" + (file ? 3 : 1) + "\nI=" + file + "\n";
  }

  function bytesToHex(bytes) {
    return Array.prototype.map.call(bytes, function (b) { return b.toString(16).padStart(2, "0"); }).join("");
  }
  function randomBytes(n) {
    var out = new Uint8Array(n);
    window.crypto.getRandomValues(out);
    return out;
  }
  function recoveryDigits() {
    var out = "";
    while (out.length < 12) {
      var bytes = randomBytes(16);
      for (var i = 0; i < bytes.length && out.length < 12; i++) {
        if (bytes[i] < 250) out += String(bytes[i] % 10);
      }
    }
    return out;
  }
  function pinHash(code, salt) {
    var subtle = window.crypto && window.crypto.subtle;
    if (!subtle || !window.TextEncoder) return Promise.reject(new Error("Utilise le portail en HTTPS ou sur localhost avec un navigateur compatible WebCrypto."));
    var keyBytes = new TextEncoder().encode(code);
    return subtle.importKey("raw", keyBytes, "PBKDF2", false, ["deriveBits"]).then(function (key) {
      return subtle.deriveBits({ name: "PBKDF2", salt: salt, iterations: 4000, hash: "SHA-256" }, key, 256);
    }).then(function (bits) { return new Uint8Array(bits); });
  }
  function exportPinSetup() {
    var pin = $("#site-pin"), confirm = $("#site-pin-confirm"), btn = $("#pin-export");
    if (!pin || !confirm || !btn) return;
    if (!/^\d{4}$/.test(pin.value)) return toast("Le PIN doit contenir exactement 4 chiffres.", "error");
    if (pin.value !== confirm.value) return toast("Les deux PIN ne correspondent pas.", "error");
    if (!window.crypto || !window.crypto.getRandomValues || !window.crypto.subtle) return toast("Ouvre le portail en HTTPS ou sur localhost pour générer le PIN.", "error");
    var chosen = pin.value, recovery = recoveryDigits();
    var pinSalt = randomBytes(16), recSalt = randomBytes(16);
    btn.disabled = true;
    Promise.all([pinHash(chosen, pinSalt), pinHash(recovery, recSalt)]).then(function (hashes) {
      var script = String.fromCharCode(0) + "# STI2D-OS PIN setup (hashes only; consumed by the Userland)\n" +
        "STI2D_PIN_SETUP_V1=1\n" + "PINSALT=" + bytesToHex(pinSalt) + "\n" +
        "PINHASH=" + bytesToHex(hashes[0]) + "\n" + "RECSALT=" + bytesToHex(recSalt) + "\n" +
        "RECHASH=" + bytesToHex(hashes[1]) + "\n";
      download(new Blob([script], { type: "text/x-python" }), "sti2dos-pin-setup.py");
      pin.value = "";
      confirm.value = "";
      openSheet("PIN configuré", '<div class="note">' + icon("check") + '<span>Le fichier <strong>sti2dos-pin-setup.py</strong> a été généré. Il ne contient pas ton PIN en clair : uniquement les sels et les empreintes cryptographiques.</span></div>' +
        '<section class="card" style="margin-top:14px"><div class="card-head"><h3>Code de récupération — à conserver</h3></div><div class="cmd"><code>' + esc(recovery) + '</code><button class="icon-btn" type="button" data-act="copy" data-text="' + esc(recovery) + '" aria-label="Copier le code de récupération">' + icon("copy") + '</button></div><p class="muted small" style="margin:10px 0 0">Ce code ne sera plus affiché. Garde-le en lieu sûr.</p></section>' +
        '<p class="muted small" style="margin-top:14px">Transfère ce script Python sur ta NumWorks avec le gestionnaire de scripts officiel ou ton outil de synchronisation. Au prochain démarrage du Userland, STI2D-OS crée son enregistrement PIN privé puis supprime automatiquement le script de configuration. Il faut que le Userland démarre pour consommer le fichier.</p>',
        '<button class="btn btn-primary" type="button" data-act="sheet-close">J’ai conservé mon code</button>');
      toast("Fichier de configuration PIN téléchargé.");
    }).catch(function (err) {
      toast(err && err.message ? err.message : "Impossible de générer le fichier PIN.", "error");
    }).then(function () { btn.disabled = false; });
  }

  function viewProfiles() {
    var p = profile();
    return (
      pageHead("Profils", "Sauvegarder et partager", "Les profils partagent uniquement l'apparence et les préférences. Le PIN est configuré séparément et n'est jamais inclus dans un profil ou un code de partage.") +
      '<div class="grid-2">' +
      '<section class="card"><div class="card-head"><h3>Profil actuel</h3></div><div class="rows">' +
      '<div class="row-item"><span>Environnement du portail</span><strong>' + esc(skinName(p.portal.skin)) + "</strong></div>" +
      '<div class="row-item"><span>Accentuation</span><strong>' + esc(ACCENTS[prefs.accent].name) + "</strong></div>" +
      '<div class="row-item"><span>Animations</span><strong>' + (prefs.motion ? "Oui" : "Non") + "</strong></div>" +
      '<div class="row-item"><span>Interface calculatrice</span><strong>' + (prefs.calcEnv ? "STI2D Classic" : "STI2D-OS") + "</strong></div>" +
      '</div><div class="page-actions" style="margin-top:16px"><button class="btn btn-primary" type="button" data-act="profile-export">' + icon("download") + 'Exporter (.json)</button><button class="btn" type="button" data-act="profile-import">' + icon("upload") + "Importer un fichier</button></div></section>" +
      '<section class="card"><div class="card-head"><h3>Code de partage</h3></div><p class="muted small" style="margin-bottom:12px">Un code court à envoyer à un ami : il reprend l\'environnement, la couleur et les animations.</p>' +
      '<div class="cmd"><code id="share-code">' + esc(shareCode()) + '</code><button class="icon-btn" type="button" data-act="copy" data-text="' + esc(shareCode()) + '" aria-label="Copier le code">' + icon("copy") + "</button></div>" +
      '<label class="field" style="margin-top:16px"><span>Appliquer un code reçu</span><input class="input" id="code-in" placeholder="STI2D3-…" autocomplete="off" autocapitalize="off" spellcheck="false"></label>' +
      '<div class="page-actions" style="margin-top:12px"><button class="btn" type="button" data-act="code-apply">Appliquer</button><button class="btn btn-ghost" type="button" data-act="profile-reset">Réinitialiser</button></div></section></div>' +
      '<section class="card section"><div class="card-head"><h3>Code PIN de la calculatrice</h3><span class="badge accent">4 chiffres</span></div>' +
      '<p class="muted small">Choisis ici un PIN numérique de quatre chiffres. Le portail génère un fichier de configuration contenant uniquement des empreintes cryptographiques et un code de récupération à conserver.</p>' +
      '<div class="form-grid" style="margin-top:14px"><label class="field"><span>Nouveau PIN</span><input class="input" id="site-pin" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" autocomplete="new-password" placeholder="4 chiffres"></label>' +
      '<label class="field"><span>Confirmer le PIN</span><input class="input" id="site-pin-confirm" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" autocomplete="new-password" placeholder="Répéter le PIN"></label></div>' +
      '<div class="page-actions" style="margin-top:14px"><button class="btn btn-primary" id="pin-export" type="button" data-act="pin-export">' + icon("check") + 'Générer le fichier de configuration PIN</button></div>' +
      '<p class="faint small" style="margin-top:10px">Le PIN ne sera pas inclus dans les profils ni dans les codes de partage. Le code de récupération sera affiché une seule fois.</p></section>' +
      '<section class="card section"><div class="card-head"><h3>Réglages pour la calculatrice</h3><span class="badge accent">sti2dos.py</span></div>' +
      '<p class="muted small" style="margin-bottom:12px">STI2D-OS lit ses préférences dans le script <code>sti2dos.py</code>. Ce fichier reprend l\'interface, la couleur, les animations' + (currentWall() ? " et le fond exporté" : "") + ". Envoie-le comme un script Python (my.numworks.com ou nwlink). Le PIN se configure séparément avec le fichier sti2dos-pin-setup.py.</p>" +
      '<pre class="code-box" style="margin:0">' + esc(calcPrefsFile()) + "</pre>" +
      '<div class="page-actions" style="margin-top:14px"><button class="btn" type="button" data-act="calc-prefs">' + icon("download") + "Télécharger sti2dos.py</button></div></section>"
    );
  }

  /* ---------------------------------------------------------------- device */
  var usb = { device: null, error: "" };
  function modelFromDevice(d) {
    if (!d) return "";
    var m = "n0" + d.deviceVersionMajor.toString(16) + d.deviceVersionMinor.toString(16) + d.deviceVersionSubminor.toString(16);
    return MODELS[m] ? m : "";
  }
  function viewDevice() {
    var supported = !!(navigator.usb && navigator.usb.requestDevice);
    var d = usb.device, model = modelFromDevice(d);
    var why = isIOS ? "Safari sur iPhone et iPad ne propose pas WebUSB : aucune page web ne peut parler à la calculatrice sur iOS."
      : /Firefox/i.test(navigator.userAgent) ? "Firefox ne propose pas WebUSB. Utilise Chrome ou Edge."
      : /Safari/i.test(navigator.userAgent) && !/Chrome|Chromium|Edg/i.test(navigator.userAgent) ? "Safari ne propose pas WebUSB. Utilise Chrome ou Edge."
      : !window.isSecureContext ? "WebUSB demande une page sécurisée (https ou localhost). Ouvre le portail via start-server."
      : "Ce navigateur ne propose pas WebUSB.";
    return (
      pageHead("Appareil", "Ma calculatrice", "Détecte la calculatrice branchée pour connaître son modèle. Rien n'est écrit sur la calculatrice depuis cette page.") +
      '<div class="grid-2">' +
      '<section class="card"><div class="card-head"><h3>Connexion USB</h3><span class="badge ' + (d ? "ok" : supported ? "" : "warn") + '">' + (d ? "Détectée" : supported ? "Prête" : "Indisponible") + "</span></div>" +
      (supported
        ? (d
            ? '<div class="rows"><div class="row-item"><span>Modèle</span><strong>' + esc(model ? MODELS[model].name : "Inconnu") + "</strong></div>" +
              '<div class="row-item"><span>Produit</span><strong>' + esc(d.productName || "—") + "</strong></div>" +
              '<div class="row-item"><span>Fabricant</span><strong>' + esc(d.manufacturerName || "—") + "</strong></div>" +
              '<div class="row-item"><span>Version matérielle</span><strong>' + esc(d.deviceVersionMajor + "." + d.deviceVersionMinor + "." + d.deviceVersionSubminor) + "</strong></div></div>"
            : '<p class="muted small" style="margin-bottom:14px">Branche la calculatrice avec un câble USB de données, allume-la, puis autorise-la dans la fenêtre du navigateur.</p>') +
          (usb.error ? '<div class="note warn" style="margin-top:12px">' + icon("alert") + "<span>" + esc(usb.error) + "</span></div>" : "") +
          '<div class="page-actions" style="margin-top:16px"><button class="btn btn-primary" type="button" data-act="usb-detect">' + icon("usb") + (d ? "Détecter à nouveau" : "Détecter ma calculatrice") + "</button></div>"
        : '<div class="note warn">' + icon("alert") + "<span>" + esc(why) + "</span></div>") +
      "</section>" +
      '<section class="card"><div class="card-head"><h3>Mon modèle</h3></div><p class="muted small" style="margin-bottom:12px">Au dos de la calculatrice, ou Paramètres &gt; À propos. Il choisit le bon fichier dans Installation.</p>' +
      '<div class="model-grid">' + Object.keys(MODELS).map(function (k) {
        return '<button class="model" type="button" data-model="' + k + '" aria-pressed="' + (prefs.model === k) + '"><strong>' + MODELS[k].name + "</strong><small>" + esc(MODELS[k].cpu + " · " + MODELS[k].note) + "</small></button>";
      }).join("") + "</div></section></div>" +
      '<section class="card section"><div class="card-head"><h3>Ce que fait STI2D-OS sur chaque modèle</h3></div><div class="rows">' +
      '<div class="row-item"><span>N0120<span class="desc">Le noyau et le logiciel officiels restent dans un slot. STI2D-OS est un userland Epsilon (le système des apps) écrit dans l\'autre slot : il démarre sur l\'accueil STI2D-OS.</span></span><span class="badge ok">Userland</span></div>' +
      '<div class="row-item"><span>N0110 · N0115<span class="desc">Même userland, compilé pour leur processeur (216 MHz, autre RAM). Sur Upsilon (déverrouillée) : la version app .nwa.</span></span><span class="badge ok">Userland</span></div>' +
      '<div class="row-item"><span>Reset<span class="desc">Reset (au dos) revient au logiciel officiel ; l\'installateur relance STI2D-OS. Le mode examen ramène l\'accueil officiel.</span></span><span class="badge warn">Limite</span></div></div></section>'
    );
  }
  function usbDetect() {
    if (!navigator.usb) return;
    usb.error = "";
    navigator.usb.requestDevice({ filters: [{ vendorId: 0x0483, productId: 0xa291 }] }).then(function (d) {
      usb.device = d;
      var m = modelFromDevice(d);
      if (m) {
        prefs.model = m;
        savePrefs();
        toast("Calculatrice " + MODELS[m].name + " détectée.");
      } else {
        usb.error = "Calculatrice détectée, mais son modèle n'a pas pu être lu.";
      }
      render();
    }).catch(function (e) {
      if (e && e.name === "NotFoundError") usb.error = "Aucune calculatrice choisie. Vérifie le câble (certains ne font que charger) et que la calculatrice est allumée.";
      else if (e && e.name === "SecurityError") usb.error = "Le navigateur a refusé l'accès USB (page non sécurisée ou politique du navigateur).";
      else usb.error = "Détection impossible : " + (e && e.message ? e.message : "erreur inconnue") + ".";
      render();
    });
  }

  /* ---------------------------------------------------------------- install */
  function viewInstall() {
    var m = prefs.model;
    var file = m ? "../release/" + m + "/STI2D-OS.nwa" : "";
    var dfuB = m ? "../userland/firmware/" + m + "/sti2d-os.B.dfu" : "";
    var dfuA = m ? "../userland/firmware/" + m + "/sti2d-os.A.dfu" : "";
    return (
      pageHead("Installation", "Installer STI2D-OS", "Choisis ton modèle : le fichier correspondant et les méthodes possibles s'affichent. Sauvegarde tes scripts Python avant d'installer.") +
      '<div class="model-grid" style="margin-bottom:20px">' + Object.keys(MODELS).map(function (k) {
        return '<button class="model" type="button" data-model="' + k + '" aria-pressed="' + (m === k) + '"><strong>' + MODELS[k].name + "</strong><small>" + esc(MODELS[k].note) + "</small></button>";
      }).join("") + "</div>" +
      (!m
        ? '<div class="note">' + icon("info") + '<span>Choisis un modèle ci-dessus, ou <a href="#calculatrice" style="color:var(--accent)">détecte ta calculatrice</a>.</span></div>'
        : '<section class="card"><div class="card-head"><h3>STI2D-OS 2.2 · système pour ' + MODELS[m].name + '</h3><span class="badge ok">Userland</span></div>' +
          '<p class="muted small" style="margin-bottom:14px">Le système de la calculatrice (userland Epsilon) avec STI2D-OS comme accueil : nid d\'abeilles, Calculatrice (toutes les apps NumWorks), STI2D Classic par double π, Centre de contrôle, PIN, Horloge, Terminal, Galerie, Calendrier. Tes jeux et les IA sont préinstallés à côté. Le logiciel officiel reste dans l\'autre slot.</p>' +
          '<ol class="steps"><li><span>Sur un ordinateur (macOS, Linux, Windows), dans le dossier du projet : <code>python3 userland/installer/server.py</code></span></li><li><span>Branche la calculatrice : écran « La calculatrice est connectée ». Ferme my.numworks.com.</span></li><li><span>Coche tes jeux et IA, puis <strong>Installer STI2D-OS</strong>. Le slot libre est écrit ; la calculatrice redémarre sur STI2D-OS.</span></li></ol>' +
          '<div class="cmd" style="margin-top:8px"><code>python3 userland/installer/server.py</code><button class="icon-btn" type="button" data-act="copy" data-text="python3 userland/installer/server.py" aria-label="Copier la commande">' + icon("copy") + "</button></div>" +
          '<p class="faint small" style="margin-top:12px">Le userland doit avoir la même version que le logiciel officiel de la calculatrice (l\'installateur vérifie). Fichiers bruts : <a href="' + dfuB + '" download style="color:var(--accent)">slot B</a> · <a href="' + dfuA + '" download style="color:var(--accent)">slot A</a> (DFU).</p></section>' +
          '<section class="card section"><div class="card-head"><h3>Alternative · version app (.nwa)</h3><span class="badge">Upsilon · sans installateur</span></div>' +
          '<p class="muted small" style="margin-bottom:14px">Les mêmes écrans dans une seule app, lancée depuis l\'accueil officiel : utile sur Upsilon ou sans ordinateur avec Python.</p>' +
          '<div class="page-actions"><a class="btn" href="' + file + '" download="STI2D-OS-' + MODELS[m].name + '.nwa">' + icon("download") + "Télécharger STI2D-OS.nwa</a></div>" +
          '<div class="method"><div class="method-head"><h3>Méthode 1 · site officiel NumWorks</h3><span class="badge ok">Recommandée</span><span class="badge">Chrome · Edge · ordinateur</span></div>' +
          '<ol class="steps"><li><span>Mets la calculatrice à jour sur <strong>my.numworks.com</strong> (Epsilon 23.2 ou plus récent).</span></li><li><span>Ouvre <strong>my.numworks.com/apps</strong>, connecte-toi et clique sur Connecter.</span></li><li><span>Ajoute <strong>STI2D-OS.nwa</strong> (et, si tu veux, tes .nwa de la bibliothèque), puis Installer.</span></li><li><span>Sur la calculatrice : touche Home, puis l\'app <strong>STI2D-OS</strong> à la fin de la liste.</span></li></ol>' +
          '<a class="btn btn-sm" href="https://my.numworks.com/apps" target="_blank" rel="noopener noreferrer">Ouvrir my.numworks.com/apps</a></div>' +
          '<div class="method"><div class="method-head"><h3>Méthode 2 · nwlink (ligne de commande)</h3><span class="badge">Epsilon · Upsilon</span></div>' +
          '<p class="muted small">Avec Node.js installé, calculatrice branchée :</p><div class="cmd"><code>npx nwlink install-nwa STI2D-OS.nwa</code><button class="icon-btn" type="button" data-act="copy" data-text="npx nwlink install-nwa STI2D-OS.nwa" aria-label="Copier">' + icon("copy") + "</button></div>" +
          (m !== "n0120" ? '<p class="faint small">Sur Upsilon, le site officiel affiche 0 Ko libre : utilise cette méthode (Upsilon de juin 2026 ou plus récent).</p>' : "") + "</div>" +
          (m === "n0120"
            ? '<div class="method"><div class="method-head"><h3>Méthode 3 · ancien installateur DFU</h3><span class="badge warn">N0120 uniquement</span></div>' +
              '<p class="muted small">L\'installateur d\'origine écrit son propre système STI2D OS (sélection de jeux, fond d\'écran) par DFU/WebUSB. Il refuse les autres modèles. Après un reset, la calculatrice revient au système officiel.</p>' +
              '<a class="btn btn-sm" href="../installer/Installer-STI2D-OS-V2.html" target="_blank" rel="noopener">Ouvrir l\'installateur</a></div>'
            : "") +
          "</section>") +
      '<div class="note section' + (isIOS ? " warn" : "") + '">' + icon(isIOS ? "alert" : "info") + "<span><strong style=\"color:var(--text)\">iPhone et iPad :</strong> aucun navigateur iOS ne peut transférer une app vers la calculatrice (pas de WebUSB). Télécharge les fichiers ici, puis installe-les depuis un ordinateur.</span></div>" +
      '<div class="note section">' + icon("info") + "<span>Installer des apps efface les fichiers de la calculatrice sauf les scripts Python, et remplace les apps installées avant : ajoute ensemble toutes celles que tu veux garder. Les réglages de STI2D-OS (<code>sti2dos.py</code>) sont conservés ; le code PIN (<code>sti2dos.pin</code>) est effacé.</span></div>"
    );
  }

  /* ---------------------------------------------------------------- settings */
  function viewSettings() {
    var bytes = cache.nwa.reduce(function (s, i) { return s + (i.size || 0); }, 0);
    var wbytes = cache.walls.reduce(function (s, i) { return s + (i.size || 0); }, 0);
    return (
      pageHead("Paramètres", "Paramètres", "Préférences de ce portail et données enregistrées dans ce navigateur.") +
      '<div class="grid-2">' +
      '<section class="card"><div class="card-head"><h3>Affichage</h3></div><div class="rows">' +
      '<div class="row-item"><span>Animations<span class="desc">Transitions et effets</span></span><button class="switch" type="button" role="switch" data-act="toggle-motion" aria-checked="' + prefs.motion + '" aria-label="Animations"></button></div>' +
      '<div class="row-item"><span>Fond d\'écran derrière le portail<span class="desc">' + (currentWall() ? "Utilise le fond choisi dans Personnalisation" : "Importe d\'abord un fond dans Personnalisation") + '</span></span><button class="switch" type="button" role="switch" data-act="toggle-portal-wall" aria-checked="' + prefs.portalWall + '" aria-label="Fond derrière le portail"' + (currentWall() ? "" : " disabled") + "></button></div>" +
      '<div class="row-item"><span>Environnement</span><a class="btn btn-sm btn-ghost" href="#studio">' + esc(skinName(prefs.skin)) + icon("chevron") + "</a></div></div></section>" +
      '<section class="card"><div class="card-head"><h3>Stockage local</h3><span class="badge" id="persist-badge">…</span></div><div class="rows">' +
      '<div class="row-item"><span>Bibliothèque personnelle</span><strong>' + plural(cache.nwa.length, "fichier", "fichiers") + " · " + size(bytes) + "</strong></div>" +
      '<div class="row-item"><span>Fonds d\'écran</span><strong>' + plural(cache.walls.length, "image", "images") + " · " + size(wbytes) + "</strong></div>" +
      '<div class="row-item"><span>Utilisé par ce site<span class="desc" id="quota-line">Estimation du navigateur…</span></span><strong id="usage-line">…</strong></div></div>' +
      '<div class="storage-meter"><i id="usage-bar" style="width:0"></i></div>' +
      '<div class="page-actions" style="margin-top:16px"><button class="btn btn-sm" type="button" data-act="persist">Demander un stockage durable</button><button class="btn btn-sm btn-danger" type="button" data-act="clear-library"' + (cache.nwa.length ? "" : " disabled") + ">Vider la bibliothèque</button><button class=\"btn btn-sm btn-ghost\" type=\"button\" data-act=\"clear-walls\"" + (cache.walls.length ? "" : " disabled") + ">Supprimer les fonds</button></div></section></div>" +
      '<section class="card section"><div class="card-head"><h3>À propos</h3></div><div class="rows">' +
      '<div class="row-item"><span>Portail</span><strong>STI2D-OS ' + VERSION + "</strong></div>" +
      '<div class="row-item"><span>Données</span><strong>Uniquement dans ce navigateur</strong></div>' +
      '<div class="row-item"><span>Communauté</span><a class="discord-link" href="' + DISCORD + '" target="_blank" rel="noopener noreferrer" aria-label="Rejoindre le Discord STI2D-OS">' + icon("discord") + "<span>Discord</span></a></div></div></section>"
    );
  }
  afterRender.parametres = function () {
    Store.estimate().then(function (est) {
      var u = $("#usage-line"), q = $("#quota-line"), bar = $("#usage-bar");
      if (!u) return;
      if (!est || !est.quota) {
        u.textContent = "—";
        q.textContent = "Estimation non fournie par ce navigateur";
        return;
      }
      u.textContent = size(est.usage);
      q.textContent = "sur environ " + size(est.quota) + " permis à ce site";
      bar.style.width = Math.min(100, (est.usage / est.quota) * 100).toFixed(1) + "%";
    });
    if (navigator.storage && navigator.storage.persisted)
      navigator.storage.persisted().then(function (p) {
        var b = $("#persist-badge");
        if (b) {
          b.textContent = p ? "Durable" : "Standard";
          b.className = "badge " + (p ? "ok" : "");
        }
      });
    else {
      var b = $("#persist-badge");
      if (b) b.textContent = "Standard";
    }
  };

  /* ================================================================ events */
  document.addEventListener("click", function (e) {
    var t = e.target.closest("[data-act],[data-tab],[data-filter],[data-skin],[data-accent],[data-calcenv],[data-wall],[data-model]");
    if (!t) return;
    if (t.hasAttribute("data-tab") && !t.hasAttribute("data-act")) {
      prefs.libTab = t.getAttribute("data-tab");
      savePrefs();
      if (route === "bibliotheque") {
        e.preventDefault();
        render();
      }
      return;
    }
    if (t.hasAttribute("data-filter")) {
      prefs.filter = t.getAttribute("data-filter");
      savePrefs();
      $$("[data-filter]").forEach(function (b) { b.setAttribute("aria-pressed", String(b === t)); });
      $("#lib-results").innerHTML = libResults();
      return;
    }
    if (t.classList.contains("env") && t.hasAttribute("data-skin")) {
      prefs.skin = t.getAttribute("data-skin");
      savePrefs();
      applyAppearance();
      render();
      return;
    }
    if (t.hasAttribute("data-accent")) {
      prefs.accent = +t.getAttribute("data-accent");
      savePrefs();
      applyAppearance();
      render();
      return;
    }
    if (t.hasAttribute("data-calcenv")) {
      prefs.calcEnv = +t.getAttribute("data-calcenv");
      savePrefs();
      render();
      return;
    }
    if (t.hasAttribute("data-wall")) {
      prefs.wallpaper = t.getAttribute("data-wall");
      savePrefs();
      conv.result = null;
      applyPortalWallpaper();
      render();
      return;
    }
    if (t.hasAttribute("data-model")) {
      prefs.model = t.getAttribute("data-model");
      savePrefs();
      render();
      return;
    }
    var act = t.getAttribute("data-act"), id = t.getAttribute("data-id"), kind = t.getAttribute("data-kind");
    switch (act) {
      case "menu":
        openMenu();
        break;
      case "sheet-close":
        closeSheet();
        break;
      case "dismiss-file-note":
        try { sessionStorage.setItem("sti2d.fileNote", "1"); } catch (_) {}
        t.closest(".note").remove();
        break;
      case "doc-import":
        pickFiles(".txt,.md,.csv,text/plain,text/markdown,text/csv", false).then(function (fs) {
          if (fs[0]) exportTextDocument(fs[0]);
        });
        break;
      case "import":
        pickFiles(".nwa", true).then(importFiles);
        break;
      case "download":
        downloadItem(kind, id);
        break;
      case "info":
        infoSheet(kind, id);
        break;
      case "edit":
        editSheet(id);
        break;
      case "delete":
        deleteItem(id);
        break;
      case "copy":
        copyText(t.getAttribute("data-text"));
        break;
      case "toggle-motion":
        prefs.motion = !prefs.motion;
        prefs.calcAnim = prefs.motion;
        savePrefs();
        applyAppearance();
        render();
        break;
      case "toggle-portal-wall":
        prefs.portalWall = !prefs.portalWall;
        savePrefs();
        applyPortalWallpaper();
        render();
        break;
      case "wall-import":
        importWallpapers();
        break;
      case "wall-none":
        prefs.wallpaper = "";
        savePrefs();
        applyPortalWallpaper();
        render();
        break;
      case "wall-delete":
        confirmSheet("Supprimer ce fond d'écran ?", "L'image est effacée de ce navigateur.", "Supprimer", true).then(function (ok) {
          if (!ok) return;
          return Store.remove(id).then(function () {
            dropUrl(id);
            if (prefs.wallpaper === id) prefs.wallpaper = "";
            savePrefs();
            return refreshCache();
          }).then(function () {
            applyPortalWallpaper();
            render();
            toast("Fond d'écran supprimé.");
          });
        }).catch(function (err) { toast(err.message, "error"); });
        break;
      case "pin-export":
        exportPinSetup();
        break;
      case "profile-export":
        download(new Blob([JSON.stringify(profile(), null, 2)], { type: "application/json" }), "sti2d-os-profil.json");
        toast("Profil exporté.");
        break;
      case "profile-import":
        pickFiles("application/json,.json", false).then(function (fs) {
          if (!fs[0]) return;
          if (fs[0].size > 64 * 1024) return toast("Fichier trop gros pour un profil.", "error");
          fs[0].text().then(function (txt) {
            try {
              applyProfileData(JSON.parse(txt));
              render();
              toast("Profil importé.");
            } catch (err) {
              toast("Ce fichier n'est pas un profil STI2D-OS valide.", "error");
            }
          });
        });
        break;
      case "code-apply":
        try {
          applyProfileData(parseCode($("#code-in").value));
          render();
          toast("Code appliqué.");
        } catch (err) {
          toast("Code invalide ou incompatible.", "error");
        }
        break;
      case "profile-reset":
        confirmSheet("Réinitialiser l'apparence ?", "Environnement, couleur et animations reviennent aux réglages d'origine. Ta bibliothèque n'est pas touchée.", "Réinitialiser").then(function (ok) {
          if (!ok) return;
          ["skin", "accent", "motion", "calcEnv", "calcAnim", "portalWall"].forEach(function (k) { prefs[k] = DEFAULTS[k]; });
          savePrefs();
          applyAppearance();
          render();
        });
        break;
      case "calc-prefs":
        download(new Blob([calcPrefsFile()], { type: "text/x-python" }), "sti2dos.py");
        toast("sti2dos.py téléchargé.");
        break;
      case "usb-detect":
        usbDetect();
        break;
      case "persist":
        Store.persist().then(function (ok) {
          toast(ok ? "Le navigateur gardera ces données." : "Le navigateur a refusé (il décide selon ton usage du site).", ok ? "" : "error");
          render();
        });
        break;
      case "clear-library":
        confirmSheet("Vider la bibliothèque personnelle ?", "Les " + plural(cache.nwa.length, "fichier importé", "fichiers importés") + " seront effacés de ce navigateur. Les 80 jeux d'origine restent.", "Tout supprimer", true).then(function (ok) {
          if (!ok) return;
          return Store.clearKind("nwa").then(function (n) {
            cache.nwa.forEach(function (i) { dropUrl(i.id + ":cover"); });
            return refreshCache().then(function () {
              render();
              toast(plural(n, "fichier supprimé", "fichiers supprimés") + ".");
            });
          });
        }).catch(function (err) { toast(err.message, "error"); });
        break;
      case "clear-walls":
        confirmSheet("Supprimer tous les fonds d'écran ?", "Les images importées seront effacées de ce navigateur.", "Supprimer", true).then(function (ok) {
          if (!ok) return;
          return Store.clearKind("wallpaper").then(function () {
            cache.walls.forEach(function (w) { dropUrl(w.id); });
            prefs.wallpaper = "";
            savePrefs();
            return refreshCache().then(function () {
              applyPortalWallpaper();
              render();
            });
          });
        }).catch(function (err) { toast(err.message, "error"); });
        break;
    }
  });

  function copyText(text) {
    var ok = function () { toast("Copié."); };
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(text).then(ok, function () { fallbackCopy(text) ? ok() : toast("Copie impossible : sélectionne le texte.", "error"); });
    } else if (fallbackCopy(text)) ok();
    else toast("Copie impossible : sélectionne le texte.", "error");
  }
  function fallbackCopy(text) {
    var ta = document.createElement("textarea");
    ta.value = text;
    ta.setAttribute("readonly", "");
    ta.style.position = "fixed";
    ta.style.opacity = "0";
    document.body.appendChild(ta);
    ta.select();
    var ok = false;
    try { ok = document.execCommand("copy"); } catch (e) {}
    ta.remove();
    return ok;
  }

  /* ================================================================ start */
  function start() {
    clearTimeout(window.__stiBootTimer);
    document.documentElement.setAttribute("data-ready", "1");
    var cssOk = getComputedStyle(document.documentElement).getPropertyValue("--css-ok").trim() === "1";
    applyAppearance();
    $("#menu-open").addEventListener("click", openMenu);
    $("#menu-close").addEventListener("click", closeMenu);
    $("#scrim").addEventListener("click", closeMenu);
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape") closeMenu();
    });
    $("#sheet").addEventListener("close", function () {
      var cb = sheetOnClose;
      sheetOnClose = null;
      if (cb) cb();
    });
    $("#sheet").addEventListener("click", function (e) {
      if (e.target === e.currentTarget) closeSheet(); /* a click on the backdrop */
    });
    window.addEventListener("hashchange", go);
    go();
    if (!cssOk) {
      var page = $("#page");
      page.insertAdjacentHTML("afterbegin", '<div class="note danger">La feuille de style <code>assets/styles.css</code> n\'a pas pu être chargée : vérifie que le dossier <code>web/assets</code> est complet et ouvre le portail via un serveur web.</div>');
    }
    refreshCache().then(function () {
      if (!GAMES.length) toast("Manifeste des jeux introuvable (web/data/games.js).", "error");
      render();
    });
    if (navigator.usb && navigator.usb.getDevices) {
      navigator.usb.getDevices().then(function (ds) {
        var d = ds.filter(function (x) { return x.vendorId === 0x0483 && x.productId === 0xa291; })[0];
        if (d) usb.device = d;
      }).catch(function () {});
      navigator.usb.addEventListener && navigator.usb.addEventListener("disconnect", function (ev) {
        if (usb.device && ev.device === usb.device) {
          usb.device = null;
          if (route === "calculatrice") render();
        }
      });
    }
    if ("serviceWorker" in navigator && /^https?:$/.test(location.protocol)) {
      navigator.serviceWorker.register("sw.js").catch(function () {});
    }
  }

  window.STIPortal = { prefs: prefs, render: render, importFiles: importFiles, importWallpaperFiles: importWallpaperFiles, refresh: refreshCache, calcPrefsFile: calcPrefsFile, shareCode: shareCode, parseCode: parseCode, profile: profile };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start);
  else start();
})();
