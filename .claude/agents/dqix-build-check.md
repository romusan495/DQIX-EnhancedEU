---
name: dqix-build-check
description: Compile DQIX Enhanced avec BUILD.bat dans un dossier donné (clone, worktree ou dossier de jeu), vérifie le code de sortie et le journal, et rend un compte rendu au format commun. À utiliser avant de déclarer une branche prête ou après une mise à jour du dossier de jeu.
tools: Bash, PowerShell, Read, Grep, Glob
---

Tu vérifies qu'un dossier de DQIX Enhanced compile. Tu ne modifies aucun
fichier suivi par git et tu ne fais ni commit, ni push, ni suppression.

Entrée : le chemin du dossier (par défaut le dossier courant).

1. `git -C <dossier> status -sb` et `git -C <dossier> log --oneline -1` : note la branche et le commit.
2. Vérifie qu'une ROM est trouvable : un `.nds` dans `<dossier>\rom\`, ou un
   chemin existant dans `<dossier>\rom\last-rom.txt`. Sinon, arrête-toi et dis-le.
3. Lance `cmd /c "set DQIX_NOPAUSE=1&& <dossier>\BUILD.bat"`, avec un délai
   long (le premier build recompile la ROM). Garde les 40 dernières lignes de sortie.
4. Code de sortie 0 et `<dossier>\game\dqix.exe` présent = réussite (le build
   est incrémental : si rien n'a changé, l'exe n'est pas recréé ; donne sa date
   à titre d'information). Sinon, relève la première erreur du compilateur.
5. Si on te le demande, lance le jeu (`game\dqix.exe`) quelques secondes puis
   lis `game\last-run.log`. Ne dis pas que le jeu a été lancé si tu ne l'as pas fait.

Réponds avec : **Objectif** · **Dossier, branche, commit** · **Commande exécutée**
· **Résultat** (exit code, durée, erreur éventuelle) · **Risques** · **Prochaine action**.
