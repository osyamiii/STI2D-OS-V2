# Importer un PDF dans STI2D-OS

Le lecteur Fichiers de la NumWorks est volontairement un lecteur de texte léger : il ne contient pas de moteur PDF graphique. Pour conserver une consommation mémoire adaptée à la calculatrice, l'extraction du texte du PDF est réalisée sur l'ordinateur, puis le résultat est converti dans le format `STI2D-DOC-V1` déjà compris par le lecteur.

## Installation

```bash
python -m pip install -r requirements-pdf.txt
```

Ou directement : `python -m pip install pypdf`.

## Conversion

```bash
python tools/pdf_to_stidoc.py mon-cours.pdf
```

Le convertisseur crée `stidoc_mon_cours.py`. Transfère ce script sur la calculatrice comme un script Python, puis ouvre `STI2D-OS > Fichiers`.

Pour n'extraire que certaines pages :

```bash
python tools/pdf_to_stidoc.py mon-cours.pdf --pages 1-3,5,8-9
```

Le convertisseur refuse les documents dépassant 16 Kio, la limite conservatrice du lecteur. Il ne tronque jamais silencieusement le contenu. Réduis la sélection de pages si nécessaire. Les PDF chiffrés doivent être déverrouillés avant conversion. Les PDF scannés sans couche de texte nécessitent une OCR sur ordinateur avant cette étape.

## Sécurité et limites

- Le convertisseur produit un script dont chaque ligne de document commence par `#|`.
- Le lecteur C lit ces commentaires et n'exécute pas le script importé.
- Les pages sont séparées par un marqueur textuel.
- Ce n'est pas un moteur de rendu PDF : images, colonnes complexes, graphiques et mise en page peuvent ne pas être conservés.
- La limite de 16 Kio s'applique au script exporté, pas à la taille du PDF source.
