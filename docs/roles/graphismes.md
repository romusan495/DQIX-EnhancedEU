# Rôle : Graphismes et interface

Threads `[Graph] …`, branches `graph/<sujet>`.

## Périmètre
Textures HD (dump, upscale, remplacement), cinématiques HD, skins de la DS
Lite, restyle visuel des fenêtres et des boîtes de dialogue, maquettes.

## Possède (seul à écrire)
- `tools/textures-hd.c`, `tools/upscale_movies.py`, `tools/split_movies.py`,
  `tools/movie_frames.py`, `runtime/hdmovie.c` / `.h`, `skins/`.
- Hors git, dans le dossier de jeu : `game\textures`, `game\movies`,
  `game\textures-hd-outil`. Ils ne sont jamais versionnés ; sauvegarde dans
  `J:\DQIX-Backups\` avant de les remplacer, et seulement avec l'accord de Romuald.

## Partagé
Voir la liste des fichiers partagés dans `CLAUDE.md` (surtout `gl_render.c`,
`gpu3d.c`, `menu_win32.c`). Le restyle des dialogues touche aussi `runtime/hdtext.c` (rôle Langues) : à faire
l'un après l'autre, pas en parallèle.

## À savoir
- Méthode 5 pour toute texture : Real-ESRGAN mélangé à un agrandissement
  Lanczos de l'original (PR #1).
- `textures-hd.exe` n'est pas construit par BUILD.bat : il se compile à part
  depuis `tools/textures-hd.c`.
- Vérifier le rendu en jeu (Graphics > Enhanced) et joindre une capture au compte rendu.
