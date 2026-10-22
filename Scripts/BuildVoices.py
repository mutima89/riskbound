"""Builds the colleagues' recorded voice lines into Data/Assets/generated/audio/Voice/VO_<key>.wav, ready for
Scripts/ImportAudio.py (which imports them to /Game/Riskbound/Audio/Voice/VO_<key>; RB_AUDIO_GROUPS=Voice
imports only these).

Every line a colleague says aloud is collected from the content (Data/Dialogue.json conversations,
Data/Missions/*.json interviews, Data/Events.json messages delivered in person) and from the fixed lines
in Data/VoiceCast.json, then spoken offline by Kokoro-82M (Apache 2.0) through kokoro-onnx (MIT), with one
voice per character as cast in Data/VoiceCast.json. Nothing runs at game time: the game ships the clips.

<key> is rb::VoiceLineKey (Source/RiskboundSim/Public/RbVoice.h): speaker id plus a hash of the exact
line, so an edited line has no clip until this is run again (the game then falls back to text-timed
lip-sync). Unchanged lines are skipped, so a rebuild only speaks what changed.

  pip install kokoro-onnx soundfile
  python Scripts/BuildVoices.py            download the model (verified), speak new lines
  python Scripts/BuildVoices.py --list     print the lines and keys only
"""
import argparse
import glob
import hashlib
import json
import os
import re
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "Data")
OUT = os.path.join(DATA, "Assets", "generated", "audio", "Voice")
MODEL_DIR = os.path.join(DATA, "Assets", "raw", "voice")
MODEL_URL = "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/"
MODEL_FILES = {
    "kokoro-v1.0.onnx": "7d5df8ecf7d4b1878015a32686053fd0eebe2bc377234608764cc0ef3636a6c5",
    "voices-v1.0.bin": "bca610b8308e8d99f32e6fe4197e7ec01679264efed0cac9140fe9c29f1fbf7d",
}
SPACE = re.compile(r"[ \t\n\r]+")
# Spoken as words, not letter by letter.
WORD_ACRONYMS = {"OK", "NATO", "COVID"}
SAY = {"EBITDA": "ee bit dah", "RCSA": "R C S A", "KRIs": "K R Is", "CRO": "C R O"}


# ---------------------------------------------------------------- key (mirrors RbVoice.cpp)

def normalize(text):
    return SPACE.sub(" ", text).strip(" \t\n\r")


