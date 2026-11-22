# Matrices V2

Application NumWorks (.nwa) : assistant hors ligne pour les cours (STI2D, maths, physique-chimie,
francais, philo, histoire-geo, langues), les calculs et quelques conseils pratiques.
Aucune connexion Internet n'est utilisee.

## V2.4 - Visuel propre
- Fond uni (carte) derriere le chat : plus de texte qui "bave" sur les themes -> zero bloc parasite
- Clavier a touches pleines, selection nette, touche EXE mise en avant
- Accueil et en-tete redessines (liseres d'accent, espacements coherents)
- Icone refaite : coins transparents (plus d'angles noirs), degrade jaune/orange, motif [M]

## V2 - Nouveautes
- **Clavier on-screen UHQ** : AZERTY + Numerique + Symboles
  - Fleches pour naviguer sur les touches
  - OK pour taper la touche selectionnee
  - EXE envoie la question
  - DEL, ESP, MAJ, 123/ABC/SYM
- Menu burger complet (themes, config clavier, historique, credits, reset)
- Message "Reflexion en cours..." avant la reponse
- Suggestions de questions liees apres la reponse (cliquables)
- Matching plus strict pour des reponses uniques (ex: loi des mailles seule)
- Icone jaune/orange (couleurs NumWorks) sans angles noirs
- Credits ameliores + mention Discord

## Contenu
- `src/main.c`   : application complete V2
- `src/mini.h`   : fonctions de chaines
- `src/kb.h`     : base de connaissances (generee)
- `src/icon.png` : icone 55x56 jaune/orange
- `kb.txt`       : questions / reponses
- `caps.txt`     : pays et capitales
- `tools/gen.py` : regenere kb.h et test-ia.html

## Utilisation du clavier
1. Fleches haut/bas/gauche/droite : deplacer le curseur sur le clavier
2. OK : appuyer sur la touche selectionnee
3. EXE : envoyer la question (ou valider une suggestion)
4. Back : ouvrir le menu
5. Shift : basculer majuscules (ou epingler dans l'historique)

## Compilation
Voir INSTALL.md (meme procedure que le sample officiel NumWorks).
