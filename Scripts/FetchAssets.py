"""Downloads the third-party art assets listed in Data/AssetManifest.json and verifies their SHA-256.

Only redistributable licences are allowed in the manifest (CC0 from Poly Haven / ambientCG, SIL OFL fonts).
Raw downloads go to Data/Assets/raw/ (not committed); Scripts/ImportAssets.py turns them into
Content/Riskbound/ assets inside the editor.

  python Scripts/FetchAssets.py             download + verify everything in the manifest
  python Scripts/FetchAssets.py --resolve   rebuild the manifest from WANTED (queries the Poly Haven API,
                                            downloads, records sha256), then write Docs/AssetCredits.md
"""
import argparse
import hashlib
import io
import json
import os
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "Data", "AssetManifest.json")
RAW = os.path.join(ROOT, "Data", "Assets", "raw")
FONTS = os.path.join(ROOT, "Resources", "Fonts")
CREDITS = os.path.join(ROOT, "Docs", "AssetCredits.md")
UA = {"User-Agent": "Riskbound-asset-fetch/1.0"}

# (kind, Poly Haven id, role in the game)
WANTED = [
    ("texture", "herringbone_parquet", "Parquet floor: reception, manager's office, boardroom"),
    ("texture", "laminate_floor_02", "Work-area floor: analyst workspace, training, monitoring"),
    ("texture", "smooth_concrete_floor", "Corridor floor"),
    ("texture", "plastered_wall_04", "Interior walls"),
    ("texture", "oak_veneer_01", "Light oak desks and tables"),
    ("texture", "black_walnut_veneer_01", "Walnut: manager's desk, boardroom table"),
    ("texture", "poly_wool_herringbone", "Acoustic wall panels, soft furnishings"),
    ("texture", "terrazzo_tiles", "Reception feature floor"),
    ("model", "modern_arm_chair_01", "Task and visitor chairs"),
    ("model", "dining_chair_02", "Boardroom and meeting chairs"),
    ("model", "sofa_02", "Reception sofa"),
    ("model", "modern_coffee_table_01", "Reception coffee table"),
    ("model", "potted_plant_02", "Large plants"),
    ("model", "potted_plant_04", "Desk plants"),
    ("model", "drawer_cabinet", "Filing cabinet (insurance schedule evidence)"),
    ("model", "binder_notebook", "Maintenance binder (evidence)"),
    ("model", "clipboard", "Manual temperature log clipboard (evidence)"),
    ("model", "office_notepads", "Document wallet / desk notepads (evidence, dressing)"),
    ("model", "wall_clock", "Wall clocks"),
    ("model", "projector_screen", "Meeting room screen"),
    ("model", "modern_ceiling_lamp_01", "Boardroom pendant lights"),
    ("hdri", "canary_wharf", "Business-district view through the windows, sky light"),
]
# Real-world brand marks found in downloaded textures (Docs/GameDesign.md "Fictional names"):
# each box is painted over with the average colour of a 6 px ring around it.
EDITS = {
    "polyhaven/wall_clock/wall_clock_diff_1k.jpg": [(252, 612, 307, 660)],             # "URBAN quartz" on the dial
    "polyhaven/projector_screen/projector_screen_frame_diff_1k.jpg": [(702, 458, 746, 546)],  # "SIRIUS steering" on the housing
}
EDITED = os.path.join(ROOT, "Data", "Assets", "edited")
FONT_ZIP = ("font", "inter", "https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip",
            ["Inter-Regular.ttf", "Inter-Medium.ttf", "Inter-SemiBold.ttf", "Inter-Bold.ttf"])
FONT_LICENCE = "https://raw.githubusercontent.com/google/fonts/main/ofl/inter/OFL.txt"


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r:
        return r.read()


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def polyhaven_files(asset_id, kind):
    """Returns [(url, relative path)] for the 1k JPG maps (2k HDR for HDRIs) of an asset."""
    files = json.loads(fetch("https://api.polyhaven.com/files/" + asset_id))
    out = []
    if kind == "hdri":
        f = files["hdri"]["2k"]["hdr"]
        return [(f["url"], f"{asset_id}/{asset_id}_2k.hdr")]
    if kind == "model":
        f = files["fbx"]["1k"]["fbx"]
        out.append((f["url"], f"{asset_id}/{asset_id}_1k.fbx"))
    for key, res in files.items():
        # diffuse, DirectX normal and packed AO/Rough/Metal per material part.
        if key in ("Diffuse", "nor_dx", "arm") or key.endswith(("_diff", "_nor_dx", "_arm")):
            jpg = res.get("1k", {}).get("jpg")
            if jpg:
                out.append((jpg["url"], f"{asset_id}/{os.path.basename(jpg['url'])}"))
    return out