def line_hash(text):
    h = 2166136261
    for b in normalize(text).encode("utf-8"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def line_key(person, text):
    if not person or not normalize(text):
        return ""
    return "%s_%08x" % (person, line_hash(text))


def check_key():
    # The same vectors as Tests/Core/TestVoice.cpp: if these fail the game would not find the clips.
    assert line_hash("") == 0x811C9DC5 and line_hash("a") == 0xE40C292C and line_hash("foobar") == 0xBF9CF968
    assert line_key("priya", "  Two cases in.\n\nHow are you?  ") == "priya_4001ff99"
    assert line_key("priya", "It’s fine.") == "priya_175ecf54"


# ---------------------------------------------------------------- lines

def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def paragraphs(value):
    # A content text is a string or a list of paragraphs (RbJsonReader.h Text()).
    return "\n\n".join(value) if isinstance(value, list) else value


def approach_line(prefix, subject):
    # Mirrors ARbHUD::TickApproach.
    return prefix + subject + ("" if subject[-1:] in ".?!" else ".")


def collect(cast):
    lines = {}  # key -> (person, text, source)

    def add(person, text, source):
        key = line_key(person, text)
        if key and person in cast["cast"]:
            lines.setdefault(key, (person, normalize(text), source))

    for c in load(os.path.join(DATA, "Dialogue.json"))["conversations"]:
        for n in c["nodes"]:
            add(c["person"], paragraphs(n["line"]), "Dialogue %s/%s" % (c["id"], n["id"]))
    for path in sorted(glob.glob(os.path.join(DATA, "Missions", "*.json"))):
        m = load(path)
        for e in m.get("evidence", []):
            if e.get("person"):
                add(e["person"], paragraphs(e["body"]), "%s interview %s" % (m.get("id", os.path.basename(path)), e["id"]))
    a = cast["approach"]
    senders = set()
    for e in load(os.path.join(DATA, "Events.json"))["events"]:
        if e.get("category") in a["categories"] and e.get("from") and e["from"] not in a["excludeFrom"]:
            add(e["from"], approach_line(a["prefix"], e["subject"]), "Event %s (walks over)" % e["id"])
            senders.add(e["from"])
    for person in sorted(senders):
        add(person, a["busy"], "Walks over while the player is busy")
    for f in cast["fixed"]:
        add(f["person"], f["text"], f["where"])
    return lines


def spoken_form(text, pronounce):
    """What the TTS reads: acronyms spelled out, a few fixed pronunciations. The key never uses this."""
    s = text
    for pattern, repl in pronounce.items():
        if pattern != "comment":
            s = re.sub(pattern, repl, s)
    for word, said in SAY.items():
        s = re.sub(r"\b%s\b" % re.escape(word), said, s)

    def spell(m):
        w = m.group(0)
        if w in WORD_ACRONYMS:
            return w
        plural = w.endswith("s")
        letters = w[:-1] if plural else w
        return " ".join(letters) + ("s" if plural else "")

    s = re.sub(r"\b[A-Z]{2,5}s?\b", spell, s)
    s = s.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    return s


# ---------------------------------------------------------------- model

def fetch_model():
    os.makedirs(MODEL_DIR, exist_ok=True)
    for name, sha in MODEL_FILES.items():
        path = os.path.join(MODEL_DIR, name)
        if not os.path.exists(path):
            print("Downloading %s ..." % name)
            req = urllib.request.Request(MODEL_URL + name, headers={"User-Agent": "Riskbound-asset-fetch/1.0"})
            with urllib.request.urlopen(req) as r, open(path + ".part", "wb") as f:
                while True:
                    chunk = r.read(1 << 20)
                    if not chunk:
                        break
                    f.write(chunk)
            os.replace(path + ".part", path)
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        if h.hexdigest() != sha:
            sys.exit("%s: SHA-256 mismatch (%s), delete it and run again" % (name, h.hexdigest()))
    return os.path.join(MODEL_DIR, "kokoro-v1.0.onnx"), os.path.join(MODEL_DIR, "voices-v1.0.bin")


def voice_style(kokoro, voice):
    if isinstance(voice, str):
        return voice
    total = float(sum(voice.values()))
    return sum(kokoro.get_voice_style(v) * (w / total) for v, w in voice.items())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", action="store_true", help="print the lines and their keys, speak nothing")
    ap.add_argument("--force", action="store_true", help="speak every line again")
    args = ap.parse_args()
    check_key()
    cast = load(os.path.join(DATA, "VoiceCast.json"))
    lines = collect(cast)
    pronounce = cast.get("pronounce", {})
    if args.list:
        for key, (person, text, source) in sorted(lines.items()):
            print("%s\t%s\t%s" % (key, source, spoken_form(text, pronounce)))
        print("%d lines, %d characters" % (len(lines), sum(len(t) for _, t, _ in lines.values())))
        return

    import numpy as np
    import soundfile as sf
    from kokoro_onnx import Kokoro

    kokoro = Kokoro(*fetch_model())
    os.makedirs(OUT, exist_ok=True)
    wanted = {"VO_%s.wav" % k for k in lines}
    for stale in sorted(set(os.listdir(OUT)) - wanted - {"manifest.json"}):
        if stale.startswith("VO_") and stale.endswith(".wav"):
            os.remove(os.path.join(OUT, stale))
    manifest = {}
    spoken = 0
    for key, (person, text, source) in sorted(lines.items()):
        path = os.path.join(OUT, "VO_%s.wav" % key)
        if args.force or not os.path.exists(path):
            c = cast["cast"][person]
            audio, rate = kokoro.create(spoken_form(text, pronounce), voice=voice_style(kokoro, c["voice"]),
                                        speed=c.get("speed", 1.0), lang=c.get("lang", "en-gb"))
            # Trim the model's leading and trailing near-silence so lip-sync starts with the voice, then
            # leave a short, even tail; normalise peaks to -3 dBFS so every voice sits at the same level.
            audio = np.asarray(audio, dtype=np.float32)
            loud = np.flatnonzero(np.abs(audio) > 0.01)
            if loud.size:
                audio = audio[max(0, loud[0] - int(0.02 * rate)):loud[-1] + int(0.15 * rate)]
            peak = float(np.max(np.abs(audio))) if audio.size else 0.0
            if peak > 0:
                audio = audio * (0.708 / peak)
            sf.write(path, audio, rate, subtype="PCM_16")
            spoken += 1
            print("spoke %s (%s)" % (key, source))
        info = sf.info(path)
        manifest[key] = {"person": person, "seconds": round(info.frames / info.samplerate, 3), "source": source, "text": text}
    with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False, sort_keys=True)
    total = sum(v["seconds"] for v in manifest.values())
    print("RbVoice: %d lines (%d spoken now), %.1f minutes of speech in %s" % (len(manifest), spoken, total / 60.0, OUT))


if __name__ == "__main__":
    main()
