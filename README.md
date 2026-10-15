# Black Mesa VR for macOS

Play Black Mesa in VR on Apple Silicon with **CrossOver, DXVK and [MoltenVR](https://github.com/MoltenVR/MoltenVR-Preview)**.

Tested on **M3 Max + Quest 3 over USB**, using HEVC at 200 Mbps. The measured Office Complex scene runs at approximately **72 FPS at 2144×2304 rendered pixels per eye**, with deferred shadows disabled. Performance varies by location and settings.

One DLL supports Black Mesa main build **19042901** and public beta **25844216**. Testing used CrossOver Preview. Regular CrossOver is expected to work with the same setup, but has not yet been tested.

Based on [Black Mesa VR by Hochgeschwindigkeitsrennfahrer](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest).

## Features

- Stereo rendering and tracked headset/controllers through MoltenVR.
- VR main and pause menus with a right-hand laser pointer and trigger selection.
- Large menu panels that stay in place when you turn your head.
- Menu recentering when you reset your position in Quest.
- Orange controller markers in menus.
- Hold both thumbstick buttons for three seconds to recenter.
- Two-handed aiming for supported long guns: grab with the support hand and hold grip to keep holding the weapon.
- Smaller stereo aiming reticle and hand HUD.
- **Optional FSR 1 upscaling (EASU + RCAS), disabled by default.**
- **Optional seated mode with height compensation, disabled by default.**
- Upstream experimental gameplay updates included for both supported game builds.

## Installation

### 1. Set up CrossOver and MoltenVR

Install Windows Steam and Black Mesa in a CrossOver bottle. Select **DXVK** as the bottle's graphics backend.

Install [MoltenVR and its Quest client](https://github.com/MoltenVR/MoltenVR-Preview), following the project's setup instructions. Register its OpenXR runtime in the same CrossOver bottle, connect Quest and start a streaming session.

### 2. Install MoltenVK 1.4.2

Download the macOS libraries from [MoltenVK 1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2). MoltenVK translates Vulkan graphics to Metal; MoltenVR handles VR tracking and streaming.

Quit CrossOver and its Windows applications. Back up the existing libraries, then copy the universal macOS `libMoltenVK.dylib` into both locations:

```text
/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib/x86_64/libMoltenVK.dylib
/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib/aarch64/libMoltenVK.dylib
```

For Preview, use `CrossOver Preview.app` instead of `CrossOver.app`. If CrossOver is installed elsewhere, use that location. CrossOver updates may replace these libraries.

### 3. Install the mod

1. Install the original mod assets from the [upstream release](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest) appropriate to your game version.
2. Download and extract **BlackMesa-MoltenVR-v70-macOS.zip** from [this fork's release](https://github.com/armax9/black-mesa-vr-macos/releases/tag/v70-macos-preview).
3. Close the game. Open Terminal in the extracted folder and run:

```bash
python3 install.py "/path/to/SteamLibrary/steamapps/common/Black Mesa"
```

Replace the example path with your Black Mesa folder containing `bms.exe`. The installer backs up replaced files under `VR/backups/` and preserves existing configuration files.

The package installs the mod DLL, matching OpenXR helper, default configuration and launcher. The original mod supplies the required hands and weapon-wheel assets. Keep the 64-bit `openxr_loader.dll` beside `VR/openxr_helper64/OpenXRHelper64.exe`.

## Launch

The supplied `start.command` prepares the required Wine/MoltenVK environment. Open it in a text editor and set:

- `cx` to your CrossOver Wine executable. Regular CrossOver uses `/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine`; Preview uses `CrossOver Preview.app`.
- The bottle name after `--bottle` to your Steam bottle's name. The default is `Steam`.

Start the MoltenVR stream, wear the headset, then run `start.command` from the Black Mesa folder:

```bash
bash "/path/to/SteamLibrary/steamapps/common/Black Mesa/start.command"
```

Load your save from the main menu.

For launches through Windows Steam in the same configured bottle, use these game launch options:

```text
-oldgameui -enabledxvk -novid -dev +developer 0 +exec bmvr_performance.cfg +snd_mix_async 0 +snd_async_fullyasync 0
```

The launcher supplies these options automatically. `-oldgameui` selects the classic menu; `-dev` skips the background 3D map to avoid rendering corruption when loading a save from the menu.

### Configuration

Mod options are in `VR/config.txt`. FSR and seated mode are optional and disabled in the supplied defaults. Restart the game after changing them.

`bms/cfg/bmvr_performance.cfg` contains the performance profile used for the measurement above: deferred shadows disabled (`nr_shadow_active 1`), VSync disabled and no FPS cap. MSAA and FXAA were disabled for the baseline measurement. Render resolution and headset stream resolution are separate settings.

## Preview status

The supported game builds have been tested, but full-campaign and level-transition testing is still in progress. Later game updates may require compatibility changes. The classic menu with its 3D background disabled is the supported menu configuration.

## Source and credits

The release includes `BlackMesa-MoltenVR-v70-source.zip`; `macos-v70.patch` contains tracked source changes. `SHA256SUMS.txt` provides checksums for the release archives.

Based on [Black Mesa VR](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest), L4D2VR and DXVK. Original copyright and license notices are retained.
