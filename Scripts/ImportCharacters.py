"""Editor script: imports the office cast (Microsoft Rocketbox avatars, MIT licence) into the project.

Source: https://github.com/microsoft/Microsoft-Rocketbox (fetched by Scripts/FetchCharacters.py into
<root>/../RiskboundAssets/rocketbox, textures reduced to 1024 px PNG). Run inside the editor:
  UnrealEditor.exe Riskbound.uproject -ExecutePythonScript="<path>/Scripts/ImportCharacters.py"

Output under /Game/Riskbound/Characters:
  Materials/M_RbSkin, M_RbMasked                 two master materials (opaque skin/cloth, masked hair/lashes)
  People/<Avatar>/SK_<Avatar> and one material instance per slot (re-parented to the masters)
  Skeletons/SK_RbMale_Skeleton, SK_RbFemale_Skeleton (every avatar of a sex shares one skeleton)
  Anims/A_<name>                                  in-place idles and listening loops
Progress: Saved/RbImportCharacters.log
"""
import glob
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(os.path.dirname(ROOT), "RiskboundAssets", "rocketbox")
LOG = os.path.join(ROOT, "Saved", "RbImportCharacters.log")
BASE = "/Game/Riskbound/Characters"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log("RbImportCharacters: " + m)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(m + "\n")


def master(name, masked):
    path = BASE + "/Materials/" + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        if not m.get_editor_property("used_with_skeletal_mesh"):
            m.set_editor_property("used_with_skeletal_mesh", True)
            mel.recompile_material(m)
            eal.save_loaded_asset(m)
        return m
    m = tools.create_asset(name, BASE + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    # Without this flag a packaged game cannot compile the shaders and falls back to the grey default material.
    m.set_editor_property("used_with_skeletal_mesh", True)
    if masked:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property("two_sided", True)
    col = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, -200)
    col.set_editor_property("parameter_name", "BaseColor")
    col.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    mel.connect_material_property(col, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if masked:
        mel.connect_material_property(col, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    nrm = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 100)
    nrm.set_editor_property("parameter_name", "Normal")
    nrm.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    spec = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, 350)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    # Rocketbox specular maps: bright = glossy. Roughness = lerp(0.85, 0.35, spec).
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -300, 350)
    lerp.set_editor_property("const_a", 0.85)
    lerp.set_editor_property("const_b", 0.35)
    mel.connect_material_expressions(spec, "R", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def import_texture(png, dest, normal):
    name = "T_" + os.path.splitext(os.path.basename(png))[0]
    path = dest + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    t = unreal.AssetImportTask()
    t.filename, t.destination_path, t.destination_name = png, dest, name
    t.automated, t.save, t.replace_existing = True, True, True
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(path)
    if tex and normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)  # 3ds Max normals
        eal.save_loaded_asset(tex)
    return tex


def fbx_task(fbx, dest, name, skeleton, anim):
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", not anim)
    o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("import_animations", anim)
    o.set_editor_property("import_materials", not anim)
    o.set_editor_property("import_textures", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION if anim else unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skeleton:
        o.set_editor_property("skeleton", skeleton)
    o.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    o.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    if anim:
        o.anim_sequence_import_data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        o.anim_sequence_import_data.set_editor_property("remove_redundant_keys", True)
    t = unreal.AssetImportTask()
    t.filename, t.destination_path, t.destination_name = fbx, dest, name
    t.automated, t.save, t.replace_existing, t.options = True, True, True, o
    tools.import_asset_tasks([t])
    return [unreal.load_asset(p) for p in t.imported_object_paths]


def part_textures(texdir, slot):
    """Picks colour/normal/specular PNGs whose name shares the slot's part (head, body, opacity, ...)."""
    slot = slot.lower()
    parts = [p for p in ("head", "body", "opacity", "hair", "eye", "teeth") if p in slot]
    out = {}
    for png in glob.glob(os.path.join(texdir, "*.png")):
        n = os.path.basename(png).lower()
        if parts and not any(p in n for p in parts):
            continue
        if "normal" in n:
            out.setdefault("Normal", png)
        elif "spec" in n:
            out.setdefault("Specular", png)
        elif "color" in n or "diffuse" in n:
            out.setdefault("BaseColor", png)
    return out


def main():
    skin, masked = master("M_RbSkin", False), master("M_RbMasked", True)
    skeletons = {}
    for folder in sorted(glob.glob(os.path.join(SRC, "avatars", "*"))):
        avatar = os.path.basename(folder)
        fbx = os.path.join(folder, avatar + ".fbx")
        if not os.path.exists(fbx):
            log(avatar + ": no fbx")
            continue
        sex = "Female" if "Female" in avatar else "Male"
        dest = BASE + "/People/" + avatar
        mesh_path = dest + "/SK_" + avatar
        if eal.does_asset_exist(mesh_path):
            mesh = unreal.load_asset(mesh_path)
        else:
            objs = fbx_task(fbx, dest, "SK_" + avatar, skeletons.get(sex), False)
            mesh = next((o for o in objs if isinstance(o, unreal.SkeletalMesh)), None)
        if not mesh:
            log(avatar + ": import FAILED")
            continue
        if sex not in skeletons:
            skeletons[sex] = mesh.get_editor_property("skeleton")
        texdir = os.path.join(folder, "tex")
        mats = mesh.get_editor_property("materials")
        for i, sm in enumerate(mats):
            slot = str(sm.get_editor_property("material_slot_name"))
            tex = part_textures(texdir, slot)
            is_masked = "opacity" in slot.lower() or "hair" in slot.lower() or "lash" in slot.lower()
            # The FBX import made one material instance per slot; point it at our master and give it the textures.
            mi = sm.get_editor_property("material_interface")
            if not isinstance(mi, unreal.MaterialInstanceConstant):
                log("%s: slot %s has no material instance" % (avatar, slot))
                continue
            mel.set_material_instance_parent(mi, masked if is_masked else skin)
            for param, png in tex.items():
                t = import_texture(png, dest + "/Textures", param == "Normal")
                if t:
                    mel.set_material_instance_texture_parameter_value(mi, param, t)
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi)
            stale = dest + "/MI_" + avatar + "_" + slot  # left by an earlier version of this script
            if eal.does_asset_exist(stale):
                eal.delete_asset(stale)
        eal.save_directory(dest)
        log("%s: %d materials, bounds %s" % (avatar, len(mats), mesh.get_bounds().box_extent))
    for fbx in sorted(glob.glob(os.path.join(SRC, "anims", "*.fbx"))):
        n = os.path.splitext(os.path.basename(fbx))[0]
        sex = "Female" if n.startswith("f_") else "Male"
        if sex not in skeletons:
            continue
        if eal.does_asset_exist(BASE + "/Anims/A_" + n):
            continue
        objs = fbx_task(fbx, BASE + "/Anims", "A_" + n, skeletons[sex], True)
        log("anim %s: %s" % (n, [o.get_name() for o in objs if o]))
    for sex, sk in skeletons.items():
        log("skeleton %s: %s" % (sex, sk.get_path_name()))
    eal.save_directory(BASE)
    log("done")


try:
    main()
except Exception as e:
    log("FAILED " + str(e))
unreal.SystemLibrary.quit_editor()
