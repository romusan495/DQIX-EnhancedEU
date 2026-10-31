"""Smoke test of a built game: play a scripted input from a save, take window
shots at chosen frames, and compare them with reference shots.

    python3 tests/smoke/run.py --game game --save dqix.sav --input input.txt \
        --frames 3300,5300 --out out/eu [--ref ref/eu] [--tolerance 6]

Linux only (runs the game under xvfb-run, OpenGL through Mesa). Nothing from
the game or the player is kept in the project: the save and its input script
live outside the repository (see docs/SUIVI.md for the cloud recipe).

Exit code: 0 when the game ran to the last frame, every shot exists and, with
--ref, every shot is within the tolerance (mean difference per channel,
0-255); 1 otherwise. A shot without a reference is reported, not failed, so
the first run can make the references (--ref pointing at an empty folder
fills it).
"""
import argparse
import os
import shutil
import subprocess
import sys

SETTINGS = """settings_version = 2
mode = enhanced
update_check = 0
render_scale = 2
window_scale = 4
wireless = 0
"""
LINKS = ("data", "fonts", "textures", "skins", "wfc")


def read_ppm(path):
    """(width, height, rgb bytes) of a binary PPM (P6, maxval 255)."""
    with open(path, "rb") as f:
        data = f.read()
    fields, pos = [], 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            pos = data.index(b"\n", pos) + 1
            continue
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[pos:end])
        pos = end
    if fields[0] != b"P6" or fields[3] != b"255":
        raise ValueError(f"{path}: not an 8-bit P6 PPM")
    w, h = int(fields[1]), int(fields[2])
    return w, h, data[pos + 1:pos + 1 + w * h * 3]


def mean_diff(a, b):
    """Mean absolute difference per channel of two PPMs, or None if their sizes differ."""
    wa, ha, pa = read_ppm(a)
    wb, hb, pb = read_ppm(b)
    if (wa, ha) != (wb, hb):
        return None
    step = 7                                   # every 7th byte is plenty and keeps it quick
    total = sum(abs(pa[i] - pb[i]) for i in range(0, len(pa), step))
    return total / len(range(0, len(pa), step))


def to_png(ppm):
    """A PNG beside the PPM for people to look at, when Pillow is there."""
    try:
        from PIL import Image
    except ImportError:
        return
    Image.open(ppm).save(ppm[:-4] + ".png")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game", required=True, help="the built game folder (tools/build-game.sh)")
    ap.add_argument("--save", required=True, help="the save to start from (copied, never changed)")
    ap.add_argument("--input", required=True, help="DQIX_INPUT_FILE script")
    ap.add_argument("--frames", required=True, help="frames to shoot, e.g. 3300,5300")
    ap.add_argument("--out", required=True, help="run folder (emptied first)")
    ap.add_argument("--ref", help="reference shots folder (winNNNNNN.ppm)")
    ap.add_argument("--tolerance", type=float, default=6.0)
    ap.add_argument("--rtc", default="1792693605", help="fixed clock start (DQIX_RTC)")
    ap.add_argument("--timeout", type=int, default=1200)
    a = ap.parse_args()

    game = os.path.abspath(a.game)
    exe = os.path.join(game, "dqix")
    if not os.path.exists(exe):
        sys.exit(f"no game at {exe}: build it first (sh tools/build-game.sh <rom>)")
    frames = sorted(int(x) for x in a.frames.split(","))
    out = os.path.abspath(a.out)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    shutil.copy(a.save, os.path.join(out, "dqix.sav"))
    shutil.copy(a.input, os.path.join(out, "input.txt"))
    with open(os.path.join(out, "settings.ini"), "w") as f:
        f.write(SETTINGS)
    for name in LINKS:
        if os.path.exists(os.path.join(game, name)):
            os.symlink(os.path.join(game, name), os.path.join(out, name))

    env = dict(os.environ, DQIX_FAST="1", DQIX_QUIT_NOASK="1", DQIX_RTC=a.rtc,
               DQIX_INPUT_FILE="input.txt", DQIX_WINSHOT_AT=",".join(map(str, frames)))
    cmd = ["xvfb-run", "-a", "-s", "-screen 0 1920x1200x24",
           exe, "data/dqix.nds", "data/code", str(frames[-1] + 30), "1", "dqix.sav"]
    with open(os.path.join(out, "run.log"), "w") as log:
        try:
            rc = subprocess.run(cmd, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT,
                                timeout=a.timeout).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    ok = rc == 0
    print(f"game: exit {rc}" + ("" if ok else "  FAIL (see run.log)"))

    for fr in frames:
        name = f"win{fr:06d}.ppm"
        shot = os.path.join(out, name)
        if not os.path.exists(shot):
            print(f"{name}: missing  FAIL")
            ok = False
            continue
        to_png(shot)
        if not a.ref:
            print(f"{name}: taken")
            continue
        ref = os.path.join(a.ref, name)
        if not os.path.exists(ref):
            os.makedirs(a.ref, exist_ok=True)
            shutil.copy(shot, ref)
            print(f"{name}: no reference yet, this one kept as the reference")
            continue
        d = mean_diff(shot, ref)
        if d is None:
            print(f"{name}: size differs from the reference  FAIL")
            ok = False
        else:
            bad = d > a.tolerance
            print(f"{name}: difference {d:.2f}" + ("  FAIL" if bad else ""))
            ok = ok and not bad
    print("smoke test: " + ("OK" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
