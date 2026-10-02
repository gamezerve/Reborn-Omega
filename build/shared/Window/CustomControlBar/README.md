# Reborn: Fixed Custom Control Bar layouts

The game uses these checked-in WNDs and ordinary mapped images. It does not scan,
mount, unpack or merge packages at runtime.

| Layout theme | Custom bar disabled | Custom bar enabled |
| --- | --- | --- |
| Zero Hour | Existing normal WNDs | Custom WNDs for the automatically selected art set; other menus stay Zero Hour |
| Generals | Existing Gen WNDs | Custom WNDs for the automatically selected art set; other menus stay Generals |

Reborn: The options menu exposes only the enable toggle. Displays up to 1920x1080
use the 1080p art/profile; larger width or height uses the 2160p art/profile.
The older five directories remain as offline preparation outputs, not selectable options.

Covered screens: ControlBar, ControlBarPopupDescription, Diplomacy,
GeneralsExpPoints, the three GenPowersShortcutBar screens, QuitMenu,
QuitNoSave, QuitMessageBox and ObserverQuit. Gen variants keep the canonical
window names expected by existing theme callbacks.

The WNDs retain Reborn's callbacks and controls. No special shortcut-button
compatibility adjustment has been added. The retail visual values were prepared
offline against the current mod templates; no retail-only input-blocking parents
are imported.

## Reborn: Assets and live switching

Custom art is isolated under RebornCBP_1080_ and RebornCBP_2160_ names. Both sets
use the normal mapped-image loader. Schemes and small Appearance.txt records are
under Data/INI/CustomControlBar. On a live switch, the appearance records update
existing control bar, science and power-shortcut windows without deleting their
owners, command definitions or input bindings. Quit and diplomacy use their
existing safe refresh lifecycle. Options itself is not reconstructed for a
custom bar change.

Keep Appearance.txt synchronized with WND geometry/font/draw-data changes. The
offline preparation tool emits both files together. This avoids parsing or
merging WND text during a live switch.

Reborn: Preferences store only UseCustomControlBar = yes/no. Missing values default
to disabled; legacy CustomControlBarResolution values are ignored and removed on save.

## Reborn: Offline preparation

Compile scripts/PrepareCustomControlBars.cpp with MSVC C++17 and run it with the
repository root as its only argument. It reads the unpacked BIG files under
build/shared/RebornOmegaData/CustomControlBar, skips installers/privacy add-ons,
and writes only UI resources into build/shared. No package files are required
by the game after preparation.

Control Bar Pro v1.2 source package credits: EA Games, FAS and xezon.
Its original ReadMe.txt remains in the supplied package folders.
