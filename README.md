# STI2D-OS V2

Environnement personnalisé pour calculatrices NumWorks (N0110, N0115, N0120), basé sur l'écosystème Epsilon et STI2D-Play, avec son portail web compagnon.

## Démarrer

**Portail** (bibliothèque, import de `.nwa`, personnalisation, profils, installation) :

```bash
./start-server.sh        # macOS / Linux
start-server.bat         # Windows
```

Puis ouvre `http://localhost:8000/web/`. Le script affiche aussi l'adresse à ouvrir **depuis un téléphone** sur le même Wi-Fi. Ouvrir `web/index.html` en double-clic fonctionne sur ordinateur ; sur iPhone, un fichier ouvert depuis l'app Fichiers n'est qu'un aperçu sans style ni script : passe par le serveur.

**Calculatrice (système, recommandé)** : le userland STI2D-OS, `python3 userland/installer/server.py` (voir `userland/README.md`).

**Version app facultative** : `release/<modèle>/STI2D-OS.nwa` est un binaire EADK autonome de compatibilité. Il ne remplace pas le Userland intégré.

## Contenu

```
STI2D-OS-V2/
├── userland/                   sources Epsilon modifiées, hôte STI2D, installateur et images Userland
├── release/n0110|n0115|n0120/STI2D-OS.nwa   app EADK autonome facultative
├── firmware/
│   ├── sti2d-play-source-v1/   launcher STI2D-Play + Makefile principal + outils (émulateur ARM…)
│   ├── v2/                     couche STI2D-OS V2 + tests
│   └── n0110/ n0115/ n0120/ common/   notes par modèle
├── web/                        portail (index.html, assets/, data/)
├── library/games/              80 jeux NumWorks `.nwa` (pas de ROM `.gb`)
├── library/ai/                 Aelyx IA, Matrices V4, STMG IA
├── installer/                  installateur DFU/WebUSB d'origine (N0120)
├── reference/v1/               référence V1
├── scripts/                    manifestes du portail, serveur local
├── tests/                      validation du bundle, vérification croisée des images
├── messaging/                  relais, compagnon PC, protocole et tests de messagerie
├── docs/                       ARCHITECTURE, SECURITY (PIN), STIMG (images)
├── BUILD.md                    commandes exactes
└── RAPPORT-FINAL.md            ce qui est terminé, testé, et ce qui reste à tester
```

## Sur la calculatrice

- **Accueil STI2D-OS** : disposition V1 4-3-4 conservée et améliorée, avec Calculatrice, Games, Favorites, Intelligences (Aelyx IA, Matrices V4, STMG IA), My Apps (applications injectées personnellement), Clock, Calendar, Gallery, Files, Terminal et Control Center.
- **Double π** sur l'accueil : passage à **STI2D Classic** (le carrousel V1) et retour. Un seul π ne fait rien ; ailleurs π garde son rôle.
- **Centre de contrôle** : interface, thème, mode clair/sombre, accent, animations, fond d'écran (uni, dégradé, animé, image), code PIN, informations système, paramètres avancés.
- **PIN** : quatre chiffres, configuré depuis le portail puis transféré par fichier à usage unique, code de récupération et attente croissante après 5 échecs (voir `docs/SECURITY.md`).
- **Clock**, **Terminal**, **Gallery**, **Calendar** et **Files** (TXT/Markdown/CSV depuis le portail ; PDF convertis en texte paginé sur ordinateur avec `tools/pdf_to_stidoc.py`). Le rendu PDF graphique embarqué n'est pas disponible.
- **STI2D Messages (PC)** : relais réseau, comptes sur invitation, envoi/réception en direct et compagnon PC sont fournis dans `messaging/`. Le pont calculatrice USB attend encore l'ajout d'un canal USB applicatif dans Epsilon ; le câble NumWorks actuel ne fournit pas le port série requis. Voir `messaging/README.md` et `docs/STORAGE-AND-MESSAGING.md`.

## Licence et propriété intellectuelle

Les éléments originaux de STI2D-OS V2 sont soumis à une licence propriétaire « Tous droits réservés ». Toute copie, republication, redistribution, revente ou modification de ces éléments nécessite une autorisation écrite préalable. Cette restriction ne s’étend pas aux composants tiers : Epsilon/NumWorks, jeux, applications, bibliothèques et ressources restent soumis à leurs propres licences. Lire [`LICENSE`](LICENSE) et [`docs/IP-PROTECTION.md`](docs/IP-PROTECTION.md) avant toute réutilisation.

**Créateurs / contacts :** Yamii (`cjnw`) et Derim (`irationnel`).  
**Discord officiel :** https://discord.gg/sti2d

## Communauté

Portail : dix aperçus OS (STI2D-OS, Classic, 3DS, PS5, iOS, Windows, Xbox, Linux/GNOME, Android, macOS). Les neuf derniers sont des aperçus web seulement. Le portail accepte les `.nwa` en glisser-déposer et prépare les documents texte ; un convertisseur PDF hors ligne est fourni dans `tools/`.

Discord : https://discord.gg/sti2d

Projet communautaire, non affilié à NumWorks. Les jeux inclus gardent leurs licences et crédits propres. La licence propriétaire STI2D-OS ne s’applique pas aux œuvres tierces.
