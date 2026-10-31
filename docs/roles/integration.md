# Rôle : Intégration, tests et revue

Thread `[Intégration] Fusions et build`, branches `integ/<sujet>`. Un seul
thread à la fois tient ce rôle.

## Périmètre
Revue de chaque PR, fusion dans `main`, build de contrôle, mise à jour du
dossier de jeu, tenue du suivi central.

## Possède (seul à écrire)
- `main` (fusions), le clone `J:\DQIX-Enhanced-repo` (reste sur `main`),
  le dossier de jeu `J:\DQIX-Enhanced`.
- `docs/SUIVI.md`, `CLAUDE.md`, `docs/roles/`, `.claude/`, `CONTRIBUTING.md`,
  `docs/STATE.md`, et les branches `orga/`.
- Build, installation et versions : `BUILD.bat`, `build-pc.bat`, `recomp.bat`,
  `Install Dragon Quest IX.bat`, `tools/make-release.py`, `tools/setup-wizard.ps1`,
  `tools/uninstall.ps1`, `tools/rom_icon.py`, `tools/where.sh`, `release-notes/`, `.github/`.
- Documentation : `README.md`, `ROADMAP.md`, `START HERE.txt`, `docs/TECHNICAL.md`
  (chaque rôle propose ses changements dans sa propre PR).

## Procédure pour chaque PR
1. Le thread de dev a déjà compilé et testé sa branche (compte rendu commun).
2. Sous-agent `pr-review-toolkit:code-simplifier` sur le diff, puis
   `/pr-review-toolkit:review-pr` (code-reviewer, silent-failure-hunter ;
   pr-test-analyzer si `tests/` change). Les corrections reviennent au thread de dev.
3. Sous-agent `dqix-build-check` sur la branche (ou son worktree).
4. Fusion dans `main` **avec l'accord de Romuald** ; mise à jour de `docs/SUIVI.md`.
5. Dossier de jeu, avec son accord : `git -C J:\DQIX-Enhanced status` (doit être
   propre), `git pull --ff-only`, BUILD.bat, puis `dqix-build-check` sur `J:\DQIX-Enhanced`.

## À savoir
- Sauvegarde du dossier de jeu avant une mise à jour risquée : `J:\DQIX-Backups\<date>-<raison>`.
- Les branches fusionnées ne se suppriment qu'avec l'accord de Romuald.
- Pas de CI GitHub : la vérification se fait sur ce PC.

## Release hebdomadaire (décidé avec Romuald le 2026-10-22)
Une tâche Claude planifiée par semaine ; rien n'est publié sans le feu vert de
Romuald dans le thread `[Intégration] Releases hebdomadaires`.
Numérotation (Romuald, 2026-10-30) : versions entières à partir de v32
(v32, v33...). Les v31.1 à v31.11 l'ont précédée.
1. Lister les PR fusionnées dans `main` depuis le dernier tag `vN`. Aucune :
   pas de release cette semaine.
2. Branche `integ/release-vN` : `runtime/version.h` (`DQIX_VERSION` et la
   date), `release-notes/vN.md` (anglais, style des notes existantes :
   « What's new since v(N-1) », comment installer, rien du jeu inclus ; une
   ligne par point, sans retour à la ligne forcé). PR.
3. `dqix-build-check` sur la branche (PC allumé). Demander le feu vert.
4. Après accord : fusion, tag `vN` sur `main`, branche `stable` déplacée sur
   ce tag (le bouton « Check for updates » la lit), release GitHub `vN` avec
   la note comme texte. Seulement le *Source code (zip)* automatique de GitHub.
- **Jamais dans une release** : ROM, `dqix.exe`, C généré, `build/`, `game/`,
  textures ou films upscalés, `wfc/` et les extras (données du jeu).
- Nos numéros ne suivent plus ceux de l'upstream (sa v32 n'est pas la nôtre) :
  un port d'une version upstream devient simplement la release suivante.
