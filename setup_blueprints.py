"""
HeritageJarvis UE5 — Blueprint & Map Setup Script
===================================================

Run inside the UE5 Editor via:  Tools > Execute Python Script
(or paste into the Output Log Python console)

This script creates all Blueprint assets, Widget Blueprint assets,
and starter maps required by the HeritageJarvis project.

It is idempotent — safe to run multiple times.  Existing assets are
skipped with a log message.

Created assets:
  /Game/Blueprints/BP_HJMainGameMode      (parent: HJMainGameMode)
  /Game/Blueprints/BP_TartariaGameMode     (parent: TartariaGameMode)
  /Game/Blueprints/BP_TartariaCharacter    (parent: TartariaCharacter)
  /Game/Blueprints/BP_Artifact             (parent: HJArtifactActor)

  /Game/UI/WBP_Main           (parent: HJMainWidget)
  /Game/UI/WBP_Chat           (parent: HJChatWidget)
  /Game/UI/WBP_Projects       (parent: HJProjectsWidget)
  /Game/UI/WBP_Library        (parent: HJLibraryWidget)
  /Game/UI/WBP_Settings       (parent: HJSettingsWidget)
  /Game/UI/WBP_HUD            (parent: HJHUDWidget)
  /Game/UI/WBP_Pause          (parent: HJPauseWidget)
  /Game/UI/WBP_Notifications  (parent: HJNotificationWidget)
  /Game/UI/WBP_Loading        (parent: HJLoadingWidget)
  /Game/UI/WBP_Debug          (parent: HJDebugWidget)

  /Game/Maps/MainMenu         (empty level + PlayerStart)
  /Game/Maps/TartariaWorld    (level + PlayerStart + sky/lighting)
"""

import unreal


# ============================================================
# Helpers
# ============================================================

def _load_native_class(class_name):
    """
    Load a C++ class from the HeritageJarvis module by its undecorated name.
    For example 'HJMainGameMode' loads /Script/HeritageJarvis.HJMainGameMode.
    Returns the UClass* or None on failure.
    """
    path = "/Script/HeritageJarvis.{}".format(class_name)
    try:
        cls = unreal.load_class(None, path)
        if cls is not None:
            return cls
    except Exception:
        pass

    # Fallback: some classes are registered with their A/U prefix stripped
    # differently — try the raw path through load_object.
    try:
        cls = unreal.load_object(None, path)
        if cls is not None:
            return cls
    except Exception:
        pass

    unreal.log_warning("  Could not find native class: {} — skipping".format(path))
    return None


def create_blueprint(asset_path, native_class_name, asset_tools):
    """
    Create a standard Blueprint asset parented to a C++ class.

    Parameters
    ----------
    asset_path : str
        e.g. '/Game/Blueprints/BP_HJMainGameMode'
    native_class_name : str
        e.g. 'HJMainGameMode'
    asset_tools : unreal.AssetTools
        Cached reference from AssetToolsHelpers.
    """
    # Already exists?
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log("  [skip] Already exists: {}".format(asset_path))
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    parent_class = _load_native_class(native_class_name)
    if parent_class is None:
        return None

    package_path = asset_path.rsplit("/", 1)[0]
    asset_name = asset_path.rsplit("/", 1)[1]

    try:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        bp = asset_tools.create_asset(asset_name, package_path, None, factory)
        if bp is not None:
            unreal.log("  [ok]   Created: {}".format(asset_path))
        else:
            unreal.log_warning("  [fail] create_asset returned None for {}".format(asset_path))
        return bp
    except Exception as exc:
        unreal.log_error("  [error] {} — {}".format(asset_path, exc))
        return None


def create_widget_blueprint(asset_path, native_class_name, asset_tools):
    """
    Create a Widget Blueprint asset parented to a C++ UUserWidget subclass.

    Parameters
    ----------
    asset_path : str
        e.g. '/Game/UI/WBP_Main'
    native_class_name : str
        e.g. 'HJMainWidget'
    asset_tools : unreal.AssetTools
        Cached reference from AssetToolsHelpers.
    """
    # Already exists?
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log("  [skip] Already exists: {}".format(asset_path))
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    parent_class = _load_native_class(native_class_name)
    if parent_class is None:
        return None

    package_path = asset_path.rsplit("/", 1)[0]
    asset_name = asset_path.rsplit("/", 1)[1]

    try:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        wbp = asset_tools.create_asset(asset_name, package_path, None, factory)
        if wbp is not None:
            unreal.log("  [ok]   Created: {}".format(asset_path))
        else:
            unreal.log_warning("  [fail] create_asset returned None for {}".format(asset_path))
        return wbp
    except Exception as exc:
        unreal.log_error("  [error] {} — {}".format(asset_path, exc))
        return None


