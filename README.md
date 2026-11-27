# Vector Kernel for POCO X4 Pro 5G / Redmi Note 11 Pro 5G (`veux` / `peux`)

Linux `5.4.302` for Snapdragon 695 (`sm6375` / `blair`), forked from [positron kernel](https://github.com/dereference23/kernel_xiaomi_sm6375.git).

## Features

### Memory & Compression
- **[Maple Tree](https://docs.kernel.org/core-api/maple_tree.html)** (`MT_FLAGS_USE_RCU`): Linux 6.1 VMA management replacing RB-tree.
- **[Speculative Page Faults (SPF)](https://lwn.net/Articles/783556/)**: Lockless anonymous & file page faults without `mmap_sem` contention.
- **[MGLRU](https://docs.kernel.org/admin-guide/mm/multigen_lru.html)**: Multi-generational LRU page reclaim enabled by default.
- **[Swappiness `0..200`](https://docs.kernel.org/admin-guide/sysctl/vm.html#swappiness)**: Extended `vm.swappiness` and `memory.swappiness` for zram-biased reclaim.
- **[ZRAM Multi-Compression](https://docs.kernel.org/admin-guide/blockdev/zram.html#recompression)**: Linux 6.6 backport — fast **[LZ4](https://github.com/lz4/lz4)** (ARM64 NEON) primary + **[ZSTD v1.5.7](https://github.com/facebook/zstd/releases/tag/v1.5.7)** secondary recompression (Android 17 [`mmd`](https://source.android.com/docs/core/perf/mmd) ready).

### Scheduler, Compiler & I/O
- **CASS + 16ms PELT**: Capacity-aware task placement with `schedutil`.
- **[Clang Full LTO](https://docs.kernel.org/kbuild/llvm.html)**: Built with `-march=armv8.2-a -mtune=cortex-a55` and `BPF_JIT_ALWAYS_ON`.
- **[F2FS](https://docs.kernel.org/filesystems/f2fs.html)**: `lookup_mode=perf` (hash-only casefold lookup) and `SCHED_IDLE` background GC.
- **[TCP BBRv3](https://github.com/google/bbr)**: Default congestion control + ARM64 CE crypto & optimized IP/IPv6 checksums.

### Power & Thermal
- **Charging**: Charge limiter, bypass charging, and USB current override (up to 1500 mA).
- **Thermal Bypass**: Vendor charging, CPU, GPU, and backlight throttling bypassed by default with battery & chassis safety trips ([Documentation/vector.md](Documentation/vector.md)).

### Root, Installer & App
- **Root**: [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) + [SUSFS](https://gitlab.com/simonpunk/susfs4ksu) (full feature support) + [KernelPatch (KPM)](https://github.com/bmax121/KernelPatch) in `-KSU` builds.
- **Universal Dual-Installer**: Flashable in Recovery or as a KernelSU / Magisk module with automatic slot backups.
- **[Vector Link](vector-link/)**: Material 3 companion app for live battery wattage, charging/thermal controls, ZSTD ZRAM recompression toggle, and boot persistence.
