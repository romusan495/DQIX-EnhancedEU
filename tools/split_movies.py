"""Cut the HD movies (tools/upscale_movies.py) into parts under a size, at groups of pictures:
<name>.mpg -> <name>.1.mpg, <name>.2.mpg... (the game reads either). Small ones are copied.

usage: split_movies.py <in_dir> <out_dir> [max MB, default 25]
"""
import os, re, shutil, sys
src, out = sys.argv[1], sys.argv[2]
limit = int(float(sys.argv[3] if len(sys.argv) > 3 else 25) * 1024 * 1024)
os.makedirs(out, exist_ok=True)
for n in sorted(os.listdir(src)):
    if n.endswith(".sig"): shutil.copy(f"{src}/{n}", f"{out}/{n}")
    if not n.endswith(".mpg") or n.count(".") > 1: continue
    d = open(f"{src}/{n}", "rb").read()
    if len(d) <= limit: shutil.copy(f"{src}/{n}", f"{out}/{n}"); continue
    cuts = [m.start() for m in re.finditer(b"\x00\x00\x01\xb3", d)] + [len(d)]
    parts, start = [], 0
    for a, b in zip(cuts, cuts[1:]):
        if b - start > limit and a > start: parts.append((start, a)); start = a
    parts.append((start, len(d)))
    for i, (a, b) in enumerate(parts, 1):
        open(f"{out}/{n[:-4]}.{i}.mpg", "wb").write(d[a:b])
    print(n, len(parts), "parts")
