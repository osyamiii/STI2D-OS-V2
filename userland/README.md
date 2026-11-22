# STI2D-OS — userland Epsilon

Le **système** STI2D-OS : un userland Epsilon 26.x (le système des apps NumWorks) dont l'accueil est le shell STI2D-OS. Ce n'est pas une app `.nwa`.

## Ce qui se passe au démarrage

1. Le noyau officiel (slot A ou B) lance ce userland (écrit dans l'autre slot par `installer/`).
2. `AppsContainer::switchToBuiltinApp(home)` lance le **shell STI2D-OS** (`epsilon/apps/sti2d/host.cpp`) au lieu de la grille d'icônes.
3. Le shell (C, `firmware/sti2d-play-source-v1/launcher` + `firmware/v2`, compilés avec `STI_USERLAND=1`) reçoit la RAM des apps externes comme arène, tourne avec Home retenu, et rend ce que l'utilisateur ouvre :
   - une app NumWorks (Calculs, Grapheur, Python… via la tuile **Calculatrice**, ou Paramètres via le Centre de contrôle) ;
   - une app installée (Games = jeux inclus, Favorites = favoris de jeux, Intelligences = Aelyx IA / Matrices V4 / STMG IA, My Apps = uniquement les apps ajoutées personnellement) ;
   - la grille NumWorks d'origine (dernière ligne de Calculatrice).
4. Home ou Back dans une app NumWorks revient à l'accueil STI2D-OS.

Le shell surveille aussi ce que la boucle d'Epsilon ferait : On/Off (veille, puis écran PIN), câble USB (écran « calculatrice connectée » pour l'installateur), batterie vide, inactivité (assombrissement 30 s, veille 5 min). Avec un PIN, le réveil depuis n'importe quelle app ramène l'écran de verrouillage. En mode examen, l'accueil officiel revient (le shell est désactivé).

## Fichiers

| | |
|---|---|
| `epsilon-src/` | arbre Epsilon (`EPSILON_COMMIT`) modifié |
| `epsilon-src/epsilon/apps/sti2d/` | hôte du shell (`host.cpp`), Makefile, liens vers `firmware/` |
| `sti2d-os-userland.patch` | les modifications d'Epsilon hors `apps/sti2d/` |
| `build.sh` | compilation N0110 / N0115 / N0120, slots A et B |
| `firmware/<modèle>/` | binaires prêts (`.dfu` à flasher, `.elf` sans débogage, pour l'installateur) |
| `installer/` | installateur local (page web + ligne de commande) |
| `tools/link_apps.py` | préinstallation des `.nwa` dans la zone d'apps du slot, bornes extraites de l'ELF |
| `tools/userland_emu.py` | démarrage du userland dans Unicorn (s'arrête au FPU double précision, non émulé) |

## Mémoire (mesurée sur les binaires)

| Modèle | Userland précompilé présent | RAM du shell (arène) | Zone d'apps |
|---|---:|---:|---|
| N0110 / N0115 | 1,63 Mo | 147 704 o | calculée depuis l'ELF et la configuration Epsilon |
| N0120 | 1,57 Mo | 147 704 o | calculée depuis l'ELF et la configuration Epsilon |

La taille de zone d'apps varie selon le modèle et la version Epsilon. L'installateur relit les bornes Flash depuis les symboles de l'ELF ; c'est cette valeur calculée qui fait foi, pas une valeur générique de 6 Mo. La limite d'upload de 6 Mo du serveur local n'est pas une capacité de stockage.

Une option XL de 6 MiB existe dans des outils NumPlay, mais elle utilise l'espace normalement réservé à l'autre copie du logiciel. Elle n'est pas activée dans l'installateur standard STI2D-OS. Voir `docs/STORAGE-AND-MESSAGING.md`.

## Limites

- Reset (au dos) revient au logiciel officiel ; l'installateur relance STI2D-OS (comme PSP-OS).
- Les fichiers du userland officiel (scripts Python) ne sont pas ceux de STI2D-OS : sauvegarde-les avant. Les PDF textuels sont convertis en scripts document par `tools/pdf_to_stidoc.py` ; voir `docs/PDF-IMPORT.md`.
- La version du userland doit égaler celle du noyau officiel ; elle n'est pas lisible en DFU, l'installateur l'affiche « ? » et utilise `APP_VERSION` (26.4.0 par défaut).
- N0110 / N0115 : des images précompilées sont présentes dans l'archive. La compilation croisée et le test matériel n'ont pas été refaits dans l'audit de fusion actuel.
