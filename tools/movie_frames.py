"""Decode a DS Mobiclip .mods into RGB frames, as the game shows them (YCoCg, chroma not interpolated)."""
import subprocess, numpy as np
def frames(path):
    p = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "yuv420p", "-"], capture_output=True).stdout
    fs = 256 * 192 * 3 // 2
    raw = np.frombuffer(p, np.uint8)[: len(p) // fs * fs].reshape(-1, fs)
    for r in raw:
        r = r.astype(np.int16)
        Y = r[:49152].reshape(192, 256)
        Cg = r[49152:61440].reshape(96, 128).repeat(2, 0).repeat(2, 1) - 128
        Co = r[61440:].reshape(96, 128).repeat(2, 0).repeat(2, 1) - 128
        t = Y - Cg
        yield np.clip(np.stack([t + Co, Y + Cg, t - Co], -1), 0, 255).astype(np.uint8)
