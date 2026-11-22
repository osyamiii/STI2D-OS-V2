# Rapport de fusion — STI2D-OS Userland + portail

**Date de cette passe : 21/11/2026**  
**Base de travail :** archive `STI2D-OS-V2-by claude code.zip`, conservée intacte à côté de l'arbre de travail.

## 1. Résultat de l'audit et de la fusion

Le dépôt contient bien un arbre de sources Epsilon et des modifications d'intégration du shell STI2D dans l'environnement Userland. Le point d'entrée spécifique à STI2D remplace l'accueil habituel lorsque le Userland est actif. Les applications EADK `.nwa` restent des applications externes préinstallées ou ajoutées ; elles ne constituent pas le Userland lui-même.

Cette passe a fusionné et corrigé les éléments suivants :

- **Portail :** ajout des dix choix visuels — STI2D-OS, STI2D Classic, Nintendo 3DS, PlayStation 5, iOS, Windows, Xbox, Linux/GNOME, Android et macOS. Les neuf environnements autres que STI2D-OS sont des aperçus web, pas des systèmes installables sur la calculatrice.
- **Catalogue :** manifestes ramenés à 80 jeux `.nwa`, trois applications IA séparées (Aelyx IA, Matrices V4 et STMG IA), retrait de la ROM `.gb` de la bibliothèque distribuée.
- **Classement dans le Userland :** `Games` contient les jeux fournis ; `Favorites` est centré sur les jeux ; `Intelligences` expose les trois applications IA ; `My Apps` est réservé aux applications personnelles importées et exclut les jeux intégrés et les trois IA.
- **Accueil WatchOS-inspiré :** conservation de la composition V1 en disposition 4-3-4, des icônes rondes et de l'effet de grossissement ; ajout de la tuile `Fichiers` sans reconstruire l'accueil en simple nid d'abeille générique. Les crédits restent accessibles depuis STI2D Classic.
- **Double appui sur π :** conservation du comportement dédié depuis l'accueil ; l'appui simple et les autres contextes doivent garder leur comportement normal.
- **Faux mode examen de STI2D Classic :** écran de démonstration explicitement marqué comme simulation, sans activer le mode examen officiel ni imposer ses restrictions. Le pont LED a été ajouté au code hôte ; il n'a pas été vérifié sur une calculatrice physique pendant cette passe.
- **Lecteur Fichiers :** ajout d'une application intégrée au shell qui parcourt et lit les documents STI2D exportés en TXT/Markdown/CSV via scripts `.py` de commentaires. Les documents ne sont pas exécutés. Le format exporté est limité à 16 Ko et le lecteur affiche les lignes compatibles ; aucun moteur PDF complet n'est fourni.
- **PIN :** le portail impose exactement quatre chiffres, demande une confirmation et génère un fichier de configuration à usage unique avec sels et empreintes, pas de PIN en clair. Le code de récupération 12 chiffres est affiché une seule fois. Les limites de sécurité sont explicitées dans `docs/SECURITY.md`.
- **Messagerie :** relais réel et compagnon PC livrés ; deux comptes PC peuvent échanger des messages en direct et retrouver leur boîte de réception hors ligne. Le pont calculatrice attend le canal USB applicatif Epsilon, absent du firmware actuel.
- **Stockage :** la documentation distingue limite d'upload 6 MiB, région Flash pour applications, stockage de fichiers, RAM et heap. La zone expérimentale de 6 MiB qui compromet le slot de secours n'est pas activée.
- **Documentation :** `README.md`, `BUILD.md`, `docs/ARCHITECTURE.md`, `docs/STORAGE-AND-MESSAGING.md` et `docs/SECURITY.md` sont alignés sur la structure réelle du projet et ses limites.

## 2. Vérifications exécutées dans cette passe

| Vérification | Résultat |
|---|---:|
| Tests hôte C avec ASan/UBSan | 140/140 réussis |
| Tests C en mode Userland avec ASan/UBSan | 31/31 réussis |
| Contrôle syntaxique JavaScript (`node --check`) | réussi |
| Validation du bundle et des manifestes | 0 échec |
| Tests de messagerie PC (API, WebSocket, protocole) | 9/9 réussis |
| Essai local de bout en bout (2 comptes, WebSocket + boîte de réception) | réussi |
| Validation JSON et compilation syntaxique Python des scripts de validation | réussies |
| Test visuel du lecteur Fichiers dans le simulateur hôte | capture générée et vérifiée |

Les tests C valident les modules et le shell sur hôte avec les API EADK simulées. Ce n'est pas une compilation croisée du firmware Epsilon complet.

## 3. Ce qui n'a PAS été validé dans cette passe

- **Compilation ARM finale :** l'outil `arm-none-eabi-gcc` n'est pas installé dans l'environnement de travail. Les sources modifiées et le Userland doivent encore être reconstruits avec la toolchain ARM compatible.
- **Binaires précompilés :** les fichiers DFU/ELF déjà présents dans l'archive proviennent de la version Claude avant cette fusion. Ils n'intègrent donc pas nécessairement les nouveaux changements (notamment l'application Fichiers) et ne doivent pas être présentés comme des builds finaux de cette passe.
- **Matériel physique :** aucun test N0110, N0115 ou N0120 n'a été effectué pendant cette passe. Le comportement réel de la LED, la stabilité après démarrage et la compatibilité de chaque révision restent à vérifier.
- **Messagerie calculatrice :** pas encore de port USB applicatif ni d'écran Messages relié au transport. Le relais PC-à-PC et le compagnon sont disponibles dans `messaging/`.
- **PDF :** pas de lecteur PDF complet ; le lecteur actuel traite les documents texte exportés par le portail.
- **Capacité de 6 Mo :** non démontrée et non activée. La limite de 6 MiB trouvée dans le serveur est une limite de transfert, pas une capacité de stockage disponible sur NumWorks.

## 4. Suites nécessaires avant une livraison installable définitive

1. Installer/utiliser la toolchain ARM attendue et reconstruire Epsilon/Userland pour chaque modèle et version Epsilon prise en charge.
2. Vérifier les symboles, la taille réelle de la zone d'applications, le slot actif/inactif et le contenu de chaque image DFU générée.
3. Comparer les nouveaux artefacts avec les binaires précompilés de l'archive, puis ne distribuer comme versions finales que les artefacts reconstruits et validés.
4. Effectuer les tests sur du matériel N0120, puis N0110/N0115 séparément : démarrage, navigation, double π, LED de démonstration, stockage, PIN et retour au système de secours.
5. Ajouter le transport USB applicatif Epsilon, relier l'écran Messages du Userland, puis tester la messagerie avec deux calculatrices et deux utilisateurs distincts.

## 5. Rapport historique de Claude

Les rapports de compilation et de flashage présents dans l'archive d'origine sont conservés comme historique du projet. Ils ne constituent pas une preuve que les sources modifiées pendant la fusion ont été recompilées ou testées sur matériel. Seuls les résultats explicitement listés à la section 2 ont été reproduits dans cette passe.
