"""Editor import step for audio: Data/Assets/generated/audio/<Group>/<Name>.<wav|ogg|mp3> (built by
Scripts/BuildAudio.py) -> /Game/Riskbound/Audio/<Group>/<Name> sound waves. Idempotent.
Ambience and music loop; everything else is one-shot. Run through Scripts/ImportAssets.bat (after the art
import) or on its own with -script=Scripts/ImportAudio.py.
"""
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Data", "Assets", "generated", "audio")
DEST = "/Game/Riskbound/Audio"
LOOPING = ("Ambience", "Music")
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
count = errors = 0
for group in sorted(os.listdir(SRC)) if os.path.isdir(SRC) else []:
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
unreal.log("RbAudio: imported %d sounds, %d errors" % (count, errors))
