# Journal de fusion — version de travail STI2D-OS

Cette archive fusionne les sources de l'archive Claude Code et les améliorations apportées pendant l'audit.

## Changements

- Portail : 10 environnements sélectionnables, avec aperçus web distincts.
- Catalogue : 80 jeux NumWorks `.nwa`, aucune ROM `.gb`, trois IA conservées séparément.
- Userland : séparation des listes `Games`, `Favorites`, `Intelligences` et `My Apps`.
- Accueil : identité V1 conservée (grille 4-3-4, icônes rondes et effet de grossissement), tuile Fichiers ajoutée.
- Navigation : double appui sur π depuis l'accueil vers STI2D Classic.
- STI2D Classic : démonstration du mode examen, sans activer le mode examen officiel.
- PIN : choix sur le portail, quatre chiffres exactement, fichier à usage unique avec sels et empreintes, code de récupération affiché une fois.
- Application Fichiers : lecture de documents texte préparés par le portail, sans exécuter leur contenu.
- Documentation : architecture Userland/Epsilon, limites de stockage, sécurité PIN, état de la messagerie et limites de validation actualisés.

## Vérifications

- Tests hôte : 140/140 réussis (ASan/UBSan).
- Tests Userland hôte : 31/31 réussis (ASan/UBSan).
- Validation bundle/manifeste : 0 échec.
- Syntaxe JavaScript : validée par `node --check`.

## Limites connues

- Cette passe n'a pas reconstruit les images ARM/DFU finales : `arm-none-eabi-gcc` n'est pas disponible dans l'environnement utilisé. Les images précompilées conservées dans l'archive sont celles fournies par la version Claude d'origine et ne doivent pas être considérées comme intégrant les toutes dernières modifications.
- Aucun test physique n'a été effectué pendant cette passe.
- À cette étape historique, la messagerie n'était pas encore implémentée ; voir l'ajout ultérieur « Messagerie PC » ci-dessous.
- Le lecteur traite les documents texte ; aucun moteur PDF complet n'est inclus.
- Les 6 MiB constituent une limite d'upload pour le serveur, pas une capacité de stockage garantie sur la calculatrice. Le mode étendu qui empiète sur le slot de secours reste désactivé.


## Finalisation technique — 21/11/2026

- Ajout de `tools/pdf_to_stidoc.py` : extraction de texte PDF côté ordinateur et export paginé au format de commentaires `STI2D-DOC-V1`, avec limite de 16 Kio et sélection de pages.
- Ajout de `docs/PDF-IMPORT.md` et `requirements-pdf.txt`, et mise à jour de la page Lecteur de fichiers du portail.
- Ajout de tests du convertisseur et intégration à `make bundle-test`.
- Les tests hôte couvrent maintenant les suites shell et Userland. Les en-têtes EADK du dépôt suffisent pour les tests hôte, sans téléchargement de `nwlink`.
- Limites confirmées : compilation ARM non faite (toolchain absente) ; messagerie calculatrice-à-calculatrice non implémentée (aucun endpoint USB applicatif dans le pilote actuel) ; tests matériels non réalisés. La zone expérimentale 6 MiB reste désactivée pour préserver le slot de secours.
- Rapport détaillé : `docs/FINALIZATION-STATUS.md`.


## Messagerie PC — ajout ultérieur

- Relais FastAPI/SQLite avec inscription sur invitation, mots de passe PBKDF2, jetons opaques, messages persistants 30 jours, WebSocket et limitation de débit.
- Compagnon PC multi-OS : comptes, envoi, réception en direct, boîte de réception et adaptateur série v1.
- Protocole série documenté et 9 tests automatisés.
- Limite conservée : le firmware NumWorks actuel n'expose pas de port USB CDC applicatif ; la liaison calculatrice ↔ compagnon n'est pas encore opérationnelle.

## Protection propriétaire — 21 novembre 2026
- Ajout d'une licence propriétaire « Tous droits réservés » pour les contributions originales STI2D-OS, avec titulaires/contacts Yamii (`cjnw`) et Derim (`irationnel`) et Discord officiel `discord.gg/sti2d`.
- Ajout d'une page publique `web/LEGAL.html`, d'une mention copyright dans le portail et de notices dans les sources web originales.
- Renforcement du serveur de développement : liste blanche des ressources publiques, blocage des dossiers de sources firmware/Epsilon, docs, tests, configuration et messagerie, désactivation de l'indexation des répertoires et ajout d'en-têtes HTTP de sécurité.
- Documentation explicite : les ressources frontend envoyées au navigateur restent inspectables via F12 ; aucune désactivation de raccourci ne peut empêcher techniquement leur copie. Les secrets et traitements sensibles doivent rester côté serveur.
- Les composants tiers (Epsilon/NumWorks, jeux, bibliothèques et ressources) conservent leurs propres licences ; aucune revendication de propriété n'est faite sur ces éléments.
