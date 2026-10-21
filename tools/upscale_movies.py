"""Upscale DQIX's Mobiclip movies 4x with Real-ESRGAN (realesr-animevideov3) into MPEG-1 files
the game plays instead (runtime/hdmovie.c), plus each movie's signature file.

usage: upscale_movies.py <model.pth> <movie_dir with *.mods> <out_dir>

out_dir/<name>.mpg  1024x768 MPEG-1, one picture per movie frame, no B-frames
out_dir/<name>.sig  "DQMS", u32 frames, then per frame 16x12 RGB (each the mean of an
                    16x16 block of the frame as the game shows it, 8 bits per channel)
"""
import os, sys, struct, subprocess, time
import numpy as np, torch, spandrel
sys.path.insert(0, os.path.dirname(__file__))
from movie_frames import frames

model = spandrel.ModelLoader().load_from_file(sys.argv[1]).model.eval()
src, out = sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
names = sorted(n[:-5] for n in os.listdir(src) if n.endswith(".mods"))
if len(sys.argv) > 4: names = [n for n in names if n in sys.argv[4:]]
for name in names:
    if os.path.exists(f"{out}/{name}.mpg"): continue
    sigs = []
    enc = subprocess.Popen(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "1024x768", "-r", "24", "-i", "-",
                            "-c:v", "mpeg1video", "-q:v", "3", "-g", "24", "-bf", "0", "-f", "mpeg1video", f"{out}/{name}.tmp"], stdin=subprocess.PIPE)
    t0 = time.time()
    for i, f in enumerate(frames(f"{src}/{name}.mods")):
        sigs.append(f.reshape(12, 16, 16, 16, 3).mean(axis=(1, 3)).round().astype(np.uint8).tobytes())
        with torch.no_grad():
            o = model(torch.from_numpy(f).float().div(255).permute(2, 0, 1)[None])
        enc.stdin.write((o[0].permute(1, 2, 0).clamp(0, 1).numpy() * 255).round().astype(np.uint8).tobytes())
        if i % 100 == 0: print(name, i, f"{time.time() - t0:.0f}s", flush=True)
    enc.stdin.close(); enc.wait()
    with open(f"{out}/{name}.sig", "wb") as s:
        s.write(b"DQMS" + struct.pack("<I", len(sigs)) + b"".join(sigs))
    os.replace(f"{out}/{name}.tmp", f"{out}/{name}.mpg")
    print(name, "done", len(sigs), "frames", flush=True)
