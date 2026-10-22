"""Editor import step for audio: Data/Assets/generated/audio/<Group>/<Name>.<wav|ogg|mp3> (built by
Scripts/BuildAudio.py) -> /Game/Riskbound/Audio/<Group>/<Name> sound waves. Idempotent.
Ambience and music loop; everything else is one-shot. Run through Scripts/ImportAssets.bat (after the art
import) or on its own with -script=Scripts/ImportAudio.py. RB_AUDIO_GROUPS=Voice (comma-separated) limits the
import to those groups. Voice lines (Scripts/BuildVoices.py) whose source is gone are deleted, so an edited
line never keeps an old recording.
"""
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Data", "Assets", "generated", "audio")
DEST = "/Game/Riskbound/Audio"
LOOPING = ("Ambience", "Music")
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
ONLY = [g for g in os.environ.get("RB_AUDIO_GROUPS", "").split(",") if g]
count = errors = 0
for group in sorted(os.listdir(SRC)) if os.path.isdir(SRC) else []:
    if ONLY and group not in ONLY:
        continue
    for fname in sorted(os.listdir(os.path.join(SRC, group))):
        name, ext = os.path.splitext(fname)
        if ext.lower() not in (".wav", ".ogg", ".mp3"):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SRC, group, fname))
        task.set_editor_property("destination_path", DEST + "/" + group)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        TOOLS.import_asset_tasks([task])
        paths = list(task.get_editor_property("imported_object_paths") or [])
        wave = unreal.load_asset(paths[0]) if paths else None
        if not isinstance(wave, unreal.SoundWave):
            unreal.log_error("RbAudio: failed to import %s/%s" % (group, fname))
            errors += 1
            continue
        wave.set_editor_property("looping", group in LOOPING)
        EAL.save_asset(wave.get_path_name(), only_if_is_dirty=False)
        count += 1
removed = 0
VOICE_SRC = os.path.join(SRC, "Voice")
if os.path.isdir(VOICE_SRC) and (not ONLY or "Voice" in ONLY):
    keep = {os.path.splitext(f)[0] for f in os.listdir(VOICE_SRC) if f.lower().endswith(".wav")}
    for path in EAL.list_assets(DEST + "/Voice", recursive=False, include_folder=False):
        name = path.split("/")[-1].split(".")[0]
        if name.startswith("VO_") and name not in keep:
            EAL.delete_asset(path.split(".")[0])
            removed += 1
unreal.log("RbAudio: imported %d sounds, %d errors, %d old voice lines removed" % (count, errors, removed))
