# Suivi du projet

Tenu par le rôle Intégration (voir `docs/roles/integration.md`). Les autres
threads envoient leur compte rendu au coordinateur, qui relaie. Dernière mise à
<<<<<<< HEAD
jour : 2026-10-23.
=======
jour : 2026-10-28 (audit du cahier des charges, `main` = `19741c6`, v31.9).
>>>>>>> origin/main

États : `à faire` · `en cours` · `en revue` · `fusionné` · `en pause` · `abandonné`.

## Tâches

| Tâche | Rôle | Thread | Branche / PR | État | Dépend de | Prochaine étape |
|---|---|---|---|---|---|---|
<<<<<<< HEAD
| Écran unique et widescreen en combat | Dev | [Dev] Écran unique et combat | `ecran-unique`, PR #2 | fusionné | — | Dans le dossier de jeu depuis le 21/10 |
| Caméra dézoomée en combat | Dev | [Dev] Écran unique et combat | — | à faire | — | Plan avant de coder |
| Marqueurs sur la carte : coffres, pots, tonneaux (phase 1) | Dev | [Dev] Écran unique et combat | `claude/project-thread-6dtla9`, PR #8 | fusionné | — | Dans le dossier de jeu (v31.1) |
| Carte : bibliothèques et contenu des bâtiments (phase 2) | Dev | [Dev] Écran unique et combat | `claude/project-thread-6dtla9`, PR #9 | fusionné | PR #8 | Dans le dossier de jeu (v31.1) |
| Carte : marqueurs de quêtes avec leur état (phase 3) | Dev | [Dev] Écran unique et combat | `claude/project-thread-6dtla9`, PR #10 | fusionné | PR #9 | Dans le dossier de jeu (v31.1) |
| Textures HD méthode 5 (outil) | Graph | [Graph] Textures HD et interface | `textures-hd-melange`, PR #1 | fusionné | — | Installer les textures restantes |
| Style de fenêtres « Minimal » | Graph | [Graph] Textures HD et interface | `graph/fenetres-minimal`, PR #5 | fusionné | — | Retour de Romuald en jeu |
| HUD de combat « Minimal » (cartes transparentes, couleur du membre) | Graph | [Graph] Textures HD et interface | `graph/combat-minimal`, PR #7 | fusionné | PR #5 | Dans le dossier de jeu (v31.1) |
| Icônes de métiers modernes (sprites HD) | Graph | [Graph] Textures HD et interface | `graph/icones-metiers`, PR #12 | fusionné | — | Dans le dossier de jeu |
| HUD de combat : panneaux en bas à gauche, fenêtres XP/niveau | Graph | [Graph] Textures HD et interface | `graph/combat-disposition`, PR #13 | fusionné | PR #7 | Dans le dossier de jeu |
| Style Minimal : contour blanc conservé | Graph | [Graph] Textures HD et interface | `graph/contour-blanc`, PR #24 | fusionné | PR #5 | Dans le dossier de jeu (v31.4) |
| Réglages par défaut du projet | Dev | [Dev] Réglages par défaut | `claude/project-thread-7yvhih`, PR #15 | fusionné | — | Dans le dossier de jeu (v31.4) |
| Widescreen en mode Classic | Dev | [Dev] Bugs signalés | `dev/widescreen-classic`, PR #16 | fusionné | — | Dans le dossier de jeu (v31.4) |
| Rapport de bug F10 | Dev | [Dev] Bugs signalés | `dev/rapport-f10`, PR #17 | fusionné | — | Dans le dossier de jeu (v31.4) |
| Modern : vitesse des combats x2/x3, monstres sur la carte, copies de sauvegarde, correctif farm d'or | Dev | [Dev] Idées QoL inspirées de Final Fantasy | `claude/project-thread-vewvl8`, PR #19 | fusionné | — | Dans le dossier de jeu (v31.4) |
| Menu Cheats Action Replay, valeurs personnalisées | Dev | [Dev] Menu de cheats Action Replay | `claude/project-thread-yv3b7m`, PR #20 et #22 | fusionné | PR #19 | Dans le dossier de jeu (v31.4) |
| Mini-carte, touche M/R3 | Dev | [Dev] Mini-carte | `claude/project-thread-6h3zvz`, PR #21, #23, #28 | fusionné | — | #28 pas encore dans le dossier de jeu |
| Carte : plan des pièces à l'intérieur des bâtiments | Dev | [Dev] Mini-carte | `claude/project-thread-6h3zvz`, PR #26 et #27 | fusionné | — | Pas encore dans le dossier de jeu |
| Polices PC dans les dialogues (9 polices) | Langues | [Graph] Textures HD et interface (ex « Moderniser ») | `dialogues-police-pc`, PR #3 | fusionné | — | Retour de Romuald en jeu |
| Menu Langue EN/FR/DE/IT/ES | Langues | à créer | — | à faire | — | Vérifier ce que fait déjà le menu Langue du commit EU |
| Wi-Fi / DQVC avec la ROM EU | Dev | à créer | — | à faire (non testé) | — | Tester avec la ROM EU |
| BUILD : « cmp: commande introuvable » (`tools/build-game.sh:99`) | Intégration | à créer | — | à faire | — | Installer `diffutils` dans MSYS2 ou éviter `cmp` ; le build réussit quand même |
| `tests/gpu3d/t3d.c` ne compile plus (`gpu3d_init` non déclaré) | Dev | à créer | — | à faire | — | Mettre le test à jour avec l'API actuelle de `gpu3d.c` |
| Releases GitHub hebdomadaires (v31.1, v31.2...) | Intégration | [Intégration] Releases hebdomadaires | `claude/project-thread-on5r0q`, PR #6 et #11 (notes v31.1) (touche `runtime/update.c/.h`, `version.h` : fichiers Dev, accord du coordinateur) | fusionné | — | v31.1 à v31.4 publiées (PR #14, #18, #25), branche `stable` |
| Réorganisation Claude Code | Intégration | [Orga] Réorganisation | `orga/claude-config`, PR #4 | fusionné | — | — |

