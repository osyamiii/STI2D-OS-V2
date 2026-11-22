# STI2D Messages — relais et compagnon PC

Cette livraison ajoute une messagerie réelle **entre comptes PC** : inscription sur invitation, authentification, envoi, réception en direct, boîte de réception persistante, compagnon CLI Windows/macOS/Linux et adaptateur de pont série prêt pour le protocole calculatrice.

## 1. Démarrer le relais

Python 3.11+ et Docker Compose sont nécessaires pour le serveur conteneurisé.

```sh
cp messaging/.env.example messaging/.env
# Remplacer STI2D_INVITE_CODE par une longue valeur aléatoire
cd messaging
docker compose --env-file .env up --build -d
```

Le service écoute uniquement sur `127.0.0.1:8765`. Pour l'ouvrir sur Internet, placer un reverse proxy TLS devant lui (HTTPS/WSS), limiter l'accès réseau et ne jamais exposer le port sans TLS. Le relais n'active pas CORS et n'inclut pas d'interface d'administration.

Pour un test sur un seul PC, sans Docker, définir `STI2D_INVITE_CODE` puis lancer `messaging/start-local.sh` (macOS/Linux) ou `messaging\start-local.bat` (Windows). Le relais local est alors disponible sur `http://127.0.0.1:8765`. Ce mode local ne rend pas le service accessible depuis Internet.

## 2. Installer le compagnon

```sh
python -m venv .venv
# Windows : .venv\\Scripts\\activate
# macOS/Linux : source .venv/bin/activate
pip install -r requirements-messages.txt
```

Créer un compte (le code d'invitation est celui du serveur) :

```sh
python messaging/companion/sti2d_messages.py --server https://messages.example.org register Quassim
```

Se connecter sur un deuxième ordinateur avec un autre pseudo :

```sh
python messaging/companion/sti2d_messages.py --server https://messages.example.org register Imad
```

Envoyer et recevoir :

```sh
python messaging/companion/sti2d_messages.py --server https://messages.example.org send Imad "Salut, ça marche !"
python messaging/companion/sti2d_messages.py --server https://messages.example.org listen
# Pour révoquer la session sur le serveur :
python messaging/companion/sti2d_messages.py --server https://messages.example.org logout
```

Le jeton est stocké dans `~/.config/sti2d-messages/account.json` avec des permissions restrictives quand le système le permet. `logout` est disponible via l'API ; supprimer ce fichier ne révoque pas le jeton serveur.

## 3. Pont calculatrice

Quand le firmware exposera un port série USB applicatif, lancer :

```sh
python messaging/companion/sti2d_messages.py --server https://messages.example.org bridge --port COM5
# macOS/Linux : --port /dev/ttyACM0 (nom dépendant du pilote)
```

Le pont envoie au calculateur les messages entrants et relaie les paquets `send` de la calculatrice vers le serveur. La spécification de trames est dans `messaging/protocol/README.md`.

**Limite actuelle, à ne pas masquer :** le firmware STI2D-OS de la fusion n'expose pas de port USB CDC/série. Le relais et la messagerie PC sont fonctionnels, mais le chemin calculatrice ↔ USB n'est pas encore connecté. Le pont série ne peut donc pas communiquer avec une NumWorks sur le firmware actuel. Le firmware doit d'abord ajouter un transport applicatif USB dédié, distinct de DFU, puis l'écran Messages du Userland doit être relié à ce transport. Aucun message de démonstration n'est affiché comme s'il provenait d'un vrai utilisateur.

## Sécurité et conservation

- Inscription protégée par un code d'invitation serveur.
- Mot de passe dérivé avec PBKDF2-HMAC-SHA256 (310 000 itérations) et sel aléatoire.
- Jetons opaques aléatoires, stockés hachés côté serveur, expiration 30 jours.
- Limite de 30 messages/minute/compte et 1000 caractères/message.
- Les messages sont conservés au maximum 30 jours (purge au démarrage et lors des opérations d'envoi/lecture) ; chaque participant peut supprimer sa propre copie.
- Le contenu n'est pas chiffré de bout en bout ni chiffré par l'application dans SQLite. TLS est obligatoire pour un déploiement public. Ne pas envoyer d'informations sensibles.
