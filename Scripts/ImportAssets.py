"""Editor import step: turns Data/Assets/raw (+ edited/) into Content/Riskbound/ assets.

Run headless through Scripts/ImportAssets.bat (UnrealEditor-Cmd -run=pythonscript). Idempotent: existing
assets are replaced. Writes Saved/RbImportReport.json listing every mesh and its material slots.
Creates:
  /Game/Riskbound/Materials/M_RbWorld      world-aligned PBR (architecture; tiling never stretches)
  /Game/Riskbound/Materials/M_RbSurface    UV PBR (imported models)
  /Game/Riskbound/Materials/M_RbGlass      translucent glass
  /Game/Riskbound/Materials/M_RbEmissive   light panels and screens
  /Game/Riskbound/Materials/M_RbSky        HDRI sky sphere (lat-long, unlit)
  /Game/Riskbound/Materials/M_RbHighlight  focus overlay (fresnel)
  /Game/Riskbound/Materials/Instances/MI_* one instance per surface and per model material slot
"""
import json
import os
import struct
import zlib

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "Data", "Assets", "raw")
EDITED = os.path.join(ROOT, "Data", "Assets", "edited")
GAME = "/Game/Riskbound"
MAT = GAME + "/Materials"
MI = MAT + "/Instances"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
report = {"meshes": {}, "materials": [], "errors": []}


def log(msg):
    unreal.log("RbImport: " + msg)


def err(msg):
    unreal.log_error("RbImport: " + msg)
    report["errors"].append(msg)


def source(rel):
    edited = os.path.join(EDITED, rel)
    return edited if os.path.exists(edited) else os.path.join(RAW, rel)


