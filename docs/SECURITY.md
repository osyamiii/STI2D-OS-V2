# Code PIN STI2D-OS — conception et limites réelles

## Configuration depuis le portail

Le PIN est **exactement composé de quatre chiffres** (0000 à 9999, zéros initiaux compris). Il peut être choisi dans la page Profils du portail, qui génère un fichier de configuration à usage unique `sti2dos-pin-setup.py`.

Pendant cette opération, le navigateur reçoit momentanément le PIN saisi afin de calculer son empreinte PBKDF2-HMAC-SHA256 (4 000 itérations, sel aléatoire de 16 octets). Le fichier téléchargé contient seulement les deux sels et les deux empreintes (PIN et code de récupération), jamais le PIN ni le code de récupération en clair. Le code de récupération de 12 chiffres est généré aléatoirement et affiché une seule fois. Il faut le conserver séparément.

Après transfert du script sur la calculatrice, le Userland le consomme au démarrage, crée l'enregistrement binaire privé `sti2dos.pin`, puis supprime le script de configuration. Le PIN n'est pas inclus dans les profils JSON ni dans les codes de partage. Le navigateur ne le conserve pas dans `localStorage`, mais il est présent temporairement en mémoire pendant la génération : ne configure pas ton PIN sur un ordinateur partagé ou compromis.

Le PIN peut également être créé depuis l'interface STI2D-OS. Dans ce parcours local, le sel est généré par le générateur aléatoire de la calculatrice.

## Parcours

- **Configuration web** : Profils → Code PIN → saisir et confirmer 4 chiffres → télécharger le fichier de configuration → transférer le script Python à la calculatrice → démarrer le Userland pour le consommer → conserver le code de récupération affiché une seule fois.
- **Création locale** : Centre de contrôle → Code PIN → Créer → saisir et confirmer quatre chiffres → conserver le code de récupération affiché une seule fois.
- **Verrouillage** : à l'ouverture de STI2D-OS et lors des retours protégés prévus par le shell.
- **Oubli** : sur l'écran de verrouillage, touche Toolbox → code de récupération. S'il est correct, le PIN est retiré ; il faut en recréer un.
- **Désactivation** : Centre de contrôle → Code PIN → Désactiver, avec le PIN actuel.

## Limites de sécurité

1. **Ce PIN protège l'interface STI2D-OS, pas la calculatrice entière.** Les autres applications Epsilon (Calculs, Python, etc.) restent accessibles selon le contexte. Les fichiers ne sont pas chiffrés.
2. **Un PIN de quatre chiffres est faible par conception.** Il n'existe que 10 000 combinaisons. PBKDF2 ralentit les essais, mais 4 000 itérations ne transforment pas ce code court en secret fort.
3. **Réinstaller des applications depuis un ordinateur peut effacer `sti2dos.pin`** selon le flux de stockage utilisé. Cela peut permettre de retirer le verrou sans connaître le code. Conserver une copie du code de récupération.
4. **Attaque hors ligne :** une personne ayant accès au stockage ou à une copie du fichier privé peut tester les combinaisons. L'enregistrement contient des empreintes et sels, pas le PIN.
5. Un enregistrement endommagé ou dont le contrôle d'intégrité échoue est refusé ; le shell peut alors démarrer sans verrou. Ce mécanisme n'est pas une protection anti-altération certifiée.
6. Les échecs sont conservés dans l'enregistrement ; après cinq erreurs, l'attente augmente. L'horloge n'étant pas garantie persistante lorsque la calculatrice est éteinte, la durée d'attente n'est pas une défense inviolable.
7. Le code de récupération de 12 chiffres est lui aussi une donnée sensible. Le portail ne l'enregistre pas durablement, mais l'affiche une seule fois au moment où le script est généré.

**Conclusion :** le PIN est un verrou de confort de l'interface STI2D-OS, pas un système de sécurité certifié, un chiffrement des données ou un mode examen officiel. Ne l'utilise pas pour protéger des informations sensibles.