def resolve():
    assets = []
    for kind, asset_id, role in WANTED:
        info = json.loads(fetch("https://api.polyhaven.com/info/" + asset_id))
        entry = {
            "id": asset_id, "kind": kind, "role": role, "source": "Poly Haven",
            "page": f"https://polyhaven.com/a/{asset_id}", "licence": "CC0 1.0",
            "authors": sorted((info.get("authors") or {}).keys()), "name": info.get("name", asset_id), "files": [],
        }
        for url, rel in polyhaven_files(asset_id, kind):
            data = fetch(url)
            entry["files"].append({"url": url, "path": f"polyhaven/{rel}", "sha256": sha256(data), "size": len(data)})
            save(os.path.join(RAW, "polyhaven", rel), data)
        print(f"resolved {asset_id}: {len(entry['files'])} files")
        assets.append(entry)
    data = fetch(FONT_ZIP[2])
    assets.append({"id": FONT_ZIP[1], "kind": "font", "role": "UI typeface", "source": "rsms/inter (Rasmus Andersson)",
                   "page": "https://rsms.me/inter/", "licence": "SIL Open Font License 1.1", "authors": ["Rasmus Andersson"],
                   "name": "Inter 4.1", "extract": FONT_ZIP[3],
                   "files": [{"url": FONT_ZIP[2], "path": "fonts/Inter-4.1.zip", "sha256": sha256(data), "size": len(data)}]})
    save(os.path.join(RAW, "fonts", "Inter-4.1.zip"), data)
    lic = fetch(FONT_LICENCE)
    assets[-1]["files"].append({"url": FONT_LICENCE, "path": "fonts/OFL.txt", "sha256": sha256(lic), "size": len(lic)})
    save(os.path.join(RAW, "fonts", "OFL.txt"), lic)
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    with open(MANIFEST, "w", encoding="utf-8", newline="\n") as f:
        json.dump({"version": 1, "assets": assets}, f, indent=1)
        f.write("\n")
    print("wrote", MANIFEST)


def save(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def download(manifest):
    bad = 0
    for a in manifest["assets"]:
        for f in a["files"]:
            path = os.path.join(RAW, f["path"])
            data = open(path, "rb").read() if os.path.exists(path) else None
            if data is None or sha256(data) != f["sha256"]:
                data = fetch(f["url"])
                if sha256(data) != f["sha256"]:
                    print("SHA-256 MISMATCH", f["url"])
                    bad += 1
                    continue
                save(path, data)
        print("ok", a["id"])
    return bad


def install_fonts(manifest):
    for a in manifest["assets"]:
        if a["kind"] != "font":
            continue
        os.makedirs(FONTS, exist_ok=True)
        z = zipfile.ZipFile(io.BytesIO(open(os.path.join(RAW, "fonts", "Inter-4.1.zip"), "rb").read()))
        for wanted in a["extract"]:
            member = next(n for n in z.namelist() if n.endswith("/" + wanted) or n == wanted)
            save(os.path.join(FONTS, wanted), z.read(member))
        save(os.path.join(FONTS, "OFL.txt"), open(os.path.join(RAW, "fonts", "OFL.txt"), "rb").read())
        print("fonts installed in", FONTS)


def apply_edits():
    from PIL import Image  # pip install pillow
    for rel, boxes in EDITS.items():
        img = Image.open(os.path.join(RAW, rel)).convert("RGB")
        px = img.load()
        for (x0, y0, x1, y1) in boxes:
            ring = [px[x, y] for x in range(x0 - 6, x1 + 6) for y in range(y0 - 6, y1 + 6)
                    if not (x0 <= x < x1 and y0 <= y < y1)]
            avg = tuple(sum(c[i] for c in ring) // len(ring) for i in range(3))
            for x in range(x0, x1):
                for y in range(y0, y1):
                    px[x, y] = avg
        out = os.path.join(EDITED, rel)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        img.save(out, quality=95)
        print("edited", rel)


def write_credits(manifest):
    lines = ["# Asset credits", "",
             "Generated from Data/AssetManifest.json by Scripts/FetchAssets.py. Every asset below allows redistribution in a game.",
             "Assets under the Fab Standard License or Epic template content are not stored in this public repository (see the end).", "",
             "| Asset | Use in Riskbound | Source | Author(s) | Licence |", "|---|---|---|---|---|"]
    for a in manifest["assets"]:
        lines.append(f"| [{a['name']}]({a['page']}) | {a['role']} | {a['source']} | {', '.join(a['authors'])} | {a['licence']} |")
    lines += ["", "## Logo and brand check",
              "Every model and texture above was inspected for real-world logos or brand text before use (contact sheet of all diffuse maps, then zoomed crops).",
              "- Wall Clock: dial printed \"URBAN quartz\" - painted out (Scripts/FetchAssets.py EDITS; the importer uses Data/Assets/edited/).",
              "- Projector Screen: housing printed \"SIRIUS steering\" - painted out the same way.",
              "- Stationery Supplies: several labels printed \"Haven Pinworks\" - model excluded rather than edited.",
              "- All other assets: no text or logos found.", "",
              "## Not in the repository",
              "- UE mannequin (Priya Okafor fallback), Epic template content: copied from the local engine install by `Scripts/CopyEngineContent.bat` into `Content/Characters/` (git-ignored).", ""]
    with open(CREDITS, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("wrote", CREDITS)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--resolve", action="store_true")
    args = p.parse_args()
    if args.resolve:
        resolve()
    manifest = json.load(open(MANIFEST, encoding="utf-8"))
    bad = download(manifest)
    install_fonts(manifest)
    apply_edits()
    if args.resolve:
        write_credits(manifest)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
