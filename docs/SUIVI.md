# Suivi du projet

Tenu par le rôle Intégration (voir `docs/roles/integration.md`). Les autres
threads envoient leur compte rendu au coordinateur, qui relaie. Dernière mise à
jour : 2026-10-22.

États : `à faire` · `en cours` · `en revue` · `fusionné` · `en pause` · `abandonné`.

## Tâches

| Tâche | Rôle | Thread | Branche / PR | État | Dépend de | Prochaine étape |
|---|---|---|---|---|---|---|
| Écran unique et widescreen en combat | Dev | [Dev] Écran unique et combat | `ecran-unique`, PR #2 | fusionné | — | Dans le dossier de jeu depuis le 21/10 |
| Caméra dézoomée en combat | Dev | [Dev] Écran unique et combat | — | à faire | — | Plan avant de coder |
| Marqueurs sur la carte : coffres, pots, tonneaux (phase 1) | Dev | [Dev] Écran unique et combat | `claude/project-thread-6dtla9`, PR #8 | fusionné | — | Test Windows dans le dossier de jeu |
| Carte : bibliothèques et contenu des bâtiments (phase 2) | Dev | [Dev] Écran unique et combat | `claude/project-thread-6dtla9`, PR #9 | fusionné | PR #8 | Test Windows dans le dossier de jeu |
| Carte : marqueurs de quêtes avec leur état (phase 3) | Dev | [Dev] Écran unique et combat | `claude/project-thread-6dtla9`, PR #10 | fusionné | PR #9 | Test Windows dans le dossier de jeu |
| Textures HD méthode 5 (outil) | Graph | [Graph] Textures HD et interface | `textures-hd-melange`, PR #1 | fusionné | — | Installer les textures restantes |
| Style de fenêtres « Minimal » | Graph | [Graph] Textures HD et interface | `graph/fenetres-minimal`, PR #5 | fusionné | — | Retour de Romuald en jeu |
| HUD de combat « Minimal » (cartes transparentes, couleur du membre) | Graph | [Graph] Textures HD et interface | `graph/combat-minimal`, PR #7 | fusionné | PR #5 | Test Windows dans le dossier de jeu |
| Polices PC dans les dialogues (9 polices) | Langues | [Graph] Textures HD et interface (ex « Moderniser ») | `dialogues-police-pc`, PR #3 | fusionné | — | Retour de Romuald en jeu |
| Menu Langue EN/FR/DE/IT/ES | Langues | à créer | — | à faire | — | Vérifier ce que fait déjà le menu Langue du commit EU |
| Wi-Fi / DQVC avec la ROM EU | Dev | à créer | — | à faire (non testé) | — | Tester avec la ROM EU |
| BUILD : « cmp: commande introuvable » (`tools/build-game.sh:99`) | Intégration | à créer | — | à faire | — | Installer `diffutils` dans MSYS2 ou éviter `cmp` ; le build réussit quand même |
| `tests/gpu3d/t3d.c` ne compile plus (`gpu3d_init` non déclaré) | Dev | à créer | — | à faire | — | Mettre le test à jour avec l'API actuelle de `gpu3d.c` |
| Releases GitHub hebdomadaires (v31.1, v31.2...) | Intégration | [Intégration] Releases hebdomadaires | `claude/project-thread-on5r0q`, PR #6 et #11 (notes v31.1) (touche `runtime/update.c/.h`, `version.h` : fichiers Dev, accord du coordinateur) | fusionné | — | Tag `v31.1` + branche `stable` + release, avec l'accord de Romuald |
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
| `runtime/gl_render.c`, `build.mk`, `tools/build-game.sh` | `graph/icones-metiers` (pas encore poussée) | `draw_sprites` dans `gl_compose` et crochet de sprites dans le pack de textures (`dump-2d/` ignoré par le scan) ; dépendance `runtime/hdsprite.h` ; appel de `tools/job_icons.py` vers `game/textures/jobs` |
| `runtime/frontend_sdl.c` | `graph/combat-disposition` (pas encore poussée) | Disposition du HUD de combat, fenêtres d'expérience et de montée de niveau |
| `runtime/gl_render.c`, `runtime/gpu2d.c` | `graph/contour-blanc` (pas encore poussée) | Style Minimal : contour blanc conservé |

## Décisions

| Date | Décision | Raison |
|---|---|---|
| 2026-10-21 | Le dépôt `romusan495/DQIX-EnhancedEU` est la seule source ; le dossier de jeu ne change que par `git pull --ff-only` + BUILD.bat. | Des éditions directes dans le dossier de jeu ne revenaient pas au dépôt. |
| 2026-10-21 | Méthode 5 (IA + original mélangés) pour toutes les textures HD. | Choix de Romuald après comparaison. |
| 2026-10-22 | Release GitHub hebdomadaire : sources seules (jamais ROM, exe ni données du jeu), préparée par une tâche Claude, publiée après le feu vert de Romuald. | Choix de Romuald ; seul son PC peut vérifier le build. |
| 2026-10-21 | 5 rôles, un thread par fonctionnalité, un worktree par thread de dev. | Plan de réorganisation validé. |

## Idées (non planifiées)
Voir aussi `ROADMAP.md` (Linux/Steam Deck, multijoueur, options modernes).