## Fichiers partagés touchés par des branches

| Fichier | Branche / PR | Ce qui change |
|---|---|---|
| `runtime/gl_render.c`, `gl_render.h` | `graph/fenetres-minimal` #5, `graph/combat-minimal` #7, `claude/project-thread-6dtla9` #8 | Style Minimal ; HUD de combat Minimal (`hud_update` appelé avant `gl_compose`) ; dessin des marqueurs de carte |
| `runtime/frontend_sdl.c` | #5, #7, #8 | Idem (appels du style Minimal, du HUD et des marqueurs) |
| `runtime/menu_win32.c`, `menu.h` | #5, #8 (`MI_MAPMARKS` 340/341), #10 (libellés « Map: … » du menu Modern) | Entrées de menu |
| `runtime/gpu2d.c` | #5, #7 | Fenêtres et HUD Minimal |
| `runtime/update.c/.h`, `version.h` | `claude/project-thread-on5r0q` #6 | Releases GitHub |
| `runtime/mapmarks.c/.h` (nouveau) | `claude/project-thread-6dtla9` #8, #9, #10 | Marqueurs de carte |
| `runtime/gl_render.c`, `gpu2d.c`, `build.mk`, `tools/build-game.sh` | `graph/icones-metiers` #12 | `draw_sprites` dans `gl_compose`, sprites HD du pack de textures (`runtime/hdsprite.h`), `tools/job_icons.py` vers `game/textures/jobs` |
| `runtime/frontend_sdl.c` | #13, #15, #16, #17, #19, #20, #21, #22, #23, #24, #26, #28 | Presque toutes les fonctionnalités passent par ce fichier : à fusionner une branche à la fois, en reprenant `main` avant chaque PR |
| `runtime/menu_win32.c`, `menu.h` | #15, #19, #20, #21, #22, #23, #26 | Entrées des menus Modern, Cheats et carte |
| `runtime/gpu2d.c` | #15 (`g_winstyle = 1` par défaut), #24 (contour blanc) | Style des fenêtres |
| `runtime/gpu3d.c`, `gpu3d.h`, `controls.c/.h` | #21, #23 | Mini-carte et touche M/R3 |
| `runtime/mapmarks.c/.h` | #19, #26, #27 | Monstres sur la carte, plans des pièces |
| `runtime/version.h` | #14, #18, #25 | Numéros de version des releases |
| `runtime/gpu3d.c`, `gl_render.c`, `frontend_sdl.c`, `gpu2d.c`, `mapmarks.c` | PR #34 (bugs de Dr.Stone + bestiaire, 618ff9a) | Fusionnée le 26/10 ; dans le dossier de jeu depuis le 26/10 |
| `runtime/frontend_sdl.c`, `mapmarks.c` | PR #36 (disposition de combat du début + 2 bugs de mise en page, 5733d56) | Fusionnée le 27/10 ; pas encore dans le dossier de jeu |
| `runtime/frontend_sdl.c` | PR #38 (listes Sorts/Objets/Aptitudes en bas à gauche en combat, a1490e2) | Fusionnée le 27/10 ; pas encore dans le dossier de jeu |
| `frontend_sdl.c`, `gl_render.c`, `gl_render.h` | PR #40 (cinématique, PNJ coupés, écran rose, mini-carte dans les magasins, 75d3804) | Fusionnée le 28/10 ; pas encore dans le dossier de jeu |
| `runtime/frontend_sdl.c`, `menu_win32.c`, `card.c`, `nds.h`, `menu.h`, `hooks.txt` (hook ov017 `0218d0ec`), nouveaux `runtime/autosave.c/.h` | `claude/project-thread-q9flgx` ([Dev] Sauvegarde automatique, 097d6ae), pas encore poussée | Conflit probable sur `frontend_sdl.c` avec [Dev] Fiches en pop-up au centre (en cours) |
| `runtime/frontend_sdl.c` (`window_fullscreen`, réglage `hdr_fullscreen_fix`), `menu_win32.c` (Affichage > « Normal brightness in fullscreen with HDR », `display_hdr_on`), `menu.h`, une ligne par `lang/*.txt` | `dev/hdr-fullscreen` ([Dev] Corriger la luminosité en plein écran HDR), en cours | [Dev] Corriger la chute de FPS au dialogue du roi risque aussi de toucher `frontend_sdl.c`/`gl_render.c` : surveiller les conflits |
| `runtime/frontend_sdl.c` | PR #29 (confirmation avant de quitter) | Fusionnée |
| `runtime/frontend_sdl.c`, `menu_win32.c`, `card.c`, `input.c`, `menu.h`, nouveau `bootskip.c` | `claude/project-thread-9e00md` ([Dev] Vers un vrai jeu PC, démarrage direct), en cours | Conflit probable sur `frontend_sdl.c` avec [Dev] Mini-carte (fiches d'objets, en cours) |
| `runtime/frontend_sdl.c`, `mapmarks.c`, `mapmarks.h` | `claude/project-thread-6h3zvz` ([Dev] Mini-carte, correctif des fiches d'objets, 7cd0b23), pas encore poussée | Conflit probable sur `frontend_sdl.c` avec `claude/project-thread-9e00md` |
| `runtime/frontend_sdl.c`, `menu_win32.c` (le menu devient un arbre portable, la barre Windows est construite depuis lui), `gl_render.c` (`gl_set_menu`, dessin par-dessus l'image), `build.mk` (nouveau `runtime/gamemenu.c`) | `claude/project-thread-9e00md` ([Dev] Vers un vrai jeu PC : menu d'options en jeu, Échap + bouton PS, 0747191), pas encore poussée | Gros changement de `menu_win32.c` : conflits probables avec toute branche qui ajoute des entrées de menu ; la fusionner tôt et faire reprendre `main` aux autres |
=======
| Fiches du haut en pop-up, magasins sur un écran | Dev | [Dev] Fiches en pop-up au centre | PR #42, #44 | fusionné (v31.9) | — | Boîte description magasin : fin de fusion dans ce thread |
| Cadre moderne sur toutes les fenêtres (+ fiches, confirmations, chargements du § 5.2) | Graph | [Graph] Cadre moderne sur toutes les fenêtres | `claude/project-thread-r80vbi` | en cours | PR #44 | Captures, accord de Romuald |
| Police unique sans noms coupés | Langues | [Langues] Police unique sans noms coupés | — | en cours | — | Compte rendu du thread |
| Menu d'options dans le jeu (Échap / bouton PS), puis style moderne | Dev | [Coord] Audit et feuille de route | PR #33, PR suivante | fusionné (#33 `e17a0dd`) ; style moderne en revue | — | — |
| Tests automatiques réparés, test de fumée `tests/smoke/run.py` | Dev | [Coord] Audit et feuille de route | PR #45 | fusionné | — | — |
| Mise à jour de ce suivi (audit, feuille de route, idées) | Intégration | [Coord] Audit et feuille de route | PR #45 | fusionné | — | — |
| Combats plus lisibles et plus rapides | Dev | [Dev] Combats plus lisibles et plus rapides | `claude/project-thread-ep4nol` | en cours | — | Sans toucher aux boîtes de combat |
| Inventaire et comparaison d'équipement | Dev | [Dev] Inventaire et comparaison d'équipement | — | en cours | — | Étude du sac en RAM |
| Alchimie, collections et cartes au trésor (onglets du Journal) | Dev | [Dev] Alchimie, collections et cartes au trésor | `claude/project-thread-vbq30f` | en cours | Journal | Ajoute ses onglets au Journal du fil Quêtes |
| Journal (onglets quêtes, progression ; alchimie et collections ajoutent les leurs) | Dev | [Dev] Journal des quêtes et progression | `claude/project-thread-utz7dx` | en cours | — | Un seul Journal, propriétaire = ce fil |
| Boutons manette à l'écran, légende de la carte | Dev | [Dev] Boutons manette et légende de la carte | `claude/project-thread-g6uxea` | en cours | — | — |
| Effets visuels, contrôle des packs de textures HD | Graph | [Graph] Effets visuels et contrôle des textures HD | — | en cours | — | — |
| Menus PC en français, accessibilité | Langues | [Langues] Menus PC en français et accessibilité | `claude/project-thread-7yyjwb` | en cours | PR #33 | Rebaser après la fusion de #33 |
| Caméra dézoomée en combat | Dev | — | — | à faire | — | Plan avant de coder (ne pas toucher aux boîtes de combat) |
| BUILD : « cmp: commande introuvable » (`tools/build-game.sh`) | Intégration | — | — | à faire | — | Installer `diffutils` dans MSYS2 ou éviter `cmp` ; le build réussit quand même |

### Fichiers partagés touchés par les fils en cours (2026-10-28 soir)
#33 est fusionnée : chaque fil rebase sur `main` avant sa PR, et les PR passent
une par une. Conflits probables signalés.

| Fichier | Fils |
|---|---|
| `runtime/frontend_sdl.c` | Boutons manette, Cadre moderne, Menus en français, Journal (Quêtes + Alchimie), Combats, menu moderne |
| `runtime/menu_win32.c`, `runtime/menu.h` | Boutons manette, Cadre moderne, Menus en français, Journal, Combats |
| `runtime/gl_render.c` (`.h`) | menu moderne (`gl_set_menu_box`, `draw_menu`), Boutons manette (3e overlay), Journal (calque) : mêmes lignes de `gl_present`, à accorder ; Cadre moderne ; Combats si besoin |
| `runtime/controls.c` / `.h` | Boutons manette, Menus en français, Journal |
| `build.mk` | Menus en français (`uitext.c`), Journal (`journal.c`) |
| `runtime/gpu2d.c` | Cadre moderne |
| `tools/build-game.sh`, `build-windows.sh` | Menus en français (copie de `lang/*.txt`) |
| `runtime/mapmarks.c` | Boutons manette (légende), Journal (lecteurs de la ROM rendus publics) |

Terminé depuis le 21/10 (détail dans `release-notes/v31.*.md`) : écran unique
#2, réglages par défaut #15, mini-carte et suppression des dispositions #21 #23
#28, marqueurs de carte, plans des intérieurs #26 #27, style de fenêtres minimal
#5 #7, contour blanc #24, icônes de vocations #12, HUD de combat #36 #38, cheats
#20 #22, confirmation pour quitter #29, fiches côte à côte #31 puis pop-up #42
#44, démarrage rapide #32, bugs #34 #40, sauvegarde auto #41, rapport F10 #17,
releases v31.1 à v31.9.
>>>>>>> origin/main

## Décisions

| Date | Décision | Raison |
|---|---|---|
| 2026-10-21 | Le dépôt `romusan495/DQIX-EnhancedEU` est la seule source ; le dossier de jeu ne change que par `git pull --ff-only` + BUILD.bat. | Des éditions directes dans le dossier de jeu ne revenaient pas au dépôt. |
| 2026-10-21 | Méthode 5 (IA + original mélangés) pour toutes les textures HD. | Choix de Romuald après comparaison. |
| 2026-10-21 | 5 rôles, un thread par fonctionnalité, un worktree par thread de dev. | Plan de réorganisation validé. |
| 2026-10-22 | Release GitHub hebdomadaire : sources seules (jamais ROM, exe ni données du jeu), publiée après le feu vert de Romuald. | Choix de Romuald ; seul son PC peut vérifier le build. |
| 2026-10-22 | Ne jamais toucher aux boîtes de combat (commandes, PV/PM). | Choix de Romuald. |
| 2026-10-22 | Pas de test de jeu sur le PC de Romuald : tests dans le cloud (Linux, xvfb), ROM EU et USA. | Demande de Romuald. |
| 2026-10-23 | Une seule fenêtre : la carte en haut à droite, plus de seconde fenêtre ; options de disposition retirées des menus. | Choix de Romuald (« comme un vrai jeu PC »). |
| 2026-10-28 | Multijoueur, Wi-Fi/DQVC et réseau hors périmètre (`wifi.c`, `wfc.c`, `net.c`, `relay.c`, `upnp.c`, `portmap.c`, `session.c`, `air.c`, `hosting.c`, `third_party/slirp`, `tools/relay/`). | Cahier des charges § 12 et consigne de Romuald. |

## Audit du 2026-10-28 (cahier des charges de modernisation)

Le cahier des charges est une vision cible. Cet audit compare ses exigences au
code de `main` (`19741c6`, v31.9) pour ne rien refaire et choisir la suite.

### Architecture constatée
- `recomp/` (Python) traduit le code ARM9/ARM7 de la ROM en C (`build/recomp`,
  jamais versionné) ; `tools/region.py` décale la carte des symboles USA pour la
  ROM EU. `tools/build-game.sh` fait tout (BUILD.bat sous MSYS2, ou Linux).
- `runtime/` (C, SDL2 + OpenGL) simule le matériel DS : `gpu2d.c` (2D par
  ligne, 256x192), `gpu3d.c` (géométrie) + `gl_render.c` (3D sur la carte
  graphique, mode Enhanced, widescreen), `sound.c`, `card.c` (sauvegarde
  EEPROM, compatible DS), `input.c`/`controls.c` (clavier, souris = stylet,
  manette SDL, `controls.ini`).
- Interface PC : `frontend_sdl.c` (fenêtre, disposition, HUD de combat
  `hud_update`, pop-up `pop_update`, magasins `shop_update`, `settings.ini`
  version 2), `menu_win32.c` (barre de menus Windows, options cachées si
  `all_options = 0`), `report.c` (F10, replay déterministe).
- Ajouts du fork : `hdtext.c` (texte HD), `hdmovie.c` (films HD), `mapmarks.c`
  (marqueurs, plans des intérieurs, magasins), `autosave.c`, `bootskip.c`,
  `cheats.c`, `farm.c`, pack de textures HD dans `gl_render.c` + `tools/textures-hd.c`.

### Matrice d'avancement
TV = terminé et vérifié · AA = implémenté, à améliorer · P = partiel ·
N = non implémenté · B = bloqué par une dépendance · ? = indéterminé.
« Vérifié » = testé dans le cloud sur les ROM EU et USA avec captures, et
validé par Romuald avant fusion.

| Domaine (§ du cahier) | État | Preuves | Remarques |
|---|---|---|---|
| Compilation reproductible (§4) | TV | `tools/build-game.sh` : build EU Linux exit 0 le 28/10 (~6 min) ; BUILD.bat sur le PC de Romuald | Avertissement `cmp` sous MSYS2 (tâche ci-dessus) |
| Tests automatiques (§4, §11) | AA | `tests/semantics/run.py`, `tests/verify_decode.py`, `tests/gpu3d/t3d.c` | `semantics` et `gpu3d` ne compilaient plus (réparés dans ce lot) ; `verify_decode` passe. Pas de test de non-régression du jeu en dehors des recettes cloud des threads |
| Journal d'erreurs (§4) | AA | `last-run.log`, F10 `report.c`, `DQIX_*` dans `docs/TECHNICAL.md` | Bruit répétitif (écritures VRAM non mappée en entrée de combat, null-pointer au démarrage) |
| Sauvegardes (§4) | TV | `card.c`, copies `save-backups/` (#19), sauvegarde auto #41 ; même `dqix.sav` pour EU et USA | — |
| Réglages utilisateur (§4) | TV | `settings_load/save`, `SETTINGS_VERSION 2`, `LOOK_KEYS` | Migration une seule fois des anciens réglages d'affichage |
| Mono-écran en exploration (§5.1, §2.2) | TV | `g_wide`, mini-carte `g_minimap` (#21 #23 #28), panneau « X Menu » déplacé (`gpu3d.c` `g_sign_move`) | Plus de seconde fenêtre ni d'option de disposition dans les menus |
| Mono-écran en combat (§5.1) | TV | `hud_update` (#2, #36, #38) | Boîtes de combat : décision permanente, ne pas y toucher |
| Fiches, titres, magasins (§5.1, §5.2) | TV | `pop_update`, `pop_place`, `shop_update`, `mapmarks_shop` (#42 #44) | Liste des monstres (modèle 3D en haut) encore côte à côte ; journal des quêtes jamais atteint en test |
| Vestiges double écran (§2.2) | P | `MI_STACKED`, `MI_SIDE`, `MI_CORNER`, `MI_TWO`, `MI_SWAP`, `g_wide_pop` dans `frontend_sdl.c` | Code mort : plus aucun menu ni touche ne les active. Le mode Classique (exact DS) reste à deux écrans empilés : c'est le secours sans OpenGL 3.3 |
| Plein écran, fenêtre, redimensionnement (§5.1) | AA | F11, `SDL_WINDOW_RESIZABLE`, rendu 3D Auto jusqu'à 12x | En plein écran la barre de menus est masquée : options inaccessibles sans quitter le plein écran (PR #33 y répond) |
| Interface homogène (§5.2) | P | Style minimal #5 #7, contour blanc #24, cadre moderne (en cours) | Fenêtres du jeu restylées progressivement ; interface PC (menus Windows) séparée et en anglais |
| Commandes clavier, souris, manette (§5.3) | AA | `controls.c` (remappage, `controls.ini`), manette branchée à chaud (`SDL_CONTROLLERDEVICEADDED`), marche analogique | Indications à l'écran = boutons DS (« X Menu ») ; aucun accès aux options à la manette sans PR #33 |
| Carte et marqueurs (§5.4) | TV | `mapmarks.c` (coffres, pots, étagères, quêtes, monstres, plans), touche M / R3 | Pas de plan pour les grottes générées (Z00Mxx) |
| Personnages, contours (§6.2) | AA | Contours DS / fin / aucun (`g_outline`), contour blanc #24 | Transparence vérifiée au cas par cas (raccords #40) |
| Textures HD (§6.3, §6.5) | AA | Dump `textures/dump`, `dump-2d`, pack `textures/**`, rechargement, `tools/textures-hd.c` (méthode 5) | Restaurer = décocher le pack ou supprimer le PNG. Manque un contrôle des packs (dimensions, alpha, fichiers invalides, comparaison avant/après) |
| Textes HD, polices (§6, §9) | AA | `hdtext.c`, 15 polices (#3) | Police unique en cours ([Langues]) |
| Films HD (§6) | TV | `hdmovie.c`, 6 films x4 | — |
| Effets, éclairage, eau, particules (§6.4) | N | — | Rien d'entrepris ; demande une étude de `gl_render.c` |
| Inventaire, tri, filtres, comparaison (§7.1) | N | Le jeu a déjà son tri et l'aperçu des statistiques à l'équipement | Ajout PC = lire le sac en RAM (structures non documentées) : étude d'abord |
| Journal des quêtes (§7.2) | P | Bulles de quêtes sur la carte (`mapmarks.c`) | Le journal du jeu est intact ; rien de plus |
| Progression, compétences (§7.3) | N | — | Études seulement |
| Combats (§7.4) | AA | Vitesse x1/x2/x3 (`g_battle_speed`, #19), avance rapide (Tab), farm d'or | Ne change pas les règles. Caméra dézoomée : à faire |
| Alchimie, collections (§7.5) | N | — | Données du jeu à localiser |
| Contenus P3 (§8) | N | — | Après stabilisation |
| Langues EN/FR/DE/IT/ES (§9) | AA | Menu Language (ROM EU seulement, `rom_is_eur()`), `settings.ini language` | Les menus PC, notices et messages restent en anglais (codés en dur dans `menu_win32.c` / `frontend_sdl.c`) |
| Démarrage façon PC (§5) | TV | `bootskip.c` (#32) | — |
| Multijoueur, réseau (§12) | hors périmètre | — | Ne pas toucher |

### Tests disponibles (exécutés le 2026-10-28, Linux cloud)
- `python3 tests/semantics/run.py` : 1 200 000 vérifications ARM + Thumb,
  0 échec, exit 0 (après réparation du harnais).
- `python3 tests/verify_decode.py build/extract build/region/arm9` (ROM EU) :
  727 987 instructions, 0 désaccord avec LLVM.
- `tests/gpu3d/t3d.c` : `gcc -I../../runtime t3d.c ../../runtime/gpu3d.c -lm`
  puis `./a.out` → `t3d.ppm` (triangle dégradé, quad translucide, second quad
  éliminé) ; compile à nouveau (stubs ajoutés).
- Jeu EU sous xvfb (sauvegarde de test de Stornway, `input.txt` de
  `ecran-unique/test-combat`) : église, ville avec mini-carte et marqueurs,
  combat contre un Funghoul avec le HUD ; pas de plantage sur 5 400 images.
- Test de fumée `tests/smoke/run.py` (voir plus bas) : EU et USA, deux
  passages chacun, écart 0,00 avec les références : le jeu est déterministe,
  la moindre différence visible signale un changement.
- Non exécutés : parcours complet, changement de langue en jeu, build Windows
  complet (seuls les fichiers de la PR #33 ont été compilés avec mingw-w64).

#### Test de fumée avant chaque fusion (cloud Linux)
```
apt-get install -y libsdl2-dev python3-yaml xvfb
sh tools/build-game.sh <rom.nds>            # ~6 min la première fois
python3 tests/smoke/run.py --game game \
  --save <dqix.sav> --input <input.txt> --frames 2000,3300,5300 \
  --out /tmp/fumee --ref <références>
```
Sauvegarde, recette et références (dérivées du jeu, jamais dans le dépôt) :
`/mnt/project-files/tests-fumee/` (`input.txt`, `ref-eu/`, `ref-usa/`) avec la
sauvegarde `/mnt/project-files/ecran-unique/test-combat/dqix.sav` : église de
Stornway (2000), ville avec mini-carte (3300), combat contre un Funghoul en EU
(5300). Un changement visuel voulu donne un écart : regarder les PNG puis
refaire les références.

### Problèmes identifiés
1. Tests automatiques cassés depuis l'ajout de `g_cyc` et des appels de
   `gpu3d.c` au reste du runtime : personne ne le voyait (réparé dans ce lot).
2. Ce suivi n'était plus à jour depuis le 21/10 ; `ROADMAP.md` et
   `docs/STATE.md` décrivent surtout l'upstream (multijoueur, Linux).
3. PR #33 (menu d'options dans le jeu) ouverte depuis 5 jours : en plein écran
   ou à la manette, les options restent inaccessibles.
4. Interface PC uniquement en anglais alors que le jeu se joue en français.
5. Code mort des anciennes dispositions dans `frontend_sdl.c` (fichier partagé
   de 2 750 lignes) : risque de confusion pour les prochains changements.
6. Pas de contrôle automatique des packs de textures HD.

### Feuille de route priorisée
Chaque ligne = une tâche courte pour un thread, avec captures pour tout visuel.

| # | Priorité | Tâche | Rôle | Risque | Valeur |
|---|---|---|---|---|---|
| 1 | P0 | ✓ Réparer les tests automatiques (PR #45) | Dev | très faible (`tests/` seul) | Détecte les régressions du recompilateur et de la 3D |
| 2 | P0 | ✓ (PR #45) Script de test de fumée cloud : build EU + USA, recette d'entrée, captures aux images clés, comparaison avec des captures de référence (sauvegarde de test hors dépôt) | Dev / Intégration | faible (`tests/` seul) | Non-régression visuelle avant chaque fusion |
| 3 | P0 | PR #33 reprise par Romuald : `main` fusionné, testé EU + USA ; à pousser et fusionner | Dev | moyen (`frontend_sdl.c`, `menu_win32.c`) | Options au clavier et à la manette, en plein écran |
| 4 | P1 | Terminer les fils en cours (cadre moderne, police unique, pop-up) | Graph, Langues, Dev | — | Interface homogène |
| 5 | P1 | Interface PC traduite (FR d'abord) selon la langue choisie | Langues | moyen (fichiers partagés, après #3) | Cohérence pour un joueur francophone |
| 6 | P1 | Contrôle des packs de textures : dimensions multiples de l'original, alpha, PNG illisibles, planche avant/après | Graph | très faible (outil à part) | Ressources remplaçables et restaurables en confiance |
| 7 | P1 | Liste des monstres et journal des quêtes en pop-up, après une recette pour les atteindre | Dev | faible | Finir le mono-écran |
| 8 | P1 | Caméra dézoomée en combat (sans toucher aux boîtes) | Dev | moyen | Suggestion de sackchief |
| 9 | P2 | Études (lecture seule) : structure du sac et des recettes d'alchimie en RAM, avant tout tri, filtre ou aide à l'alchimie | Dev | nul | Prépare § 7 |
| 10 | P2 | Retirer le code mort des anciennes dispositions | Dev | moyen | Seulement avec l'accord de Romuald |
| 11 | P3 | Effets et éclairage, journal des cartes au trésor, encyclopédie | — | élevé | Après stabilisation |

### Nouvelles idées du cahier (§ 5 à 9) et faisabilité
Chacune a besoin de données du jeu. « Repères » = ce que le code ou la carte
des symboles (`config/usa/arm9/symbols.txt`, ~9 % de noms) donne déjà ;
coût : S = un soir, M = quelques jours, L = plus.

| Idée | Fil | Repères déjà connus | Coût | Risque |
|---|---|---|---|---|
| Dégâts et altérations d'état plus visibles, temps morts réduits, animations accélérées | Combats | `CalculatePhysicalDamage` (0x020744c0), vitesse de combat (`farm.c` `party_in_battle`, `sound.c`) | M | moyen : ne pas toucher aux boîtes ni aux règles |
| Comparaison d'équipement (+/− par membre), filtres, favoris, quantités | Inventaire | Fiches de membres 0x964 octets (`cheats.c`), `itemdt_en.nat` (`mapmarks.c`), le jeu a déjà son tri et son aperçu | M | faible si affichage seul |
| Livre des recettes d'alchimie, ingrédients manquants | Alchimie | Aucun repère sur les recettes : à chercher dans les fichiers de la ROM | M-L | faible |
| Monstres rencontrés, objets obtenus, boss vaincus | Alchimie / collections | Liste des monstres du jeu (page du haut), drapeaux de progression 0x021088D0 | M | faible |
| Journal des cartes au trésor, grottes explorées, filtres | Alchimie / cartes | Bien nommé : `DetailedTreasureMapData`, `TreasureMapMetadata`, `GetTreasureMapTypeFromItemID`, `GameState::GetGrottoStruct` ; plan des grottes possible avec `FloorMapGenerator`/`FloorMap::GetTile` | M | faible |
| Journal des quêtes (objectifs connus, sans solution) | Quêtes | Statut des quêtes 0x02108B10 (`mapmarks.c`), quartets dans la sauvegarde à 12300 | M | faible |
| Compétences acquises / à venir, comparaison de vocations, ensembles d'équipement | Quêtes et progression | Points de compétence 0x020F3DB0 + 0x964 par membre (`cheats.c`) ; ensembles = écrire l'équipement en RAM | M (L pour les ensembles) | moyen pour les ensembles (modifie la partie) |
| Boutons PS5/Xbox à la place de « X Menu », légende de la carte | Boutons manette | Panneau « X Menu » = un quad 3D déplacé (`gpu3d.c` `g_sign_move`) ; marqueurs (`mapmarks.c`) | S-M | faible |
| Météo, eau, ombres, éclairage, effets de sort | Effets visuels | Nommés : `LightingManager` (20 fonctions), `AtmosphericEffectSet` (13) ; rendu GPU dans `gl_render.c` | L | élevé : rester sur des effets optionnels |
| Contrôle des packs de textures (taille, alpha, avant/après) | Effets visuels / textures | `textures/dump`, `dump-2d`, `tools/textures-hd.c` | S | très faible (outil à part) |
| Menus PC traduits, contraste, textes jamais coupés | Menus en français | Chaînes en dur dans `menu_win32.c` / `frontend_sdl.c` ; langue `g_fw_language` | M | moyen (fichiers partagés) |
| Fiches de personnages, confirmations, messages système, chargements | Cadre moderne | Style minimal (`g_winstyle`), boîte de description #44 | M | moyen |
| Défis facultatifs, quêtes annexes, encyclopédie, personnalisation | — | — | L | P3 : après stabilisation, étude d'équilibrage d'abord |

### Décisions attendues de Romuald
- PR #33 et menus en français : décidé le 28/10 au soir (reprendre, traduire).
- Retirer le code des anciennes dispositions (déjà invisibles) ou le laisser ?
- `ROADMAP.md` : le réécrire pour ce fork (sans multijoueur) ou le garder tel
  que l'upstream ?

## Idées (non planifiées)
Voir aussi `ROADMAP.md` (upstream : Linux/Steam Deck, options modernes ; son
volet multijoueur est hors périmètre ici).
