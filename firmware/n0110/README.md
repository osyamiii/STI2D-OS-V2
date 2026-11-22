# N0110 — STI2D-OS

- Processeur 216 MHz (environ 2,5 fois plus lent que la N0120). Logiciels possibles : Epsilon ≥ 23.2, ou Upsilon (versions de juin 2026 ou plus récentes, qui lancent les `.nwa`).
- Build : `make n0110` → `release/n0110/STI2D-OS.nwa` (`-DSTI_TARGET=110`, animations coupées par défaut, réactivables dans le Centre de contrôle).
- Vérifié à la compilation : édition de liens finale pour Epsilon (148 928 o de RAM) **et** pour Upsilon (107 674 o : le launcher tient ; un jeu qui demande plus l'indique sur sa carte).
- Installation : my.numworks.com/apps (Epsilon) ou `npx nwlink install-nwa STI2D-OS.nwa` (Epsilon ou Upsilon ; sur Upsilon le site officiel affiche 0 Ko libre).

**Firmware complet personnalisé** : possible seulement si la calculatrice est déverrouillée (selon la version d'Epsilon installée, beaucoup ne le sont pas). Ce cas demande l'arbre source d'un firmware tiers et n'est pas produit dans ce projet ; STI2D-OS fonctionne sans.
