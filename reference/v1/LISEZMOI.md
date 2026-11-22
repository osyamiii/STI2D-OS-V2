# STI2D OS

STI2D OS est un système pour calculatrice **NumWorks N0120**, développé par
**Yamii et Derim**. Il reprend l'interface de l'Apple Watch : cadran de montre,
écran d'accueil en nid d'abeille avec effet loupe, icônes rondes et animations
fluides. Toutes les applications de la NumWorks sont là, avec **STI2D-Play**
préinstallé.

Serveurs Discord : **discord.gg/sti2d** et **discord.gg/derim**

## Installation

1. Ouvre `Installer-STI2D-OS.html` dans **Chrome** ou **Edge** sur un ordinateur.
   Ferme d'abord les onglets my.numworks.com.
2. Mets la calculatrice à jour sur <https://my.numworks.com/devices/upgrade>.
3. Branche la calculatrice : elle affiche « La calculatrice est connectée ».
4. Dans l'installateur :
   * **coche les jeux** de STI2D-Play à préinstaller. La barre du haut montre
     en direct la place utilisée, et refuse un jeu quand il n'y a plus de
     place ;
   * choisis le **fond d'écran** : noir classique, une couleur unie, ou une
     image de **320 × 240 pixels exactement** (elle est compressée
     automatiquement, et toute autre taille est refusée avec un message) ;
   * vérifie la **version d'Epsilon** de ta calculatrice (Paramètres ›
     À propos › Version du logiciel, 26.4.0 par défaut) ;
   * clique sur **Installer STI2D OS** et choisis la calculatrice.
5. Au bout d'environ une minute, la calculatrice affiche « UNOFFICIAL SOFTWARE »,
   puis l'animation de démarrage de STI2D OS.

Si le navigateur refuse d'ouvrir la page en double-cliquant dessus, mets le
dossier sur un site (GitHub Pages, par exemple). Tu peux aussi lancer
`python -m http.server` dans le dossier et ouvrir <http://localhost:8000>.

### À savoir sur la N0120

* NumWorks n'autorise sur la N0120 qu'un système non officiel « par-dessus »
  son système officiel. Après un **reset** ou un **plantage**, la calculatrice
  revient au système officiel : il suffit de relancer l'installateur.
* Le **mode examen** n'est pas disponible tant que STI2D OS tourne.
* L'installation efface la plupart des données : sauvegarde tes scripts Python.
* Si la calculatrice affiche « OFFICIAL UPGRADE REQUIRED », la version saisie
  dans l'installateur ne correspond pas à celle de ta calculatrice : corrige-la
  et réinstalle.
* STI2D OS est construit sur le code source public d'Epsilon (26.1). Si une
  version officielle plus récente (26.4) a changé l'interface interne entre le
  noyau et le système, la calculatrice plante au démarrage de STI2D OS. Elle
  revient alors d'elle-même au système officiel, sans dégât.

## Commandes

