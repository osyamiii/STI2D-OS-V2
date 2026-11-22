# N0120 — userland STI2D-OS sur Epsilon

- Processeur 550 MHz. Logiciel : **Epsilon officiel**, conservé (noyau et système).
- STI2D-OS est une **app EADK** installée dans l'espace des apps : il ne remplace pas le noyau et n'en a pas besoin.
- Build : `make n0120` → `release/n0120/STI2D-OS.nwa` (`-DSTI_TARGET=120`, animations actives par défaut).
- Vérifié à la compilation : édition de liens finale de l'installateur, 148 928 o de RAM (Epsilon 23.2), espace apps 1,94 Mo.
- Installation : my.numworks.com/apps (Chrome/Edge sur ordinateur) ou `npx nwlink install-nwa STI2D-OS.nwa`.

Limites : pas de mode examen avec une app non officielle ; un reset efface les fichiers non Python (dont le PIN) ; pas d'horloge sauvegardée accessible aux apps.

L'installateur d'origine (`installer/`) est un autre chemin, réservé à la N0120 : il écrit son propre système « STI2D OS » par DFU. Il est conservé tel quel.
