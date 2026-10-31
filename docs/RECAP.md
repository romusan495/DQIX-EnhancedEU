# DQIX Enhanced EU : récapitulatif complet du projet

État au 29 octobre 2026 (`main` = `780f520`, dernière version publiée v31.10).
Écrit pour un deuxième développeur qui rejoint le projet avec son propre Claude.
Ce document se lit par un humain comme par un Claude : chaque section est
autonome ; les sections 11 et 12 expliquent comment les deux équipes Claude
travaillent ensemble, et la section 13 à quoi ce dépôt servira pour le futur
remake du jeu sous Godot.

Sommaire :
1. Le projet en une page
2. Où vit quoi
3. Architecture technique
4. Compiler et tester
5. Comment le projet est organisé aujourd'hui
6. Règles permanentes (décisions de Romuald)
7. Ce qui a été fait, par thème, avec les PR
8. Versions publiées
9. Relations avec le projet d'origine (sackchief)
10. Chantiers en cours et pistes restantes
11. Coordonner deux équipes Claude sur le même dépôt
12. Mise en place pour le deuxième développeur (pas à pas)
13. Le projet remake (Godot) : à quoi sert ce dépôt
14. Questions encore ouvertes

---

## 1. Le projet en une page

**DQIX Enhanced** fait tourner *Dragon Quest IX : Les Sentinelles du firmament*
(Nintendo DS) comme un vrai programme Windows, sans émulateur. Le code ARM du
jeu est **recompilé en C** à partir de la ROM du joueur, puis lié à un
**runtime** en C qui recrée le matériel de la DS (2D, 3D, son, cartouche,
entrées) avec SDL2 et OpenGL.

- Projet d'origine : `sackchief/dqix-enhanced` (fonctionnait seulement avec la
  ROM américaine).
- Ce fork : `romusan495/DQIX-EnhancedEU`, tenu par Romuald avec Claude depuis le
  19 octobre 2026.
- Point de départ : **corriger la ROM européenne** (fait le 19/10). Depuis, le
  fork est devenu une **modernisation façon jeu PC** : un seul écran, fenêtres
  modernes, textures et films HD, texte HD, interface traduite, menu d'options
  dans le jeu, journal des quêtes, carte avec marqueurs, sauvegarde auto, etc.
- Rien du jeu n'est jamais distribué : chaque joueur compile à partir de sa
  propre ROM (USA ou EU).
- Vision cible : un **cahier des charges** de modernisation (28/10) avec des
  priorités P0 stabilité, P1 écran unique / interface / graphismes, P2 confort,
  P3 contenu. Le multijoueur et le réseau sont **hors périmètre**.
- Et après : quand la décompilation du jeu sera finie, le jeu sera **réécrit
  sous Godot**, sur un seul écran et pensé pour le PC. Ce dépôt sert de banc
  d'essai et de référence pour ce remake (section 13).

Chiffres utiles : 14 815 fonctions ARM9 et 962 fonctions ARM7 recompilées ;
environ 9 % des symboles ont un vrai nom (carte de `dqix-decomp`) ; 59 PR
fusionnées dans `main` du 21 au 29 octobre ; 10 versions publiées (v31.1 à v31.10).

---

## 2. Où vit quoi