# ---------------------------------------------------------------- import helpers
def import_file(path, dest, name=None, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    if name:
        task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths") or [])
    if not paths:
        err("nothing imported from " + path)
    return paths


def write_png(path, rgb):
    """4x4 solid-colour PNG without third-party modules."""
    raw = b"".join(b"\x00" + bytes(rgb) * 4 for _ in range(4))
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def setup_texture(asset_path, kind):
    tex = unreal.load_asset(asset_path)
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    EAL.save_asset(asset_path, only_if_is_dirty=False)
    return tex


def import_texture(rel, dest):
    path = source(rel)
    name = "T_" + os.path.splitext(os.path.basename(path))[0]
    kind = "normal" if "_nor_dx" in name else "mask" if name.endswith("_arm_1k") else "color"
    imported = import_file(path, dest, name)
    return setup_texture(dest + "/" + name, kind) if imported else None


# ---------------------------------------------------------------- material graph helpers
def new_material(name):
    path = MAT + "/" + name
    if EAL.does_asset_exist(path):
        # Reuse the asset (other assets may reference it) and rebuild its graph from scratch.
        m = unreal.load_asset(path)
        MEL.delete_all_material_expressions(m)
    else:
        m = TOOLS.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    # The HQ draws repeated geometry as instanced static meshes; a cooked game silently falls
    # back to the default material for any material without this usage flag.
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def expr(m, cls, x, y, **props):
    e = MEL.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        err("link failed %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def prop(e, out, p):
    if not MEL.connect_material_property(e, out, p):
        err("property link failed %s.%s" % (e.get_name(), out))


def finish(m):
    MEL.recompile_material(m)
    EAL.save_asset(m.get_path_name(), only_if_is_dirty=False)
    report["materials"].append(m.get_path_name())
    return m


def pbr_tail(m, base, normal_out, arm, x):
    """Common scalar/vector parameters and outputs: Tint, RoughnessScale, MetallicScale."""
    tint = expr(m, unreal.MaterialExpressionVectorParameter, x, -300, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = expr(m, unreal.MaterialExpressionMultiply, x + 250, -250)
    link(base[0], base[1], mul, "A")
    link(tint, "", mul, "B")
    prop(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    prop(normal_out[0], normal_out[1], unreal.MaterialProperty.MP_NORMAL)
    for i, (pname, channel, mp, default) in enumerate([("RoughnessScale", "G", unreal.MaterialProperty.MP_ROUGHNESS, 1.0),
                                                       ("MetallicScale", "B", unreal.MaterialProperty.MP_METALLIC, 1.0),
                                                       ("AOScale", "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 1.0)]):
        mask = expr(m, unreal.MaterialExpressionComponentMask, x, 50 + i * 150, r=channel == "R", g=channel == "G", b=channel == "B", a=False)
        link(arm[0], arm[1], mask, "")
        scale = expr(m, unreal.MaterialExpressionScalarParameter, x, 120 + i * 150, parameter_name=pname, default_value=default)
        out = expr(m, unreal.MaterialExpressionMultiply, x + 250, 80 + i * 150)
        link(mask, "", out, "A")
        link(scale, "", out, "B")
        prop(out, "", mp)


def build_surface(defaults):
    m = new_material("M_RbSurface")
    m.set_editor_property("used_with_skeletal_mesh", True)  # Priya's outfit
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    uvs = expr(m, unreal.MaterialExpressionScalarParameter, -1200, 120, parameter_name="UVScale", default_value=1.0)
    uvm = expr(m, unreal.MaterialExpressionMultiply, -1000, 40)
    link(uv, "", uvm, "A")
    link(uvs, "", uvm, "B")
    samples = {}
    for i, (pname, tex, sampler) in enumerate([("BaseColorMap", defaults["color"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                               ("NormalMap", defaults["normal"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                               ("ARMMap", defaults["mask"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]):
        s = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, -300 + i * 300, parameter_name=pname, texture=tex, sampler_type=sampler)
        link(uvm, "", s, "UVs")
        samples[pname] = s
    pbr_tail(m, (samples["BaseColorMap"], "RGB"), (samples["NormalMap"], "RGB"), (samples["ARMMap"], "RGB"), -400)
    return finish(m)


def build_world(defaults):
    m = new_material("M_RbWorld")
    m.set_editor_property("tangent_space_normal", False)  # world-aligned normals are in world space
    wat = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture")
    wan = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal.WorldAlignedNormal")
    size = expr(m, unreal.MaterialExpressionVectorParameter, -1400, 0, parameter_name="TileSize", default_value=unreal.LinearColor(200, 200, 200, 0))
    outs = {}
    for i, (pname, tex, sampler, fn) in enumerate([("BaseColorMap", defaults["color"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, wat),
                                                   ("NormalMap", defaults["normal"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, wan),
                                                   ("ARMMap", defaults["mask"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, wat)]):
        obj = expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, -300 + i * 300, parameter_name=pname, texture=tex, sampler_type=sampler)
        call = expr(m, unreal.MaterialExpressionMaterialFunctionCall, -900, -300 + i * 300)
        call.set_editor_property("material_function", fn)
        link(obj, "", call, "TextureObject")
        link(size, "", call, "TextureSize")
        outs[pname] = call
    pbr_tail(m, (outs["BaseColorMap"], "XYZ Texture"), (outs["NormalMap"], "XYZ Texture"), (outs["ARMMap"], "XYZ Texture"), -500)
    return finish(m)


def build_glass():
    m = new_material("M_RbGlass")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)  # cheap: large facades
    m.set_editor_property("two_sided", True)
    tint = expr(m, unreal.MaterialExpressionVectorParameter, -600, -200, parameter_name="Tint", default_value=unreal.LinearColor(0.75, 0.85, 0.88, 1))
    prop(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for i, (pname, default, mp) in enumerate([("Opacity", 0.18, unreal.MaterialProperty.MP_OPACITY),
                                              ("Roughness", 0.05, unreal.MaterialProperty.MP_ROUGHNESS),
                                              ("Specular", 1.0, unreal.MaterialProperty.MP_SPECULAR)]):
        p = expr(m, unreal.MaterialExpressionScalarParameter, -600, i * 150, parameter_name=pname, default_value=default)
        prop(p, "", mp)
    return finish(m)


def build_emissive():
    m = new_material("M_RbEmissive")
    color = expr(m, unreal.MaterialExpressionVectorParameter, -700, 0, parameter_name="Color", default_value=unreal.LinearColor(1, 0.95, 0.88, 1))
    inten = expr(m, unreal.MaterialExpressionScalarParameter, -700, 200, parameter_name="Intensity", default_value=8.0)
    mul = expr(m, unreal.MaterialExpressionMultiply, -400, 50)
    link(color, "", mul, "A")
    link(inten, "", mul, "B")
    prop(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    black = expr(m, unreal.MaterialExpressionConstant3Vector, -400, -150, constant=unreal.LinearColor(0.02, 0.02, 0.02, 1))
    prop(black, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(m, unreal.MaterialExpressionConstant, -400, 250, r=0.3)
    prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(m)


def build_sky(hdri):
    """Unlit HDRI cubemap on an inside-out sphere, sampled along the view direction; marked IsSky."""
    m = new_material("M_RbSky")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("is_sky", False)  # a plain unlit dome; IsSky needs full-screen coverage
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -1100, 0)
    neg = expr(m, unreal.MaterialExpressionMultiply, -850, 0)
    minus = expr(m, unreal.MaterialExpressionConstant, -1100, 150, r=-1.0)
    link(cam, "", neg, "A")
    link(minus, "", neg, "B")
    tex = expr(m, unreal.MaterialExpressionTextureSampleParameterCube, -500, 0, parameter_name="SkyCube", texture=hdri)
    link(neg, "", tex, "UVs")
    inten = expr(m, unreal.MaterialExpressionScalarParameter, -500, 250, parameter_name="Intensity", default_value=1.0)
    mul = expr(m, unreal.MaterialExpressionMultiply, -250, 50)
    link(tex, "RGB", mul, "A")
    link(inten, "", mul, "B")
    prop(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(m)


def build_screen(defaults):
    """Self-lit display: a texture shown as emissive light over a dark glossy base."""
    m = new_material("M_RbScreen")
    tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0, parameter_name="ScreenTexture", texture=defaults["color"],
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    inten = expr(m, unreal.MaterialExpressionScalarParameter, -700, 250, parameter_name="Intensity", default_value=12.0)
    mul = expr(m, unreal.MaterialExpressionMultiply, -400, 50)
    link(tex, "RGB", mul, "A")
    link(inten, "", mul, "B")
    prop(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    black = expr(m, unreal.MaterialExpressionConstant3Vector, -400, -150, constant=unreal.LinearColor(0.01, 0.01, 0.012, 1))
    prop(black, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(m, unreal.MaterialExpressionConstant, -400, 250, r=0.15)
    prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(m)


def build_highlight():
    m = new_material("M_RbHighlight")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # The overlay also goes on people (skinned, with face shapes); without these the game draws the default material.
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_morph_targets", True)
    fres =expr(m, unreal.MaterialExpressionFresnel, -600, 100, exponent=2.5)
    color = expr(m, unreal.MaterialExpressionVectorParameter, -600, -150, parameter_name="Color", default_value=unreal.LinearColor(0.17, 0.64, 0.66, 1))
    em = expr(m, unreal.MaterialExpressionMultiply, -300, -100)
    link(color, "", em, "A")
    link(fres, "", em, "B")
    boost = expr(m, unreal.MaterialExpressionMultiply, -100, -100)
    k = expr(m, unreal.MaterialExpressionConstant, -300, 0, r=4.0)
    link(em, "", boost, "A")
    link(k, "", boost, "B")
    prop(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    op = expr(m, unreal.MaterialExpressionMultiply, -300, 200)
    half = expr(m, unreal.MaterialExpressionConstant, -500, 300, r=0.65)
    link(fres, "", op, "A")
    link(half, "", op, "B")
    prop(op, "", unreal.MaterialProperty.MP_OPACITY)
    return finish(m)


# ---------------------------------------------------------------- instances
def instance(name, parent, textures=None, scalars=None, vectors=None):
    path = MI + "/" + name
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
        MEL.clear_all_material_instance_parameters(mi)
    else:
        mi = TOOLS.create_asset(name, MI, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    MEL.update_material_instance(mi)
    EAL.save_asset(path, only_if_is_dirty=False)
    return mi


def texture_set(asset_id, part=""):
    """Imports diff / nor_dx / arm for an asset (or one part of a model) and returns the parameter map."""
    dest = GAME + "/Textures/" + asset_id
    prefix = asset_id + ("_" + part if part else "")
    out = {}
    for key, pname in [("diff", "BaseColorMap"), ("nor_dx", "NormalMap"), ("arm", "ARMMap")]:
        rel = "polyhaven/%s/%s_%s_1k.jpg" % (asset_id, prefix, key)
        if os.path.exists(source(rel)):
            tex = import_texture(rel, dest)
            if tex:
                out[pname] = tex
    return out


def import_model(asset_id):
    dest = GAME + "/Models/" + asset_id
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_as_skeletal", False)
    sm = opts.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    paths = import_file(source("polyhaven/%s/%s_1k.fbx" % (asset_id, asset_id)), dest, "SM_" + asset_id, opts)
    meshes = [p for p in paths if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    if not meshes:
        err("no static mesh for " + asset_id)
        return
    mesh = unreal.load_asset(meshes[0])
    parts = sorted({f[len(asset_id) + 1:-len("_diff_1k.jpg")] for f in os.listdir(os.path.join(RAW, "polyhaven", asset_id))
                    if f.endswith("_diff_1k.jpg") and f != asset_id + "_diff_1k.jpg"})
    slots = []
    for i, mat in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(mat.get_editor_property("material_slot_name"))
        part = next((p for p in parts if p.lower() in slot.lower()), "")
        textures = texture_set(asset_id, part)
        mi = instance("MI_%s%s" % (asset_id, "_" + part if part else ""), SURFACE, textures)
        mesh.set_material(i, mi)
        slots.append({"slot": slot, "part": part, "material": mi.get_path_name(), "textures": len(textures)})
        if not textures:
            err("no textures matched slot %s of %s" % (slot, asset_id))
    mesh.set_editor_property("nanite_settings", unreal.MeshNaniteSettings(enabled=False))
    EAL.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
    bounds = mesh.get_bounding_box()
    report["meshes"][asset_id] = {"path": mesh.get_path_name(), "slots": slots,
                                  "size_cm": [round(bounds.max.x - bounds.min.x, 1), round(bounds.max.y - bounds.min.y, 1), round(bounds.max.z - bounds.min.z, 1)],
                                  "min_z": round(bounds.min.z, 1)}
    log("model %s: %d slots" % (asset_id, len(slots)))


# ---------------------------------------------------------------- main
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
tmp = os.path.join(ROOT, "Saved", "RbImportTmp")
os.makedirs(tmp, exist_ok=True)
defaults = {}
for kind, name, rgb in [("color", "T_RbWhite", (255, 255, 255)), ("mask", "T_RbDefaultARM", (255, 140, 0)), ("normal", "T_RbFlatNormal", (128, 128, 255))]:
    write_png(os.path.join(tmp, name + ".png"), rgb)
    import_file(os.path.join(tmp, name + ".png"), GAME + "/Textures/Defaults", name)
    defaults[kind] = setup_texture(GAME + "/Textures/Defaults/" + name, kind)

hdr_paths = import_file(os.path.join(RAW, "polyhaven", "canary_wharf", "canary_wharf_2k.hdr"), GAME + "/HDRI", "T_CanaryWharfSky")
hdri = unreal.load_asset(hdr_paths[0]) if hdr_paths else None
log("HDRI imported as %s" % (hdri.get_class().get_name() if hdri else "nothing"))

SURFACE = build_surface(defaults)
WORLD = build_world(defaults)
build_glass()
build_emissive()
build_highlight()
if hdri and isinstance(hdri, unreal.TextureCube):
    build_sky(hdri)
else:
    err("HDRI is not a TextureCube; sky material skipped")

# Architecture surfaces (world-aligned). TileSize is the real-world size of one texture repeat in cm.
for name, asset_id, tile, tint, rough in [
        ("MI_FloorParquet", "herringbone_parquet", 160, (1, 1, 1), 2.6),
        ("MI_FloorLaminate", "laminate_floor_02", 220, (1, 1, 1), 1.5),
        ("MI_FloorConcrete", "smooth_concrete_floor", 300, (1, 1, 1), 1.3),
        ("MI_FloorTerrazzo", "terrazzo_tiles", 180, (1, 1, 1), 2.0),
        ("MI_Carpet", "poly_wool_herringbone", 45, (0.3, 0.32, 0.34), 2.0),
        ("MI_FabricDark", "poly_wool_herringbone", 30, (0.12, 0.13, 0.15), 2.0),
        ("MI_Rug", "poly_wool_herringbone", 35, (0.52, 0.48, 0.42), 2.0),
        ("MI_WallPlaster", "plastered_wall_04", 260, (0.93, 0.91, 0.88), 1.0),
        ("MI_WoodOak", "oak_veneer_01", 140, (1, 1, 1), 0.9),
        ("MI_WoodWalnut", "black_walnut_veneer_01", 140, (1, 1, 1), 0.85),
        ("MI_AcousticPanel", "poly_wool_herringbone", 60, (0.55, 0.78, 0.8), 1.0)]:
    instance(name, WORLD, texture_set(asset_id), {"MetallicScale": 0.0, "RoughnessScale": rough}, {"Tint": tint + (1,), "TileSize": (tile, tile, tile, 0)})
    log("surface " + name)
# Untextured surfaces: defaults are white base, ARM = (AO 1, rough 0.55, metal 0) scaled below.
for name, tint, rough, metal in [("MI_Graphite", (0.06, 0.065, 0.075), 0.75, 1.0), ("MI_Chrome", (0.8, 0.8, 0.82), 0.3, 1.0),
                                 ("MI_CeilingWhite", (0.86, 0.86, 0.85), 1.6, 0.0), ("MI_TealAccent", (0.1, 0.55, 0.58), 1.0, 0.0),
                                 ("MI_Skirting", (0.12, 0.12, 0.13), 1.2, 0.0), ("MI_DeviceBlack", (0.02, 0.02, 0.025), 0.6, 0.0),
                                 ("MI_SkinTone", (0.45, 0.3, 0.22), 1.0, 0.0), ("MI_SuitNavy", (0.04, 0.06, 0.12), 1.4, 0.0),
                                 ("MI_Paper", (0.9, 0.9, 0.88), 1.7, 0.0), ("MI_Ceramic", (0.92, 0.92, 0.9), 0.35, 0.0),
                                 ("MI_PlasticGrey", (0.35, 0.36, 0.38), 0.8, 0.0), ("MI_Brass", (0.75, 0.55, 0.3), 0.35, 1.0)]:
    instance(name, SURFACE, {}, {"RoughnessScale": rough / 0.55, "MetallicScale": metal}, {"Tint": tint + (1,)})
instance("MI_Glass", unreal.load_asset(MAT + "/M_RbGlass"))
instance("MI_GlassFrosted", unreal.load_asset(MAT + "/M_RbGlass"), {}, {"Opacity": 0.55, "Roughness": 0.45}, {"Tint": (0.9, 0.93, 0.94, 1)})
instance("MI_LightPanel", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 60.0}, {"Color": (1, 0.97, 0.92, 1)})
instance("MI_ScreenTeal", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 18.0}, {"Color": (0.12, 0.45, 0.55, 1)})
instance("MI_ScreenAlert", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 16.0}, {"Color": (0.75, 0.3, 0.12, 1)})
instance("MI_Highlight", unreal.load_asset(MAT + "/M_RbHighlight"))
# Ceiling: faintly self-lit so it reads as bounce-lit white rather than black between panels.
instance("MI_CeilingSoft", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 13.0}, {"Color": (0.9, 0.9, 0.89, 1)})
# Ceiling T-bars: a touch darker than the tiles so the grid reads softly instead of as black lines.
instance("MI_CeilingGrid", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 10.5}, {"Color": (0.82, 0.83, 0.84, 1)})
instance("MI_LampWarm", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 45.0}, {"Color": (1.0, 0.78, 0.5, 1)})
instance("MI_ReceptionGlow", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 30.0}, {"Color": (0.17, 0.64, 0.66, 1)})
instance("MI_LedStrip", unreal.load_asset(MAT + "/M_RbEmissive"), {}, {"Intensity": 70.0}, {"Color": (0.95, 0.97, 1.0, 1)})

# Our own textures (Scripts/GenerateTextures.py): fictional screen UIs, whiteboard, wall prints.
GEN = os.path.join(ROOT, "Data", "Assets", "generated")
gen = {}
for f in sorted(os.listdir(GEN)) if os.path.isdir(GEN) else []:
    if f.endswith(".png"):
        name = os.path.splitext(f)[0]
        import_file(os.path.join(GEN, f), GAME + "/Textures/Generated", name)
        gen[name] = setup_texture(GAME + "/Textures/Generated/" + name, "color")
        if name.startswith("T_Screen"):  # small on screen but must stay legible: keep every mip resident
            gen[name].set_editor_property("never_stream", True)
            EAL.save_asset(gen[name].get_path_name(), only_if_is_dirty=False)
screen = build_screen(defaults)
for mi, tex, inten in [("MI_ScreenDashboard", "T_ScreenDashboard", 14.0), ("MI_ScreenSheet", "T_ScreenSheet", 7.0),
                       ("MI_ScreenEmail", "T_ScreenEmail", 14.0), ("MI_ScreenAlarmUI", "T_ScreenAlarm", 14.0),
                       ("MI_ScreenKriTicker", "T_ScreenKriTicker", 12.0)]:
    if tex in gen:
        instance(mi, screen, {"ScreenTexture": gen[tex]}, {"Intensity": inten})
for mi, tex in [("MI_Whiteboard", "T_Whiteboard"), ("MI_ArtPrintA", "T_ArtPrintA"), ("MI_ArtPrintB", "T_ArtPrintB"), ("MI_ArtPrintC", "T_ArtPrintC")]:
    if tex in gen:
        instance(mi, SURFACE, {"BaseColorMap": gen[tex]}, {"RoughnessScale": 1.5, "MetallicScale": 0.0})
# Clean polished concrete (monitoring room, corridor): our own texture, low roughness, 2.4 m slabs.
if "T_PolishedConcrete" in gen:
    instance("MI_FloorPolished", WORLD, {"BaseColorMap": gen["T_PolishedConcrete"]}, {"MetallicScale": 0.0, "RoughnessScale": 0.62},
             {"Tint": (0.8, 0.8, 0.8, 1), "TileSize": (240, 240, 240, 0)})
if EAL.does_asset_exist(MAT + "/M_RbSky"):
    instance("MI_Sky", unreal.load_asset(MAT + "/M_RbSky"), {}, {"Intensity": 14.0})

for asset_id in ["modern_arm_chair_01", "dining_chair_02", "sofa_02", "modern_coffee_table_01", "potted_plant_02", "potted_plant_04",
                 "drawer_cabinet", "binder_notebook", "clipboard", "office_notepads", "wall_clock", "projector_screen", "modern_ceiling_lamp_01"]:
    try:
        import_model(asset_id)
    except Exception as e:  # keep going so one bad asset does not block the rest
        err("model %s failed: %s" % (asset_id, e))

with open(os.path.join(ROOT, "Saved", "RbImportReport.json"), "w", encoding="utf-8") as f:
    json.dump(report, f, indent=1)
log("done: %d meshes, %d materials, %d errors" % (len(report["meshes"]), len(report["materials"]), len(report["errors"])))
