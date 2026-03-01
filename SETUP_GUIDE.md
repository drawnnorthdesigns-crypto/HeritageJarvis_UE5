# Heritage Jarvis UE5 — Setup Guide

## Prerequisites (already done)
- [x] Visual Studio 2022 Community (C++ Desktop + Game workloads)
- [x] Epic Games Launcher
- [ ] Unreal Engine 5.4+ (install via Epic Launcher → Library → Engine Versions)

---

## Step 1 — Open the project in UE5

1. Double-click `HeritageJarvis.uproject`
2. If prompted to select an engine version, pick your installed UE5 version
3. When asked "This project has C++ source files, would you like to build now?" → click **Yes**
4. Visual Studio will open and compile — wait for it to finish (first compile takes a few minutes)
5. UE5 editor will open

---

## Step 2 — Fix the EngineAssociation (if needed)

If you installed UE 5.5 instead of 5.4, right-click `HeritageJarvis.uproject` →
"Switch Unreal Engine version" → select your installed version.

---

## Step 3 — Create Blueprint children in the editor

The C++ classes are base classes. Create Blueprint children for each:

### Game Mode & Character
| Blueprint | Parent C++ class | Folder |
|-----------|-----------------|--------|
| `BP_HJMainGameMode` | `HJMainGameMode` | Content/Blueprints/ |
| `BP_TartariaGameMode` | `TartariaGameMode` | Content/Blueprints/ |
| `BP_TartariaCharacter` | `TartariaCharacter` | Content/Blueprints/ |
| `BP_Artifact` | `HJArtifactActor` | Content/Blueprints/ |

### Main Menu UI (WBP = Widget Blueprint)
| Blueprint | Parent C++ class | Folder |
|-----------|-----------------|--------|
| `WBP_Main` | `HJMainWidget` | Content/UI/ |
| `WBP_Projects` | `HJProjectsWidget` | Content/UI/ |
| `WBP_Chat` | `HJChatWidget` | Content/UI/ |
| `WBP_Library` | `HJLibraryWidget` | Content/UI/ |
| `WBP_Settings` | `HJSettingsWidget` | Content/UI/ |

### In-Game / Overlay UI
| Blueprint | Parent C++ class | Folder |
|-----------|-----------------|--------|
| `WBP_HUD` | `HJHUDWidget` | Content/UI/ |
| `WBP_Pause` | `HJPauseWidget` | Content/UI/ |
| `WBP_Notifications` | `HJNotificationWidget` | Content/UI/ |
| `WBP_Loading` | `HJLoadingWidget` | Content/UI/ |
| `WBP_Debug` | `HJDebugWidget` | Content/UI/ |

To create a Blueprint from C++:
- Content Drawer → right-click → Blueprint Class → search for the C++ class name

### Assign widget classes in BP_TartariaGameMode (Details panel)
After creating the WBPs above, open `BP_TartariaGameMode` and set:
- **HUD Widget Class** → `WBP_HUD`
- **Pause Widget Class** → `WBP_Pause`
- **Notification Widget Class** → `WBP_Notifications`
- **Debug Widget Class** → `WBP_Debug`

### Assign widget classes in WBP_Main
Open `WBP_Main` and set:
- **Notification Widget Class** → `WBP_Notifications`
- **Debug Widget Class** → `WBP_Debug`
- **Loading Widget Class** → `WBP_Loading`
- **Settings Widget Class** → `WBP_Settings`

---

## Step 4 — Set up the Game Instance

1. Edit → Project Settings → Maps & Modes
2. Set **Game Instance Class** to `HJGameInstance`
3. Set **Default GameMode** to `BP_HJMainGameMode`

---

## Step 5 — Create Maps

1. File → New Level → Empty Level
2. Save as `Content/Maps/MainMenu`
3. File → New Level → Open World (for Tartaria)
4. Save as `Content/Maps/TartariaWorld`

---

## Step 6 — Set up BP_TartariaCharacter

1. Open `BP_TartariaCharacter`
2. In the Details panel, assign:
   - **Default Mapping Context** → create `IMC_Tartaria` Input Mapping Context asset
   - Create one Input Action asset for each of the following:

| Input Action | Suggested key | Value type |
|-------------|--------------|------------|
| `IA_Move` | WASD | Axis2D |
| `IA_Look` | Mouse XY | Axis2D |
| `IA_Jump` | Space | Digital |
| `IA_Sprint` | Shift | Digital |
| `IA_Interact` | E | Digital |
| `IA_OpenMenu` | Escape | Digital |
| `IA_DebugToggle` | F3 | Digital |

---

## Step 7 — Launch

Run `launch_hj.bat` — it starts Flask then opens UE5 with the project.

Update the `UE5_PATH` in `launch_hj.bat` if your Epic install is on a different drive.

---

## Project Structure

```
HeritageJarvis_UE5/
├── HeritageJarvis.uproject       ← Open this in UE5
├── launch_hj.bat                 ← Launcher (starts Flask + UE5)
├── Config/
│   ├── DefaultEngine.ini         ← Lumen, Nanite, DX12 enabled
│   ├── DefaultGame.ini
│   └── DefaultInput.ini
└── Source/HeritageJarvis/
    ├── Core/
    │   ├── HJApiClient           ← All Flask HTTP calls
    │   ├── HJEventPoller         ← Polls Flask for pipeline status (SSE substitute)
    │   └── HJGameInstance        ← Persists across levels, owns ApiClient + EventPoller
    ├── UI/
    │   ├── HJMainWidget          ← Root UI, tab navigation
    │   ├── HJProjectsWidget      ← Projects tab
    │   ├── HJChatWidget          ← Steward AI chat
    │   ├── HJMainGameMode        ← Main menu level mode
    └── Game/
        ├── TartariaCharacter     ← Player character (3rd person)
        └── TartariaGameMode      ← Game world mode
```
