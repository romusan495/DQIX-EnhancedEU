# Working on this fork

This repository (`romusan495/DQIX-EnhancedEU`) is the single source of truth for
the code. Two folders on the play PC use it:

| Folder | What it is |
|---|---|
| `J:\DQIX-Enhanced-repo` | Development clone. Make branches, edit, build and test here. |
| `J:\DQIX-Enhanced` | The folder the game is played from (`game\dqix.exe`, saves, HD textures, movies). Also a git checkout of `main`, but only updated from `main`, never edited by hand. |

## Workflow

1. Start from an up-to-date `main` in the development clone:
   ```
   git checkout main
   git pull --ff-only
   git checkout -b <feature-name>
   ```
2. Edit, then build (see below) and test with `game\dqix.exe` in the clone.
3. Commit on the branch, push it, open a pull request to `main` and merge it once it builds.
4. Update the play folder from `main`:
   ```
   cd /d J:\DQIX-Enhanced
   git pull --ff-only
   set DQIX_NOPAUSE=1 && BUILD.bat
   ```
   `git pull --ff-only` stops without touching anything if the play folder has
   local edits that conflict; commit those to a branch first. The build only
   replaces the program files in `game\`; saves, `settings.ini`, `controls.ini`,
   HD textures and movies stay.

One branch per feature. Two people (or sessions) never edit the same files on
two branches at once without saying so.

## Building

`BUILD.bat` does everything: it installs MSYS2 to `C:\msys64` the first time,
then the compiler and libraries, then recompiles the ROM and builds `game\`.

- The ROM is the first `.nds` in `rom\`, otherwise the path in `rom\last-rom.txt`
  (both are git-ignored). In the development clone, `rom\last-rom.txt` points to
  the ROM in the play folder.
- Set `DQIX_NOPAUSE=1` to run it unattended (no `pause` at the end); the exit
  code is 0 on success.
- From a shell: `cmd /c "set DQIX_NOPAUSE=1&& J:\DQIX-Enhanced-repo\BUILD.bat"`.
- A first build takes several minutes (recompiling the ROM); later builds only
  rebuild what changed.

## Never commit

Anything from the ROM or the play folder: `*.nds`, `build/`, `game/`, saves,
dumps, extracted files, upscaled textures or movies. `.gitignore` covers these;
check `git status` before committing.
