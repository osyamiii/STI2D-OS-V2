# Stockage et messagerie — état réel et trajectoire

## PDF : import de texte paginé

Le lecteur Fichiers ne contient pas de moteur PDF graphique embarqué. Le projet fournit maintenant `tools/pdf_to_stidoc.py` : il extrait le texte d’un PDF sur ordinateur et le convertit dans le format de commentaires `STI2D-DOC-V1`, que le lecteur sait déjà afficher. Le script limite la sortie à 16 Kio et refuse de tronquer silencieusement le document. Les images, la mise en page complexe et les PDF scannés sans OCR ne sont pas rendus. Voir `docs/PDF-IMPORT.md`.

## Stockage : ce que signifie « 6 Mo »

Le projet contient plusieurs valeurs qui désignent des choses différentes :

- `userland/installer/server.py` : `MAX_UPLOAD = 6 * 1024 * 1024` est la taille maximale d'un fichier `.nwa` accepté par le serveur local. Ce n'est pas la place installable sur la calculatrice.
- `userland/epsilon-src/epsilon/apps/sti2d/play/Makefile` : `APP_SPACE = 2031616` représente une zone standard utilisée par les tests d'installation. Une configuration N0120/Epsilon 26.4 peut disposer d'une zone de départ plus basse, documentée dans ce Makefile.
- `XL_APP_SPACE = 6291456` est une variante élargie qui étend la zone jusqu'à `0x90800000` et passe sur l'espace normalement réservé à l'autre copie du logiciel.
- La RAM du shell, le heap d'un jeu, les fichiers de l'archive Python et le binaire Userland sont encore d'autres ressources.

### Décision de cette fusion

La capacité de 6 Mo n'est pas activée automatiquement. Elle compromettrait la séparation des deux slots et la possibilité de revenir facilement au firmware de secours. L'installateur continue à calculer la zone réellement disponible depuis l'ELF de la cible et refuse les dépassements. Le site doit parler de capacité calculée et ne jamais présenter la limite d'upload de 6 Mo comme de la mémoire physique. La limite de transfert ne doit pas être augmentée en prétendant que cela agrandit la mémoire Flash.

Pour toute augmentation future, il faudra un profil expérimental distinct, des sauvegardes, une vérification exacte du modèle/slot/version, un avertissement visible et des essais physiques contrôlés. Aucune modification de ce type n'est faite par défaut ici.

## Messagerie entre humains : relais PC livré, USB calculatrice restant à intégrer

La messagerie n'est plus seulement une architecture documentée : `messaging/` contient un relais FastAPI/SQLite, un compagnon PC Windows/macOS/Linux, un protocole de trames et des tests. Deux comptes distincts peuvent s'inscrire sur invitation, envoyer et recevoir des messages en direct depuis leurs PC. Les messages hors ligne sont conservés jusqu'à 30 jours ; les utilisateurs peuvent supprimer leur copie.

### Ce qui fonctionne

- Création de comptes protégée par un code d'invitation serveur ; connexion par mot de passe.
- Dérivation PBKDF2-HMAC-SHA256 (310 000 itérations), jetons opaques stockés hachés et expirant après 30 jours.
- Envoi à un pseudo, réception WebSocket en direct, boîte de réception hors ligne, suppression par utilisateur.
- Limite de 30 messages/minute/compte et 1000 caractères/message.
- Compagnon CLI PC : `register`, `login`, `send`, `listen`, `logout`.
- Adaptateur `bridge --port ...` prêt pour les trames série USB définies dans `messaging/protocol/README.md`.

### Ce qui reste nécessaire pour la calculatrice

L'audit des sources Epsilon confirme que le contrôleur USB actuel est orienté DFU et n'expose qu'EP0 ; aucun port série USB/CDC applicatif n'est fourni au shell. Le compagnon ne peut donc pas encore échanger des trames avec une NumWorks utilisant le firmware actuel. Le transport de messagerie devra être ajouté à Epsilon comme interface applicative dédiée, distincte de DFU, puis relié à un écran Messages dans le Userland. Il ne faut pas détourner le protocole DFU pour le chat.

Trajet visé : NumWorks A → interface USB applicative → compagnon PC A → relais HTTPS/WSS → compagnon PC B → USB → NumWorks B. La partie PC A → relais → PC B est implémentée et testée ; les deux liaisons calculatrice/USB ne le sont pas.

### Sécurité et validation

Le relais doit être publié derrière un reverse proxy TLS ; le conteneur écoute par défaut uniquement sur `127.0.0.1`. Le contenu n'est pas chiffré de bout en bout ni chiffré par l'application dans SQLite. Ne pas envoyer d'informations sensibles. Les tests automatisés valident l'inscription, l'authentification, l'envoi, la réception WebSocket, la boîte de réception, l'idempotence, la suppression et les trames du compagnon. Ils ne remplacent pas un essai avec deux calculatrices réelles.

Commandes : voir `messaging/README.md` et lancer `make messaging-test`.
