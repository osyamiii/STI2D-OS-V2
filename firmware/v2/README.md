# STI2D-OS V2 — couche firmware

Compilée **dans** le launcher STI2D-Play (pas en bibliothèque isolée) par `../sti2d-play-source-v1/Makefile`.

| Fichier | Rôle |
|---|---|
| `include/sti2d_v2.h` | API publique (préférences, date, PIN, crypto, double π, écrans) |
| `include/sti_v2_ui.h` | kit d'interface interne |
| `src/config.c` | préférences `sti2dos.py`, interface active, date et heure |
| `src/pin.c`, `src/crypto.c` | PIN (PBKDF2-HMAC-SHA256, récupération, attente croissante) ; SHA-256, HMAC, PBKDF2 |
| `src/gfx_adapt.c` | adaptation au moteur graphique du launcher, clavier, éditeur alphanumérique, fond d'écran |
| `src/home_os.c` | accueil STI2D-OS en nid d'abeilles |
| `src/input_pi.c` | détecteur du double π (accueils seulement) |
| `src/control_center.c` | Centre de contrôle et informations système |
| `src/pin_screen.c` | écran de verrouillage, création/modification/désactivation, récupération |
| `src/clock.c` | heure réglable, chronomètre, minuteur avec alerte visuelle |
| `src/terminal.c` | interpréteur de diagnostic |
| `src/gallery.c`, `src/stimg.c` | Galerie, images `stimg_*.py`, fond d'écran |
| `src/calendar.c` | mois, événements `stical.py`, événements à venir |

## Tests

```bash
make test        # hôte : le vrai launcher + toute la couche V2, clavier scripté, captures PNG
make arm-check   # chaque module pour N0110/N0115/N0120 avec -Werror
python3 test/emu_scenarios.py ../sti2d-play-source-v1 auto build/emu   # binaires ARM dans l'émulateur
```

Le banc hôte (`test/host/`) remplace EADK par un clavier scripté sur une horloge virtuelle, un écran 320 × 240 capturé et un système de fichiers en mémoire limité à 42 Ko.
