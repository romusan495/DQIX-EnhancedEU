# DQIX Enhanced EU

**Dragon Quest IX : Les Sentinelles du firmament, en vrai jeu PC.**
*English version below: [jump to English](#english).*

**Rejoignez la communauté sur Discord : https://discord.gg/Ja3nJGAjDA**
(aide, nouvelles versions, retours, captures).

> **Où en est le projet ?** Une **décompilation complète** du jeu est en cours.
> La version actuelle de ce dépôt est une **ébauche** : elle sert de base de
> travail pour la **recompilation** du jeu et pour le **futur remake** (un vrai
> jeu PC sous Godot, sur un seul écran, avec des graphismes modernisés). Tout
> ce qui est fait ici sert de banc d'essai et de référence pour ce remake.

Ce projet transforme **votre propre** cartouche de Dragon Quest IX (DS, version
européenne ou américaine) en un programme Windows natif, compilé sur votre PC à
partir de la ROM que vous avez extraite de votre cartouche. **Ce dépôt ne
contient aucune donnée du jeu** : ni code, ni graphismes, ni musique, ni ROM.
Ce n'est pas un émulateur : le code ARM du jeu est **recompilé en C**, puis lié
à un runtime qui recrée le matériel de la DS (2D, 3D, son, cartouche, entrées)
avec SDL2 et OpenGL.

Projet développé par **Romuald** ([romusan495](https://github.com/romusan495))
et **TitanFr199**, avec l'aide de Claude.

> **Droits et usage — à lire.** Ce projet est réalisé **à des fins éducatives
> uniquement** (étude de la recompilation et de la préservation des jeux).
> Dragon Quest IX et tout son contenu appartiennent à leurs ayants droit
> (Square Enix, Level-5, Armor Project, Bird Studio, Koichi Sugiyama,
> Nintendo). **Aucune ROM n'est fournie et aucune ne le sera.** Pour que le
> projet fonctionne, vous devez utiliser **une ROM extraite de votre propre
> cartouche** de Dragon Quest IX, que vous possédez légalement. Ne téléchargez
> pas de ROM et ne partagez ni ROM, ni jeu compilé, ni fichiers du jeu.

---

## Travaux réalisés jusqu'à présent

### ROM européenne et langues
- **Correctif de la ROM européenne** : le code ARM9 de la version EU est décalé
  de 16 octets par rapport à la version USA ; les adresses sont maintenant
  converties automatiquement.
- **Menu Langue** : anglais, français, allemand, italien, espagnol.
- Les sauvegardes passent de la version USA à la version EU et inversement.
- **Interface PC traduite** en 5 langues (barre de menus, fenêtres, menu
  d'options dans le jeu).
- **Texte HD** avec une seule police PC lisible (Source Sans 3), espacement
  revu, lettres mélangées dans les dialogues corrigées.

### Un seul écran, comme un vrai jeu PC
- **Écran unique** en exploration et en combat, dans une seule fenêtre.
- **Mini-carte** en haut à droite (touche M / R3 pour la cacher), avec
  **marqueurs** : coffres, pots, tonneaux, étagères, donneurs de quêtes, rang
  des coffres dans les grottes et rangs possibles sur les cartes au trésor.
  Plans des intérieurs tirés des collisions du jeu, légende de la carte.
- Fiches d'objets et d'équipement en **pop-up**, **magasins** sur un seul
  écran avec fiche d'objet moderne.
- **Création du personnage sur un seul écran** (choix en pop-up, nom tapé au
  clavier en option), aussi chez Tulipe pour les compagnons.
- **Écran large 16:9 et 21:9** : l'image suit la forme de la fenêtre ou de
  l'écran, sans bandes noires ; côtés des menus floutés.
- Tableau des points de compétence à côté des caractéristiques.

### Fenêtres et interface
- **Style de fenêtres Moderne** (par défaut) sur toutes les fenêtres : fond
  sombre transparent, fin trait gris. Styles Minimal et contour blanc aussi
  disponibles.
- **Combat moderne** : HUD de combat par-dessus la 3D, cartes de l'équipe et
  menus de combat au style moderne, aussi pour un héros seul.
- **Menu d'options dans le jeu** (Échap ou bouton PS / Xbox / Home), jouable à
  la manette, jeu en pause.
- **Démarrage façon PC** : les logos sont passés.
- **Indications de boutons** avec les vrais boutons de votre manette, ou votre
  touche clavier dessinée dans le style du jeu (AZERTY compris).
- Fiche de quête en fenêtre moderne, icônes de vocation en couleur,
  confirmation avant de quitter.

### Graphismes
- Rendu 3D sur la carte graphique jusqu'à 12x la résolution de la DS (mode
  Enhanced), mode Classic identique à la DS au pixel près (F5 pour passer de
  l'un à l'autre).
- **Textures HD par IA** : toutes les textures 3D sont extraites de votre ROM
  et agrandies x4 par IA (Real-ESRGAN) automatiquement à la compilation, avec
  barre de progression et reprise. Packs de textures HD personnels possibles,
  et un outil pour les vérifier.
- **Films HD** (les 6 cinématiques agrandies x4).
- Textures xBR, 2D lissée, ombres douces, fins traits de contour, PNJ lointains
  plus nets, contours des personnages au choix.
- **HDR** : luminosité normale en plein écran avec le HDR de Windows.

### Confort
- **Journal** (touche J / L3) : quêtes, progression de l'équipe, vocations,
  alchimie (recettes connues, ingrédients manquants) et objets.
- **Combats plus rapides** (x2, x3, x4 pendant le tour seulement), messages de
  victoire automatiques.
- **Sauvegarde automatique** toutes les 10 minutes (option), copies de
  sauvegarde.
- **Menu Cheats** (codes Action Replay de données, tous désactivés par
  défaut) : or, mini-médailles, points de compétence, PV des monstres à 0,
  objets rares garantis, etc.
- Farm d'or automatique (Ctrl+G), avance rapide (Tab), clavier et manette
  entièrement reconfigurables (Jeu > Contrôles), stick gauche pour marcher dans
  toutes les directions.
- Rapport de bug complet avec F10.

### Autres fonctions
- Multijoueur (mode Tag et coopération jusqu'à 4) par Internet, boutique
  **DQVC** et quêtes téléchargeables hors ligne, mises à jour automatiques.

### Versions, tests et organisation
- **Une version GitHub par semaine environ** : de v31.1 (22 octobre 2026) à
  **v40** aujourd'hui, avec notes de version dans [`release-notes/`](release-notes/).
  Le jeu propose lui-même la mise à jour.
- Plus de 90 pull requests fusionnées, tests automatiques réparés, **test de
  fumée** (EU et USA) avant chaque fusion.
- Récapitulatif complet : [docs/RECAP.md](docs/RECAP.md) ; suivi des tâches :
  [docs/SUIVI.md](docs/SUIVI.md) ; carte technique :
  [docs/TECHNICAL.md](docs/TECHNICAL.md).

---

## Installer

Il vous faut la ROM (`.nds`) de **votre propre** cartouche (version européenne
ou américaine), Windows 10 ou 11 (64 bits), environ 3 Go libres, et Internet la
première fois.

1. Téléchargez la dernière version sur la
   **[page des versions](https://github.com/romusan495/DQIX-EnhancedEU/releases)**
   (*Source code (zip)*) et décompressez-la.
2. Double-cliquez sur **`Install Dragon Quest IX.bat`**, choisissez votre ROM
   (la fenêtre indique sa version), le dossier du jeu et les raccourcis.
   L'installation des outils (MSYS2) et la compilation prennent 10 à 20 minutes
   la première fois. Cliquez sur **Play now** à la fin.
3. Ensuite, lancez le jeu depuis le bureau ou le menu Démarrer.

Sans fenêtre d'installation : mettez votre ROM dans le dossier `rom` et
double-cliquez sur **`BUILD.bat`** ; le jeu apparaît dans `game\dqix.exe`.

La sauvegarde (`dqix.sav`) et les réglages (`settings.ini`) sont dans le
dossier `game`. **Ne partagez jamais les dossiers `game` et `build`** : ils
contiennent le jeu.

**Mises à jour** : le jeu propose la nouvelle version au démarrage (ou
Aide > Rechercher des mises à jour). Sauvegardes et réglages sont conservés,
et la version précédente reste dans `update` (`update\rollback.bat`).

## Commandes

| DS | Clavier | Manette |
|---|---|---|
| Croix | flèches | croix / stick gauche |
| A / B / X / Y | X / Z / S / A | boutons droite / bas / haut / gauche |
| L / R (caméra) | Q / W | gâchettes hautes, ou stick droit en marchant |
| START / SELECT | Entrée / Maj droite | Start / Back |
| écran tactile | clic gauche | |

Touches utiles : Échap menu d'options, J Journal, M mini-carte, Tab (maintenu)
avance rapide, P pause, Ctrl+R réinitialiser, F5 Classic / Enhanced,
F11 plein écran, F12 capture, F10 rapport de bug, Ctrl+G farm d'or. Tout se
change dans **Jeu > Contrôles**.

## Un problème ?

Appuyez sur **F10** dans le jeu : un rapport est enregistré dans
`game\reports\`. Posez vos questions sur le
**[Discord](https://discord.gg/Ja3nJGAjDA)**.

---

<a id="english"></a>

## English

**Dragon Quest IX: Sentinels of the Starry Skies, as a native PC game.**

**Join us on Discord: https://discord.gg/Ja3nJGAjDA**

> **Project status:** a **full decompilation** of the game is in progress. The
> current version of this repository is a **draft**: a working base for the
> **recompilation** and for the **upcoming remake** (a real PC game in Godot,
> on one screen, with modernised graphics). Everything done here is a test bed
> and a reference for that remake.

This project turns **your own** Dragon Quest IX cartridge (DS, European or USA
version) into a native Windows program, built on your PC from the ROM you
dumped from your cartridge. **This repository contains no game data**: no
code, graphics, music or ROM. It isn't an emulator: the game's ARM code is
**recompiled into C** and linked to a runtime that rebuilds the DS hardware
with SDL2 and OpenGL. Developed by **Romuald**
([romusan495](https://github.com/romusan495)) and **TitanFr199**, helped by
Claude.

> **Rights and use, please read.** This project is for **educational purposes
> only** (studying recompilation and game preservation). Dragon Quest IX and
> all its content belong to their rights holders (Square Enix, Level-5, Armor
> Project, Bird Studio, Koichi Sugiyama, Nintendo). **No ROM is provided, and
> none ever will be.** To make it work, you must use **a ROM dumped from your
> own cartridge** of Dragon Quest IX, which you legally own. Don't download
> ROMs, and don't share ROMs, the built game or any game files.

**Done so far**
- **European ROM support** (16-byte ARM9 shift fixed) and a **Language menu**
  (EN/FR/DE/IT/ES); saves move between the USA and EU versions; PC interface
  translated into 5 languages; HD text with one PC font (Source Sans 3).
- **Single screen** in the field and in battle, in one window, with a
  **minimap** and **markers** (chests, pots, barrels, bookshelves, quest
  givers, grotto chest ranks), interior maps, pop-up item cards, shops and
  character creation on one screen.
- **16:9 and 21:9 widescreen** fitted to any screen shape.
- **Modern window style** on every window, **modern battle HUD**, in-game
  options menu (Esc / PS button), PC-style boot, real controller buttons or
  your own keyboard key in the hints.
- **AI HD textures** (every 3D texture from your ROM upscaled x4 with
  Real-ESRGAN, automatically at build time), **HD movies**, 3D up to 12x the DS
  resolution, xBR, soft shadows, **Windows HDR** brightness fix.
- **Journal** (quests, party, vocations, alchemy, items), **faster battles**
  (up to x4), **autosave**, **Action Replay cheats** menu (all off by default),
  gold farming, remappable keyboard and controller, F10 bug reports.
- Also: online multiplayer, offline DQVC and downloadable
  quests, self-updating.
- **Weekly releases** from v31.1 to **v40**, 90+ merged pull requests, a smoke
  test on both EU and USA before each merge.

**Install**: download the latest
**[release](https://github.com/romusan495/DQIX-EnhancedEU/releases)**
(*Source code (zip)*), unzip it, double-click `Install Dragon Quest IX.bat` and
choose your own ROM (Windows 10/11 64-bit, about 3 GB free). Or put your ROM in
`rom` and run `BUILD.bat`. The game then updates itself and keeps your saves.

---

## Crédits / Credits

- Projet / project : **Romuald** ([romusan495](https://github.com/romusan495))
  et / and **TitanFr199** ([TitanFr199](https://github.com/TitanFr199)), avec
  l'aide de / helped by Claude.
- Noms et limites des fonctions : projet de décompilation de Dragon Quest IX
  (domaine public). Puce Wi-Fi, rendu 3D et son : recherches de l'émulateur
  melonDS. / Function names and boundaries from the public-domain DQIX
  decompilation project; wireless chip, 3D renderer and sound follow melonDS's
  research.

Dragon Quest IX est © Square Enix / Level-5 / Armor Project / Bird Studio /
Koichi Sugiyama. Ce projet n'est ni affilié à Square Enix ou Nintendo, ni
approuvé par eux. Il ne contient aucun fichier du jeu et ne fonctionne qu'avec
une ROM extraite de votre propre cartouche. / This project isn't affiliated
with or endorsed by Square Enix or Nintendo. It contains no game files and only
works with a ROM dumped from your own cartridge.

Licence / License : GPL-3.0-or-later (voir / see `LICENSE`). Inclut des données
du domaine public du projet de décompilation de DQIX (`config/`), libslirp
(BSD, `third_party/libslirp`), stb_truetype (domaine public / public domain) et
des parties traduites de melonDS (GPL-3.0-or-later).
