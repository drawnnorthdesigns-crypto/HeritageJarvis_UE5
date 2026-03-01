"""
Fixes game mode settings and opens MainMenu ready to play.
Run via: Tools > Execute Python Script
"""
import unreal

unreal.log("============================================================")
unreal.log("  Fixing HeritageJarvis game mode settings...")
unreal.log("============================================================")

# ---- Load the BP classes ----
main_gm_class = None
tartaria_gm_class = None

try:
    main_gm_class = unreal.load_object(
        None, "/Game/Blueprints/BP_HJMainGameMode.BP_HJMainGameMode_C")
    unreal.log("  [ok] Loaded BP_HJMainGameMode")
except:
    unreal.log_error("  [fail] Could not load BP_HJMainGameMode")

try:
    tartaria_gm_class = unreal.load_object(
        None, "/Game/Blueprints/BP_TartariaGameMode.BP_TartariaGameMode_C")
    unreal.log("  [ok] Loaded BP_TartariaGameMode")
except:
    unreal.log_warning("  [warn] Could not load BP_TartariaGameMode")

# ---- Step 1: Set project-level default game mode ----
unreal.log("")
unreal.log("Setting project defaults...")

# Use GameMapsSettings to set the global default game mode
game_maps = unreal.GameMapsSettings.get_default_object()
if game_maps and main_gm_class:
    game_maps.set_editor_property("global_default_game_mode", main_gm_class)
    game_maps.set_editor_property("game_default_map",
        unreal.SoftObjectPath("/Game/Maps/MainMenu"))
    game_maps.set_editor_property("game_instance_class",
        unreal.load_object(None, "/Script/HeritageJarvis.HJGameInstance"))
    unreal.log("  [ok] GlobalDefaultGameMode -> BP_HJMainGameMode")
    unreal.log("  [ok] GameDefaultMap -> /Game/Maps/MainMenu")
    unreal.log("  [ok] GameInstanceClass -> HJGameInstance")
else:
    unreal.log_warning("  Could not set project defaults via GameMapsSettings")

# ---- Step 2: Open MainMenu and set its world settings ----
unreal.log("")
unreal.log("Opening MainMenu map...")

try:
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_sub:
        level_sub.load_level("/Game/Maps/MainMenu")
    else:
        unreal.EditorLevelLibrary.load_level("/Game/Maps/MainMenu")
    unreal.log("  [ok] MainMenu opened")
except Exception as e:
    unreal.log_error(f"  Failed to open MainMenu: {e}")

# Set MainMenu world settings game mode override
try:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        ws = world.get_world_settings()
        if ws and main_gm_class:
            ws.set_editor_property("default_game_mode", main_gm_class)
            unreal.log("  [ok] MainMenu GameMode override -> BP_HJMainGameMode")

            # Save the map
            try:
                level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
                if level_sub:
                    level_sub.save_current_level()
                else:
                    unreal.EditorLevelLibrary.save_current_level()
                unreal.log("  [ok] MainMenu saved")
            except:
                unreal.log_warning("  [warn] Could not auto-save MainMenu (save manually with Ctrl+S)")
except Exception as e:
    unreal.log_warning(f"  World settings: {e}")

# ---- Step 3: Set TartariaWorld game mode override ----
unreal.log("")
unreal.log("Setting TartariaWorld game mode...")
try:
    tartaria_pkg = "/Game/Maps/TartariaWorld"
    if unreal.EditorAssetLibrary.does_asset_exist(tartaria_pkg):
        tartaria_world = unreal.load_object(None, tartaria_pkg + ".TartariaWorld")
        if tartaria_world and tartaria_gm_class:
            tws = tartaria_world.get_world_settings()
            if tws:
                tws.set_editor_property("default_game_mode", tartaria_gm_class)
                unreal.EditorAssetLibrary.save_asset(tartaria_pkg)
                unreal.log("  [ok] TartariaWorld GameMode -> BP_TartariaGameMode")
            else:
                unreal.log_warning("  Could not access TartariaWorld settings")
        else:
            unreal.log_warning("  Could not load TartariaWorld or BP_TartariaGameMode")
except Exception as e:
    unreal.log_warning(f"  TartariaWorld: {e} (non-critical)")

# ---- Done ----
unreal.log("")
unreal.log("============================================================")
unreal.log("  All set! Now click the GREEN PLAY BUTTON (top toolbar)")
unreal.log("  or press Alt+P to start Heritage Jarvis.")
unreal.log("============================================================")
