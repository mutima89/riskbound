"""Editor script: creates Riskbound's cast as MetaHumans with the in-editor MetaHuman Creator
(MetaHumanCharacter plugin, UE 5.8) and assembles them for the game.

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
import random

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(ROOT, "Saved", "RbMetaHumans.log")
CHAR_PATH = "/Game/Riskbound/Characters/MetaHumans"
BUILD_PATH = "/Game/Riskbound/Characters/Built"

# id, body ("female"/"male"), skin tone (u, v on the skin-tone map), face variation seed, roughness
CAST = [
    ("priya", "female", (0.62, 0.62), 11, 0.80),
    ("celeste", "female", (0.18, 0.30), 22, 0.75),
    ("hollis", "male", (0.25, 0.40), 33, 0.82),
    ("amara", "female", (0.10, 0.22), 44, 0.78),
    ("grace", "female", (0.85, 0.80), 55, 0.80),
    ("callum", "male", (0.45, 0.50), 66, 0.80),
    ("tomasz", "male", (0.20, 0.32), 77, 0.84),
    ("ruth", "female", (0.78, 0.74), 88, 0.80),
    ("mei", "female", (0.35, 0.45), 99, 0.76),
    ("helen", "female", (0.70, 0.70), 111, 0.86),
]


def log(msg):
    unreal.log("RbMetaHumans: " + msg)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def edit(subsystem, character):
    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError("cannot edit " + character.get_name())


def make(subsystem, cid, body, tone, seed, roughness):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = CHAR_PATH + "/" + cid
    character = unreal.load_asset(path + "." + cid) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if character is None:
        character = tools.create_asset(asset_name=cid, package_path=CHAR_PATH, asset_class=unreal.MetaHumanCharacter,
                                       factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
    edit(subsystem, character)
    try:
        # Skin tone and texture.
        skin = unreal.MetaHumanCharacterSkinProperties()
        skin.u, skin.v = tone
        skin.roughness = roughness
        skin.face_texture_index = seed % 8
        skin.body_texture_index = seed % 6
        settings = unreal.MetaHumanCharacterSkinSettings()
        settings.skin = skin
        settings.enable_texture_overrides = False
        subsystem.commit_skin_settings(character, settings)
        # A distinct face: small, seeded landmark offsets around the template.
        rng = random.Random(seed)
        landmarks = subsystem.get_face_landmarks(character=character)
        deltas = [unreal.Vector(rng.uniform(-0.35, 0.35), rng.uniform(-0.35, 0.35), rng.uniform(-0.35, 0.35)) for _ in landmarks]
        subsystem.translate_face_landmarks(character=character, landmark_indices=list(range(len(deltas))), deltas=deltas)
        subsystem.commit_face_state(character=character)
        log(cid + ": sculpted")
        # Cloud steps (Epic account): high-resolution textures and the face rig.
        tex = unreal.MetaHumanCharacterTextureRequestParams()
        tex.blocking = True
        tex.report_progress = False
        subsystem.request_texture_sources(character, tex)
        log(cid + ": textures")
        rig = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        rig.blocking = True
        rig.report_progress = False
        rig.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        subsystem.request_auto_rigging(character, rig)
        log(cid + ": rigged")
        build = unreal.MetaHumanCharacterEditorBuildParameters()
        build.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        build.absolute_build_path = BUILD_PATH + "/" + cid
        build.common_folder_path = BUILD_PATH + "/Common"
        build.enable_wardrobe_item_validation = False
        if not subsystem.can_build_meta_human(character, True):
            raise RuntimeError("cannot build (see log)")
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
    for cid, body, tone, seed, roughness in CAST:
        if only and cid not in only:
            continue
        try:
            make(subsystem, cid, body, tone, seed, roughness)
        except Exception as e:
            log(cid + ": FAILED " + str(e))
    log("done")


main()