| Écran | Touche | Action |
|---|---|---|
| Cadran | OK, EXE, flèches, Home | Afficher les applications |
| Applications | Flèches | Se déplacer dans le nid d'abeille |
| Applications | OK / EXE | Ouvrir l'application |
| Applications | 1 à 9 | Sélectionner une application (un second appui l'ouvre) |
| Applications | Home | Revenir à l'application du centre, puis au cadran |
| Applications | Back | Revenir au cadran |
| Dans une application | Home | Revenir au nid d'abeille |

Une touche pressée pendant une animation la termine aussitôt : on n'attend
jamais l'interface.

## L'app Personnaliser

Elle affiche le cadran en plein écran, comme l'édition d'un cadran sur
l'Apple Watch. **Haut/Bas** choisit le réglage, **Gauche/Droite** change la
valeur, et l'aperçu se met à jour en direct.

* **Cadran** : numérique, aiguilles et chiffres, aiguilles épurées ou
  modulaire.
* **Thème** : Classique, Halloween, Noël, Mario, Minecraft, Vice City, Manga
  ou Espace. Chaque thème a son décor et sa couleur.
* **Couleur** : la couleur d'accent de l'interface, celle du thème ou une
  couleur au choix parmi 9.
* **Fond d'écran** : l'image envoyée par le site, la couleur choisie sur le
  site, ou une des 7 couleurs proposées.

Les réglages restent tant que STI2D OS est installé, même calculatrice éteinte.

## L'app Messages

Discute avec 10 personnages, entièrement hors ligne :

| Personnage | Style |
|---|---|
| Jade | E-girl gameuse, un peu taquine |
| Mamadou | Le roi des blagues |
| Prof Lambert | Prof de maths en STI2D (formules, calculs) |
| Coach Karim | Sport et motivation |
| Mamie Ginette | Douceur, recettes et bons conseils |
| Kévin | Gamer et geek de la tech |
| Léa | Ta BFF, potins et musique |
| ST-42 | Robot sarcastique |
| Dr Nova | Scientifique, anecdotes « Le savais-tu ? » |
| Zorg | Extraterrestre perdu |

* **Haut/Bas** choisit un contact, **OK** ouvre la discussion.
* **Gauche/Droite** propose des réponses rapides, **OK** les envoie.
* Pour écrire toi-même, utilise **Alpha** (ou Shift + Alpha pour bloquer les
  lettres), puis **EXE**. **Effacer** corrige, et **Back** revient à la liste.
* Ils comprennent 57 sujets (cours, maths, STI2D, bac, jeux, sport, musique,
  amour, amis, famille, espace, code, IA, santé, fêtes…) avec plus de 700
  répliques. Ils calculent (« combien fait 12*7 »), donnent l'heure et la date,
  et retiennent ton prénom (« je m'appelle Lucas »).
* La bibliothèque est compressée dans le système (22 Ko). La conversation reste
  en mémoire vive, rien n'est écrit dans le stockage de la calculatrice.

## Paramètres ajoutés

* **Date et heure** : fuseau horaire (Paris par défaut, avec passage
  automatique heure d'été / heure d'hiver ; aussi Londres, New York, Montréal,
  Antilles, Guyane, La Réunion, Nouvelle-Calédonie, Tahiti, Casablanca, Alger
  et Dakar), réglage de l'heure et de la date.
* **Crédits**.
* **Langue** : français par défaut, ainsi qu'anglais, espagnol, allemand,
  portugais et italien.

### L'heure

Le noyau de la N0120 ne donne pas accès à l'horloge de la calculatrice. Pour
compenser, l'installateur écrit l'heure exacte dans STI2D OS au moment de
l'installation, et STI2D OS compte le temps à partir de là. Elle peut se
décaler si la calculatrice reste longtemps éteinte : il suffit alors de la
corriger dans Paramètres › Date et heure.

## Ajouter des applications

* Par défaut, **STI2D-Play** est préinstallé dans une version qui tient dans
  les 2,37 Mo d'apps disponibles. Elle contient Matrices IA, Asphalt Urban GT2, Rider,
  Backrooms '96, STI2D vs STMG, Balatro, Crossy Road, Block Blast, Suika Game,
  Tetris, Chrome Dino, Doodle Jump, Échecs, Flappy Bird, Pac-Man, Snake,
  Puissance 4, Solitaire, 2048, Démineur, Casse-briques, Space Invaders, Sudoku
  et Sokoban.
* Among Us (1,9 Mo à lui seul), NumDash, NumBlocks et Buckshot Roulette ne
  rentrent pas avec le reste.
* L'installateur te laisse choisir les jeux de STI2D-Play (même Among Us, en
  décochant d'autres jeux pour lui faire de la place).
* Pour installer tes propres applications `.nwa`, clique sur « Aucun jeu »
  dans l'installateur. Les 2,37 Mo restent alors libres pour
  <https://my.numworks.com/apps>. Elles apparaissent ensuite dans le nid
  d'abeille, après les applications NumWorks.

## Contenu du dossier

| Fichier | Contenu |
|---|---|
| `Installer-STI2D-OS.html` | L'installateur (tout est dedans) |
| `media/captures` | Captures d'écran en 960×720 |
| `media/animations-gif` | Les animations en GIF |
| `media/animations-mp4` | Les animations en MP4 (60 i/s), pour le montage vidéo |
| `sources/` | Les modifications du code source |

## Licences

* Code d'Epsilon : © NumWorks. NumWorks autorise la création de systèmes non
  officiels pour sa calculatrice.
* Police Inter : SIL Open Font License.
* STI2D-Play : voir ses propres licences (NumPlay de Mason Chen, GPLv3, avec
  autorisation, et les licences des jeux).
