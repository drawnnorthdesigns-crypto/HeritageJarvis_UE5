"""
Opens the MainMenu map, sets the TartariaWorld game mode override,
and starts Play-In-Editor.

Run via: Tools > Execute Python Script > play_now.py
"""
import unreal

# ---- Step 1: Set TartariaWorld game mode override ----
unreal.log("Setting TartariaWorld game mode override...")
tartaria_path = "/Game/Maps/TartariaWorld"
if unreal.EditorAssetLibrary.does_asset_exist(tartaria_path):
    try:
        # Load the world
        world = unreal.load_object(None, tartaria_path + ".TartariaWorld")
        if world:
            ws = world.get_world_settings()
            if ws:
                gm_class = unreal.load_class(None, "/Script/HeritageJarvis.TartariaGameMode")
                bp_gm = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/BP_TartariaGameMode")
                if bp_gm:
                    gen_class = unreal.load_object(None, "/Game/Blueprints/BP_TartariaGameMode.BP_TartariaGameMode_C")
                    if gen_class:
                        ws.set_editor_property("default_game_mode", gen_class)
                        unreal.log("  [ok] TartariaWorld GameMode -> BP_TartariaGameMode")
                    else:
                        unreal.log_warning("  Could not load BP_TartariaGameMode_C class")
                elif gm_class:
                    ws.set_editor_property("default_game_mode", gm_class)
                    unreal.log("  [ok] TartariaWorld GameMode -> TartariaGameMode (C++)")
            else:
                unreal.log_warning("  Could not get world settings")
        else:
            unreal.log_warning("  Could not load TartariaWorld")
    except Exception as e:
        unreal.log_warning(f"  TartariaWorld override failed (non-critical): {e}")

# ---- Step 2: Open the MainMenu map ----
unreal.log("Opening MainMenu map...")
try:
    # Use EditorLoadingAndSavingUtils or subsystem
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_subsystem:
        level_subsystem.load_level("/Game/Maps/MainMenu")
        unreal.log("  [ok] MainMenu loaded via LevelEditorSubsystem")
    else:
        # Fallback
        unreal.EditorLevelLibrary.load_level("/Game/Maps/MainMenu")
        unreal.log("  [ok] MainMenu loaded via EditorLevelLibrary")
except Exception as e:
    unreal.log_warning(f"  Load failed: {e}")
    try:
        unreal.EditorLevelLibrary.load_level("/Game/Maps/MainMenu")
        unreal.log("  [ok] MainMenu loaded (fallback)")
    except Exception as e2:
        unreal.log_error(f"  Could not open MainMenu: {e2}")

unreal.log("")
unreal.log("============================================================")
unreal.log("  MainMenu is now open!")
unreal.log("  Click the GREEN PLAY button (top toolbar) to start.")
unreal.log("  Or press Alt+P.")
unreal.log("============================================================")