| Quoi | Où | Remarque |
|---|---|---|
| Code source (seule référence) | GitHub `romusan495/DQIX-EnhancedEU`, branche `main` | Public. Jamais de ROM ni de données du jeu. |
| Branche des joueurs | `stable` (+ tags `v31.N`) | Lue par Aide > Rechercher des mises à jour. |
| Fork pour proposer au projet d'origine | `romusan495/dqix-enhanced` | Sert seulement aux PR vers sackchief. |
| Projet d'origine | `sackchief/dqix-enhanced` | En v32 depuis le 27/10 (non porté ici, choix de Romuald). |
| PC de Romuald, clone de dev | `J:\DQIX-Enhanced-repo` | Reste sur `main`. |
| PC de Romuald, worktrees | `J:\DQIX-wt\<role>-<sujet>` | Un par branche de dev. |
| PC de Romuald, le jeu joué | `J:\DQIX-Enhanced` | Checkout de `main`. **Jamais édité à la main** : `git pull --ff-only` + `BUILD.bat`, avec son accord. |
| Sauvegardes du dossier de jeu | `J:\DQIX-Backups\` | Avant toute mise à jour risquée. |
| Fichiers de travail du projet Claude de Romuald | dossier partagé privé du projet (captures, études, recettes de test, références du test de fumée) | **Pas accessible au 2e dev.** Ce qui doit être partagé passe par le dépôt (si ce n'est pas dérivé du jeu) ou directement entre humains. |

Documents de référence dans le dépôt :

| Fichier | Contenu |
|---|---|
| `CLAUDE.md` | Règles pour tout Claude qui travaille ici (build, tests, branches, fichiers partagés, compte rendu). **À lire en premier.** |
| `CONTRIBUTING.md` | Workflow git en anglais (worktrees, build, ce qu'on ne commite jamais). |
| `docs/SUIVI.md` | Registre des tâches, décisions, audit du cahier des charges, feuille de route, test de fumée. |
| `docs/roles/*.md` | Les rôles et les fichiers que chacun possède. |
| `docs/TECHNICAL.md` | Carte technique détaillée (runtime, variables `DQIX_*`). |
| `docs/STATE.md`, `ROADMAP.md` | Hérités du projet d'origine (parlent surtout de multijoueur et de Linux). |
| `release-notes/v31.N.md` | Notes de chaque version (anglais). |
| `.claude/agents/dqix-build-check.md` | Sous-agent qui lance BUILD.bat et rend un compte rendu. |

---

## 3. Architecture technique

```
ROM .nds du joueur
   │  tools/extract_rom.py, tools/region.py (décalage USA -> EU)
   ▼
recomp/ (Python)  ──►  build/recomp/*.c  (C généré, jamais versionné)
   │                         │
   │                         ▼
   │                  compilé + lié avec
   ▼                         ▼
config/usa/arm9 (symboles)   runtime/ (C, SDL2 + OpenGL)  ──►  game/dqix.exe
```

- **`recomp/`** traduit l'ARM9/ARM7 en C. `hle.txt`, `hle7.txt`, `hooks.txt`,
  `patches.txt`, `interpret.txt` règlent les cas particuliers. `hooks.txt`
  permet d'envelopper une fonction du jeu (`hook_<Nom>` appelle `<Nom>_orig`).
- **ROM européenne** : le code ARM9 principal est décalé de +0x10 (sélecteur de
  langue de 16 octets) ; overlays, ARM7 et variables du jeu ont les mêmes
  adresses. `tools/region.py` produit `build/region`, `rom_is_eur()` au runtime.
  Menu Langue EN/FR/DE/IT/ES (`settings.ini language`).
- **`runtime/`** (≈ 25 000 lignes) :
  - matériel DS : `gpu2d.c` (2D par ligne, 256x192), `gpu3d.c` (géométrie),
    `gl_render.c` (3D sur la carte graphique en mode Enhanced, composition,
    post-traitements), `sound.c`, `card.c` (sauvegarde EEPROM au format DS),
    `input.c`, `controls.c` (clavier, souris = stylet, manette, `controls.ini`) ;
  - interface PC : `frontend_sdl.c` (fenêtre, écran unique, HUD de combat,
    pop-up, magasins, `settings.ini`), `menu_win32.c` + `menu.h` (barre de
    menus Windows), `gamemenu.c` (menu d'options dans le jeu), `uitext.c` +
    `lang/*.txt` (traductions de l'interface PC), `hints.c` (boutons de manette),
    `report.c` (rapport F10, replay déterministe), `update.c` (mises à jour) ;
  - ajouts du fork : `hdtext.c` (texte HD, police Source Sans 3), `hdmovie.c`
    (films HD), `mapmarks.c` (marqueurs de carte, plans d'intérieurs, lecteurs
    de données ROM `romdata.h`), `journal*.c` (Journal), `autosave.c`,
    `bootskip.c`, `cheats.c`, `farm.c` ;
  - réseau (**ne pas toucher**) : `wifi.c`, `wfc.c`, `net.c`, `relay.c`,
    `upnp.c`, `portmap.c`, `session.c`, `air.c`, `hosting.c`, `third_party/slirp`.
- **Outils** : `tools/build-game.sh` (fait tout, appelé par BUILD.bat sous
  MSYS2 ou directement sous Linux), `tools/textures-hd.c` (upscale IA méthode 5),
  `tools/textures-rom.c` (sort les 12 670 textures 3D de la ROM),
  `tools/textures-check.c` (contrôle d'un pack HD), `tools/lang_check.py`.
- **Modes d'affichage** : Classic (exact DS, deux écrans empilés, secours sans
  OpenGL 3.3) et Enhanced (3D GPU, widescreen, écran unique : c'est le mode par
  défaut et celui de toutes les nouveautés).
- **Identifiants de menu** (`runtime/menu.h`) : chaque fonctionnalité reçoit une
  plage. Occupées : 100-299 (base), 296-299, 300-303, 310, 320-321, 330-332,
  340-347, 350-352, 360, 365, 370, 380-382, 390-391, 400-409 (Journal),
  410-419 (combats), 420-429 (langues de l'interface), 430 (abandonné),
  600+ (polices), 700+ (cheats). **Libres : 440-599 et 800-999.**
- **Adresses RAM connues** (identiques USA/EU) : or `0x020F6D48`, fiches de
  personnages tous les `0x964` octets à partir de `0x020F3888` (héros
  `0x020F54B4`), état des quêtes `0x02108B10`, recettes connues `0x020FAE9C`,
  sac `0x020F5DE8`… Voir `docs/TECHNICAL.md` et les commentaires des fichiers.

---

## 4. Compiler et tester

### Sur Windows (PC de Romuald)
```
cmd /c "set DQIX_NOPAUSE=1&& <dossier>\BUILD.bat"
```
Exit 0 = OK, résultat dans `<dossier>\game\`. BUILD.bat installe MSYS2
(`C:\msys64`) la première fois. La ROM = premier `.nds` de `rom\`, sinon le
chemin de `rom\last-rom.txt`. Premier build : plusieurs minutes.
Avertissement connu sans gravité : `cmp: commande introuvable`.

### Dans le cloud Linux (là où les Claude testent)
**Romuald ne veut pas de tests de jeu sur son PC.** Tous les tests se font sur
Linux sous xvfb, avec la ROM **EU et la ROM USA**.
```
apt-get install -y libsdl2-dev python3-yaml xvfb
sh tools/build-game.sh <rom.nds>     # ~6 min la première fois
```
Ensuite, après une modification du runtime, relancer seulement la ligne
`make -f build.mk ... game/dqix core` de `tools/build-game.sh` (quelques secondes).

Outils de test :
- `python3 tests/semantics/run.py` : 1,2 million de vérifications ARM/Thumb.
- `python3 tests/verify_decode.py build/extract build/region/arm9` : décodeur
  contre LLVM (727 987 instructions EU, 0 désaccord).
- `tests/gpu3d/t3d.c` : petit rendu 3D de contrôle.
- **Test de fumée avant chaque fusion** : `tests/smoke/run.py` lance le jeu avec
  une sauvegarde et une recette d'entrées, capture des images clés (église de
  Stornway, ville avec mini-carte, combat contre un Funghoul) et les compare
  aux références. Le jeu est déterministe : écart 0,00 attendu. La sauvegarde,
  la recette et les références sont **dérivées du jeu**, donc hors du dépôt
  (dans le dossier privé du projet de Romuald ; à recréer ou à transmettre
  entre humains).
- Variables utiles : `DQIX_INPUT_FILE` (rejouer des entrées), `DQIX_WINSHOT_AT`
  (capture de la fenêtre à une image donnée), `DQIX_CHEAT_CODE`,
  `DQIX_RAMDUMP`, `DQIX_PIXSRC`, `DQIX_GL_PROBE`. Pour de grandes captures :
  fenêtre (pas plein écran) `window_scale = 7` sur un écran xvfb 3000x1600,
  `render_scale = 2`.
- Menus Windows : compilation croisée mingw-w64 et test sous wine + xdotool.

**Toute modification visible** (interface, graphisme) doit être montrée à
Romuald en **captures avant/après** avant de demander son accord.

---

## 5. Comment le projet est organisé aujourd'hui

Romuald travaille dans un **projet Claude** (privé) :

- un **coordinateur** (Claude) dans la discussion principale : Romuald y écrit
  ses demandes, le coordinateur les range ;
- un **fil (thread) par sujet**, chacun tenu par un Claude, nommé
  « [Rôle] Sujet » ;
- une **mémoire de projet** partagée par tous ces Claude ;
- un **dossier partagé** pour les captures, études et fichiers de test.

### Les 5 rôles

| Rôle | Fils | Périmètre | Possède (seul à écrire) |
|---|---|---|---|
| Coordinateur | discussion principale, « [Coord] … » | Répartit, suit, relaie | — |
| Graphismes et interface | « [Graph] … » | Textures HD, films HD, skins, style des fenêtres | `tools/textures-*.c`, `tools/*movie*.py`, `runtime/hdmovie.*`, `skins/` |
| Langues et ressources | « [Langues] … » | ROM EU, langues, textes, polices | `tools/region.py`, `tools/extract_rom.py`, `config/`, `runtime/hdtext.*`, `runtime/text.*`, `fonts/` |
| Développement moteur et QoL | « [Dev] … » | Recompilateur, runtime, écran unique, caméra, contrôles, confort | `recomp/`, `hle*.txt`, `hooks.txt`, `patches.txt`, `runtime/` (sauf ci-dessus et fichiers partagés), `tests/` |
| Intégration, tests et revue | « [Intégration] … » | Revue, fusion, build, versions | `main`, `docs/SUIVI.md`, `CLAUDE.md`, `docs/roles/`, `.claude/`, scripts de build, `release-notes/` |

**Fichiers partagés** (plusieurs rôles y touchent ; on annonce au coordinateur
la branche qui les modifie) : `runtime/gl_render.c`, `runtime/menu_win32.c`,
`runtime/frontend_sdl.c`, `runtime/gpu3d.c`, `build-windows.sh`,
`tools/build-game.sh`, `build.mk`. En pratique aussi `runtime/menu.h`,
`runtime/controls.c`, `runtime/mapmarks.c`, `runtime/gpu2d.c`.

### Le cycle d'une fonctionnalité
1. Romuald demande dans la discussion principale ; le coordinateur ouvre un fil
   « [Rôle] Sujet » (ou relaie au fil existant).
2. Le fil travaille sur sa branche (dans le cloud : `claude/project-thread-xxxx` ;
   sur le PC : `<role>/<sujet>` dans un worktree).
3. Il compile et teste dans le cloud (EU + USA, test de fumée), fait des
   captures, et envoie un **compte rendu** : Objectif · Fichiers modifiés ·
   Tests exécutés (commande + résultat réel) · Résultats · Risques ·
   Prochaine action · État Git.
4. Romuald regarde les captures et écrit « ok push » puis « fusionne » **dans le
   fil**. Le fil pousse, ouvre la PR, la fusionne.
5. Les PR passent **une par une**, chaque branche rebasée sur `main` juste avant.
6. Le dossier de jeu est mis à jour (`git pull --ff-only` + BUILD.bat) seulement
   quand Romuald le demande.
7. Une version GitHub est préparée quand Romuald la demande (environ une fois
   par semaine) ; c'est lui qui clique sur « Publish release ».

### Fils permanents utiles
- « [Dev] Bugs signalés » : tous les rapports de bug (tester EU **et** USA).
- « [Intégration] Fusions et build », « [Intégration] Releases hebdomadaires ».
- « [Coord] Veille des versions » : surveille chaque jour les nouvelles
  versions du projet d'origine.

---

## 6. Règles permanentes (décisions de Romuald)

Ces règles valent pour **tous** les Claude, des deux équipes.

1. **Aucune modification du multijoueur, du Wi-Fi, de DQVC ou du réseau**
   (`wifi.c`, `wfc.c`, `net.c`, `relay.c`, `upnp.c`, `portmap.c`, `session.c`,
   `air.c`, `hosting.c`, `third_party/slirp`, `tools/relay/`).
2. **Ne jamais toucher aux boîtes de combat** (commandes Attaquer/Examiner,
   panneaux PV/PM). Le contour blanc existe seulement sur « Expérience gagnée »,
   « Caractéristiques augmentées » et la boîte de texte du bas en combat.
3. **Pas de test de jeu sur le PC de Romuald.** Tests dans le cloud Linux, ROM
   EU et USA. Son PC ne sert qu'au build de son jeu, quand il le demande.
4. **Jamais de données du jeu sur GitHub** : pas de `*.nds`, `build/`, `game/`,
   sauvegardes, dumps, textures ou films upscalés, C généré. Vérifier
   `git status` avant chaque commit. Une version = le code source seul.
5. **Accord explicite de Romuald** avant tout push, fusion, suppression de
   branche ou de fichier, réécriture d'historique, et avant de toucher à
   `J:\DQIX-Enhanced`. Son accord doit être écrit dans le fil qui fait l'action.
6. **Captures avant/après** pour tout changement visible, montrées avant de
   demander l'accord.
7. **Textures HD : méthode 5** (Real-ESRGAN mélangé à l'original agrandi) pour
   toutes les textures.
8. **Une seule fenêtre**, comme un vrai jeu PC : carte en haut à droite, plus de
   seconde fenêtre ni d'options de disposition dans les menus.
9. Le style de fenêtre **Moderne** est le style par défaut.
10. **Une seule police** pour le texte HD : Source Sans 3.
11. Les options nouvelles sont **désactivées par défaut** sauf décision contraire ;
    les options inutiles sont **cachées**, pas supprimées (`all_options = 0`).
12. **Pas de portage de la v32** du projet d'origine (décidé le 28/10) ; la
    veille signale seulement les versions plus récentes.
13. Textes de l'interface PC : toujours via `T()` / `TW()` avec une entrée dans
    `lang/*.txt` (vérifier avec `tools/lang_check.py`).
14. Notes de version : **une ligne par puce** (pas de retour à la ligne forcé,
    sinon l'affichage GitHub casse), et la description fournie aussi dans un bloc
    prêt à copier.
15. Commits, branches et PR en anglais ; discussions en français.

Abandonné à la demande de Romuald : comparaison d'équipement dans les magasins
(29/10), travail sur la visibilité des dégâts et des altérations d'état en combat.

---

## 7. Ce qui a été fait, par thème

PR = pull request fusionnée dans `main` de `romusan495/DQIX-EnhancedEU`.

### ROM européenne et langues
- Correctif ROM EU (19/10) : décalage des adresses ARM9, stubs BIOS réordonnés,
  menu Langue EN/FR/DE/IT/ES. Vérifié : démarrage, 5 langues, création du
  personnage, premier combat.
- Les sauvegardes passent de USA à EU et inversement (même `dqix.sav`).
- Interface PC traduite en 5 langues (barre de menus, fenêtres, menu d'options) :
  PR #47, #48.
- Police unique Source Sans 3, correction des lettres qui se mélangeaient sous
  la plaque de nom : PR #55.

### Écran unique et carte
- Écran unique en exploration et en combat (HUD de combat par-dessus la 3D) : PR #2.
- Mini-carte en haut à droite, une seule fenêtre, options de disposition
  retirées : PR #21 ; touche M / R3 pour la cacher : PR #23 ; pas de mini-carte
  avant le début de l'aventure : PR #28.
- Marqueurs sur la carte (coffres, pots, tonneaux, étagères, donneurs de
  quêtes) : PR #8, #9, #10. Plans des intérieurs depuis les collisions de la
  ROM : PR #26, style « carte de ville » : PR #27. Légende : PR #49.
- Fiches du haut (objets, équipement) côte à côte : PR #31, puis en pop-up au
  centre, magasins sur un écran : PR #42 ; boîte de description moderne avec
  l'icône : PR #44.

### Combats
- Disposition du HUD : PR #13, puis retour à la première disposition (panneaux
  en ligne en haut, commandes en bas à gauche, style DQ8) : PR #36 ; listes
  sorts/objets en bas à gauche : PR #38.
- Vitesse de combat x2/x3 : PR #19 ; x4, vitesse seulement pendant le tour,
  messages de victoire automatiques qui s'arrêtent à une montée de niveau : PR #53.

### Fenêtres et interface
- Style de fenêtres Minimal : PR #5, #7 ; contour blanc : PR #24 ; icônes de
  vocation en couleur : PR #12.
- Style **Moderne** (carré, fond sombre transparent, fin trait gris) sur toutes
  les fenêtres hors combat, par défaut : PR #50.
- Menu d'options dans le jeu (Échap ou bouton PS/Xbox/Home), utilisable à la
  manette, jeu en pause : PR #33, en style moderne : PR #46.
- Démarrage façon PC (logos passés) : PR #32.
- Boutons réels de la manette dans les indications (« X Menu »), option
  « confirmer avec le bouton du bas » : PR #49.
- Confirmation avant de quitter : PR #29. Barre de menus sans « Menu » vide : PR #47.
- Réglages par défaut = la configuration de Romuald, options inutiles cachées : PR #15.

### Graphismes
- Avant le dépôt (19-20/10) : texte HD, textures xBR, 2D lissée, packs de
  textures HD (dump + chargement), contours des personnages, films HD (6 films
  x4), outil `textures-hd` automatique.
- Méthode 5 pour les textures HD : PR #1. Polices PC des dialogues : PR #3.
- PNJ lointains, bulles et objets au sol plus nets sans raccords : PR #40, #56.
- Ombres douces dans les coins et contrôle des packs de textures
  (`textures-check`) : PR #54. Fins traits sombres autour des formes : PR #56.
- Toutes les textures 3D de la ROM extraites (`textures-rom`) : PR #58, puis
  extraites et upscalées automatiquement à chaque build : PR #59.

### Confort
- Journal (Moderne > Journal, touche J / L3) : quêtes, progression, vocations :
  PR #51 ; alchimie (recettes connues, ingrédients manquants) et objets : PR #52.
- Sauvegarde automatique toutes les 10 minutes (désactivée par défaut) : PR #41.
- Copies de sauvegarde, monstres sur la carte : PR #19.
- Menu Cheats Action Replay (codes de données seulement), valeurs
  personnalisables : PR #20, #22.
- Farm d'or automatique (Ctrl+G), remappage clavier/manette (avant le dépôt).

### Bugs et outils
- Widescreen cassé en mode Classic : PR #16. Rapport F10 plus complet : PR #17.
- Bugs signalés par un joueur (église noire, taches à la montée de niveau,
  plume rose, bestiaire) : PR #34 ; cadre rose des magasins, sprites coupés : PR #40.
- Tests automatiques réparés et test de fumée : PR #45.
- Organisation (CLAUDE.md, rôles, SUIVI, agent de build) : PR #4.
- Mises à jour pointées sur ce dépôt et branche `stable` : PR #6.

---

## 8. Versions publiées

Une version = un tag `v31.N`, la branche `stable` déplacée dessus, une note
`release-notes/v31.N.md`, seulement le *Source code (zip)* de GitHub. Les
joueurs déjà installés reçoivent la mise à jour au démarrage.

| Version | Date | Contenu principal |
|---|---|---|
| v31.1 | 22/10 | ROM EU, écran unique, marqueurs de carte, style minimal |
| v31.2 | 22/10 | Icônes de vocation, disposition de combat |
| v31.3 | 23/10 | Réglages par défaut, correctif Classic, rapport F10 |
| v31.4 | 23/10 | Vitesse de combat, cheats, mini-carte, contour blanc |
| v31.5 | 23/10 | Plans des intérieurs, confirmation pour quitter |
| v31.6 | 26/10 | Fiches côte à côte, démarrage rapide, bugs |
| v31.7 | 27/10 | Panneaux de combat en haut |
| v31.8 | 28/10 | Listes de combat |
| v31.9 | 28/10 | Pop-up, magasins sur un écran, sauvegarde auto |
| v31.10 | 29/10 | Menu d'options en jeu, interface traduite, Journal, combats x4, style Moderne, Source Sans 3 |

Prochaine : v31.11 (contiendra au moins #58 et #59 : textures de la ROM
extraites et upscalées automatiquement).

---

## 9. Relations avec le projet d'origine (sackchief)

- PR #8 sur `sackchief/dqix-enhanced` (ROM EU + premières options graphiques) :
  toujours ouverte, mais sackchief a intégré le correctif EU et le menu Langue
  dans sa v32 en créditant Romuald.
- PR #14 : écran unique en combat, proposée depuis le fork.
- Sauvegarde automatique portée sur v32, branche poussée sur le fork, PR ouverte
  par Romuald.
- La session cloud ne peut pas ouvrir de PR sur le dépôt de sackchief : Romuald
  les ouvre lui-même avec un lien préparé.
- sackchief a suggéré une caméra un peu dézoomée en combat (pas encore fait).
- Communauté Discord du projet d'origine : https://discord.gg/CczgRv36NK

---

## 10. Chantiers en cours et pistes restantes

### En cours (29/10 après-midi)
- **[Dev] Rang des coffres sur la mini-carte** : afficher le rang (S à I) des
  coffres dans les grottes, et si possible les rangs possibles avant d'entrer
  (comme le site yabd.org). Rangs trouvés dans `randTD.bin`, banc de test de
  grotte prêt, affichage pas encore codé.
- À vérifier par Romuald sur sa vraie sauvegarde : les recettes connues du Journal.
- Le dossier de jeu de Romuald n'a pas forcément été recompilé avec les
  dernières PR.

### Feuille de route restante (détail dans `docs/SUIVI.md`)
| Priorité | Piste | Remarque |
|---|---|---|
| P1 | Caméra dézoomée en combat | Sans toucher aux boîtes |
| P1 | Liste des monstres et journal des quêtes du jeu en pop-up | Recette de test d'abord |
| P1 | Effets visuels (éclairage, eau, météo) | Risque élevé, options désactivées par défaut |
| P2 | Retirer le code mort des anciennes dispositions (`MI_STACKED`, `MI_SIDE`, `g_wide_pop`…) dans `frontend_sdl.c` | Seulement avec l'accord de Romuald |
| P2 | Cartes au trésor, grottes explorées, collections (monstres vaincus) | Grottes générées depuis une graine : générateur à réécrire |
| P2 | Plans des grottes (Z00Mxx) sur la carte | Pas d'image dans la ROM |
| P3 | Contenus (défis, encyclopédie, personnalisation) | Après stabilisation |
| Divers | `cmp` manquant sous MSYS2 | Sans gravité |
| Long terme | Étude « vers un vrai jeu PC » (23/10) : nommer le code, son natif, 60 fps, réécriture progressive ; Romuald a évoqué une décompilation complète + Godot | Rien de commencé |

---

## 11. Coordonner deux équipes Claude sur le même dépôt

Le projet Claude de Romuald est **privé** : le Claude du deuxième développeur ne
peut ni y entrer, ni lire sa mémoire, ni son dossier partagé. **Le point de
rencontre est le dépôt GitHub.** Tout ce qui doit être partagé y vit (code,
règles, suivi), sauf les données du jeu, qui passent d'humain à humain.

### Principes
1. **Une seule `main`, une seule source.** Personne ne pousse directement sur
   `main` ; tout passe par une PR.
2. **Romuald garde le dernier mot sur `main`** (fusions, versions, `stable`),
   sauf s'il délègue explicitement au deuxième développeur un périmètre.
3. **Chaque équipe a son préfixe de branche**, pour voir d'un coup d'œil qui
   fait quoi :
   - équipe Romuald : `claude/project-thread-*` (cloud) et `<role>/<sujet>` ;
   - équipe 2 : **`remake/<sujet>`** (ou le pseudo du développeur), jamais un
     préfixe de l'autre équipe.
4. **`docs/SUIVI.md` est le registre commun.** Chaque tâche de l'équipe 2 y a sa
   ligne (colonne « Rôle » = `Équipe 2` ou son rôle). La ligne est ajoutée dans
   la PR de l'équipe 2 elle-même ; l'Intégration de Romuald la garde à jour à la
   fusion.
5. **Annoncer avant de toucher un fichier partagé.** Ouvrir tout de suite une
   **PR en brouillon** (draft) dès le premier commit, avec dans sa description la
   liste des fichiers touchés. Le Claude de l'autre équipe regarde les PR
   ouvertes avant de commencer un sujet (`list_pull_requests`) et signale tout
   chevauchement dans la PR.
6. **Possession des fichiers** : les rôles de `docs/roles/` s'appliquent aux deux
   équipes. Si l'équipe 2 prend un domaine entier (par exemple le « remake »),
   on lui crée un rôle et un fichier `docs/roles/remake.md` qui liste les
   fichiers et dossiers qu'elle possède (idéalement un dossier à elle, par
   exemple `remake/`). Ainsi les deux équipes ne se marchent pas dessus.
7. **Plages réservées** pour éviter les collisions :
   - identifiants de menu (`runtime/menu.h`) : **équipe 2 = 800-899** ;
   - clés de `settings.ini` : noms nouveaux, à annoncer dans la PR ;
   - textes d'interface : toujours via `lang/*.txt`.
8. **Une PR à la fois dans `main`**, rebasée sur `main` juste avant la fusion,
   et le test de fumée passé (EU + USA).
9. **Mêmes règles permanentes** (section 6) pour les deux équipes : pas de réseau,
   pas de boîtes de combat, pas de données du jeu, captures pour tout visuel,
   accord avant fusion.
10. **Communication** :
    - les deux humains se parlent directement pour les décisions ;
    - entre Claude, on écrit dans les **PR et issues GitHub** (commentaires
      signés « Generated by Claude Code ») ; une **issue épinglée
      « Coordination »** peut servir de fil commun pour les annonces (« je prends
      tel sujet », « je touche `frontend_sdl.c` cette semaine ») ;
    - Romuald relaie à son coordinateur ce que l'équipe 2 annonce, et
      inversement.

### Qui fait quoi à la fusion
| Étape | Équipe 2 | Équipe Romuald |
|---|---|---|
| Branche, code, tests cloud EU + USA, captures | oui | — |
| PR (brouillon puis prête) avec compte rendu commun et ligne SUIVI | oui | — |
| Revue (code-simplifier, review-pr, test de fumée) | peut la faire elle-même | Intégration relit |
| Accord de fusion | — | **Romuald** |
| Fusion, version, `stable`, build du dossier de jeu | — | Intégration, avec accord de Romuald |

---

## 12. Mise en place pour le deuxième développeur (pas à pas)

Pour retrouver la même organisation que Romuald avec son propre Claude :

1. **Accès GitHub** : Romuald l'ajoute comme **collaborateur** de
   `romusan495/DQIX-EnhancedEU` (décidé le 29/10) : il pousse ses branches
   `remake/…` directement sur le dépôt. Connecter GitHub à Claude et installer
   l'application Claude sur le dépôt.
2. **Créer un projet Claude** (claude.ai, Claude Code sur le web) avec ce dépôt
   comme source.
3. **Instructions du projet** : coller le bloc ci-dessous (version adaptée de
   celles de Romuald).
4. **Ses propres ressources, jamais sur GitHub** : sa ROM (USA ou EU), une
   sauvegarde de test, les références du test de fumée (à demander à Romuald ou
   à refaire avec `tests/smoke/run.py`).
5. **Premier fil** : demander à son Claude de lire `CLAUDE.md`,
   `docs/roles/`, `docs/SUIVI.md`, ce récapitulatif, et de proposer un plan pour
   le « remake » sans rien pousser.
6. **Rôles** : son coordinateur peut reprendre les mêmes rôles (fils
   « [Dev] … », « [Graph] … ») ou un rôle unique « Remake ». Les fils restent
   « [Rôle] Sujet », un fil par sujet.
7. **Annoncer** son premier sujet dans l'issue « Coordination » et par une PR
   en brouillon.

### Instructions de projet à coller (équipe 2)
```
Règles communes — DQIX Enhanced EU, équipe 2 (remake)
Tu travailles sur un portage natif PC de Dragon Quest IX déjà très modifié,
avec une autre équipe Claude (celle de Romuald) sur le même dépôt.

Au démarrage : lis CLAUDE.md, docs/roles/, docs/SUIVI.md et docs/RECAP.md
du dépôt romusan495/DQIX-EnhancedEU. Regarde les PR ouvertes avant de
commencer un sujet.

Obligatoire
* Examine le code réel ; n'invente aucune fonction, API ou structure.
* Préserve le travail existant ; n'écrase jamais celui de l'autre équipe.
* Branches préfixées remake/<sujet>. Jamais de push sur main.
* PR en brouillon dès le premier commit, avec la liste des fichiers touchés.
* Fichiers partagés (runtime/gl_render.c, runtime/menu_win32.c,
  runtime/frontend_sdl.c, runtime/gpu3d.c, runtime/menu.h, build.mk,
  tools/build-game.sh, build-windows.sh) : annonce-les dans l'issue
  « Coordination » avant de les modifier.
* Identifiants de menu : plage 800-899 seulement.
* Aucune modification du multijoueur / Wi-Fi / DQVC / réseau.
* Ne touche jamais aux boîtes de combat (commandes, PV/PM).
* Jamais de ROM, build/, game/, sauvegarde, dump, texture ou film upscalé,
  C généré sur GitHub.
* Tests dans le cloud Linux sur les ROM EU et USA ; n'annonce jamais un test
  non exécuté. Captures avant/après pour tout changement visible.
* Fusion dans main seulement avec l'accord de Romuald.

Compte rendu à chaque jalon : Objectif · Fichiers modifiés · Tests exécutés
(commande + résultat réel) · Résultats · Risques · Prochaine action · État Git.
Commits, branches et PR en anglais.
```

---

## 13. Le projet remake (Godot) : à quoi sert ce dépôt

Précisé par Romuald le 29/10. Le but final est un **remake** : quand la
**décompilation** du jeu sera terminée, le jeu sera **réécrit avec le moteur
Godot**, pensé dès le départ pour le PC, sur **un seul écran**, comme si le
second écran de la DS n'avait jamais existé.

Ce dépôt (le portage recompilé) est donc **le banc d'essai et la référence**
du remake. Tout ce qui s'y fait doit pouvoir resservir. Concrètement, ce que
le remake récupérera :

| Ce qui est appris ici | Où le trouver | Utilité pour le remake |
|---|---|---|
| Les choix d'interface validés par Romuald (écran unique, carte en haut à droite, fiches en pop-up, HUD de combat, style Moderne, Journal, indications de boutons) | sections 6 et 7, captures, `release-notes/` | Cahier des charges de l'interface Godot, déjà testé en jeu |
| Ce qui se trouvait sur l'écran du haut et où on l'a mis (carte, fiches, panneaux de combat, magasins) | `frontend_sdl.c` (`hud_update`, `pop_update`, `shop_update`), `mapmarks.c` | Inventaire des écrans à redessiner pour un seul écran |
| Les données du jeu et leur format (recettes, quêtes, objets, compétences, collisions des cartes, rangs des coffres, films) | `mapmarks.c`, `romdata.h`, `journal*.c`, mémoire du projet | Lecteurs de données réutilisables pour importer les ressources dans Godot |
| Les adresses RAM (personnages, sac, quêtes, or, vocations) | section 3, `cheats.c`, `journal*.c` | Recoupement avec la décompilation, tests de fidélité |
| Les règles permanentes (pas de réseau, boîtes de combat, pas de données du jeu publiées) | section 6 | Restent valables, sauf décision contraire de Romuald |
| Le test de fumée et les recettes d'entrées (jeu déterministe) | `tests/smoke/` | Comparer le remake au jeu d'origine scène par scène |

Conséquences pour les deux équipes, dès maintenant :
- **Documenter chaque décision d'interface** (pourquoi, capture avant/après)
  dans la PR et dans `docs/SUIVI.md` : c'est la matière première du remake.
- **Garder la lecture des données du jeu séparée de l'affichage** (comme
  `romdata.h`), pour pouvoir la réutiliser ailleurs.
- **Penser « PC d'abord »** : une nouveauté se conçoit pour un seul écran,
  clavier, souris et manette, pas comme un réarrangement des deux écrans DS.
- Le remake vivra dans **un dépôt séparé**, créé le moment venu. Il sera
  **privé ou sans données du jeu** : du code réécrit à partir de la
  décompilation et les ressources du jeu restent dérivés du jeu et ne se
  publient pas. Seul un lanceur qui lit la ROM du joueur, comme ici, peut être
  public. À décider avec Romuald avant de commencer.
- La décompilation de référence est celle de **dqix-decomp** (domaine public),
  dont vient déjà `config/usa/arm9`. Environ 9 % des fonctions y ont un nom
  aujourd'hui : nommer les fonctions utiles au passage aide les deux projets.

---

## 14. Questions encore ouvertes

- ~~Accès au dépôt~~ : réglé le 29/10, le deuxième développeur est
  **collaborateur** du dépôt.
- ~~Le « remake »~~ : réglé le 29/10, voir la section 13 (réécriture Godot
  après la décompilation, dans un dépôt séparé ; ce dépôt sert de référence).
- La décompilation : qui la mène (dqix-decomp, le 2e développeur, une équipe
  Claude ?) et à quel rythme ? À préciser entre Romuald et le 2e développeur.
- ~~Ajouter ce récapitulatif au dépôt~~ : fait le 29/10 (`docs/RECAP.md`).
