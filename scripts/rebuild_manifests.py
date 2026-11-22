#!/usr/bin/env python3
"""Rebuilds the portal's manifests (web/data/games.js, web/data/ai.js) and covers from library/.

- The manifest keeps each file's real name (spaces and all: "SubwaySurfers (1).nwa"); the portal
  encodes it once when it builds a link. The previous manifests stored names that were already
  URL-encoded ("SubwaySurfers%20(1).nwa", "Pokemon%2520-..."), which matched no file and were
  encoded a second time in the links: those downloads failed.
- `downloadName` is a clean name for the saved file (no trailing space, no " (1)").
- From each .nwa (an ELF ARM 32-bit relocatable file, as EADK apps are), the app's own name
  (.rodata.eadk_app_name), its API level and its icon (.rodata.eadk_app_icon: LZ4 block of a
  55 x 56 RGB565 image) are read; the icon becomes web/data/covers/<id>.png.
No library file is renamed, moved or modified. Python 3 only, no third-party module.
"""
import json
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAMES = ROOT / "library/games"
AI = ROOT / "library/ai"
OUT = ROOT / "web/data"
COVERS = OUT / "covers"

FRIENDLY = {
    "2048": "2048", "AmongUsv2": "Among Us", "AngryBirds": "Angry Birds", "asphalt-urban-gt2": "Asphalt Urban GT 2",
    "backrooms96": "Backrooms ’96", "BadTimeSimulator": "Bad Time Simulator", "BindingOfIsaac": "The Binding of Isaac",
    "BlockBlast": "Block Blast", "BlockBreaker": "Block Breaker", "BuckshotRoulette": "Buckshot Roulette",
    "CatNinja": "Cat Ninja", "ChampionIsland": "Champion Island", "ChromeDino": "Chrome Dino",
    "ClashRoyale": "Clash Royale", "ConnectFour": "Connect Four", "CookieClicker": "Cookie Clicker",
    "CoreBall": "Core Ball", "CrossyRoad": "Crossy Road", "DonkeyKong": "Donkey Kong", "DoodleJump": "Doodle Jump",
    "FinalEarth2": "The Final Earth 2", "FiveNightsAtFreddys": "Five Nights at Freddy’s",
    "FiveNightsAtFreddys2": "Five Nights at Freddy’s 2", "FiveNightsAtFreddys3": "Five Nights at Freddy’s 3",
    "FiveNightsAtFreddys4": "Five Nights at Freddy’s 4", "FiveNightsAtFreddysWorld": "FNaF World",
    "FlappyBird": "Flappy Bird", "FlappyDunk": "Flappy Dunk", "FridayNightFunkin": "Friday Night Funkin’",
    "HappyWheels": "Happy Wheels", "HillClimbRacing": "Hill Climb Racing", "HollowKnight": "Hollow Knight",
    "JetpackJoyride": "Jetpack Joyride", "KingdomRush": "Kingdom Rush", "LevelDevil": "Level Devil",
    "Minecraft-1.8": "Minecraft 1.8", "MonkeyMart": "Monkey Mart", "MotoX3M": "Moto X3M", "MrBullet": "Mr Bullet",
    "Nazi_Zombies_NumWorks": "Nazi Zombies NumWorks", "PacMan": "Pac-Man", "PlantsVsZombies": "Plants vs. Zombies",
    "PortalReturns": "Portal Returns", "SnowRider3D": "Snow Rider 3D", "SonicTheHedgehog": "Sonic the Hedgehog",
    "SpaceInvaders": "Space Invaders", "sti2d_vs_stmg3": "STI2D vs STMG 3", "StickmanHook": "Stickman Hook",
    "SubwaySurfers": "Subway Surfers", "SuikaGame": "Suika Game", "SuperMario64": "Super Mario 64",
    "TempleRun": "Temple Run", "TombOfTheMask": "Tomb of the Mask", "TowerBlocks": "Tower Blocks",
    "TwoPlayers": "Two Players", "UltimateTicTacToe": "Ultimate Tic-Tac-Toe", "UniversalPaperclips": "Universal Paperclips",
}
ARCADE = set("2048 AmongUsv2 AngryBirds Balatro BlockBlast BlockBreaker BuckshotRoulette CatNinja Celeste ChromeDino "
             "CrossyRoad DoodleJump FlappyBird FlappyDunk GTA HappyWheels JetpackJoyride LevelDevil MotoX3M MrBullet "
             "NumDash NumDrive NumPico PacMan PolyTrack Rider Snake SnowRider3D StickmanHook SubwaySurfers TempleRun "
             "Tetris TombOfTheMask SpaceInvaders DonkeyKong".split())
