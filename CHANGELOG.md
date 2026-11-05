Changes after f4b8a13

## Features
- Add lindroid-drm loopback driver (EVDI-based virtual display) (e6b7c86)
- Integrate BakaSU (KernelSU fork) with manual VFS hooks for root access (dce427e)

## Drivers
- Add qcacld-3.0 WiFi driver frame injection framework (monitor mode, security testing, test automation) (b4ddb9d)
- Enable asynchronous TX in qcacld-3.0 WiFi driver (75bb102)
- Add qcacld-3.0 WiFi driver policy manager API and action handling (62ec856)
- Fix qcacld-3.0 WiFi driver cfg80211 and main handling (596daab, 1e3cd4d, 46c3d42, cc641da, c0d9520)
- Fix qcacld-3.0 WiFi driver RX monitor and TX/RX paths (00adf12, 1e96ebb)
- Refactor qcacld-3.0 WiFi driver WMA frame injection and data path (a6d6473, 3b8a680, 41de64a, f744299, dbe51a6)
- Misc qcacld-3.0 WiFi driver fixes and Kbuild/defconfig updates (0e03fbb, b88238b)
- Drop silver cluster cpufreq filter in blair.dtsi to allow lower idle frequencies (6b852ac)

## Fixes
- Fix mm/vmscan.c: restore shrinker_rwsem declaration for CONFIG_MEMCG (fa6ebca)
- Protect shrink_slab_memcg with del_rwsem; disable FAIR_GROUP_SCHED to avoid contention (162d804)
- Revert MEMCG enable and vmscan changes due to slab shrinker panics (2c97074)
- Major memcg fix: rewrite shrinker allocation with mutex, fix shrink_slab_memcg iteration, enable MEMCG in defconfig (75f80d3)
- Remove KernelSU v0.9.5 integration (driver + VFS hooks) (e0dff79)
- Enable DEBUG_KERNEL in defconfig so KALLSYMS_ALL survives olddefconfig (89bf4cb)

## Performance
- Upgrade LZ4 library to latest unified upstream with ARM64 NEON hardware-accelerated decompression, per-CPU core dispatch, -O3/LZ4_FAST_DEC_LOOP optimization, and kernel mode NEON decompression for zram/EROFS (f091292)
- Backport Multi-Gen LRU (Mglru): add arch_has_hw_pte_young, generation-based page tracking, aging/eviction, PID controller, rmap spatial locality, page table walkers, mm_struct tracking, multi-memcg optimization, thrashing prevention (2e13188)
- Backport F2fs fixes and optimizations: fsync deadloop, atomic write checkpoint inconsistency, inline data/dentry fixes, node/metadata readahead optimization, UAF fix, deadlock fixes, orphan inode validation, sanity checks, remove superblock freeze for flush thread (d12a977)
- Backported optimised arm64 string routines (memcpy/memmove, memcmp, strcmp, strncmp, strlen, memchr) from upstream

## Config/Defconfig
- Enable MEMCG, container prerequisites (PID/IPC/USER namespaces, cgroup device/cpuacct/bpf), FAIR_GROUP_SCHED, CFS_BANDWIDTH, and fix config_data.gz generation (eb1e5f6)
- Test build with FAIR_GROUP_SCHED and CFS_BANDWIDTH enabled (MEMCG disabled) (238dcbd)
- Enable CONFIG_WIREGUARD in stock_defconfig (5b0bcc2)
- Enable NETFILTER_XT_TARGET_HL in stock_defconfig and veux_defconfig (44c77b9, 161910d)
- Enable kprobes, optprobes, kprobe_events, MODULES, MODULE_UNLOAD, KALLSYMS_ALL (1a527cf)
- Enable DEBUG_KERNEL in veux_defconfig (89bf4cb)
- Add USB gadget net configs (ECM, RNDIS, EEM, subset) and NETFILTER_XTABLES to veux_defconfig (880a55b)

## Upstream Merges
- Upgraded LZ4 to latest upstream unified implementation with ARM64 NEON assembly routines (f091292)
- Merged upstream Linux Mglru (multi-gen LRU) backport for 5.4 kernel (2e13188)
- Merged upstream Linux F2fs backport: fsync, atomic write, inline data, readahead, stability/concurrency fixes (d12a977)

## Security
- Integrate BakaSU (KernelSU fork) with manual hooks in exec, open, stat, reboot syscalls (dce427e)
- Remove KernelSU v0.9.5 integration (e0dff79)

## Other
- Update qcacld-3.0 driver Kbuild and default defconfig (b88238b)
