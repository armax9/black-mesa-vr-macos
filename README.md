# Black Mesa VR for macOS

Play Black Mesa in VR on Apple Silicon with **CrossOver, DXVK and [MoltenVR](https://github.com/MoltenVR/MoltenVR-Preview)**.

Tested on **M3 Max + Quest 3 over USB, HEVC at 200 Mbps**. Our earlier Office Complex benchmark reached approximately **72 FPS at 2144×2304 rendered pixels per eye**, with deferred shadows disabled. v71 has been tested successfully in the headset; a new performance comparison has not been run.

**[Download the latest macOS release](https://github.com/armax9/black-mesa-vr-macos/releases/tag/v71-macos-preview)** · [Original Black Mesa VR](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest)

## What's new in v71

This update integrates the full upstream range since our v70 baseline: **211 commits**, through [experimental-beta-26234270.6](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/tag/experimental-beta-26234270.6).

- Compatibility with the latest Black Mesa public beta, **build 26234270**.
- Water and reflective-floor rendering fixes, adjustable reflection strength, and HUD/colour-correction fixes.
- Improved iron-sight alignment, crossbow scope, muzzle flashes and projectile origins; optional weapon laser.
- Upstream VR settings panel, two-handed weapon handling, grenade handling and controller improvements.
- Save/pause overlays, scripted-camera handling and audio fixes.
- Updated MoltenVR frame transport, including coherent per-eye projection data and separate loading/transport flags.
- **Launch with the normal Play button in Windows Steam through CrossOver.** The mod applies its performance and audio configuration automatically.

The latest public beta was tested with this update. Compatibility code for main build **19042901** and earlier beta **25844216** is retained; those builds have not been retested with v71. Testing used CrossOver Preview. Regular CrossOver is expected to work with the same setup but has not been tested.

## Features

- Stereo rendering with tracked headset and controllers through MoltenVR.
- Main and pause menus with a right-hand laser pointer and trigger selection.
- Large menu panels that stay in place when you turn your head.
- Menu recentering when you reset your position in Quest; orange controller markers in menus.
- Hold both thumbstick buttons for three seconds to recenter.
- Two-handed aiming for supported long guns: hold the support grip to keep holding the weapon.
- Smaller stereo aiming reticle and hand HUD.
- **Optional FSR 1 upscaling (EASU + RCAS), disabled by default.**
- **Optional seated mode with height compensation, disabled by default.**

## Installation

### 1. CrossOver and MoltenVR

Install Windows Steam and Black Mesa in a CrossOver bottle. Select **DXVK** as the bottle's graphics backend.

Install [MoltenVR and its Quest client](https://github.com/MoltenVR/MoltenVR-Preview), following its setup instructions. Register the MoltenVR OpenXR runtime in the same bottle, connect Quest and start a streaming session.

### 2. MoltenVK

Download the macOS libraries from [MoltenVK 1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2). MoltenVK translates Vulkan graphics to Metal; MoltenVR handles tracking and streaming.

Quit CrossOver and its Windows applications. Back up the existing libraries, then copy the universal macOS `libMoltenVK.dylib` into both locations:

```text
/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib/x86_64/libMoltenVK.dylib
/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib/aarch64/libMoltenVK.dylib
```

For Preview, use `CrossOver Preview.app`. If CrossOver is installed elsewhere, use that location. CrossOver updates may replace these libraries.

### 3. Install the macOS mod

1. Download and extract **BlackMesa-MoltenVR-v71-macOS.zip** from the [latest release](https://github.com/armax9/black-mesa-vr-macos/releases/tag/v71-macos-preview).
2. Close Black Mesa.
3. Double-click **Install.command**, then choose the Black Mesa folder containing `bms.exe`.

The package includes the DLL, matching OpenXR helper, hands, weapon-wheel assets and configuration. The installer preserves existing mod settings, adds missing options and backs up replaced files under `VR/backups/`. Updating from v70 uses the same installer.

The DLL and helper form a matching pair; the installer updates them together.

## Launch from Steam

In Windows Steam inside CrossOver, open **Black Mesa → Properties → Launch Options** and set this once:

```text
-oldgameui -enabledxvk -novid -dev +developer 0
```

Start MoltenVR, then click **Play** in Steam. If Steam offers several launch choices, choose **Default**. Load your save from the menu.

`-oldgameui` selects the classic menu. `-dev` skips the background 3D map, which avoids rendering corruption after loading a save from that menu. The mod loads `bmvr.cfg` itself; the macOS configuration applies the performance profile and audio settings automatically.

## Configuration

Mod options are in `VR/config.txt`. FSR and seated mode are disabled in the supplied defaults. Restart after changing them.

- **FSR:** set `FSR=true` and a lower `RenderScale`, for example `0.75`. Its effect on frame time depends on the scene and hardware.
- **Seated mode:** set `SeatedMode=true`; `SeatedHeightOffsetMeters` adds an optional height adjustment.
- **VR settings:** open the gear entry in the weapon-selection menu for the upstream settings panel. Weapon laser and reflection controls are available there.

`bms/cfg/bmvr_performance.cfg` contains the profile used for the earlier benchmark: deferred shadows disabled (`nr_shadow_active 1`) and VSync disabled. MSAA and FXAA were disabled for that measurement. Upstream frame-cap controls are available in the VR settings. Render resolution and headset stream resolution are separate settings.

## Source and credits

Current sources are in this repository. The release also includes **BlackMesa-MoltenVR-v71-source.zip** and **SHA256SUMS.txt**. [Integration and validation notes](docs/macos-upstream-26234270.6.md) describe the build and compatibility work.

Based on [Black Mesa VR](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest), L4D2VR, DXVK, OpenXR and AMD FidelityFX FSR. Original copyright and license notices are retained. The source can be cross-built on macOS without a Windows VM.
