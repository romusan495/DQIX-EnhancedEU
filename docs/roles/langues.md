# Rôle : Langues et ressources

Threads `[Langues] …`, branches `langues/<sujet>`.

## Périmètre
ROM européenne et différences USA/EU, menu Langue (EN/FR/DE/IT/ES), textes du
jeu, polices PC (texte HD des dialogues), extraction des ressources de la ROM.

## Possède (seul à écrire)
- `tools/region.py`, `tools/extract_rom.py`, `config/`.
- `runtime/hdtext.c` / `.h`, `runtime/text.c` / `.h`, `fonts/` (chaque `.ttf` avec sa licence `-OFL.txt`).

## Partagé
Voir la liste des fichiers partagés dans `CLAUDE.md` ; ce rôle y touche surtout
`menu_win32.c` (menu Graphics > Text font) et les scripts de build (copie de
`fonts/` dans `game\fonts`).

## À savoir
- La ROM EU (En,Fr,De,Es,It) et la ROM USA doivent toutes les deux compiler et
  se lancer : tester les deux avant de demander la revue quand le code touche
  la région ou les textes.
- `config/usa/arm9` vient du projet dqix-decomp (domaine public) ; ne pas le modifier à la main.
- Une police ajoutée doit être sous licence libre (OFL) et livrée avec sa licence.
- Wi-Fi/DQVC sur la ROM EU : non testé (voir `docs/SUIVI.md`).
