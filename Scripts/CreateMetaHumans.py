"""Editor script: creates Riskbound's cast as MetaHumans from Epic's presets with the in-editor
MetaHuman Creator (MetaHumanCharacter plugin and its optional content, UE 5.8) and assembles them.

Run inside the editor (cloud texture synthesis and auto-rigging need the Epic account signed in
to the editor): Tools > Execute Python Script, or
  UnrealEditor.exe Riskbound.uproject -ExecutePythonScript="<path>/Scripts/CreateMetaHumans.py"
Optional: set RB_MH_ONLY=priya,celeste to build a subset.

Output: /Game/Riskbound/Characters/MetaHumans/<Id> (character assets) and
        /Game/Riskbound/Characters/Built/<Id>      (assembled, optimized, medium quality)
Progress: Saved/RbMetaHumans.log. Licence: MetaHuman assets are for use in Unreal Engine projects
(MetaHuman licence terms; recorded in Docs/AssetCredits.md).
"""
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(ROOT, "Saved", "RbMetaHumans.log")
CHAR_PATH = "/Game/Riskbound/Characters/MetaHumans"
BUILD_PATH = "/Game/Riskbound/Characters/Built"

PRESETS = "/MetaHumanCharacter/Optional/Presets"
# Cast id -> Epic MetaHuman preset (MetaHumanCharacter optional content). The personal contact (Jordan) is never shown.
CAST = [
    ("priya", "Asha"), ("callum", "Cameron"), ("celeste", "Vivian"), ("adrian", "Victor"),
    ("hollis", "Walter"), ("rafe", "Lorenzo"), ("ruth", "Zuri"), ("tomasz", "Mikel"),
    ("grace", "Etta"), ("amara", "Jelena"), ("mei", "Aoi"), ("helen", "Ada"),
    ("imogen", "Aera"), ("jonah", "Dominic"),
]


def log(msg):
    unreal.log("RbMetaHumans: " + msg)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def edit(subsystem, character):
    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError("cannot edit " + character.get_name())


def make(subsystem, cid, preset):
    path = CHAR_PATH + "/" + cid
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(PRESETS + "/" + preset, path):
            raise RuntimeError("cannot copy preset " + preset)
    character = unreal.load_asset(path)
    edit(subsystem, character)
    try:
        # Cloud steps (the editor must be signed in to an Epic account): face rig, then high-resolution textures.
        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking = True
        rig.report_progress = False
        rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        subsystem.request_auto_rigging(character, rig)
        log(cid + ": rigged")
        tex = unreal.MetaHumanCharacterTextureRequestParams()
        tex.blocking = True
        tex.report_progress = False
        subsystem.request_texture_sources(character, tex)
        log(cid + ": textures")
        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        build.absolute_build_path = BUILD_PATH + "/" + cid
        build.common_folder_path = BUILD_PATH + "/Common"
        build.enable_wardrobe_item_validation = False
        if not subsystem.can_build_meta_human(character, True):
            raise RuntimeError("cannot build (see the editor log; is the editor signed in?)")
        subsystem.build_meta_human(character=character, params=build)
        log(cid + ": built")
    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)
    unreal.EditorAssetLibrary.save_directory(CHAR_PATH)
    unreal.EditorAssetLibrary.save_directory(BUILD_PATH)


def main():
    only = [x for x in os.environ.get("RB_MH_ONLY", "").split(",") if x]
    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    log("start")
    for cid, preset in CAST:
        if only and cid not in only:
            continue
        try:
            make(subsystem, cid, preset)
        except Exception as e:
            log(cid + ": FAILED " + str(e))
    log("done")


main()
