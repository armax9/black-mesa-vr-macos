## Black Mesa VR for macOS — v71

All upstream changes since our v70 baseline are integrated: **211 commits**, through [experimental-beta-26234270.6](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/tag/experimental-beta-26234270.6), while retaining the CrossOver/MoltenVK/MoltenVR compatibility work.

- Supports the latest public beta, **game build 26234270**.
- Fixes for water/reflections, HUD and colour correction, scope rendering, sights, muzzle flashes and projectile origins.
- Upstream VR settings panel, optional weapon laser, reflection controls, two-handed weapons, grenades, input and audio improvements.
- Updated Mac frame transport and title/pause overlays.
- **Normal Steam Play-button launch.** The performance/audio profile loads automatically.
- Optional **FSR 1** and **seated mode**, both off by default.

Tested successfully in the headset on **M3 Max + wired Quest 3**. The earlier v70 Office Complex measurement was approximately **72 FPS at 2144×2304 rendered per eye, HEVC 200 Mbps**, with deferred shadows disabled. v71 has not been re-benchmarked. Earlier stable/beta compatibility is retained but has not been retested for this update.

### Install / update

1. Set up [MoltenVR](https://github.com/MoltenVR/MoltenVR-Preview), CrossOver and MoltenVK using the [included guide](https://github.com/armax9/black-mesa-vr-macos#installation).
2. Close Black Mesa, extract **BlackMesa-MoltenVR-v71-macOS.zip**, double-click **Install.command** and choose the game folder.
3. Set Steam launch options once:

```text
-oldgameui -enabledxvk -novid -dev +developer 0
```

Start MoltenVR, then click **Play** in Windows Steam through CrossOver. Choose **Default** if Steam asks for a launch variant.

The package includes the matching DLL/helper and required VR assets. Existing player settings are preserved and missing options are added. Replaced files are backed up under `VR/backups/`.

The complete setup guide is in the repository README. Original mod: [Black Mesa VR upstream releases](https://github.com/Hochgeschwindigkeitsrennfahrer/black-mesa-vr/releases/latest).
