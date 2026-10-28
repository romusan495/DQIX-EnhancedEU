# Suivi du projet

Tenu par le rôle Intégration (voir `docs/roles/integration.md`). Les autres
threads envoient leur compte rendu au coordinateur, qui relaie. Dernière mise à
jour : 2026-10-23.

États : `à faire` · `en cours` · `en revue` · `fusionné` · `en pause` · `abandonné`.

## Tâches

| Tâche | Rôle | Thread | Branche / PR | État | Dépend de | Prochaine étape |
|---|---|---|---|---|---|---|
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
| `runtime/frontend_sdl.c` | PR #29 (confirmation avant de quitter) | Fusionnée |
| `runtime/frontend_sdl.c`, `menu_win32.c`, `card.c`, `input.c`, `menu.h`, nouveau `bootskip.c` | `claude/project-thread-9e00md` ([Dev] Vers un vrai jeu PC, démarrage direct), en cours | Conflit probable sur `frontend_sdl.c` avec [Dev] Mini-carte (fiches d'objets, en cours) |
| `runtime/frontend_sdl.c`, `mapmarks.c`, `mapmarks.h` | `claude/project-thread-6h3zvz` ([Dev] Mini-carte, correctif des fiches d'objets, 7cd0b23), pas encore poussée | Conflit probable sur `frontend_sdl.c` avec `claude/project-thread-9e00md` |
| `runtime/frontend_sdl.c`, `menu_win32.c` (le menu devient un arbre portable, la barre Windows est construite depuis lui), `gl_render.c` (`gl_set_menu`, dessin par-dessus l'image), `build.mk` (nouveau `runtime/gamemenu.c`) | `claude/project-thread-9e00md` ([Dev] Vers un vrai jeu PC : menu d'options en jeu, Échap + bouton PS, 0747191), pas encore poussée | Gros changement de `menu_win32.c` : conflits probables avec toute branche qui ajoute des entrées de menu ; la fusionner tôt et faire reprendre `main` aux autres |

## Décisions

| Date | Décision | Raison |
|---|---|---|
| 2026-10-21 | Le dépôt `romusan495/DQIX-EnhancedEU` est la seule source ; le dossier de jeu ne change que par `git pull --ff-only` + BUILD.bat. | Des éditions directes dans le dossier de jeu ne revenaient pas au dépôt. |
| 2026-10-21 | Méthode 5 (IA + original mélangés) pour toutes les textures HD. | Choix de Romuald après comparaison. |
| 2026-10-22 | Release GitHub hebdomadaire : sources seules (jamais ROM, exe ni données du jeu), préparée par une tâche Claude, publiée après le feu vert de Romuald. | Choix de Romuald ; seul son PC peut vérifier le build. |
| 2026-10-21 | 5 rôles, un thread par fonctionnalité, un worktree par thread de dev. | Plan de réorganisation validé. |

## Idées (non planifiées)
Voir aussi `ROADMAP.md` (Linux/Steam Deck, multijoueur, options modernes).
