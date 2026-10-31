#!/usr/bin/env python3
"""The PC interface's translations (lang/*.txt, runtime/uitext.h): lists the
English texts the code shows and checks each language file against them.

    python tools/lang_check.py             check every lang/*.txt
    python tools/lang_check.py --missing fr   print the texts french.txt lacks, as entries to fill in

A text is found when the code hands it to something that translates it: the
menu bar (item, sub, TW), T(), the message boxes (info_box, choose, ask_yes,
ask_text, choice_box), the title bar (notice), the farm's messages, and the
tables of names (controls, controller buttons, cheats). Exit code 1 when a
file has a text the code no longer has, a broken %-conversion or a duplicate."""
import re
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = ["runtime/menu_win32.c", "runtime/gamemenu.c", "runtime/frontend_sdl.c", "runtime/farm.c", "runtime/cheats.c", "runtime/controls.c"]
# calls whose string arguments are shown translated
SINKS = {"item", "sub", "label", "TW", "T", "notice", "info_box", "choose", "ask_yes", "ask_text", "choice_box", "stop", "farm_cancel"}
# the session window (menu_win32.c) and its texts stay as they are: multiplayer is left alone
SKIP_REGIONS = [("runtime/menu_win32.c", "/* ---------------------------------------------------------------- the session window",
                 "/* ---------------------------------------------------------------- volume")]
LANGS = ["french", "german", "italian", "spanish"]
CODES = {"french": "fr", "german": "de", "italian": "it", "spanish": "es"}


def c_unescape(body):
    out, i = [], 0
    while i < len(body):
        c = body[i]
        if c != "\\":
            out.append(c); i += 1; continue
        n = body[i + 1]
        if n == "n": out.append("\n"); i += 2
        elif n == "t": out.append("\t"); i += 2
        elif n == "u": out.append(chr(int(body[i + 2:i + 6], 16))); i += 6
        elif n == "x":
            m = re.match(r"[0-9a-fA-F]{1,2}", body[i + 2:])
            out.append(chr(int(m.group(0), 16))); i += 2 + len(m.group(0))
        else: out.append(n); i += 2
    s = "".join(out)
    try:                                    # "\xc3\xa7" byte escapes: UTF-8
        s = s.encode("latin-1").decode("utf-8")
    except (UnicodeEncodeError, UnicodeDecodeError):
        pass
    return s


