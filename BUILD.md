# BUILD — STI2D-OS V2

Toutes les commandes se lancent depuis le dossier `STI2D-OS-V2`. Elles ont été exécutées telles quelles sur macOS (Apple Silicon) le 19/11/2026.


## 0. Userland STI2D-OS (système principal)

STI2D-OS est un **véritable Userland intégré aux sources d'Epsilon** : le shell C remplace l'accueil Epsilon par STI2D-OS lorsque le Userland est disponible. Ce n'est pas une app EADK `.nwa`. Le noyau et le bootloader officiels restent distincts et le mode examen officiel reste prioritaire.

Arbre : `userland/epsilon-src/` (Epsilon, commit `userland/EPSILON_COMMIT`, modifications : `userland/sti2d-os-userland.patch` + `epsilon/apps/sti2d/`). Le shell (launcher + couche V2) est compilé **dans** le Userland depuis `firmware/` avec `-DSTI_USERLAND=1` (liens `apps/sti2d/play` et `apps/sti2d/v2`). Les `.nwa` sont les applications externes que le Userland peut lancer, pas le système lui-même.

Outils en plus : GNU make ≥ 4.3 (`brew install make`), `dfu-util` (`brew install dfu-util`), Python 3 avec `pyusb pypng lz4` pour l'arbre Epsilon.

```bash
APP_VERSION=26.4.0 userland/build.sh n0110 n0115 n0120
#  -> userland/firmware/<modèle>/sti2d-os.{A,B}.{dfu,elf}
```

`APP_VERSION` doit être **exactement** la version du logiciel officiel de la calculatrice (sinon le noyau refuse le userland : « OFFICIAL UPGRADE REQUIRED »).

Installation (calculatrice branchée, écran « La calculatrice est connectée ») :

```bash
python3 userland/installer/server.py          # page web locale : choix des apps, capacité, installation
python3 userland/installer/sti2dos_install.py --status
python3 userland/installer/sti2dos_install.py --apps library/ai/Matrices-v4.nwa library/games/Snake.nwa
```

Tests :

```bash
make -C firmware/v2 test            # 140 vérifications, shell et modules sur hôte
make -C firmware/v2 test-userland   # 31 vérifications, shell compilé en mode Userland
python3 userland/tools/link_apps.py --model n0120 --dry-run userland/firmware/n0120/sti2d-os.B.elf library/games/Snake.nwa
```

## 1. Outils

| Outil | Version utilisée | Pour |
|---|---|---|
| Arm GNU Toolchain (`arm-none-eabi-gcc`, newlib) | 15.3.Rel2 | compiler le firmware |
| Node.js + `npx` | 18.20.8 | `nwlink@1.0.0` (en-têtes EADK, édition de liens finale), tests du portail |
| Python 3 + Pillow | 3.15 / Pillow 12 | génération du launcher (captures, polices), conversion d'images |
| `unicorn` + `pyelftools` (Python) | unicorn 2.1.4 | facultatif : exécuter le firmware ARM dans l'émulateur |
| `cc` (Clang ou GCC) | Apple Clang | tests hôte |

Installation :

```bash
# macOS
brew install --cask gcc-arm-embedded
# Debian / Ubuntu
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
# Windows : « Arm GNU Toolchain » (arm-none-eabi) depuis developer.arm.com, dans le PATH

# Python : un environnement dédié avec ce qu'il faut
python3 -m venv .venv
.venv/bin/pip install Pillow pyelftools unicorn
```

`nwlink` est récupéré automatiquement par `npx` au premier build (accès réseau requis la première fois).

## 2. Builds Userland et applications externes

### Userland Epsilon (produit principal)

```bash
APP_VERSION=26.4.0 userland/build.sh n0110 n0115 n0120
# -> userland/firmware/<modèle>/sti2d-os.{A,B}.{dfu,elf}
```