PUZZLE = set("ConnectFour CoreBall Mahjong Minesweeper NumChess Sokoban Solitaire Sudoku SuikaGame TowerBlocks "
             "TwoPlayers UltimateTicTacToe UNO".split())
ADVENTURE = set("BadTimeSimulator BindingOfIsaac ChampionIsland Deltarune Doom Dune FinalEarth2 FiveNightsAtFreddys "
                "FiveNightsAtFreddys2 FiveNightsAtFreddys3 FiveNightsAtFreddys4 FiveNightsAtFreddysWorld HollowKnight "
                "Minecraft-1.8 Nazi_Zombies_NumWorks PortalReturns SonicTheHedgehog Undertale".split())
STRATEGY = set("asphalt-urban-gt2 HillClimbRacing KingdomRush MonkeyMart PlantsVsZombies CookieClicker ClashRoyale "
               "UniversalPaperclips".split())


def clean_stem(stem):
    """'SubwaySurfers (1)' -> 'SubwaySurfers', 'FinalEarth2 ' -> 'FinalEarth2'"""
    return re.sub(r"\s*\(\d+\)$", "", stem).strip()


def elf_sections(data):
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        return None
    shoff = struct.unpack_from("<I", data, 0x20)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x2E)
    if not shoff or shstrndx >= shnum:
        return None
    heads = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    strtab = heads[shstrndx][4]
    out = {}
    for h in heads:
        end = data.index(b"\0", strtab + h[0])
        out[data[strtab + h[0]:end].decode("ascii", "replace")] = (h[1], h[4], h[5])  # type, offset, size
    return out


def lz4_block(src, size):
    out = bytearray()
    i = 0
    while i < len(src):
        token = src[i]
        i += 1
        n = token >> 4
        if n == 15:
            while True:
                b = src[i]
                i += 1
                n += b
                if b != 255:
                    break
        out += src[i:i + n]
        i += n
        if i >= len(src) or len(out) >= size:
            break
        off = src[i] | src[i + 1] << 8
        i += 2
        m = token & 15
        if m == 15:
            while True:
                b = src[i]
                i += 1
                m += b
                if b != 255:
                    break
        for _ in range(m + 4):
            out.append(out[-off])
    return bytes(out[:size])


