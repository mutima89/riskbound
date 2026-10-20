# Creates the (empty) L_MeridianHQ map asset. The HQ itself is built at runtime by
# ARbMeridianHQ (Source/Riskbound/Private/World), so this map only needs to exist.
# Run through Scripts\CreateMap.bat; safe to re-run (does nothing if the map exists).
import unreal

MAP_PATH = "/Game/Maps/L_MeridianHQ"

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.log("RbCreateMap: {} already exists".format(MAP_PATH))
else:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if levels.new_level(MAP_PATH) and levels.save_current_level():
        unreal.log("RbCreateMap: created {}".format(MAP_PATH))
    else:
        unreal.log_error("RbCreateMap: FAILED to create {}".format(MAP_PATH))

unreal.SystemLibrary.quit_editor()
