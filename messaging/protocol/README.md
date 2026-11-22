# Protocole STI2D Messages v1 (pont calculatrice ↔ compagnon)

Transport visé : flux série USB 115200 bauds, 8N1, lignes JSON UTF-8 terminées par LF. Une trame ne dépasse pas 4096 octets. Le compagnon valide chaque trame et n'exécute jamais son contenu.

## Calculatrice → PC

```json
{"v":1,"type":"send","id":"client-generated-id","to":"Pseudo","text":"Salut !"}
```

## PC → calculatrice

```json
{"v":1,"type":"deliver","id":42,"from":"Pseudo","text":"Salut !","sent_at":1795270000}
```

Autres paquets : `status` (connexion), `sent` (accusé de réception serveur), `error` (erreur lisible). Taille de texte maximale : 1000 caractères. Pseudo : 3–24 caractères `[A-Za-z0-9_]`.

**État matériel important :** le compagnon et le relais sont implémentés, mais le firmware de la fusion n'expose actuellement aucun port série USB/CDC. L'interface USB existante est centrée sur DFU et les transferts de contrôle EP0. Ce protocole décrit le contrat d'intégration à ajouter dans Epsilon ; il ne prétend pas que le câble NumWorks actuel fournit déjà ce port. Le compagnon `bridge` fonctionnera lorsqu'un adaptateur série compatible sera exposé par un firmware ultérieur. Il ne faut pas détourner DFU pour transporter des messages.
