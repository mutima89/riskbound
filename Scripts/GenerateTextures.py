"""Draws Riskbound's own textures: fictional screen UIs, a whiteboard sketch, abstract wall prints and a polished concrete floor.

All names and figures are invented (Docs/GameDesign.md "Fictional names"). Output: Data/Assets/generated/*.png,
imported by Scripts/ImportAssets.py. Deterministic: same output every run. Needs Pillow.
"""
import os
import random

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Data", "Assets", "generated")
FONT_DIR = os.path.join(ROOT, "Resources", "Fonts")

BG = (14, 18, 24)
PANEL = (24, 30, 39)
LINE = (44, 52, 64)
TEXT = (226, 230, 234)
MUTED = (140, 150, 162)
TEAL = (43, 163, 168)
AMBER = (227, 163, 59)
RED = (217, 83, 79)
GREEN = (92, 196, 138)


def font(size, weight="Regular"):
    return ImageFont.truetype(os.path.join(FONT_DIR, f"Inter-{weight}.ttf"), size)


def chrome(d, w, h, title):
    d.rectangle([0, 0, w, 44], fill=(10, 13, 18))
    d.text((18, 12), title, font=font(18, "SemiBold"), fill=TEXT)
    d.text((w - 210, 14), "Valtren risk workspace", font=font(14), fill=MUTED)
    for i, c in enumerate([RED, AMBER, GREEN]):
        d.ellipse([w - 90 + i * 22, 16, w - 78 + i * 22, 28], fill=c)


def dashboard():
    w, h = 1024, 576
    im = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(im)
    chrome(d, w, h, "Operational risk dashboard  -  Northgate")
    rnd = random.Random(7)
    tiles = [("Open risks", "37", TEXT), ("Above appetite", "4", RED), ("Overdue actions", "6", AMBER), ("KRIs red", "2", RED)]
    for i, (k, v, c) in enumerate(tiles):
        x = 18 + i * 248
        d.rounded_rectangle([x, 62, x + 236, 150], 8, fill=PANEL)
        d.text((x + 16, 74), k, font=font(16), fill=MUTED)
        d.text((x + 16, 98), v, font=font(40, "SemiBold"), fill=c)
    d.rounded_rectangle([18, 166, 620, 556], 8, fill=PANEL)
    d.text((34, 178), "Temperature excursions per week", font=font(16, "Medium"), fill=TEXT)
    base = 520
    for i in range(24):
        v = max(4, int(rnd.gauss(60, 35)) if i < 18 else int(rnd.gauss(25, 10)))
        x = 40 + i * 24
        d.rectangle([x, base - v * 2.6, x + 14, base], fill=RED if v > 90 else (AMBER if v > 60 else TEAL))
    d.line([36, base, 600, base], fill=LINE, width=2)
    d.rounded_rectangle([634, 166, 1006, 556], 8, fill=PANEL)
    d.text((650, 178), "Risk matrix (residual)", font=font(16, "Medium"), fill=TEXT)
    for r in range(5):
        for c in range(5):
            score = (5 - r) * (c + 1)
            col = RED if score >= 16 else AMBER if score >= 10 else (180, 160, 60) if score >= 5 else GREEN
            x, y = 660 + c * 66, 214 + r * 64
            d.rectangle([x, y, x + 60, y + 58], fill=tuple(int(v * 0.55) for v in col))
            if (r, c) in [(1, 3), (0, 2), (2, 4), (3, 1)]:
                d.ellipse([x + 20, y + 19, x + 40, y + 39], fill=TEXT)
    return im


