#!/usr/bin/env python3
"""Ranks how far this renderer's frames are from MilkDrop's captures of the same presets.

    python3 compare_frames.py <reference dir> <our dir> <out dir> [--top N]

Both directories hold one image per preset, named by the preset's path with '/' as '__'
(PNG or JPEG). The tool writes ranking.csv and contact sheets of the biggest differences to
<out dir>. Presets use random numbers, so two renders of the same preset are never identical:
read the scores as a way to find presets worth a look, not as a pass mark.

Scores are on 0..1 scales. brightness is the difference in mean luma, colour the distance
between mean colours, structure is 1 minus the correlation of the two luma images at 32x32
(1 when they are unrelated), and histogram the distance between their luma histograms.
"""
import csv
import os
import sys

import numpy as np
from PIL import Image, ImageDraw


def load(path, size):
    return np.asarray(Image.open(path).convert("RGB").resize((size, size), Image.BILINEAR), dtype=np.float64) / 255.0


def luma(img):
    return 0.299 * img[..., 0] + 0.587 * img[..., 1] + 0.114 * img[..., 2]


def small(img):
    return np.asarray(Image.fromarray((img * 255).astype(np.uint8)).resize((32, 32), Image.BILINEAR), dtype=np.float64) / 255.0


def correlation(a, b):
    a = a - a.mean()
    b = b - b.mean()
    denom = np.sqrt((a * a).sum() * (b * b).sum())
    if denom < 1e-9:
        return 1.0 if np.abs(a).sum() < 1e-6 and np.abs(b).sum() < 1e-6 else 0.0
    return float((a * b).sum() / denom)


def score(ref, ours):
    lr, lo = luma(ref), luma(ours)
    brightness = abs(lr.mean() - lo.mean())
    colour = float(np.linalg.norm(ref.mean((0, 1)) - ours.mean((0, 1))) / np.sqrt(3))
    structure = 1.0 - max(0.0, correlation(small(lr), small(lo)))
    hr = np.histogram(lr, bins=16, range=(0, 1))[0] / lr.size
    ho = np.histogram(lo, bins=16, range=(0, 1))[0] / lo.size
    histogram = float(0.5 * np.abs(hr - ho).sum())
    total = 0.3 * brightness + 0.25 * colour + 0.25 * structure + 0.2 * histogram
    return brightness, colour, structure, histogram, total


def index(directory):
    out = {}
    for name in os.listdir(directory):
        stem, ext = os.path.splitext(name)
        if ext.lower() in (".png", ".jpg", ".jpeg"):
            out[stem] = os.path.join(directory, name)
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    top = 24
    if "--top" in sys.argv:
        top = int(sys.argv[sys.argv.index("--top") + 1])
        args = [a for a in args if a != str(top)]
    if len(args) != 3:
        print(__doc__)
        return 2
    ref_dir, our_dir, out_dir = args
    os.makedirs(out_dir, exist_ok=True)
    ref, ours = index(ref_dir), index(our_dir)
    rows = []
    for name in sorted(set(ref) & set(ours)):
        r, o = load(ref[name], 128), load(ours[name], 128)
        rows.append((name,) + score(r, o))
    rows.sort(key=lambda r: -r[5])
    with open(os.path.join(out_dir, "ranking.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["preset", "brightness", "colour", "structure", "histogram", "score"])
        for row in rows:
            w.writerow([row[0]] + ["%.3f" % v for v in row[1:]])
    cell = 256
    per_sheet = 12
    for start in range(0, min(top, len(rows)), per_sheet):
        chunk = rows[start:start + per_sheet]
        sheet = Image.new("RGB", (cell * 2 * 3 + 40, (cell + 20) * ((len(chunk) + 2) // 3)), (30, 30, 30))
        draw = ImageDraw.Draw(sheet)
        for i, row in enumerate(chunk):
            x, y = (i % 3) * (cell * 2 + 10), (i // 3) * (cell + 20)
            sheet.paste(Image.open(ref[row[0]]).convert("RGB").resize((cell, cell)), (x, y + 20))
            sheet.paste(Image.open(ours[row[0]]).convert("RGB").resize((cell, cell)), (x + cell, y + 20))
            draw.text((x + 2, y + 4), "%.2f %s" % (row[5], row[0][:55]), fill=(220, 220, 220))
        sheet.save(os.path.join(out_dir, "worst_%02d.png" % (start // per_sheet + 1)))
    print("compared %d presets (%d only in the reference, %d only here)" % (len(rows), len(set(ref) - set(ours)), len(set(ours) - set(ref))))
    print("median score %.3f" % (float(np.median([r[5] for r in rows])) if rows else 0.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