def create_level_with_actors(level_path, actor_specs):
    """
    Create a new level and spawn actors in it.

    Parameters
    ----------
    level_path : str
        e.g. '/Game/Maps/MainMenu'
    actor_specs : list[dict]
        Each dict has:
          'class': unreal.Class  (e.g. unreal.PlayerStart)
          'label': str           (human-readable name for logging)
          'location': unreal.Vector  (optional, default origin)
          'rotation': unreal.Rotator (optional, default zero)
    """
    if unreal.EditorAssetLibrary.does_asset_exist(level_path):
        unreal.log("  [skip] Level already exists: {}".format(level_path))
        return

    try:
        unreal.EditorLevelLibrary.new_level(level_path)
        unreal.log("  [ok]   Created level: {}".format(level_path))
    except Exception as exc:
        unreal.log_error("  [error] Could not create level {} — {}".format(level_path, exc))
        return

    # Spawn actors into the currently active (new) level
    for spec in actor_specs:
        actor_class = spec.get("class")
        label = spec.get("label", str(actor_class))
        loc = spec.get("location", unreal.Vector(0.0, 0.0, 0.0))
        rot = spec.get("rotation", unreal.Rotator(0.0, 0.0, 0.0))

        if actor_class is None:
            unreal.log_warning("    [skip] No class for actor '{}'".format(label))
            continue

        try:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, loc, rot)
            if actor is not None:
                unreal.log("    [ok]   Spawned: {}".format(label))
            else:
                unreal.log_warning("    [fail] spawn_actor returned None for '{}'".format(label))
        except Exception as exc:
            unreal.log_error("    [error] {} — {}".format(label, exc))

    # Save the level
    try:
        unreal.EditorLevelLibrary.save_current_level()
        unreal.log("  [ok]   Saved level: {}".format(level_path))
    except Exception as exc:
        unreal.log_warning("  [warn] Could not save level {} — {}".format(level_path, exc))


# ============================================================
# Main
# ============================================================