def spreadsheet():
    w, h = 1024, 576
    im = Image.new("RGB", (w, h), (246, 247, 248))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w, 44], fill=(34, 40, 49))
    d.text((18, 12), "R-022 fleet fuel price scenarios", font=font(18, "SemiBold"), fill=(236, 238, 240))
    cols = ["Scenario", "Annual p", "Loss", "p x Loss"]
    # Deliberately NOT Mission 001's figures: world dressing must never hint at a rubric.
    rows = [("Diesel +10% for a quarter", "0.40", "65,000", "26,000"), ("Diesel +25% for two quarters", "0.12", "210,000", "25,200"),
            ("Supply shortage, fleet idle 3 days", "0.04", "480,000", "19,200"), ("", "", "Expected loss", "70,400"), ("", "", "Worst case", "480,000")]
    xs = [20, 420, 600, 800]
    y = 70
    d.rectangle([0, y - 6, w, y + 30], fill=(225, 238, 239))
    for x, c in zip(xs, cols):
        d.text((x, y), c, font=font(17, "SemiBold"), fill=(30, 36, 44))
    for r in rows:
        y += 46
        d.line([0, y - 8, w, y - 8], fill=(214, 218, 222))
        for x, c in zip(xs, r):
            d.text((x, y), c, font=font(17, "SemiBold" if r[0] == "" else "Regular"), fill=(30, 36, 44))
    for i in range(6):
        y += 46
        d.line([0, y - 8, w, y - 8], fill=(214, 218, 222))
    for x in xs[1:]:
        d.line([x - 14, 60, x - 14, h], fill=(214, 218, 222))
    return im


def email():
    w, h = 1024, 576
    im = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(im)
    chrome(d, w, h, "Inbox")
    mails = [("Priya Okafor", "Welcome aboard - see me when you arrive", True), ("Northgate Ops", "IR-2291 incident report attached", True),
             ("Facilities", "Bay 3 compressor service rescheduled", False), ("Kelvarin Refrigeration", "Quote: quarterly service contract", False),
             ("Risk Committee", "Agenda: operational risk appetite review", False), ("Training", "Course: control testing basics", False),
             ("Orrinbrook Mutual", "Renewal schedule - stock cover", False)]
    for i, (who, subj, unread) in enumerate(mails):
        y = 60 + i * 70
        d.rectangle([0, y, w, y + 66], fill=PANEL if unread else BG)
        if unread:
            d.rectangle([0, y, 5, y + 66], fill=TEAL)
        d.text((24, y + 10), who, font=font(17, "SemiBold" if unread else "Regular"), fill=TEXT)
        d.text((24, y + 36), subj, font=font(15), fill=MUTED)
        d.text((w - 90, y + 12), f"{9 + i % 3}:{(17 * i) % 60:02d}", font=font(14), fill=MUTED)
    return im


def alarm():
    w, h = 1024, 576
    im = Image.new("RGB", (w, h), (20, 10, 10))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w, 44], fill=(70, 18, 16))
    d.text((18, 12), "Northgate cold store - temperature alarms", font=font(18, "SemiBold"), fill=TEXT)
    rnd = random.Random(3)
    for i in range(10):
        y = 60 + i * 50
        t = 8.0 + rnd.random() * 4.5
        col = RED if t > 9.5 else AMBER
        d.rounded_rectangle([18, y, w - 18, y + 42], 6, fill=(36, 22, 22))
        d.text((32, y + 10), f"Bay {rnd.randint(1, 4)}  {t:4.1f} C", font=font(17, "SemiBold"), fill=col)
        d.text((260, y + 11), "ACKNOWLEDGED - no follow-up action recorded", font=font(15), fill=MUTED)
        d.text((w - 120, y + 11), f"0{rnd.randint(0, 5)}:{rnd.randint(10, 59)}", font=font(15), fill=MUTED)
    return im


def whiteboard():
    w, h = 1024, 384
    im = Image.new("RGB", (w, h), (238, 240, 241))
    d = ImageDraw.Draw(im)
    marker = (34, 64, 140)
    d.text((40, 24), "Cause  ->  Event  ->  Consequence", font=font(34, "Medium"), fill=marker)
    d.rectangle([40, 100, 300, 190], outline=marker, width=4)
    d.text((56, 130), "Compressor fails", font=font(24), fill=marker)
    d.rectangle([380, 100, 640, 190], outline=marker, width=4)
    d.text((396, 130), "Stock above 8 C", font=font(24), fill=marker)
    d.rectangle([720, 100, 980, 190], outline=(170, 40, 40), width=4)
    d.text((736, 130), "Penalty / client loss", font=font(24), fill=(170, 40, 40))
    for x in (300, 640):
        d.line([x + 6, 145, x + 74, 145], fill=marker, width=4)
        d.polygon([(x + 74, 135), (x + 74, 155), (x + 86, 145)], fill=marker)
    d.text((40, 230), "Controls: alarm (detective)  service (preventive)  manual checks", font=font(24), fill=(40, 120, 70))
    d.text((40, 290), "Residual = what is left AFTER controls that actually work", font=font(24, "SemiBold"), fill=marker)
    return im


