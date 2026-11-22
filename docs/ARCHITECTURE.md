# Architecture — STI2D-OS V2.2 fusion

## 1. Principe général

Le produit principal est **un Userland intégré aux sources d'Epsilon**, pas une application EADK autonome. Le noyau et le bootloader officiels ne sont pas remplacés par le shell STI2D : Epsilon démarre le Userland installé dans le slot inactif, et le code STI2D remplace l'écran d'accueil habituel tant que le mode examen officiel n'est pas actif.

Les trois éléments doivent rester distincts :

- **Epsilon/Userland** : environnement utilisateur intégré au firmware Epsilon ; `userland/epsilon-src/` contient l'arbre de sources et le patch d'intégration.
- **Applications externes `.nwa`** : programmes EADK relocalisables préinstallés dans la zone prévue par le Userland. Ce ne sont pas le Userland lui-même.
- **Portail web** : interface de configuration, de choix des aperçus, d'import/export et de téléchargement. Les neuf environnements alternatifs ne sont pas des firmwares NumWorks.

Epsilon documente ses briques principales comme Ion (matériel), Kandinsky (graphisme), Poincaré (mathématiques), Escher (interface) et Apps (applications). EADK sert à produire des applications externes : ne pas confondre ces deux niveaux. <https://github.com/numworks/epsilon> <https://github.com/numworks/epsilon/tree/master/epsilon/eadk>

## 2. Intégration du Userland dans Epsilon

Sources principales :

- `userland/epsilon-src/` : sources Epsilon du commit enregistré dans `userland/EPSILON_COMMIT`.
- `userland/sti2d-os-userland.patch` : modifications apportées à Epsilon hors du répertoire dédié STI2D.
- `userland/epsilon-src/epsilon/apps/sti2d/host.cpp` : hôte C++ qui exécute le shell C dans l'environnement Epsilon.
- `userland/epsilon-src/epsilon/apps/sti2d/` : Makefile, en-têtes et liens vers le launcher et la couche V2.
- `firmware/sti2d-play-source-v1/launcher/` et `firmware/v2/` : interface, gestion des applications et modules du shell, compilés avec `STI_USERLAND=1`.

L'intégration s'appuie notamment sur le point de retour à l'écran d'accueil d'Epsilon (`AppsContainer::switchToBuiltinApp(home)`). L'hôte STI2D démarre à la place de la grille d'applications lorsque le Userland est disponible. Le mode examen **officiel** d'Epsilon reste prioritaire : lorsque celui-ci est actif, le shell STI2D ne prend pas la main.

Le bootloader n'est pas modifié par cette fusion. L'installateur écrit dans le slot inactif ; il vérifie le modèle, le slot, la version et la relecture des données. Ne pas flasher les sources internes du bootloader ni forcer une version officielle différente.

## 3. Environnements et navigation

### Userland NumWorks

- Accueil STI2D-OS : disposition V1 4-3-4 préservée, icônes rondes, effet de grossissement de la sélection, transitions adaptées à 320 × 240.
- Double appui sur π depuis un écran d'accueil uniquement : passage à STI2D Classic. Un appui simple reste neutre sur l'accueil ; ailleurs, π garde sa fonction normale.
- STI2D Classic : carrousel V1 conservé. Le faux mode examen est une démonstration visuelle, ne déclenche pas le mode examen officiel et ne bloque aucune application. La LED n'est sollicitée que pour la démonstration, avec remise en état à la sortie.

### Portail web

Dix aperçus sont disponibles : STI2D-OS, STI2D Classic, Nintendo 3DS, PlayStation 5, iOS, Windows, Xbox, Linux/GNOME, Android et macOS. La sélection est une préférence du navigateur. Seul STI2D-OS est le Userland cible de la calculatrice ; les autres aperçus ne sont pas installables.

## 4. Catalogue et classement

Les manifestes séparent trois ensembles :

- `Games` : 80 jeux NumWorks au format `.nwa` ; aucune ROM `.gb` n'est incluse dans le catalogue distribué.
- `Intelligences` : Aelyx IA, Matrices V4 et STMG IA, proposées séparément.
- `My Apps` : uniquement les applications `.nwa` ajoutées personnellement par l'utilisateur ; les jeux inclus et les trois IA sont exclus de cette liste.

Les favoris sont calculés à partir des jeux, pas à partir des apps personnelles ou des IA. Les `.nwa` importés dans le navigateur restent dans IndexedDB et ne sont pas installés automatiquement sur la calculatrice.

## 5. Applications intégrées au shell

