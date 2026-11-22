# État de finalisation — 21 novembre 2026

Ce document distingue les modifications réellement ajoutées pendant cette passe des limites qui restent bloquantes. Il ne remplace pas les rapports historiques de fusion.

## Ajouts livrés dans cette archive

- `messaging/` : relais FastAPI/SQLite, compagnon PC, protocole série v1, déploiement Docker et tests d'échange PC à PC.
- `tools/pdf_to_stidoc.py` : extraction de texte PDF sur ordinateur, sélection de pages, génération du format commenté `STI2D-DOC-V1` utilisé par l'application Fichiers.
- `requirements-pdf.txt` et `docs/PDF-IMPORT.md` : dépendance et procédure d'utilisation.
- Page « Lecteur de fichiers » du portail : procédure PDF clairement indiquée, sans prétendre à un moteur PDF graphique embarqué.
- Tests du convertisseur intégrés à `make bundle-test`.
- Les tests hôte incluent maintenant les suites shell et Userland ; les en-têtes EADK locaux suffisent, sans télécharger `nwlink` pour ces tests.

## Vérifications exécutées dans cette passe

| Vérification | Résultat |
|---|---|
| `node --check web/assets/js/app.js` | réussi |
| Tests hôte du shell V2 | 140 réussis, 0 échec |
| Tests hôte du Userland | 31 réussis, 0 échec |
| Cross-check STIMG | 4 jeux d'images, pixels identiques |
| Validation du bundle | 0 échec |
| Tests unitaires du convertisseur PDF | 4 réussis, 0 échec |
| Tests messagerie (API, WebSocket, protocole compagnon) | 9 réussis, 0 échec |
| Essai PDF réel (PDF 2 pages généré localement) | texte extrait, 157 octets, toutes les lignes du script sont des commentaires |
| Compilation croisée ARM | non exécutée : `arm-none-eabi-gcc` et `arm-none-eabi-objcopy` absents |
| Tests matériels N0110/N0115/N0120 | non exécutés : aucun appareil physique disponible |

Les tests hôte ne sont pas une compilation ARM et ne valident pas le comportement matériel.

## Éléments qui ne peuvent pas être déclarés terminés

### Compilation firmware

Les sources fusionnées sont présentes, mais les DFU/ELF de `userland/firmware/` sont des artefacts préexistants. Ils ne sont pas reconstruits dans cette passe. La toolchain `arm-none-eabi-gcc` / `arm-none-eabi-objcopy` manque dans l'environnement. Les .nwa sous `release/` sont des applications autonomes EADK et ne remplacent pas les binaires Userland DFU.

Commande de reconstruction, à lancer dans un environnement équipé de la toolchain :

```bash
APP_VERSION=26.4.0 userland/build.sh n0110 n0115 n0120
```

`APP_VERSION` doit correspondre à la version officielle réellement installée sur chaque calculatrice. Les images générées doivent ensuite être contrôlées par modèle et testées sur matériel avant distribution comme firmwares finaux.

### Messagerie

Le relais réseau et le compagnon PC sont livrés dans `messaging/`. Les échanges réels entre deux comptes PC sont testés : inscription sur invitation, authentification, envoi, réception WebSocket, boîte de réception hors ligne et suppression. Les 9 tests dédiés passent.

La liaison calculatrice reste bloquée : l'USB Epsilon actuel expose le flux DFU et EP0, pas de port série USB/CDC applicatif. Le compagnon contient l'adaptateur série et le protocole v1, mais il ne peut pas communiquer avec une NumWorks tant que cette interface n'est pas ajoutée et compilée dans Epsilon. Aucun échange entre deux calculatrices n'a été validé sur matériel.

### PDF

L'import PDF est disponible sous forme d'extraction texte sur ordinateur. Ce n'est pas le rendu natif des pages PDF sur l'écran de la calculatrice. Les images, tableaux complexes et la mise en page ne sont pas conservés ; un PDF scanné demande une OCR séparée.

### Stockage « 6 Mo »

Aucune extension risquée n'a été activée. `MAX_UPLOAD = 6 MiB` est une limite de transfert côté serveur. L'installateur calcule la région d'applications à partir des symboles de l'ELF et refuse les dépassements. Le profil `XL_APP_SPACE` chevauche l'espace de l'autre copie du logiciel ; il reste désactivé pour préserver le slot de secours. La capacité installable doit être calculée pour le modèle, le firmware et le Userland effectivement utilisés.

## Statut honnête

Cette archive est une **livraison de sources améliorée et testée côté hôte**, pas un firmware final compilé et validé sur calculatrice. La messagerie PC-à-PC est réellement livrée ; la connexion USB de la calculatrice reste à intégrer. Les blocages majeurs restants pour une livraison système entièrement finale sont la reconstruction ARM, le canal USB applicatif et les essais matériels. Le chemin PDF texte est fourni et testé ; la zone de stockage reste volontairement protégée.
