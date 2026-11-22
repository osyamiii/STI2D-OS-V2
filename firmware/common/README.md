# Socle commun aux trois modèles

- `../sti2d-play-source-v1/launcher/src/` : launcher STI2D-Play (rendu, stockage, jeux, STI2D Classic).
- `../v2/` : couche STI2D-OS V2.

Le Makefile principal compile `launcher/src/*.c` **et** `../v2/src/*.c` dans le même binaire (`LAUNCHER_SRC`). Les différences entre modèles passent par `-DSTI_TARGET` et par les vérifications de RAM, pas par des sources séparées.