def art(seed):
    w, h = 512, 640
    rnd = random.Random(seed)
    im = Image.new("RGB", (w, h), (232, 228, 220))
    d = ImageDraw.Draw(im)
    palette = [TEAL, (36, 44, 56), AMBER, (196, 186, 170), (90, 120, 130)]
    for _ in range(7):
        x, y = rnd.randint(-60, w - 60), rnd.randint(-60, h - 60)
        s = rnd.randint(120, 320)
        if rnd.random() < 0.5:
            d.ellipse([x, y, x + s, y + s], fill=rnd.choice(palette))
        else:
            d.rectangle([x, y, x + s, y + int(s * rnd.uniform(0.3, 1.2))], fill=rnd.choice(palette))
    return im


def polished_concrete():
    """Clean, light polished concrete: soft tileable mottling, fine aggregate, one saw-cut joint per repeat."""
    import math
    w = h = 1024
    rnd = random.Random(7)
    waves = [(rnd.randint(-5, 5), rnd.randint(-5, 5), rnd.uniform(0, 6.28), rnd.uniform(0.8, 1.8)) for _ in range(14)]
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            # integer frequencies keep the pattern seamless across repeats
            v = sum(a * math.sin(2 * math.pi * (fx * x / w + fy * y / h) + ph) for fx, fy, ph, a in waves)
            g = int(184 + v)
            px[x, y] = (g, g + 1, g + 3)
    d = ImageDraw.Draw(im)
    for _ in range(9000):  # exposed fine aggregate
        x, y = rnd.randrange(w), rnd.randrange(h)
        g = rnd.randint(150, 215)
        d.point((x, y), fill=(g, g, g + 2))
    for i in range(2):  # saw-cut joints on two edges, so the repeat reads as slabs
        d.line([(0, i), (w, i)], fill=(140, 142, 145))
        d.line([(i, 0), (i, h)], fill=(140, 142, 145))
    return im


def kri_ticker():
    """Status rail under the monitoring wall: invented company-wide KRIs (not Mission 001 figures)."""
    w, h = 2048, 128
    im = Image.new("RGB", (w, h), (8, 11, 16))
    d = ImageDraw.Draw(im)
    items = [("On-time delivery", "96.2%", GREEN), ("Service backlog", "12", AMBER), ("Hours breaches", "2", RED)]
    x = 36
    d.text((x, 34), "KRI", font=font(50, "Bold"), fill=TEAL)
    x += 150
    for name, value, col in items:
        d.ellipse([x, 52, x + 24, 76], fill=col)
        d.text((x + 40, 34), name, font=font(46), fill=MUTED)
        x += 54 + int(d.textlength(name, font=font(46)))
        d.text((x, 34), value, font=font(46, "SemiBold"), fill=TEXT)
        x += int(d.textlength(value, font=font(46, "SemiBold"))) + 64
    assert x <= w, "ticker text overflows"
    centred = Image.new("RGB", (w, h), (8, 11, 16))
    centred.paste(im.crop((0, 0, x, h)), ((w - x + 64) // 2, 0))
    return centred

def main():
    os.makedirs(OUT, exist_ok=True)
    items = {"T_ScreenDashboard": dashboard(), "T_ScreenSheet": spreadsheet(), "T_ScreenEmail": email(), "T_ScreenAlarm": alarm(),
             "T_Whiteboard": whiteboard(), "T_ArtPrintA": art(11), "T_ArtPrintB": art(29), "T_ArtPrintC": art(53),
             "T_PolishedConcrete": polished_concrete(), "T_ScreenKriTicker": kri_ticker()}
    for name, im in items.items():
        im.save(os.path.join(OUT, name + ".png"))
        print("wrote", name)


if __name__ == "__main__":
    main()