def tokens(src):
    """(kind, text, pos): identifiers, punctuation, and string literals
    (adjacent ones joined, as C does); comments and preprocessor lines skipped."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i): i = src.index("\n", i) if "\n" in src[i:] else n; continue
        if src.startswith("/*", i): i = src.index("*/", i) + 2; continue
        if c == "#" and (i == 0 or src[i - 1] == "\n"):
            while i < n and src[i] != "\n":
                i += 2 if src[i] == "\\" else 1
            continue
        if c == '"' or (c == "L" and i + 1 < n and src[i + 1] == '"'):
            start = i; parts = []
            while True:
                if src[i] == "L": i += 1
                j = i + 1
                while src[j] != '"': j += 2 if src[j] == "\\" else 1
                parts.append(c_unescape(src[i + 1:j])); i = j + 1
                m = re.match(r"\s*(L?\")", src[i:])
                m2 = re.match(r"\s*(DQIX_VERSION|UPDATE_REPO|DQIX_VERSION_DATE|__DATE__)\b", src[i:])
                if m: i += len(m.group(0)) - len(m.group(1)); continue
                if m2:
                    parts.append("<" + m2.group(1) + ">"); i += len(m2.group(0))
                    m = re.match(r"\s*(L?\")", src[i:])
                    if m: i += len(m.group(0)) - len(m.group(1)); continue
                break
            out.append(("str", "".join(parts), start)); continue
        if c == "'":
            j = i + 1
            while src[j] != "'": j += 2 if src[j] == "\\" else 1
            i = j + 1; continue
        m = re.match(r"[A-Za-z_]\w*", src[i:])
        if m: out.append(("id", m.group(0), i)); i += len(m.group(0)); continue
        if not c.isspace(): out.append(("p", c, i))
        i += 1
    return out


def texts_in(path):
    src = open(os.path.join(ROOT, path), encoding="utf-8").read()
    for f, a, b in SKIP_REGIONS:
        if f == path and a in src and b in src:
            src = src[:src.index(a)] + src[src.index(b):]
    tk = tokens(src)
    found = []
    def add(t, pos):
        if t.strip() and not re.fullmatch(r"[\s%dsx\-/=+:.()]*", t):
            found.append((t, src.count("\n", 0, pos) + 1))
    named = {}                               # static const char NAME[] = "..." (texts kept in a constant)
    for k in range(len(tk) - 3):
        if tk[k][0] == "id" and tk[k][1].isupper() and len(tk[k][1]) > 2:
            j = k + 1
            while j < len(tk) and tk[j][1] in "[]*" : j += 1
            if j + 1 < len(tk) and tk[j][1] == "=" and tk[j + 1][0] == "str":
                named[tk[k][1]] = (tk[j + 1][1], tk[j + 1][2])
    for k, (kind, text, pos) in enumerate(tk):
        if kind == "id" and text in SINKS and k + 1 < len(tk) and tk[k + 1][1] == "(":
            depth, j = 0, k + 1
            while j < len(tk):
                if tk[j][1] == "(": depth += 1
                elif tk[j][1] == ")":
                    depth -= 1
                    if depth == 0: break
                elif tk[j][0] == "str": add(tk[j][1], tk[j][2])
                elif tk[j][0] == "id" and tk[j][1] in named: add(*named[tk[j][1]])
                j += 1
        # button lists and other small tables of texts: b[2] = { "...", ... }, b[nb++] = "...", N[4] = { ... }
        if kind == "id" and text in ("b", "b3", "N") and k + 1 < len(tk) and tk[k + 1][1] == "[":
            j = k + 1
            while j < len(tk) and tk[j][1] != "=" and tk[j][1] != ";": j += 1
            if j < len(tk) and tk[j][1] == "=":
                j += 1
                while j < len(tk) and tk[j][1] != ";":
                    if tk[j][0] == "str": add(tk[j][1], tk[j][2])
                    j += 1
        if kind == "id" and text == "return" and path == "runtime/farm.c" and tk[k + 1][0] == "str":
            add(tk[k + 1][1], tk[k + 1][2])
    # tables: a row's shown names
    if path == "runtime/controls.c":                       # { "id", "Name", bit }, { button, "id", "generic", "PlayStation", "Xbox", "Nintendo" }
        for m in re.finditer(r'\{ "\w+", "([^"]+)", -?\d+ \}', src): add(m.group(1), m.start())
        for m in re.finditer(r'\{ SDL_CONTROLLER_BUTTON_\w+, "\w+", ((?:"[^"]*"|0)(?:, (?:"[^"]*"|0))*) \}', src):
            for name in re.findall(r'"([^"]*)"', m.group(1)): add(name, m.start())
        for m in re.finditer(r'"(Controller|none)"', src): pass
    if path == "runtime/cheats.c":                         # { "id", "Name", ...
        for m in re.finditer(r'\{ "\w+", "([^"]+)",', src): add(c_unescape(m.group(1)), m.start())
    return found


def english_texts():
    seen = {}
    for p in SOURCES:
        for t, line in texts_in(p):
            if "<" in t and re.search(r"<(DQIX_VERSION|UPDATE_REPO|DQIX_VERSION_DATE|__DATE__)>", t):
                print(f"warning: {p}:{line}: a text with a macro in it can't be translated: {t[:60]!r}", file=sys.stderr)
                continue
            seen.setdefault(t, f"{p}:{line}")
    return seen


def esc(s):
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")


def unesc(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append({"n": "\n", "t": "\t"}.get(s[i + 1], s[i + 1])); i += 2
        else: out.append(s[i]); i += 1
    return "".join(out)


def read_lang(path):
    pairs, en, problems = {}, None, []
    for no, line in enumerate(open(path, encoding="utf-8-sig"), 1):
        line = line.rstrip("\r\n")
        if not line or line.startswith("#"): continue
        m = re.match(r"(\w\w) = (.*)$", line)
        if not m: problems.append(f"{path}:{no}: not 'xx = text'"); continue
        if m.group(1) == "en": en = unesc(m.group(2)); continue
        if en is None: problems.append(f"{path}:{no}: a translation without its English line"); continue
        if en in pairs: problems.append(f"{path}:{no}: twice: {en[:50]!r}")
        pairs[en] = (unesc(m.group(2)), no); en = None
    return pairs, problems


def conversions(s):
    return re.findall(r"%[-+ #0-9.*hlLzjt]*([a-zA-Z])", s.replace("%%", ""))


def main():
    texts = english_texts()
    if len(sys.argv) == 3 and sys.argv[1] == "--missing":
        lang = next(l for l in LANGS if CODES[l] == sys.argv[2] or l == sys.argv[2])
        path = os.path.join(ROOT, "lang", lang + ".txt")
        have = read_lang(path)[0] if os.path.exists(path) else {}
        for t, where in texts.items():
            if t not in have: print(f"# {where}\nen = {esc(t)}\n{CODES[lang]} = \n")
        return 0
    bad = 0
    print(f"{len(texts)} English texts in the code")
    for lang in LANGS:
        path = os.path.join(ROOT, "lang", lang + ".txt")
        if not os.path.exists(path): print(f"{lang}: no file"); continue
        pairs, problems = read_lang(path)
        for p in problems: print("  " + p); bad += 1
        missing = [t for t in texts if t not in pairs]
        stale = [t for t in pairs if t not in texts]
        for t, (tr, no) in pairs.items():
            if conversions(t) != conversions(tr):
                print(f"  {path}:{no}: %-conversions differ from the English: {t[:50]!r}"); bad += 1
            if t.count("\t") != tr.count("\t"):
                print(f"  {path}:{no}: the tab (shortcut column) differs: {t[:50]!r}")
            if "&" in t and "&" not in tr:
                print(f"  {path}:{no}: no & (keyboard letter) in the translation: {t[:50]!r}")
        for t in stale: print(f"  {path}:{pairs[t][1]}: not in the code any more: {t[:60]!r}"); bad += 1
        print(f"{lang}: {len(pairs) - len(stale)} of {len(texts)} translated" + (f", {len(missing)} missing" if missing else ""))
        for t in missing[:10]: print(f"    missing ({texts[t]}): {t[:70]!r}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
