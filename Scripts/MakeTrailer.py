"""Cuts a silent 1080p trailer from in-game screenshots.

Usage: python Scripts/MakeTrailer.py <shots dir> <out.mp4> [shot list .json]

The shot list is [["file name or glob", "caption"], ...] in order; without it the default list below
picks walkthrough captures (Walk_*.png). Each shot gets a slow push-in, a caption and a crossfade;
title and end cards frame the cut. Needs opencv-python, numpy and Pillow. No audio track: OpenCV
cannot mux sound, so add music in an editor (the game's own credits list the licences).
"""
import glob
import json
import os
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H, FPS = 1920, 1080, 30
SHOT_SECONDS, FADE_SECONDS, CARD_SECONDS = 3.2, 0.6, 3.0
INK, ACCENT, BG = (232, 238, 240), (43, 163, 168), (11, 18, 20)
FONT_DIR = os.path.join(os.environ.get("WINDIR", "C:/Windows"), "Fonts")

DEFAULT_SHOTS = [
    ["*Reception*", "Valtren Coldline Logistics. Your first day."],
    ["*Briefing*", "Every case starts with a brief."],
    ["*Evidence*", "Find the evidence. Some of it walks and talks."],
    ["*Register*", "The register said Medium. Was it?"],
    ["*MonteCarlo*", "Run the numbers. Expected loss is not the worst case."],
    ["*Debrief*", "The rubric stays hidden until you commit."],
    ["*Inbox*", "Your inbox will not wait."],
    ["*Promotion*", "Earn every title."],
    ["*Lectern*", "Present to the board in person."],
    ["*Ending*", "Live with what you decided."],
]


def font(size, bold=False):
    for name in (["ARIALNB.TTF", "bahnschrift.ttf"] if bold else ["bahnschrift.ttf", "ARIALN.TTF"]):
        path = os.path.join(FONT_DIR, name)
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def to_bgr(img):
    return cv2.cvtColor(np.asarray(img), cv2.COLOR_RGB2BGR)


def card(lines):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    y = H // 2 - sum(s for _, s, _ in lines) // 2
    for text, size, colour in lines:
        f = font(size, bold=size > 60)
        w = d.textlength(text, font=f)
        d.text(((W - w) / 2, y), text, font=f, fill=colour)
        y += int(size * 1.3)
    return to_bgr(img)


def caption(frame, text):
    img = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
    shade = Image.new("RGBA", (W, 170), (5, 12, 14, 170))
    img.paste(shade, (0, H - 170), shade)
    d = ImageDraw.Draw(img)
    d.rectangle([96, H - 112, 104, H - 58], fill=ACCENT)
    d.text((126, H - 118), text, font=font(48), fill=INK)
    return to_bgr(img)


def fit(path):
    img = cv2.imread(path)
    h, w = img.shape[:2]
    s = max(W / w, H / h) * 1.08  # headroom for the push-in
    img = cv2.resize(img, (int(w * s), int(h * s)), interpolation=cv2.INTER_AREA)
    return img


def shot_frames(path, text):
    big = fit(path)
    bh, bw = big.shape[:2]
    n = int(SHOT_SECONDS * FPS)
    for i in range(n):
        t = i / max(1, n - 1)
        z = 1.0 + 0.06 * t  # 1.0 -> 1.06 of the frame size
        cw, ch = int(bw / z), int(bh / z)
        x0, y0 = (bw - cw) // 2, (bh - ch) // 2
        frame = cv2.resize(big[y0:y0 + ch, x0:x0 + cw], (W, H), interpolation=cv2.INTER_LINEAR)
        yield caption(frame, text)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    shots_dir, out = sys.argv[1], sys.argv[2]
    shots = json.load(open(sys.argv[3], encoding="utf-8")) if len(sys.argv) > 3 else DEFAULT_SHOTS
    writer = cv2.VideoWriter(out, cv2.VideoWriter_fourcc(*"mp4v"), FPS, (W, H))
    fade = int(FADE_SECONDS * FPS)
    last = None

    def emit(frames):
        nonlocal last
        first = True
        for f in frames:
            if first and last is not None:
                for k in range(1, fade + 1):  # crossfade from the previous segment
                    writer.write(cv2.addWeighted(last, 1 - k / fade, f, k / fade, 0))
            first = False
            writer.write(f)
            last = f

    title = card([("RISKBOUND", 140, INK), ("Every warning you ignore comes back.", 46, ACCENT)])
    emit([title] * int(CARD_SECONDS * FPS))
    used = 0
    for pattern, text in shots:
        found = sorted(glob.glob(os.path.join(shots_dir, "**", pattern), recursive=True))
        found = [p for p in found if p.lower().endswith((".png", ".jpg"))]
        if not found:
            print("skip (no match):", pattern)
            continue
        emit(shot_frames(found[0], text))
        used += 1
    end = card([("RISKBOUND", 120, INK), ("From graduate trainee to Chief Risk Officer.", 44, ACCENT),
                ("Offline. Single-player. Windows.", 34, INK)])
    emit([end] * int(CARD_SECONDS * FPS * 1.4))
    writer.release()
    print(f"wrote {out}: {used} shots")
    return 0 if used else 2


if __name__ == "__main__":
    sys.exit(main())
