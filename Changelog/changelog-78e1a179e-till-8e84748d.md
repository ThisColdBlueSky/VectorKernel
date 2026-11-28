# Changelog: 78e1a179e → 8e84748d

## Features
- Integrate NoMount VFS path redirection subsystem (`CONFIG_NOMOUNT=y`) (`248b2914`)
- Backport Speculative Page Faults (SPF) adapted for Maple Tree with VMA refcounting, `vm_fault` field caching, and `vm_sequence` locking (`914ea403`..`d584d49a`)
- Backport Lazy RCU (`CONFIG_RCU_LAZY`), shrinker, and `rcu_nocbs=0-7` cmdline support alongside upstream RCU/NOCB/`kvfree_rcu` updates (`bda4477b`..`24158eb2`)
- Backport `CONFIG_ZRAM_MULTI_COMP` from `android15-6.6` and import ZSTD `v1.5.7` (`ecf3d0f7`, `8a13ba88`, `f433cc0d`)
- Backport F2FS linear dentry lookup (`lookup_mode` mount option & sysfs controls) and `SCHED_IDLE`/`IOPRIO_CLASS_IDLE` background GC (`a947a89c`..`ab8b5b94`, `e9f9d785`)
- Extend `vm.swappiness` and `memory.swappiness` range to `0–200` (`93527342`, `fc9a0e46`)
- Update `vector-link` companion app with ZSTD ZRAM recompression toggle and synced thermal defaults (`33285b1f`)
- Enable vendor charging/CPU/GPU/backlight thermal bypasses and disable thermal ceiling by default in `wt_chg` (`698c6799`)

## Drivers
- Restore synchronous FUSE release for destroy-time `fuseblk` mounts (`f4d9b0ee`)
- Fix `fuse_backing_mmap()` refcount on error (`0aea6798`)
- Fix `techpack` display rotator debugfs header guard mismatch (`f88a6f43`)

## Fixes
- Clear `MT_FLAGS_USE_RCU` during `dup_mmap()` bulk insertion (`df26c517`)
- Audit and fix SPF `mremap`, `filemap_fault`, `khugepaged`, `pmd`/`pte_unmap_same`, `locked_next`, and `put_page()` edge cases (`d5a5d1e5`, `2a1c3119`, `8c902ed7`, `d888f050`, `ebf6118d`)
- Fix `zstd`/`zram` `f2fs` pointer argument, 64-bit prio shift, `zs_malloc` check, and algorithm restore (`849c9425`)
- Fix F2FS linear lookup error check and `memcg` swappiness cap (`fc9a0e46`)
- Fix `rcu/nocb` shrinker race and `rcu_barrier()` wakeups (`eee748cc`, `9ce99860`, `15afd75c`)

## Performance
- Enable `-march=armv8.2-a` and `-mtune=cortex-a55` compiler tuning in `arm64/Makefile` (`94f9deff`)
- Enable Speculative Page Faults (`CONFIG_SPECULATIVE_PAGE_FAULT`) and Lazy RCU (`CONFIG_RCU_LAZY`) (`d584d49a`, `065aaf97`)
- Enable ZRAM multi-compression (`CONFIG_ZRAM_MULTI_COMP`) with ZSTD `v1.5.7` (`ecf3d0f7`, `8a13ba88`)

## Config/Defconfig
- Enable `CONFIG_NOMOUNT=y` (`248b2914`)
- Enable `CONFIG_SPECULATIVE_PAGE_FAULT` (`d584d49a`)
- Enable `CONFIG_RCU_LAZY`, `CONFIG_DEBUG_FS`, and `rcu_nocbs=0-7` (`065aaf97`, `24158eb2`)
- Enable `CONFIG_ZRAM_MULTI_COMP` and `CONFIG_ZSTD_COMMON` (`8a13ba88`, `f433cc0d`)

## Upstream Merges
- Extra backports: SPF, F2FS linear lookup, swappiness `0–200` (PR #16) (`c35607ee`)
- ZSTD `v1.5.7` & `ZRAM_MULTI_COMP` backport (PR #17) (`3fa92fb9`)
- Upstream RCU/NOCB/`kvfree_rcu` & `mem_dump_obj()` backports

## Documentation
- Update `README.md` with Vector Kernel backports, features, SukiSU Nova, and upstream Linux/ACK/CAF/CLO notes (`07e03e71`, `6d218a4d`, `8e84748d`)
