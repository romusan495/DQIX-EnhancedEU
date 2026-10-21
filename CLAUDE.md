# DQIX Enhanced (fork romusan495/DQIX-EnhancedEU)

Dragon Quest IX (DS) recompilé en C et exécuté nativement sur PC. Code :
`recomp/` (recompilateur Python), `runtime/` (C : CPU, GPU 2D/3D, son, Wi-Fi,
interface SDL/OpenGL, menus Windows), `tools/` (build, extraction ROM, textures
et films HD), `tests/`. Carte détaillée : `docs/TECHNICAL.md`. État : `docs/STATE.md`.
Tâches en cours et décisions : **`docs/SUIVI.md`** (à lire en début de session).

Réponds en français. Messages de commit, noms de branches et PR en anglais.

## Où vit quoi
- Dépôt GitHub `romusan495/DQIX-EnhancedEU`, branche `main` = seule source du code.
- `J:\DQIX-Enhanced-repo` = clone de dev, reste sur `main` (rôle Intégration).
- `J:\DQIX-wt\<role>-<sujet>` = un worktree par thread de dev (voir plus bas).
- `J:\DQIX-Enhanced` = dossier où Romuald joue (checkout de `main`).
  **Ne jamais l'éditer** : seulement `git pull --ff-only` puis BUILD.bat, par le
  rôle Intégration, avec l'accord de Romuald. Sauvegardes : `J:\DQIX-Backups\`.

## Compiler (vérifié sur ce PC)
```
cmd /c "set DQIX_NOPAUSE=1&& <dossier>\BUILD.bat"
```
Code de sortie 0 = OK. Le résultat va dans `<dossier>\game\`. Le premier build
d'un dossier recompile la ROM (plusieurs minutes). La ROM est le premier `.nds`
de `rom\`, sinon le chemin de `rom\last-rom.txt` (ignoré par git : à copier dans
chaque nouveau worktree). Sans ROM trouvable, BUILD.bat ouvre une boîte de
dialogue et reste bloqué : vérifier `rom\` avant un build en arrière-plan.
En PowerShell, lancer BUILD.bat en arrière-plan.

## Tester
- Sur ce PC, le test reproductible = BUILD.bat exit 0, puis lancer
  `game\dqix.exe` et lire `game\last-run.log`. Sous-agent : `dqix-build-check`.
- `tests/semantics/run.py` et `tests/verify_decode.py` demandent clang (cible
  ARM) / LLVM : absents de ce PC, ils tournent sous Linux.
- `tests/gpu3d/t3d.c` se compile avec le gcc de MSYS2 contre `runtime/gpu3d.c`
  (écrit `t3d.ppm`), mais ne compile plus en l'état (voir `docs/SUIVI.md`).
- Ne jamais annoncer un test comme exécuté s'il ne l'a pas été.

## Règles
- Une branche par fonctionnalité, nommée `<role>/<sujet>` : `graph/`, `langues/`,
  `dev/`, `integ/`, `orga/`. Travailler dans son worktree :
  `git -C J:\DQIX-Enhanced-repo worktree add J:\DQIX-wt\<role>-<sujet> -b <role>/<sujet> origin/main`
  (`orga/` = organisation du projet, tenue par le rôle Intégration.)
- Ne jamais commiter : `*.nds`, `build/`, `game/`, sauvegardes, dumps, textures
  ou films upscalés, C généré (dérivé du jeu). Vérifier `git status` avant.
- Pas de push, de fusion, de suppression de branche/worktree/fichier ni de
  réécriture d'historique sans l'accord explicite de Romuald.
- Avant de modifier un fichier possédé par un autre rôle (`docs/roles/`), le
  demander au coordinateur. **Fichiers partagés** (`runtime/gl_render.c`,
  `runtime/menu_win32.c`, `runtime/frontend_sdl.c`, `runtime/gpu3d.c`,
  `build-windows.sh`, `tools/build-game.sh`, `build.mk`) : annoncer au
  coordinateur la branche qui les touche ; il fait noter la ligne dans `docs/SUIVI.md`.
  Seul le rôle Intégration écrit `docs/SUIVI.md`.
- Ressources du jeu (textures, films, polices, ROM) : sauvegarde avant d'écraser.
- Textures HD : méthode 5 (IA + original mélangés) pour toutes les textures.

## Plugins (installés : superpowers, pr-review-toolkit, code-simplifier, claude-code-setup)
- Avant un gros changement : `superpowers:brainstorming` puis `superpowers:writing-plans`.
- Bug : `superpowers:systematic-debugging`. Avant « fini » : `superpowers:verification-before-completion`.
- Avant fusion : sous-agent `pr-review-toolkit:code-simplifier`, puis `/pr-review-toolkit:review-pr`.
- Ne pas recréer d'agent qu'un plugin fournit déjà.

## Rôles (instructions : `docs/roles/<role>.md`)
Coordinateur (fil principal du projet) · Graphismes et interface · Langues et
ressources · Développement moteur et QoL · Intégration, tests et revue.

## Compte rendu (à chaque jalon)
**Objectif** · **Fichiers modifiés** · **Tests exécutés** (commande + résultat
réel) · **Résultats** · **Risques** · **Prochaine action** · **État Git**
(branche, commit, poussé ou non, PR).

## Reprendre une tâche dans une nouvelle session
1. Lire ce fichier, `docs/roles/<role>.md` et la ligne de la tâche dans `docs/SUIVI.md`.
2. `git status`, `git log --oneline -10` de la branche, état de la PR.
3. Lire le dernier compte rendu du thread, puis reprendre à « Prochaine action ».
