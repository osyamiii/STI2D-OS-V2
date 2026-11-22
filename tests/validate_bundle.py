#!/usr/bin/env python3
"""Checks the STI2D-OS V2 bundle: original resources, manifests, links, portal files, firmware builds.

    python3 tests/validate_bundle.py

Only reads files. Each check prints PASS or FAIL; the exit code is the number of failures.
"""
import json
import re
import struct
import sys
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
failures = []


def check(ok, msg):
    print(("PASS  " if ok else "FAIL  ") + msg)
    if not ok:
        failures.append(msg)


def manifest(path, var):
    text = (ROOT / path).read_text(encoding="utf-8")
    m = re.match(r"\s*window\.%s\s*=\s*(\[.*\]);?\s*$" % var, text, re.S)
    return json.loads(m.group(1)) if m else None


def elf_arm32(p):
    b = p.read_bytes()[:20]
    return len(b) == 20 and b[:4] == b"\x7fELF" and b[4] == 1 and b[5] == 1 and struct.unpack_from("<H", b, 18)[0] == 40


games = sorted(p for p in (ROOT / "library/games").iterdir() if p.is_file() and not p.name.startswith("."))
ais = sorted(p for p in (ROOT / "library/ai").iterdir() if p.is_file() and p.suffix.lower() == ".nwa")
check(len(games) == 80, f"library/games : 80 fichiers ({len(games)})")
check(sum(p.suffix.lower() == ".nwa" for p in games) == 80, "80 jeux NumWorks .nwa")
check(not any(p.suffix.lower() == ".gb" for p in games), "aucune ROM Game Boy dans la distribution")
check(len(ais) == 3 and {p.name for p in ais} == {"Aelyx.IA.nwa", "Matrices-v4.nwa", "STMG_IA.nwa"}, "3 IA : Aelyx IA, Matrices V4, STMG IA")
nwa = [p for p in games if p.suffix.lower() == ".nwa"] + ais
check(all(elf_arm32(p) for p in nwa), "83 .nwa : en-tête ELF ARM 32 bits little-endian")

g = manifest("web/data/games.js", "STI2D_GAMES")
a = manifest("web/data/ai.js", "STI2D_AI")
check(g is not None and len(g) == 80, "web/data/games.js : 80 jeux .nwa")
check(a is not None and len(a) == 3, "web/data/ai.js : 3 entrées")
if g and a:
    check(all((ROOT / "library/games" / e["filename"]).is_file() for e in g), "chaque jeu du manifeste existe (noms réels, sans double encodage)")
    check(all((ROOT / "library/ai" / e["filename"]).is_file() for e in a), "chaque IA du manifeste existe")
    check(not any("%" in e["filename"] for e in g + a), "aucun nom de fichier pré-encodé (%20, %2520) dans les manifestes")
    special = [e for e in g if re.search(r"[ ()]", e["filename"])]
    check(len(special) >= 2 and all(quote(e["filename"]) != e["filename"] for e in special),
          "noms avec espaces/parenthèses encodés une seule fois par le portail")
    check(any(e["title"] == "Subway Surfers" for e in g) and any(e["title"] == "The Final Earth 2" for e in g), "Subway Surfers et Final Earth 2 référencés")
    check(not any(e.get("format") == "GB" or e["filename"].lower().endswith(".gb") for e in g), "aucune ROM dans le manifeste de jeux")
    covers = [e["cover"] for e in g + a if e.get("cover")]
    check(len(covers) == 83 and all((ROOT / "web" / c).is_file() for c in covers), "83 couvertures extraites des .nwa présentes")

for f in ["web/index.html", "web/assets/styles.css", "web/assets/js/app.js", "web/assets/js/store.js", "web/assets/js/nwa.js",
          "web/assets/js/stimg.js", "web/sw.js", "web/manifest.webmanifest", "web/assets/favicon.svg", "index.html"]:
    check((ROOT / f).is_file(), f"portail : {f}")
html = (ROOT / "web/index.html").read_text(encoding="utf-8")
for ref in re.findall(r'(?:src|href)="([^"#:]+)"', html):
    check((ROOT / "web" / ref).is_file(), f"index.html référence un fichier existant : {ref}")
