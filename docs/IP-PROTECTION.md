# Protection de la propriété intellectuelle et du portail

## Licence
Les contributions originales identifiables de STI2D-OS V2 sont couvertes par la licence propriétaire `../LICENSE`. Les jeux, Epsilon/NumWorks et autres composants tiers ne sont pas revendiqués : leurs licences et crédits continuent de s'appliquer.

## Ce que le serveur local protège
`scripts/serve.py` n'expose plus l'arborescence complète du dépôt. Il ne sert que le portail `web/`, les téléchargements `.nwa` autorisés, les fichiers `.dfu` prévus pour l'installation et la page d'installation. Les dossiers `firmware/`, `userland/epsilon-src/`, `messaging/`, `docs/`, `tests/`, les scripts de construction et les fichiers de configuration ne sont pas accessibles via ce serveur. Des en-têtes HTTP de sécurité sont ajoutés et l'indexation des dossiers n'est pas proposée.

## Limite incontournable de F12
Le navigateur doit recevoir le HTML, le CSS, le JavaScript, les images et les données nécessaires au portail. Une personne peut donc consulter ou enregistrer ces ressources avec les outils de développement, même si F12, le clic droit ou les raccourcis sont bloqués. La minification ou l'obfuscation ne ferait que ralentir la lecture, pas empêcher la copie. Cette version ne prétend donc pas rendre le frontend impossible à copier.

## Pour une vraie protection lors d'une mise en ligne
- Ne jamais placer de clé API, mot de passe, secret ou logique propriétaire critique dans `web/`.
- Déplacer les fonctions sensibles vers un backend privé avec authentification, contrôle d'accès et limitation de débit.
- Configurer l'hébergement pour ne publier que les répertoires nécessaires ; ne jamais publier le dépôt Git, les sources firmware, les fichiers `.env`, les sauvegardes ou les archives de développement.
- Activer HTTPS, des sauvegardes et une journalisation serveur adaptée.
- Conserver les sources originales, l'historique Git, les dates de versions et les preuves de création. Ajouter une mention de copyright aide à identifier les titulaires mais ne remplace pas ces preuves.

Le serveur Python fourni est destiné au développement local et au partage sur un réseau de confiance ; ce n'est pas un serveur de production sécurisé.
