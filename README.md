# Black Mesa VR for macOS / MoltenVR

Black Mesa VR on Apple Silicon through CrossOver, DXVK and MoltenVR.

On an M3 Max, our tested Office Complex scene runs at approximately **72 FPS** with **2144×2304 rendered pixels per eye** and deferred shadows disabled. This is a measured scene result, not a minimum across the campaign. The headset used is Quest 3 over USB. The user's streaming preference is HEVC at 200 Mbps; stream resolution and actual game render resolution are separate settings.

The same mod DLL contains compatibility profiles for the main Black Mesa build **19042901** and public beta **25844216**, with the upstream experimental gameplay changes integrated into both profiles. Both game profiles have rendered through this universal implementation. Campaign-wide compatibility and arbitrary later game updates are not claimed.



## Original project

This is a macOS compatibility fork of [Black Mesa VR](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr). For the original developer's current release and instructions, see [upstream releases](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest).


## Gameplay features

- VR main/pause menu panel with a right-hand ray for pointing and clicking.
- Direct ray-to-panel cursor mapping, with no extra cursor gain by default.
- Larger, more distant menu panel (2.10 m wide, 1.30 m forward by default).
- World-fixed menu anchor: it appears in front of the viewer and stays in place instead of rotating with the head.
- Panel recentering follows the runtime's reference-space reset; removed the temporary hardcoded 90-degree workaround.
- Orange controller markers while interacting with menus; black surroundings/loading presentation rather than leftover scene imagery where supported by the menu path.
- Manual recenter by holding both sticks for approximately three seconds.
- **Seated mode:** optional height compensation, disabled by default.
- Support-hand weapon aiming/visible grip for weapons supported by the mod's grip table, including shotgun and other long guns. Proximity is required only to acquire the grip; once acquired, hold lasts until grip is released. Existing weapon/menu/tracking safety resets remain.
- Smaller stereo world-space reticle. It is not restricted to one eye; no universal right-eye iron-sight realignment is claimed.
- **FSR 1 upscaling:** EASU + RCAS, optional and disabled by default.
- Hand HUD retained; duplicate desktop health/ammo HUD suppressed.
- Upstream experimental-public-beta gameplay changes integrated into the universal DLL rather than selecting an older gameplay implementation on stable.

## Installation and launch

### Tested setup

Apple Silicon M3 Max, Quest 3 over USB, CrossOver Preview, custom Black Mesa VR DXVK DLL and MoltenVK 1.4.2. Approximately 72 FPS in the measured Office Complex scene at **2144×2304 rendered per eye**, with deferred shadows disabled. Performance varies with scene and settings.

User streaming preference: HEVC, 200 Mbps. 2368×2560 per eye is a separate headset/stream target discussed during setup, not the resolution used for the cited 72 FPS measurement. Configure resolution in MoltenVR; verify the game's effective render resolution rather than assuming stream dimensions equal source pixels.

### 1. MoltenVR and CrossOver