def png(width, height, rgb):
    raw = b"".join(b"\0" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def nwa_info(path, cover_id):
    data = path.read_bytes()
    s = elf_sections(data)
    if not s:
        return {}
    info = {}
    if ".rodata.eadk_app_name" in s:
        _, off, size = s[".rodata.eadk_app_name"]
        info["appName"] = data[off:off + size].split(b"\0")[0].decode("utf-8", "replace")
    if ".rodata.eadk_api_level" in s:
        _, off, _ = s[".rodata.eadk_api_level"]
        info["apiLevel"] = struct.unpack_from("<I", data, off)[0]
    info["flash"] = sum(v[2] for k, v in s.items() if v[0] == 1 and k.startswith((".text", ".rodata", ".data")))
    info["ram"] = sum(v[2] for k, v in s.items() if v[0] == 8 or (v[0] == 1 and k.startswith(".data")))
    if ".rodata.eadk_app_icon" in s:
        _, off, size = s[".rodata.eadk_app_icon"]
        try:
            pix = lz4_block(data[off:off + size], 55 * 56 * 2)
            if len(pix) == 55 * 56 * 2:
                rgb = bytearray()
                for k in range(0, len(pix), 2):
                    c = pix[k] | pix[k + 1] << 8
                    r, g, b = c >> 11, (c >> 5) & 63, c & 31
                    rgb += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
                COVERS.mkdir(parents=True, exist_ok=True)
                (COVERS / f"{cover_id}.png").write_bytes(png(55, 56, bytes(rgb)))
                info["cover"] = f"data/covers/{cover_id}.png"
        except (IndexError, ValueError):
            pass
    return info


def size_label(n):
    return f"{n / 1048576:.1f} Mo".replace(".", ",") if n >= 1048576 else f"{max(1, round(n / 1024))} Ko"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = []
    catalog_app_names = []
    for f in sorted(GAMES.iterdir(), key=lambda x: x.name.lower()):
        if not f.is_file() or f.name.startswith(".") or f.suffix.lower() != ".nwa":
            continue
        stem = clean_stem(f.stem)
        ext = f.suffix.lower()
        gid = re.sub(r"[^a-z0-9]+", "-", stem.lower()).strip("-")
        cat = ("Puzzle" if stem in PUZZLE else "Aventure" if stem in ADVENTURE else "Arcade" if stem in ARCADE
               else "Stratégie" if stem in STRATEGY else "Classique")
        entry = {
            "id": gid, "title": FRIENDLY.get(stem, stem.replace("_", " ")), "filename": f.name,
            "downloadName": stem + ext, "path": "../library/games/" + f.name, "format": ext[1:].upper(),
            "category": cat, "size": f.stat().st_size, "sizeLabel": size_label(f.stat().st_size),
            "kind": "ROM Game Boy" if ext == ".gb" else "Application NumWorks",
        }
        info = nwa_info(f, gid)
        entry.update(info)
        app_name = info.get("appName", "").strip()
        if app_name and app_name not in catalog_app_names:
            catalog_app_names.append(app_name)
        manifest.append(entry)
    (OUT / "games.js").write_text("window.STI2D_GAMES = " + json.dumps(manifest, ensure_ascii=False, indent=1) + ";\n")
    # The integrated Userland uses Epsilon's installed-app registry. Generate the exact
    # .rodata.eadk_app_name values from the packaged NWA files so Games can distinguish
    # bundled titles from user-injected apps without relying on fragile name heuristics.
    header = ROOT / "firmware/sti2d-play-source-v1/launcher/src/game_catalog_names.h"
    def c_string(value):
        return json.dumps(value, ensure_ascii=True)
    lines = ["/* Generated by scripts/rebuild_manifests.py; do not edit by hand. */",
             "#ifndef STI2D_GAME_CATALOG_NAMES_H", "#define STI2D_GAME_CATALOG_NAMES_H", "",
             "static const char *const STI_CATALOG_GAME_NAMES[] = {"]
    lines += ["  " + c_string(name) + "," for name in sorted(catalog_app_names, key=str.casefold)]
    lines += ["};", "#define STI_CATALOG_GAME_COUNT ((int)(sizeof(STI_CATALOG_GAME_NAMES) / sizeof(STI_CATALOG_GAME_NAMES[0])))",
              "#endif", ""]
    header.write_text("\n".join(lines), encoding="utf-8")
    ais = []
    titles = {"Aelyx.IA": ("Aelyx IA", "Assistant généraliste pour réviser et poser des questions."),
              "Matrices-v4": ("Matrices V4", "L'IA STI2D : fiches de cours, formules et méthodes."),
              "STMG_IA": ("STMG IA", "Assistant léger orienté filière STMG.")}
    for f in sorted(AI.iterdir()):
        if f.is_file() and f.suffix.lower() == ".nwa":
            title, desc = titles.get(f.stem, (f.stem, ""))
            aid = "ai-" + re.sub(r"[^a-z0-9]+", "-", f.stem.lower()).strip("-")
            entry = {"id": aid, "title": title, "description": desc, "filename": f.name, "downloadName": f.name,
                     "path": "../library/ai/" + f.name, "size": f.stat().st_size, "sizeLabel": size_label(f.stat().st_size)}
            entry.update(nwa_info(f, aid))
            ais.append(entry)
    (OUT / "ai.js").write_text("window.STI2D_AI = " + json.dumps(ais, ensure_ascii=False, indent=1) + ";\n")
    covers = sum(1 for e in manifest + ais if "cover" in e)
    print(f"{len(manifest)} NWA game records, {len(ais)} AI records, {covers} covers extracted; {len(catalog_app_names)} game IDs exported to C.")


if __name__ == "__main__":
    main()
