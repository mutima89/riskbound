"""Builds the game's audio sources into Data/Assets/generated/audio/<Group>/<Name>.<ext>, ready for
Scripts/ImportAssets.py (which imports them to /Game/Riskbound/Audio/<Group>/<Name>).

Three kinds of source, all licensed for redistribution in a game (Docs/AssetCredits.md "Audio"):
  - downloaded: Kenney packs (CC0) and Kevin MacLeod music (CC BY 4.0), listed in Data/AudioManifest.json
    with SHA-256; files are verified before use.
  - extracted: single files taken from those downloads (renamed to the game's names).
  - synthesised here (original work for Riskbound): ambience beds and event stingers, made with numpy/scipy
    from noise, additive and FM synthesis and a convolution reverb. Deterministic (fixed seeds).

  python Scripts/BuildAudio.py            download, verify, extract, synthesise
  python Scripts/BuildAudio.py --resolve  (re)record SHA-256 and sizes in the manifest after a download
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import urllib.request
import wave
import zipfile

import numpy as np
from scipy import signal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "Data", "AudioManifest.json")
RAW = os.path.join(ROOT, "Data", "Assets", "raw", "audio")
OUT = os.path.join(ROOT, "Data", "Assets", "generated", "audio")
SR = 48000
UA = {"User-Agent": "Riskbound-asset-fetch/1.0"}

# Game name -> (download id, member inside the zip or None for the file itself)
EXTRACT = {
    "UI/UI_Click": ("kenney_interface_sounds", "Audio/click_002.ogg"),
    "UI/UI_Confirm": ("kenney_interface_sounds", "Audio/confirmation_002.ogg"),
    "UI/UI_Back": ("kenney_interface_sounds", "Audio/back_002.ogg"),
    "UI/UI_Notice": ("kenney_interface_sounds", "Audio/question_001.ogg"),
    "Foley/FS_Step_01": ("kenney_impact_sounds", "Audio/footstep_carpet_000.ogg"),
    "Foley/FS_Step_02": ("kenney_impact_sounds", "Audio/footstep_carpet_001.ogg"),
    "Foley/FS_Step_03": ("kenney_impact_sounds", "Audio/footstep_carpet_002.ogg"),
    "Foley/FS_Step_04": ("kenney_impact_sounds", "Audio/footstep_carpet_003.ogg"),
    "Foley/FS_Step_05": ("kenney_impact_sounds", "Audio/footstep_carpet_004.ogg"),
    "Music/MUS_Menu": ("macleod_clean_soul", None),
    "Music/MUS_Focus": ("macleod_thinking_music", None),
    "Music/MUS_Tension": ("macleod_lightless_dawn", None),
    "Music/MUS_Crisis": ("macleod_hitman", None),
}


def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r:
        return r.read()


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def download(manifest, resolve):
    for a in manifest["downloads"]:
        path = os.path.join(RAW, a["file"])
        data = open(path, "rb").read() if os.path.exists(path) else None
        if data is None or (not resolve and sha256(data) != a.get("sha256")):
            print("downloading", a["url"])
            data = fetch(a["url"])
            os.makedirs(RAW, exist_ok=True)
            with open(path, "wb") as f:
                f.write(data)
        if resolve:
            a["sha256"], a["size"] = sha256(data), len(data)
        elif sha256(data) != a["sha256"]:
            sys.exit(f"SHA-256 mismatch for {a['file']}")
    if resolve:
        with open(MANIFEST, "w", encoding="utf-8", newline="\n") as f:
            json.dump(manifest, f, indent=1)
            f.write("\n")


def extract(manifest):
    by_id = {a["id"]: a for a in manifest["downloads"]}
    for name, (src, member) in EXTRACT.items():
        a = by_id[src]
        path = os.path.join(RAW, a["file"])
        ext = os.path.splitext(member or a["file"])[1]
        dest = os.path.join(OUT, name + ext)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        if member:
            with zipfile.ZipFile(path) as z:
                with z.open(member) as s, open(dest, "wb") as d:
                    shutil.copyfileobj(s, d)
        else:
            shutil.copyfile(path, dest)


# ---------------------------------------------------------------- synthesis helpers
def t(seconds):
    return np.arange(int(seconds * SR)) / SR


def norm(x, peak_db=-3.0):
    peak = np.max(np.abs(x)) or 1.0
    return x / peak * 10 ** (peak_db / 20)


def lowpass(x, hz, order=4):
    return signal.sosfilt(signal.butter(order, hz, "low", fs=SR, output="sos"), x, axis=0)


def highpass(x, hz, order=2):
    return signal.sosfilt(signal.butter(order, hz, "high", fs=SR, output="sos"), x, axis=0)


def bandpass(x, lo, hi, order=3):
    return signal.sosfilt(signal.butter(order, [lo, hi], "band", fs=SR, output="sos"), x, axis=0)


def pink(n, rng):
    # Voss-McCartney approximation via 1/f spectral shaping.
    spectrum = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    f[0] = f[1]
    return np.fft.irfft(spectrum / np.sqrt(f), n)


def reverb(x, seconds=2.4, wet=0.35, seed=7, damp_hz=6000):
    """Stereo convolution reverb with a decorrelated, damped, exponentially decaying noise impulse."""
    rng = np.random.default_rng(seed)
    n = int(seconds * SR)
    env = np.exp(-6.9 * np.arange(n) / n)  # -60 dB over the length
    ir = np.stack([lowpass(rng.standard_normal(n), damp_hz) * env for _ in range(2)], axis=1)
    ir[: int(0.012 * SR)] = 0  # pre-delay
    ir /= np.sqrt(np.sum(ir ** 2, axis=0))
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    out = np.zeros((len(x) + n - 1, 2))
    for c in range(2):
        out[:, c] = signal.fftconvolve(x[:, c], ir[:, c])
    dry = np.zeros_like(out)
    dry[: len(x)] = x
    return dry * (1 - wet) + out * wet


def env_adsr(n, a=0.005, d=0.1, s=0.6, r=0.4):
    e = np.ones(n) * s
    na, nd, nr = int(a * SR), int(d * SR), int(r * SR)
    e[:na] = np.linspace(0, 1, na)
    e[na:na + nd] = np.linspace(1, s, nd)
    e[-nr:] *= np.linspace(1, 0, nr)
    return e


def bell(freq, seconds, ratio=3.5, index=2.2, decay=1.6):
    tt = t(seconds)
    mod_env = np.exp(-tt * decay * 2.2)
    car_env = np.exp(-tt * decay)
    attack = np.minimum(1, tt / 0.004)
    return np.sin(2 * np.pi * freq * tt + index * mod_env * np.sin(2 * np.pi * freq * ratio * tt)) * car_env * attack


def mallet(freq, seconds, decay=4.5):
    tt = t(seconds)
    e = np.exp(-tt * decay) * np.minimum(1, tt / 0.003)
    return (np.sin(2 * np.pi * freq * tt) + 0.35 * np.sin(2 * np.pi * freq * 4 * tt) * np.exp(-tt * decay * 3)) * e


def place(buf, x, at):
    i = int(at * SR)
    buf[i:i + len(x)] += x[: max(0, len(buf) - i)]


def seamless(x, fade=2.0):
    """Loop-ready: the last `fade` seconds are crossfaded into the start."""
    n = int(fade * SR)
    head, body, tail = x[:n], x[n:-n] if x.ndim == 1 else x[n:-n], x[-n:]
    w = np.linspace(0, 1, n)
    if x.ndim > 1:
        w = w[:, None]
    blended = tail * (1 - w) + head * w
    return np.concatenate([body, blended])


def write_wav(name, x):
    path = os.path.join(OUT, name + ".wav")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if x.ndim == 1:
        x = np.stack([x, x], axis=1)
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


# ---------------------------------------------------------------- the sounds
def room_tone():
    rng = np.random.default_rng(1)
    n = int(64 * SR)
    l, r = pink(n, rng), pink(n, rng)
    x = np.stack([lowpass(l, 900), lowpass(r, 900)], axis=1)
    hum = 0.04 * np.sin(2 * np.pi * 50 * t(64)) + 0.02 * np.sin(2 * np.pi * 100 * t(64))
    x += hum[:, None] * np.std(x)
    return norm(seamless(x, 4.0), -12)


def hvac():
    rng = np.random.default_rng(2)
    n = int(40 * SR)
    base = np.cumsum(rng.standard_normal((n, 2)), axis=0)
    base = highpass(base, 30)
    x = bandpass(base, 60, 420)
    swell = 1 + 0.15 * np.sin(2 * np.pi * 0.07 * t(40))
    return norm(seamless(x * swell[:, None], 4.0), -10)


def servers():
    rng = np.random.default_rng(3)
    n = int(40 * SR)
    fans = bandpass(rng.standard_normal((n, 2)), 250, 2600)
    whine = 0.08 * np.sin(2 * np.pi * 1180 * t(40) + 0.4 * np.sin(2 * np.pi * 0.3 * t(40)))
    hum = 0.3 * np.sin(2 * np.pi * 120 * t(40))
    x = fans + (whine + hum)[:, None] * np.std(fans)
    return norm(seamless(x, 4.0), -9)


def office_bed():
    """Distant keyboards, a mouse, paper: sparse, soft, roomy. 60 s loop."""
    rng = np.random.default_rng(4)
    seconds = 64
    buf = np.zeros(int(seconds * SR))
    clock = 0.0
    while clock < seconds - 1:
        burst = rng.integers(4, 18)  # a typing run
        for _ in range(burst):
            click = rng.standard_normal(int(0.012 * SR)) * np.exp(-np.arange(int(0.012 * SR)) / (0.002 * SR))
            click = bandpass(click, 1500, 6000) * rng.uniform(0.3, 1.0)
            place(buf, click, clock)
            clock += rng.uniform(0.07, 0.19)
        clock += rng.exponential(2.5)
    for at in rng.uniform(0, seconds - 1, 6):  # paper
        rustle = bandpass(rng.standard_normal(int(0.5 * SR)), 2000, 8000) * signal.windows.hann(int(0.5 * SR)) * 0.25
        place(buf, rustle, at)
    x = reverb(lowpass(buf, 5000), 1.6, 0.6, seed=11, damp_hz=3500)[: int(seconds * SR)]
    return norm(seamless(x, 4.0), -14)


def message_ping():
    x = np.zeros(int(1.4 * SR))
    place(x, 0.6 * bell(1318.5, 1.0, ratio=2.0, index=0.8, decay=4.0), 0.0)
    place(x, 0.5 * bell(1975.5, 1.0, ratio=2.0, index=0.6, decay=4.0), 0.09)
    return norm(reverb(x, 1.2, 0.25), -4)


def promotion():
    notes = [261.63, 329.63, 392.0, 493.88, 587.33]  # Cmaj9, arpeggiated
    x = np.zeros(int(5.0 * SR))
    for i, f in enumerate(notes):
        place(x, 0.5 * bell(f, 3.8, decay=0.9), i * 0.11)
        place(x, 0.2 * bell(f * 2, 2.5, ratio=2.0, index=0.8, decay=1.4), i * 0.11 + 0.02)
    pad = sum(np.sin(2 * np.pi * f * t(5.0)) for f in (130.81, 196.0, 261.63)) * env_adsr(int(5 * SR), 0.6, 0.5, 0.6, 2.5) * 0.12
    return norm(reverb(x + lowpass(pad, 1200), 3.0, 0.4), -2)


def passed():
    x = np.zeros(int(2.8 * SR))
    place(x, mallet(392.0, 2.0), 0.0)
    place(x, mallet(587.33, 2.2), 0.14)
    place(x, 0.5 * mallet(783.99, 2.2), 0.28)
    return norm(reverb(x, 2.0, 0.35), -4)


def failed():
    x = np.zeros(int(3.0 * SR))
    place(x, lowpass(mallet(293.66, 2.0, decay=3.0), 1800), 0.0)
    place(x, lowpass(mallet(246.94, 2.4, decay=2.5), 1600), 0.22)
    return norm(reverb(x, 2.2, 0.4), -6)


def incident():
    tt = t(2.2)
    tone = np.where((tt * 2.4) % 1 < 0.5, 880.0, 698.46)
    phase = 2 * np.pi * np.cumsum(tone) / SR
    alert = np.sin(phase) * (0.6 + 0.4 * np.sin(2 * np.pi * 9 * tt)) * env_adsr(len(tt), 0.01, 0.2, 0.8, 0.7) * 0.45
    sub = np.sin(2 * np.pi * 55 * tt) * np.exp(-tt * 3) * 0.9
    return norm(reverb(lowpass(alert, 3000) + sub, 1.8, 0.3), -3)


def burnout():
    x = np.zeros(int(4.5 * SR))
    beat = lambda: lowpass(np.sin(2 * np.pi * 48 * t(0.25)) * np.exp(-t(0.25) * 18), 200)
    for at in (0.0, 0.28, 1.1, 1.38, 2.3, 2.6):
        place(x, beat(), at)
    ring = 0.03 * np.sin(2 * np.pi * 4200 * t(4.5)) * env_adsr(int(4.5 * SR), 1.0, 0.5, 0.8, 1.5)
    return norm(reverb(x + ring, 2.5, 0.3), -4)


SYNTH = {
    "Ambience/AMB_RoomTone": room_tone,
    "Ambience/AMB_Hvac": hvac,
    "Ambience/AMB_Servers": servers,
    "Ambience/AMB_Office": office_bed,
    "UI/UI_Message": message_ping,
    "Stingers/STG_Promotion": promotion,
    "Stingers/STG_Pass": passed,
    "Stingers/STG_Fail": failed,
    "Stingers/STG_Incident": incident,
    "Stingers/STG_Burnout": burnout,
}

# Looping sounds (ImportAssets.py sets looping on these).
LOOPING = ["Ambience/", "Music/"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--resolve", action="store_true")
    args = ap.parse_args()
    manifest = json.load(open(MANIFEST, encoding="utf-8"))
    download(manifest, args.resolve)
    extract(manifest)
    for name, fn in SYNTH.items():
        write_wav(name, fn())
        print("synthesised", name)
    print("audio sources in", OUT)


if __name__ == "__main__":
    main()