Install [MoltenVR](https://github.com/MoltenVR/MoltenVR-Preview) using its current setup guide and headset client. It provides the macOS OpenXR runtime and headset streaming. Connect Quest 3 and verify the MoltenVR session works.

Install Windows Steam and Black Mesa inside the CrossOver bottle you will actually use. Register/install MoltenVR's OpenXR runtime into that same bottle. Selecting the same Wine engine in another app does not copy the bottle's registry/runtime configuration.

### 2. MoltenVK graphics layer

Use [MoltenVK 1.4.2](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2). This is the Vulkan-to-Metal layer, separate from MoltenVR's OpenXR runtime/streaming.

Quit all applications using the CrossOver engine before replacing its libraries. Back up the original dylibs. In our tested CrossOver Preview installation, the upstream universal macOS `libMoltenVK.dylib` was installed at BOTH:

```
CrossOver Preview.app/Contents/SharedSupport/CrossOver/lib/x86_64/libMoltenVK.dylib
CrossOver Preview.app/Contents/SharedSupport/CrossOver/lib/aarch64/libMoltenVK.dylib
```

Paths may differ in other CrossOver distributions. Do not replace unrelated GStreamer framework libraries. A CrossOver update can restore its original graphics libraries. The mod DLL also contains DXVK compatibility changes; replacing MoltenVK alone is insufficient.

### 3. Mod files

The current candidate is an UPDATE package: first obtain the required original mod assets from [upstream releases](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest), choosing a release appropriate to the game build. Close the game before copying files.

Install our `d3d9.dll` in all three locations:

```
Black Mesa/d3d9.dll
Black Mesa/bin/d3d9.dll
Black Mesa/bin/thirdparty/dxvk-windows-x86/d3d9.dll
```

Install the matching helper at `Black Mesa/VR/openxr_helper64/OpenXRHelper64.exe`. Keep the required 64-bit `openxr_loader.dll` alongside it. Do not overwrite the helper with a probe/benchmark build. Preserve existing `VR/config.txt` and personal offsets when updating. The defaults are examples, not personal settings from the developer's machine.

The package installer backs up changed binaries. Keep the mod's required hands, weapon-wheel assets and action manifest. Game/Valve files are not included in this fork's update archive.

### 4. Graphics and launch

Select the DXVK backend in CrossOver. D3DMetal does not implement this mod's D3D9/Vulkan path. Set these Steam game launch options:

```
-oldgameui -enabledxvk -novid -dev +developer 0 +exec bmvr_performance.cfg +snd_mix_async 0 +snd_async_fullyasync 0
```

Do not add `-nohmd` or `-forcestartupmenu`. The classic menu is required; the beta's replacement game menu is not supported. `-dev` prevents the background 3D map from loading. It does not remove gameplay maps.

The CrossOver/Wine launch environment should include:

```
BMVR_ENABLE_DEP=1
BMVR_CPU_BRIDGE=1
BMVR_SHARED_GPU_READBACK=1
BMVR_QUIET=1
BMVR_STALL_CAPTURE=0
BMVR_PERF_DIAG=0
BMVR_ENTITY_DIAG=0
BMVR_SHARED_GPU_DIAG=0
MVK_CONFIG_API_VERSION_TO_ADVERTISE=4206592
MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1
MVK_CONFIG_LOG_LEVEL=0
WINEDEBUG=-all
DXVK_LOG_LEVEL=none
BMVR_SHADER_CACHE_PATH=VR/cache
DXVK_STATE_CACHE_PATH=VR/cache
```

These are environment variables, not console commands to paste into Steam's launch-options box. Use the supplied CrossOver launch script for the prepared environment. No NotProton modification is required or supplied by this guide.

Install `bmvr_performance.cfg` in `bms/cfg/`, not only `VR/`. It sets `nr_shadow_active 1`, `mat_vsync 0`, `fps_max 0` and `engine_no_focus_sleep 0`. Deferred shadows off is part of the cited performance profile. Keep `FSR=false` and MSAA/FXAA off for the baseline; optional FSR can be enabled in `VR/config.txt` after restarting.

### 5. Launch

Start the MoltenVR stream, wear the headset, then launch Black Mesa in the configured CrossOver bottle. Load a save from the classic menu. Hold both stick buttons for approximately three seconds to recenter if needed. Seated mode is opt-in in the mod config.

If startup reports `XR_ERROR_RUNTIME_UNAVAILABLE`, fix the OpenXR registration/libraries inside the active bottle. An OpenVR fallback error is secondary; installing random `openvr_api.dll` files does not repair the missing OpenXR runtime.

### Known limits

- Main build 19042901 and public beta 25844216 have compatibility profiles and runtime rendering evidence. Later updates can change interfaces.
- Multi-level transition testing is still underway. The no-background-menu workaround is not a universal geometry-corruption fix.
- No full campaign or guaranteed minimum FPS claim.
- FSR is optional and off by default; no claimed latency benefit.
- NotProton is excluded from current release support.

## Credits

Based on [Black Mesa VR](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest), L4D2VR and DXVK. Original copyright and license notices are retained.
