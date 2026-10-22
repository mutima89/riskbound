"""Downloads the office cast from Microsoft Rocketbox (MIT licence, github.com/microsoft/Microsoft-Rocketbox)
into <repo>/../RiskboundAssets/rocketbox: avatar FBX files (the "_facial" exports, which carry 15 visemes and
ARKit/FACS face shapes for lip-sync and expressions), textures reduced to 1024 px PNG, and the body animations
RbPerson uses (idles, talking and listening gestures, reactions, sitting, walking).
Usage: python Scripts/FetchCharacters.py   (then run Scripts/ImportCharacters.py in the editor). Needs Pillow.
"""
import json, os, sys, urllib.request
from PIL import Image
ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "RiskboundAssets", "rocketbox")
_tree = json.load(urllib.request.urlopen("https://api.github.com/repos/microsoft/Microsoft-Rocketbox/git/trees/master?recursive=1"))
TREE = [t["path"] for t in _tree["tree"]]
RAW = "https://media.githubusercontent.com/media/microsoft/Microsoft-Rocketbox/master/"
RAW2 = "https://raw.githubusercontent.com/microsoft/Microsoft-Rocketbox/master/"
AV = ["Professions/Business_Female_0%d" % i for i in range(1, 5)] + ["Professions/Business_Male_0%d" % i for i in range(1, 8)] + \
     ["Adults/Female_Adult_0%d" % i for i in range(1, 6)] + ["Adults/Male_Adult_0%d" % i for i in range(1, 5)]
AN = [p + n for p in ("m_", "f_") for n in (
    "idle_neutral_01", "idle_neutral_02", "idle_breathe_01", "idle_look_around_01", "documents_idle", "gestic_listen_neutral_01", "cell_phone_listen_01",
    # talking, listening and reactions (RbPerson)
    "gestic_talk_neutral_01", "gestic_talk_neutral_02", "gestic_talk_neutral_03", "gestic_talk_relaxed_01", "gestic_listen_neutral_02",
    "gestic_listen_accept_01", "gestic_listen_accept_02", "gestic_listen_deny_01", "gestic_listen_deny_02", "gestic_shrug_01", "gestic_thoughtful_01",
    # meetings
    "sit_table_idle_neutral_01", "sit_table_breathe_01", "sit_table_gestic_thoughtful", "sit_table_gestic_shrug_01", "sit_chair_idle_neutral_01")]
# Walking loops carry travel ("motextr_xy"); RbPerson pins the hips and moves the person itself.
WALK = [p + n for p in ("m_", "f_") for n in ("walk_neutral_01", "walk_neutral_02")]

def get(path, out):
    if os.path.exists(out) and os.path.getsize(out) > 1000: return
    os.makedirs(os.path.dirname(out), exist_ok=True)
    q = urllib.parse.quote(path)
    for base in (RAW, RAW2):
        try:
            urllib.request.urlretrieve(base + q, out)
            if os.path.getsize(out) > 1000 and not open(out, "rb").read(40).startswith(b"version https://git-lfs"): return
        except Exception as e:
            pass
    print("FAILED", path)

import urllib.parse
from concurrent.futures import ThreadPoolExecutor
jobs = []
def tex_job(p, out):
    png = out[:-4] + ".png"
    if os.path.exists(png): return
    get(p, out)
    try:
        im = Image.open(out); im.thumbnail((1024, 1024)); im.save(png); os.remove(out)
    except Exception as e:
        print("TEX FAIL", p, e)
for a in AV:
    name = a.split("/")[1]
    pre = "Assets/Avatars/%s/" % a
    for p in TREE:
        if not p.startswith(pre): continue
        rel = p[len(pre):]
        if rel.endswith("_facial.fbx") and rel.startswith("Export/"):
            # The facial export is the same body and skeleton plus face shapes.
            get(p, os.path.join(ROOT, "avatars", name, name + "_facial.fbx"))
        elif rel.endswith(".fbx") and "facial" not in rel and rel.startswith("Export/"):
            get(p, os.path.join(ROOT, "avatars", name, os.path.basename(rel)))
        elif rel.startswith("Textures/") and rel.lower().endswith(".tga") and "wrinkle" not in rel:
            jobs.append((p, os.path.join(ROOT, "avatars", name, "tex", os.path.basename(rel))))
with ThreadPoolExecutor(8) as ex:
    list(ex.map(lambda j: tex_job(*j), jobs))
print("textures", len(jobs), flush=True)
for n in AN:
    p = "Assets/Animations/all_animations_max_motextr_static/%s.max.fbx" % n
    if p in TREE: get(p, os.path.join(ROOT, "anims", n + ".fbx"))
    else: print("missing anim", n)
for n in WALK:
    p = "Assets/Animations/all_animations_max_motextr_xy/%s.max.fbx" % n
    if p in TREE: get(p, os.path.join(ROOT, "anims", n + ".fbx"))
    else: print("missing anim", n)
print("done")
