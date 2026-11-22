# Format STIMG — images pour la calculatrice

La calculatrice ne décode ni PNG, ni JPEG, ni GIF, et son système de fichiers contient environ **42 Ko** au total. Le portail convertit donc une image en un petit fichier à palette, emballé dans un **script Python composé uniquement de commentaires** : il s'envoie comme n'importe quel script (my.numworks.com, `nwlink`) et survit aux réinstallations d'apps.

```
#STIMG1 <largeur> <hauteur> <couleurs>
#P <palette : 4 chiffres hexadécimaux RGB565 par couleur>
#D <base64 des pixels compressés>        (lignes de 76 caractères, autant que nécessaire)
```

- Largeur ≤ 160, hauteur ≤ 120 (affichage ×2 sur l'écran 320 × 240), 2 à 16 couleurs.
- Pixels : indices de palette en RLE, un octet par plage : `(longueur - 1) << 4 | indice` (plages de 1 à 16).
- Nom du fichier : `stimg_<nom>.py`, 23 caractères au plus.
- Sur la calculatrice, l'enregistrement d'un script commence par un octet d'état d'import Python ; le décodeur l'ignore.

Encodeur : `web/assets/js/stimg.js` (quantification par médiane, tramage Floyd-Steinberg facultatif). Décodeur : `firmware/v2/src/stimg.c`. `make stimg-check` vérifie que les deux donnent les mêmes pixels.

**GIF** : seule la première image est exportée. Une animation 160 × 120 en 16 couleurs dépasserait vite la mémoire de fichiers, et le firmware n'a pas de décodeur GIF ; le portail, lui, affiche le GIF animé.

Tailles typiques : photo 160 × 120 en 16 couleurs ≈ 3 à 6 Ko sans tramage, 15 à 20 Ko avec tramage ; 80 × 60 ≈ 1 à 5 Ko.
