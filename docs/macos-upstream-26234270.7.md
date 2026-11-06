# macOS integration through experimental-beta-26234270.7

The full 42-commit upstream range c1097b5f..8347108 is integrated on top of v71.

Changes include per-eye bloom/auto-exposure and target viewports, per-eye deferred light culling, stereo lens-flare layout and strength/fade controls, FXAA parameter delivery and panel controls, two-column settings, hand roll and support-hand placement, the G-Man gesture, early render-target interception, corrected rope normals, and build-aware stale-flag invalidation. Upstream diagnostics remain opt-in.

Mac-specific transport, pose snapshots, controller conversion, menu/recenter handling, mandatory old UI/background-menu workaround, and MoltenVK/SPIR-V compatibility remain. FSR stays optional and disabled by default. Existing user configuration values are preserved.

The Touch hand path already derives its neutral palm orientation from the model. Its author-default90-degree roll therefore means no additional rotation on that path; user adjustments are relative to90. The new gun-grip positional adjustment applies to that asset-defined path as well. This avoids stacking two independent thumb-up rotations.

Older game ABI selection and allocator signatures are retained. New flare/frustum absolute data addresses are enabled only on verified build26234270; older game builds keep their existing path. The FXAA bind site uses a byte signature and checks that its parameter blocks are adjacent. Fresh configuration uses the new upstream OffCenterEyes/hold-for-settings defaults; upgrade preserves existing values. No graphics preset or AA setting is forced by this integration.

Validation: x86 DLL and x64 helper cross-built on macOS. Transactional installer preservation/rollback tests and read-only installed-module signatures passed. CPU bridge/ABI/FSR compatibility checks are executed for both architectures. Bridge ABI remains20, CPU shared GPU metadata2. Headset gameplay and performance have not been tested for v72. No FPS improvement is claimed.