- Calculatrice : accès aux applications NumWorks d'Epsilon.
- Games, Favorites, Intelligences et My Apps : vues distinctes du catalogue d'applications préinstallées.
- Clock : horloge réglable, chronomètre et minuteur. L'heure ne survit pas à un arrêt complet car le compteur accessible aux apps repart au démarrage.
- Calendar : calendrier et événements persistants.
- Gallery : images exportées depuis le portail, aperçu, suppression et fond d'écran.
- Files : lecteur léger TXT/Markdown/CSV via scripts `stidoc_*.py` générés par le portail ; les PDF textuels peuvent être convertis sur ordinateur avec `tools/pdf_to_stidoc.py`. Le lecteur n'exécute pas le document ; il affiche seulement les lignes commentées. Taille limitée à 16 Kio par document exporté.
- Terminal : diagnostics locaux STI2D, ce n'est pas un shell Linux.
- Control Center : thèmes, couleurs, animations, fond d'écran, PIN et informations système.

Le rendu PDF graphique n'est pas embarqué. Le texte des PDF est extrait sur ordinateur puis converti au format commentaire `STI2D-DOC-V1` ; voir `docs/PDF-IMPORT.md`. Les images et la mise en page PDF ne sont pas conservées.

## 6. Stockage et mémoire

Ne pas confondre :

1. taille du binaire Userland dans le slot firmware ;
2. région Flash des applications `.nwa` ;
3. stockage de fichiers Python (limité par Epsilon) ;
4. RAM/heap du shell ;
5. limite d'upload d'un fichier depuis le portail.

Le serveur d'installation fixe `MAX_UPLOAD = 6 MiB` pour un fichier `.nwa`. **Cette limite d'envoi n'est pas une capacité de stockage de la calculatrice.** Le programme de liaison lit la zone réelle depuis les symboles de l'ELF du Userland (`userland/tools/link_apps.py::area`) et refuse d'écrire au-delà de cette zone.

La configuration standard réserve l'espace dans le slot courant. Le Makefile de NumPlay documente une zone de 2 031 616 octets dans la configuration de base et une variante N0120/Epsilon 26.4 démarrant plus tôt. La capacité réelle doit être calculée pour le binaire et le modèle utilisés ; elle ne doit pas être remplacée par un compteur fictif.

Une option expérimentale `XL_APP_SPACE = 6 291 456` prolonge la zone jusqu'à la fin de la Flash externe (`0x90800000`) et chevauche l'espace normalement utilisé par l'autre copie du logiciel. Ce n'est **pas** l'option standard de l'installateur STI2D, et elle n'est pas activée par cette fusion : sacrifier la copie de secours/récupération n'est pas acceptable par défaut. La Flash externe est structurée en deux slots ; cette disposition et ses risques doivent être compris avant toute expérimentation. <https://guide.getomega.dev/fr/docs/how-does-it-work/> <https://www-fourier.univ-grenoble-alpes.fr/~parisse/numworks/khicasnw.html>

Le portail et l'installateur doivent toujours afficher les capacités mesurées ou calculées, jamais promettre 6 Mo de capacité standard. La mémoire de fichiers Python reste distincte de la zone Flash des apps.

## 7. Import de documents et d'applications

- `.nwa` : le portail vérifie le type ELF ARM32 et garde le fichier importé dans IndexedDB ; l'installation réelle passe par le flux installateur/DFU approprié.
- Images : le portail les convertit au format compact STI2D puis exporte un script Python `stimg_*.py`.
- Documents texte : le portail exporte un script Python `stidoc_*.py` ne contenant que des commentaires `#|`. L'app Files ignore tout contenu non conforme à ce format et n'exécute jamais le script.
- PIN : le portail génère un fichier d'installation à usage unique avec sels et empreintes ; il ne l'intègre pas aux profils partageables.

## 8. Messagerie

Il n'existe pas encore de messagerie entre utilisateurs. L'écran USB actuel détecte la connexion à l'ordinateur et sert le flux d'installation ; il ne constitue pas un canal de chat bidirectionnel. La conception réaliste est documentée dans `docs/STORAGE-AND-MESSAGING.md` : application compagnon PC, protocole USB spécifique au Userland, puis relais réseau authentifié. Aucun bot ni message fictif n'est utilisé.

## 9. Compatibilité et validation

Le dépôt contient les sources Epsilon, une configuration de build et des images Userland précompilées N0110/N0115/N0120. Leur présence n'implique pas que chaque cible a été reconstruite dans l'environnement d'audit actuel. Les tests hôte et Userland compilent le shell C réel avec ASan/UBSan ; ils ne remplacent ni la compilation croisée complète du firmware Epsilon ni les essais sur calculatrice physique.