check('href="https://discord.gg/sti2d"' in html and 'aria-label="Rejoindre le Discord STI2D-OS"' in html, "lien Discord dans le pied de page de toutes les pages")
check("<style>" in html and "--boot-bg" in html, "styles critiques en ligne (page lisible sans styles.css)")
root_index = (ROOT / "index.html").read_text(encoding="utf-8")
check("web/index.html" in root_index, "index.html racine → web/index.html (fonctionne aussi en file://)")
app = (ROOT / "web/assets/js/app.js").read_text(encoding="utf-8")
check("Ajouter un jeu ou une application" in app, "bouton « + Ajouter un jeu ou une application »")
check("function viewSystems()" in app and all(('id: "' + x + '"') in app for x in ["sti2d", "classic", "3ds", "ps5", "ios", "windows", "xbox", "linux", "android", "macos"]), "les 10 environnements OS sont sélectionnables dans le portail")
check('systemes: { title: "Systèmes"' in app, "page dédiée Systèmes dans la navigation")
check('"80 jeux .nwa"' in app or '80 jeux NumWorks' in app, "statistiques portail débarrassées de la ROM .gb")
check("localStorage.setItem(\"sti2d.prefs" in app and "Store.put(item, file" in app, "fichiers importés dans IndexedDB, préférences seules en localStorage")
check(not re.search(r"pin", json.dumps(re.findall(r"function profile\(\)[\s\S]*?\n  }", app)), re.I), "le profil exporté ne contient aucun PIN")

installer = (ROOT / "installer/Installer-STI2D-OS-V2.html").read_text(errors="ignore")
check("requestDevice" in installer and "0xa291" in installer, "installateur DFU/WebUSB conservé")
check("adsWatched" not in installer and "ADS_REQUIRED" not in installer, "pas de verrou publicitaire dans l'installateur")

for m in ("n0110", "n0115", "n0120"):
    p = ROOT / "release" / m / "STI2D-OS.nwa"
    check(p.is_file() and elf_arm32(p), f"release/{m}/STI2D-OS.nwa : firmware compilé (ELF ARM)")
    if p.is_file():
        check(f"N0{m[2:]}".encode() in p.read_bytes(), f"release/{m} : construit pour son modèle")

src = ROOT / "firmware/v2/src"
for f in ["config.c", "pin.c", "crypto.c", "control_center.c", "clock.c", "terminal.c", "input_pi.c", "pin_screen.c",
          "gfx_adapt.c", "home_os.c", "gallery.c", "calendar.c", "stimg.c"]:
    check((src / f).is_file(), f"module V2 : {f}")
mk = (ROOT / "firmware/sti2d-play-source-v1/Makefile").read_text()
check("../v2/src/*.c" in mk and "sti2dos-n0110" in mk and "sti2dos-n0120" in mk, "Makefile : modules V2 dans LAUNCHER_SRC, cibles par modèle")
header = ROOT / "firmware/sti2d-play-source-v1/launcher/src/game_catalog_names.h"
check(header.is_file() and header.read_text().count('"') >= 160, "catalogue C généré à partir des 80 noms embarqués des jeux")
launcher = (ROOT / "firmware/sti2d-play-source-v1/launcher/src/sti2d.c").read_text()
check("is_catalog_game_app" in launcher and "l->only == 1 && (game || ai)" in launcher, "filtrage Epsilon : Games / My Apps / Intelligences séparés")
home = (src / "home_os.c").read_text()
check("gfx_circle(cx, cy" in home and "d2 < 6400" in home, "accueil circulaire avec effet de loupe conservant la grille V1")
check("STI_PIN_MIN_LEN 4" in (ROOT / "firmware/v2/include/sti2d_v2.h").read_text() and "STI_PIN_MAX_LEN 4" in (ROOT / "firmware/v2/include/sti2d_v2.h").read_text() and 'id="site-pin"' in app and 'data-act="pin-export"' in app and "function exportPinSetup()" in app, "PIN à 4 chiffres configurable depuis le site")
check('sti_v2_pin_set("0000")' not in (src / "control_center.c").read_text(), "aucun PIN automatique 0000")

print(f"\n{len(failures)} échec(s).")
sys.exit(len(failures))