`APP_VERSION` doit correspondre exactement à la version du noyau officiel actif. Le Userland est écrit dans le slot inactif, sans modifier le bootloader. N'installe pas si le modèle, le slot ou la version ne correspondent pas.

```bash
python3 userland/installer/server.py
python3 userland/installer/sti2dos_install.py --status
python3 userland/installer/sti2dos_install.py --apps library/ai/Matrices-v4.nwa library/games/Snake.nwa
```

Le linker lit les bornes de la région d'applications depuis l'ELF du Userland. Les limites varient selon le modèle et la version Epsilon. La limite de 6 Mo du serveur est une taille maximale d'upload `.nwa`, pas la mémoire disponible. L'option de zone élargie 6 MiB qui chevauche le slot de secours n'est pas utilisée par défaut ; voir `docs/STORAGE-AND-MESSAGING.md`.

### App EADK `.nwa` (compatibilité / version autonome distincte)

Les cibles `make n0110`, `make n0115`, `make n0120` et `make firmware` dans le Makefile racine construisent une **application autonome EADK** dans `release/`. Cette variante peut être installée comme une app externe sur Epsilon, mais ce n'est pas le Userland intégré décrit plus haut. Elle est conservée comme artefact de compatibilité et de comparaison ; ne pas la présenter comme la livraison système principale.

## 3. Tests

```bash
make host-test PY=.venv/bin/python   # launcher réel + V2 compilés pour l'ordinateur (ASan/UBSan), 140 vérifications
make arm-check                       # chaque module V2 compilé pour N0110, N0115, N0120 avec -Werror
make stimg-check                     # encodeur d'images du portail ↔ décodeur C du firmware, pixel par pixel
make bundle-test PY=.venv/bin/python # ressources, manifestes, liens, portail, binaires publiés (62 vérifications)
make emu-test PY=.venv/bin/python    # binaires ARM exécutés dans l'émulateur Cortex-M7 du projet
make test PY=.venv/bin/python        # tout (emu-test si unicorn est installé)
```

Captures produites : `firmware/v2/build/host-shots/*.png` (hôte) et `firmware/v2/build/emu/*/shot_*.png` (émulateur ARM).

## 4. Documents PDF

Le lecteur Fichiers affiche du texte léger, pas un PDF graphique. Pour extraire le texte d’un PDF sur ordinateur :

```bash
python3 -m pip install -r requirements-pdf.txt
python3 tools/pdf_to_stidoc.py mon-cours.pdf
# ou certaines pages :
python3 tools/pdf_to_stidoc.py mon-cours.pdf --pages 1-3,5
```

Transfère ensuite `stidoc_*.py` sur la calculatrice comme un script Python. Le convertisseur refuse les exports supérieurs à 16 Kio. Voir `docs/PDF-IMPORT.md`.

## 5. Portail web

Statique, sans compilation. Après un ajout ou un changement dans `library/` :

```bash
make portal PY=.venv/bin/python      # web/data/games.js, ai.js, covers/*.png + contrôle de syntaxe JS
```

Lancer le portail :

```bash
./start-server.sh                    # macOS / Linux  → http://localhost:8000/web/
start-server.bat                     # Windows
```

Le script affiche aussi l'adresse à ouvrir depuis un téléphone sur le même Wi-Fi.

## 6. Installer sur la calculatrice

Depuis un ordinateur avec Chrome ou Edge :

- La version autonome `.nwa` facultative peut être ajoutée via **my.numworks.com/apps** (Epsilon compatible) ; cela n'installe pas le Userland.
- ou en ligne de commande (Epsilon ou Upsilon) :

```bash
npx nwlink install-nwa release/n0120/STI2D-OS.nwa
```

Images de la Galerie, documents texte et préférences : les fichiers `stimg_*.py`, `stidoc_*.py`, `sti2dos.py` et le fichier PIN à usage unique exportés par le portail se transfèrent comme des scripts Python. Voir `docs/SECURITY.md` et `docs/ARCHITECTURE.md`.
