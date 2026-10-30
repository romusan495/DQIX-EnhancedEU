# Rôle : Graphismes et interface

Threads `[Graph] …`, branches `graph/<sujet>`.

## Périmètre
Textures HD (dump, upscale, remplacement), cinématiques HD, skins de la DS
Lite, restyle visuel des fenêtres et des boîtes de dialogue, maquettes.

## Possède (seul à écrire)
- `tools/textures-hd.c`, `tools/textures-rom.c`, `tools/upscale_movies.py`, `tools/split_movies.py`,
  `tools/movie_frames.py`, `runtime/hdmovie.c` / `.h`, `skins/`.
- Hors git, dans le dossier de jeu : `game\textures`, `game\movies`,
  `game\textures-hd-outil`. Ils ne sont jamais versionnés ; sauvegarde dans
  `J:\DQIX-Backups\` avant de les remplacer, et seulement avec l'accord de Romuald.

## Partagé
Voir la liste des fichiers partagés dans `CLAUDE.md` (surtout `gl_render.c`,
`gpu3d.c`, `menu_win32.c`). Le restyle des dialogues touche aussi `runtime/hdtext.c` (rôle Langues) : à faire
l'un après l'autre, pas en parallèle.

## À savoir
- Textures HD : « IA seule » (Real-ESRGAN anime sans mélange), choix de Romuald
  du 2026-10-29 qui remplace la méthode 5 (mélange à 50 % avec un agrandissement
  Lanczos, toujours possible avec `TEXHD_SHARP=50`). Pas de version HD pour les
  textures vides ou unies.
  Passage pour ceux qui ont déjà un pack : au premier lancement de `textures-hd`
  (donc à la mise à jour), les textures HD qui ont leur original dans
  `textures\dump` sont déplacées dans `game\textures-methode5\` puis refaites
  (`textures\hd-style.txt` = fait).
- `textures-hd.exe` se compile depuis `tools/textures-hd.c` ; BUILD.bat le
  (re)construit seulement si `game\textures-hd-outil` existe (voir plus bas).
- `textures-rom.exe` (pareil : `gcc -O2 -I runtime -o game/textures-rom.exe tools/textures-rom.c`)
  sort toutes les textures 3D de la ROM du joueur dans `textures\dump` sans jouer
  (12 670, mêmes noms EU et USA), puis lance `textures-hd.exe`. Les visages et
  cheveux du héros, les lettres et les morceaux des PNJ lointains n'y sont pas
  tels quels : ils restent au dump en jouant.
- À chaque build (donc à chaque mise à jour), si `game\textures-hd-outil` existe,
  `tools/build-game.sh` recompile ces deux outils puis lance `textures-rom.exe --auto` :
  extraction une seule fois par ROM (`textures\dump\rom-textures.txt`), puis
  `textures-hd.exe` en arrière-plan. `textures-hd` saute les textures déjà présentes
  dans n'importe quel dossier du pack (`hd\`, `rom\`...) et ne tourne qu'une fois
  à la fois (`textures\hd-auto.lock`).
- Vérifier le rendu en jeu (Graphics > Enhanced) et joindre une capture au compte rendu.
