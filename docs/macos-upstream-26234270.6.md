# macOS integration through experimental-beta-26234270.6

Upstream range: b10c5847d014342e39f78742cdbb2665b81d7fc8 through c1097b5f1abaaf24eacfadf2c1b4d409fa6ba332 (211 commits). The actual macOS baseline is the source archive distributed with v70, not the fork root's older src files.

## Integrated upstream work

- Engine/interface/RVA changes for the October public betas, through game build 26234270.
- Reflection/refraction target sizing, viewport remapping, colour-correction/HUD fixes, deferred rendering fixes, and per-eye visibility queries.
- Material reflection strength controls; no repeated whole-material walk on each new material.
- Scope zoom/stereo/eye selection, muzzle flashes, projectile origins, sights, aim dot and laser beam.
- Two-handed weapons, grenade handling/throwing, flashlight options, controller input, crouch/sprint, gestures and the VR settings panel.
- Pause/save thumbnails, chapter/title overlays, scripted-camera handling, frame cap controls and audio changes.

## Mac integration choices

The existing DXVK/MoltenVK compatibility code and SPIR-V specialization remain. CPU transport, coherent render-pose snapshots, controller conversion, the menu ray/anchor and Quest recenter handling remain where the upstream Windows GPU-handle path cannot replace them. The real D3D9 Reset path remains: a size-only early return lost state on the menu-to-level path. The launcher retains -dev to avoid the known background-map corruption path.

The author's shot-origin implementation replaces the competing old server-hook installation. The Mac crowbar contact path is handled by that single hook. New per-eye frusta are carried in the same CPU snapshot as the pixels and render pose. Title overlays use the author's VIEW reference space. Overlays are not submitted with stale pixels before the first blit of a new showing.

The prior loading flag and helper-capability bit shared reservedTail0. Loading now has a separate field. Bridge ABI is version 20; CPU GPU-readback metadata is version 2. Install the matching DLL and helper together. Slot/control layouts stay 128/448 bytes for both architectures.

Original stable/previous-beta selections are retained where the baseline supplied verified values. New beta addresses were read-only checked against installed game modules. The stable game is not installed and has not been retested with this integration.

## Build and validation

Cross-built on macOS with clang-cl/lld-link (LLVM 23.1.4), xwin Microsoft Windows SDK/CRT, Ninja and CMake. The x86 toolchain uses /EHa and disables lifetime markers to preserve guarded asynchronous SEH loads. No Windows VM is needed for the build.

CPU tests cover x86/x64 bridge layout, per-eye tangent transport, separate loading/capability storage, SEH fault recovery, ABI selection and FSR constants. Public D3D9 exports are compared to v70. Game-module checks cover RenderView, CreateMove, CalcViewModelView, the server shot source, material RT access and engine screen-size entry points.

FSR wrapper recovery and SPIR-V provenance are recorded in experiments/openxr-fsr/RECOVERY.md. The optional FSR setting stays off unless enabled in the user's configuration.

Status: compiled; offline CPU tests and static game-module validation. No headset or gameplay validation for this build yet. No FPS comparison has been made.
