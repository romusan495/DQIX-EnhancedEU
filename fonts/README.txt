Fonts for HD text (Graphics > Text font), copied into game/fonts by the build.
All are under the SIL Open Font License 1.1: each one's licence is in
<name>-OFL.txt next to it.

Unchanged files from Google Fonts: SourceSans3, Lora, CrimsonPro, Alegreya,
Spectral, EBGaramond, Philosopher, Marcellus (variable fonts are drawn at
their default weight; Source Sans 3's is its lightest, which the game's
thickened strokes make a regular weight).
SourceSans3-Regular.ttf: Adobe's own regular cut of it (unchanged, from
github.com/adobe-fonts/source-sans), for the in-game menu (Esc).
Source Sans 3 is the default and the only one in the menu; the others are
there with all_options = 1 in settings.ini.
Nunito.ttf and Fredoka.ttf are the medium (500) weight cut out of the
variable fonts with fontTools' instancer: their default weights are too thin
for the game's small letters. Neither has a Reserved Font Name.
