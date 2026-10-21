# Rôle : Développement moteur et QoL

Threads `[Dev] …`, branches `dev/<sujet>`.

## Périmètre
Recompilateur, runtime (CPU, mémoire, threads, son, entrées), écran unique et
widescreen, caméra, remappage clavier/manette, farm d'or, Wi-Fi/DQVC et
multijoueur, portage Linux/Steam Deck, corrections techniques.

## Possède (seul à écrire)
- `recomp/`, `patches.txt`, `hle.txt`, `hle7.txt`, `interpret.txt`, `hooks.txt`.
- `runtime/` sauf les fichiers des rôles Graphismes (`hdmovie.*`) et Langues
  (`hdtext.*`, `text.*`), et sauf les fichiers partagés ci-dessous.
- `wfc/`, `tools/relay/`, `tools/dqvc_*.py`, `tests/`, `third_party/`, `tags.txt`.

## Partagé
Voir la liste des fichiers partagés dans `CLAUDE.md`.

## À savoir
- Le C généré à partir de la ROM ne se partage jamais (dérivé du jeu).
- Tests : voir la section « Tester » de `CLAUDE.md`.
- Les rapports F10 (`game\reports`) et le replay déterministe aident à reproduire un bug.
