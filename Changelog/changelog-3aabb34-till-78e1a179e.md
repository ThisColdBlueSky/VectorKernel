# Changelog: 3aabb34 → 78e1a179e

## Features
- Replace BakaSU with SukiSU-Ultra and add SUSFS v2.4.0 core with SUS_PATH, OPEN_REDIRECT, execve sucompat/preinit, read/write/stat, mm, fs, kernel, proc and SELinux hooks (464bc628c, dda202e09, 8e5ed5103, dcc447ed8, 246dc0f54, 5fe0fdda8, e957c116e)
- Add kernel-side battery charge limit with hysteresis and charge_stop_pct/charge_resume_pct controls (323c758ff, 7c6ae3015)
- Add runtime bypass charging and SDP USB input-current override (080945b13, 3709a3343)
- Expose unified /sys/kernel/vector/ controls with 0660 permissions while preserving /sys/class/qcom-battery/ compatibility (db8cb853b, 5445f6091, 607b7e374)
- Add safe thermal throttle override using BMS battery temperature hysteresis and JEITA protection, and drop voltage hysteresis (8897d9fe7, 8894d65ca)
- Add safe CPU/GPU/backlight thermal overrides with separate requested state and reapply (6d5fac3de, 758bb231e, 7422128da)
- Add thermal_override parameter setters, debug trips, lifecycle cleanup, per-device serialization and CONFIG_WT_CHG stubs (9a39ef6dc, a134f91ed, 07cf15209, cd8fbcdcf)

## Drivers
- Revert NetHunter-style qcacld monitor-mode/frame-injection changes and restore exact upstream HDD files (2c4c6657e, 6bdc76ee1)
- Harden wt_chg workqueue teardown, shutdown handling and g_batt_chg access (c0373ba39, f0cec9533)
- Rename ignore_vendor_thermal to ignore_vendor_charging_thermal with stock default (014c4fb58)
- Eliminate redundant qcom power-supply I2C register dumps and unplugged polling (525885b79)
- Disable Lindroid EVDI driver and remove its Kconfig/Makefile build hooks (8b35eb307)
- Decouple CPU, GPU and backlight thermal overrides from charging switch so features operate independently under safety guards (e1f2bb137)

## Fixes
- Add SELinux 5.4 compatibility includes and version-guard selinux_hide hooks (6bb522d36, 16c6dcbec)
- Isolate 5.4 SukiSU-Ultra compatibility in sukisu_compat.h and preserve vdso_clock_mode ABI/enum token (75cf77f32, 806e8c84c)
- Fix KSU manual hook prototypes and ksu_is_init_rc_hook_enabled static_key_false type (3d51c38a0)
- Serialize cooling set_cur_state/reapply with per-device mutex and guard pre-probe thermal init (07cf15209, a134f91ed)
- Update SukiSU-Ultra submodule to include the merged 5.4 reset_avc_cache fix (25e2a36d7, e736a1040)
- Restore the sysfs attribute-group permission mask to 0664 after dropping the 0666 allowance (607b7e374)
- Restore static_key_true declaration for ksu_is_init_rc_hook_enabled to match runtime definition, fixing broken module mounts (0df4f415c)
- Centralize KSU/SUSFS hook declarations in include/linux/ksu_hook.h, fix ksu_handle_vfs_read return type, and add drivers/ksu_check compile-time verification (51c3c1b06)
- Remove soundcard model name overrides in audio DTSI to ensure native 'holi-qrdsku1-snd-card' registration, fixing microphone and audio HAL mixer path lookup (78e1a179e)

## Performance
- Backport Maple tree and convert VMA tracking/walkers from rbtree/vmacache to Maple tree (0468a12fd)
- Backport TCP BBRv3, retain BBRv1 as bbr1, and set BBRv3 as default (b0abf8885, 31b5cd5e2)
- Add optimized arm64 IP/IPv6 checksum routines (~6.4x throughput on 1500-byte packets) (68abe30bc)
- Route LZ4 decompress_safe/partial through ARM64 NEON and select no-prefetch NEON on Cortex-A55 (2f3932610, ec294f9dd)
- Enable ARM64 UAO and CNP (f13571a62)

## Config/Defconfig
- Enable MEMCG and MEMCG_SWAP, keep only required cgroup controllers, and disable UBSAN (4c2311162, 7cb13f4d7)
- Enable ARM64 UAO and CNP in veux_defconfig (f13571a62)
- Enable CONFIG_KSU_SUSFS and verified sub-options (35c58f3c3)
- Set BBRv3 as default TCP congestion control (31b5cd5e2)
- Disable Lindroid EVDI in vector_defconfig (8b35eb307)
- Enable CONFIG_TMPFS_XATTR in veux_defconfig (2c63ed4a3)

## Upstream Merges
- Maple tree port (#6) (0468a12fd)
- BBRv3 backport (#7) (b0abf8885)
- SUSFS v2.4.0 core and hooks (dda202e09, 8e5ed5103)

## Security
- SukiSU-Ultra root integration with SUSFS hooks across exec, namei, read/write, stat, mm, fs, kernel, proc and SELinux (464bc628c, dda202e09, 8e5ed5103, dcc447ed8, 246dc0f54, 5fe0fdda8, e957c116e)
- SELinux 5.4 compatibility guards and includes (6bb522d36, 16c6dcbec)
- Centralize KSU/SUSFS hook declarations in shared header with build-time validation against SukiSU drivers (51c3c1b06)
