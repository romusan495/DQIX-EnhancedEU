# Suivi du projet

Tenu par le rôle Intégration (voir `docs/roles/integration.md`). Les autres
threads envoient leur compte rendu au coordinateur, qui relaie. Dernière mise à
jour : 2026-10-21.

États : `à faire` · `en cours` · `en revue` · `fusionné` · `en pause` · `abandonné`.

## Tâches

| Tâche | Rôle | Thread | Branche / PR | État | Dépend de | Prochaine étape |
|---|---|---|---|---|---|---|
| Écran unique et widescreen en combat | Dev | [Dev] Écran unique et combat | `ecran-unique`, PR #2 | fusionné | — | Dans le dossier de jeu depuis le 21/10 |
| Caméra dézoomée en combat | Dev | [Dev] Écran unique et combat | — | à faire | — | Plan avant de coder |
| Marqueurs coffres/quêtes sur la carte | Dev | [Dev] Écran unique et combat | — | à faire (recherche) | — | Trouver où le jeu garde ces données |
| Textures HD méthode 5 (outil) | Graph | [Graph] Textures HD et interface | `textures-hd-melange`, PR #1 | fusionné | — | Installer les textures restantes |
| Restyle des fenêtres et boîtes de dialogue | Graph | à créer | — | à faire | Polices PC fusionnées | Maquettes |
| Polices PC dans les dialogues (9 polices) | Langues | [Graph] Textures HD et interface (ex « Moderniser ») | `dialogues-police-pc`, PR #3 | fusionné | — | Retour de Romuald en jeu |
| Menu Langue EN/FR/DE/IT/ES | Langues | à créer | — | à faire | — | Vérifier ce que fait déjà le menu Langue du commit EU |
| Wi-Fi / DQVC avec la ROM EU | Dev | à créer | — | à faire (non testé) | — | Tester avec la ROM EU |
| BUILD : « cmp: commande introuvable » (`tools/build-game.sh:99`) | Intégration | à créer | — | à faire | — | Installer `diffutils` dans MSYS2 ou éviter `cmp` ; le build réussit quand même |
| `tests/gpu3d/t3d.c` ne compile plus (`gpu3d_init` non déclaré) | Dev | à créer | — | à faire | — | Mettre le test à jour avec l'API actuelle de `gpu3d.c` |
| Releases GitHub hebdomadaires (v31.1, v31.2...) | Intégration | [Intégration] Releases hebdomadaires | `claude/project-thread-on5r0q` (touche `runtime/update.c/.h`, `version.h` : fichiers Dev, accord du coordinateur) | en revue | — | Fusion, puis tag `v31.1` + branche `stable` + release, avec l'accord de Romuald |
| Réorganisation Claude Code | Intégration | [Orga] Réorganisation | `orga/claude-config` | en cours | — | Revue, accord pour pousser |

## Décisions

| Date | Décision | Raison |
|---|---|---|
| 2026-10-21 | Le dépôt `romusan495/DQIX-EnhancedEU` est la seule source ; le dossier de jeu ne change que par `git pull --ff-only` + BUILD.bat. | Des éditions directes dans le dossier de jeu ne revenaient pas au dépôt. |
| 2026-10-21 | Méthode 5 (IA + original mélangés) pour toutes les textures HD. | Choix de Romuald après comparaison. |
| 2026-10-22 | Release GitHub hebdomadaire : sources seules (jamais ROM, exe ni données du jeu), préparée par une tâche Claude, publiée après le feu vert de Romuald. | Choix de Romuald ; seul son PC peut vérifier le build. |
| 2026-10-21 | 5 rôles, un thread par fonctionnalité, un worktree par thread de dev. | Plan de réorganisation validé. |

## Idées (non planifiées)
Voir aussi `ROADMAP.md` (Linux/Steam Deck, multijoueur, options modernes).