def main():
    unreal.log("=" * 60)
    unreal.log("  HeritageJarvis — Blueprint & Map Setup")
    unreal.log("=" * 60)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    # ----------------------------------------------------------
    # 1.  Standard Blueprints  (/Game/Blueprints/)
    # ----------------------------------------------------------
    unreal.log("")
    unreal.log("[1/3] Creating Blueprints ...")
    unreal.log("-" * 40)

    blueprints = [
        ("/Game/Blueprints/BP_HJMainGameMode",  "HJMainGameMode"),
        ("/Game/Blueprints/BP_TartariaGameMode", "TartariaGameMode"),
        ("/Game/Blueprints/BP_TartariaCharacter", "TartariaCharacter"),
        ("/Game/Blueprints/BP_Artifact",          "HJArtifactActor"),
    ]

    for path, cls_name in blueprints:
        create_blueprint(path, cls_name, asset_tools)

    # ----------------------------------------------------------
    # 2.  Widget Blueprints  (/Game/UI/)
    # ----------------------------------------------------------
    unreal.log("")
    unreal.log("[2/3] Creating Widget Blueprints ...")
    unreal.log("-" * 40)

    widget_blueprints = [
        ("/Game/UI/WBP_Main",          "HJMainWidget"),
        ("/Game/UI/WBP_Chat",          "HJChatWidget"),
        ("/Game/UI/WBP_Projects",      "HJProjectsWidget"),
        ("/Game/UI/WBP_Library",       "HJLibraryWidget"),
        ("/Game/UI/WBP_Settings",      "HJSettingsWidget"),
        ("/Game/UI/WBP_HUD",           "HJHUDWidget"),
        ("/Game/UI/WBP_Pause",         "HJPauseWidget"),
        ("/Game/UI/WBP_Notifications", "HJNotificationWidget"),
        ("/Game/UI/WBP_Loading",       "HJLoadingWidget"),
        ("/Game/UI/WBP_Debug",         "HJDebugWidget"),
    ]

    for path, cls_name in widget_blueprints:
        create_widget_blueprint(path, cls_name, asset_tools)

    # ----------------------------------------------------------
    # 3.  Maps  (/Game/Maps/)
    # ----------------------------------------------------------
    unreal.log("")
    unreal.log("[3/3] Creating Maps ...")
    unreal.log("-" * 40)

    # --- MainMenu: bare level with a PlayerStart at origin ---
    create_level_with_actors(
        "/Game/Maps/MainMenu",
        [
            {
                "class": unreal.PlayerStart,
                "label": "PlayerStart",
                "location": unreal.Vector(0.0, 0.0, 100.0),
            },
        ],
    )

    # --- TartariaWorld: full outdoor level with sky and lighting ---
    tartaria_actors = [
        {
            "class": unreal.PlayerStart,
            "label": "PlayerStart",
            "location": unreal.Vector(0.0, 0.0, 100.0),
        },
        {
            # DirectionalLight — acts as the sun
            "class": unreal.DirectionalLight,
            "label": "DirectionalLight (Sun)",
            "location": unreal.Vector(0.0, 0.0, 500.0),
            # Pitch -40 gives a pleasant late-morning sun angle
            "rotation": unreal.Rotator(-40.0, -60.0, 0.0),
        },
        {
            # SkyLight — ambient fill from sky cubemap
            "class": unreal.SkyLight,
            "label": "SkyLight",
            "location": unreal.Vector(0.0, 0.0, 500.0),
        },
        {
            # SkyAtmosphere — physically-based sky rendering
            "class": unreal.SkyAtmosphere,
            "label": "SkyAtmosphere",
            "location": unreal.Vector(0.0, 0.0, 0.0),
        },
        {
            # ExponentialHeightFog — ground-level atmospheric haze
            "class": unreal.ExponentialHeightFog,
            "label": "ExponentialHeightFog",
            "location": unreal.Vector(0.0, 0.0, 200.0),
        },
    ]

    create_level_with_actors("/Game/Maps/TartariaWorld", tartaria_actors)

    # ----------------------------------------------------------
    # 4.  Save everything
    # ----------------------------------------------------------
    unreal.log("")
    unreal.log("Saving all assets under /Game/ ...")
    try:
        unreal.EditorAssetLibrary.save_directory("/Game/")
        unreal.log("[ok] All assets saved.")
    except Exception as exc:
        unreal.log_warning("[warn] save_directory error — {}".format(exc))
        unreal.log("  Individual assets may still have been saved.")

    # ----------------------------------------------------------
    # Done
    # ----------------------------------------------------------
    unreal.log("")
    unreal.log("=" * 60)
    unreal.log("  Setup Complete!")
    unreal.log("=" * 60)
    unreal.log("")
    unreal.log("Next steps:")
    unreal.log("  1. Open Project Settings > Maps & Modes")
    unreal.log("     - Set Default GameMode to BP_HJMainGameMode")
    unreal.log("     - Set Editor Startup Map to /Game/Maps/MainMenu")
    unreal.log("     - Set Game Default Map  to /Game/Maps/MainMenu")
    unreal.log("")
    unreal.log("  2. Open BP_TartariaGameMode in the Blueprint Editor")
    unreal.log("     - Set HUDWidgetClass    -> WBP_HUD")
    unreal.log("     - Set PauseWidgetClass  -> WBP_Pause")
    unreal.log("     - Set NotificationWidgetClass -> WBP_Notifications")
    unreal.log("     - Set DebugWidgetClass  -> WBP_Debug")
    unreal.log("")
    unreal.log("  3. Open BP_HJMainGameMode in the Blueprint Editor")
    unreal.log("     - Set MainWidgetClass   -> WBP_Main")
    unreal.log("")
    unreal.log("  4. Open TartariaWorld in the Content Browser")
    unreal.log("     - Set World Settings > GameMode Override to BP_TartariaGameMode")
    unreal.log("     - Set Default Pawn Class to BP_TartariaCharacter")
    unreal.log("")


# Run immediately when the editor executes the script
main()
