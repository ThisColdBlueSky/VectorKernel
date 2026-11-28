# Vector Kernel for POCO X4 Pro 5G / Redmi Note 11 Pro 5G (`veux` / `peux`)

Linux `5.4.302` kernel for Qualcomm Snapdragon 695 (`sm6375` / `blair`), forked from [positron kernel](https://github.com/dereference23/kernel_xiaomi_sm6375.git).

## Backports

- **[MGLRU](https://docs.kernel.org/admin-guide/mm/multigen_lru.html)** — Multi-Gen LRU page reclamation enabled by default
- **[Maple Tree](https://docs.kernel.org/core-api/maple_tree.html)** — RCU-safe VMA tree backport
- **[Speculative Page Faults (SPF)](https://lwn.net/Articles/752572/)** — Lockless page fault handling adapted for Maple Tree
- **[Lazy RCU](https://lwn.net/Articles/912028/)** — `CONFIG_RCU_LAZY` with `rcu_nocbs=0-7` and upstream `kvfree_rcu` / RCU backports
- **[ZRAM Multi-Compression](https://docs.kernel.org/admin-guide/blockdev/zram.html#multi-compression-streams)** & **[ZSTD v1.5.7](https://github.com/facebook/zstd/releases/tag/v1.5.7)** — Backported from `android15-6.6`
- **[Swappiness 0–200](https://docs.kernel.org/admin-guide/sysctl/vm.html#swappiness)** — Extended `vm.swappiness` & `memory.swappiness` range
- **[F2FS](https://docs.kernel.org/filesystems/f2fs.html)** — Upstream backports including `lookup_mode=perf` and idle-class background GC (`SCHED_IDLE` / `IOPRIO_CLASS_IDLE`)
- **[TCP BBRv3](https://github.com/google/bbr)** — Default TCP congestion control algorithm
- **[LZ4](https://github.com/lz4/lz4) & ARM64 String Routines** — Upgraded LZ4 with ARM64 NEON optimizations and backported ARM64 string functions

## Features & Changes

- **Silver Cluster 691.2 MHz Voltage-Optimal Floor** — Default Silver (`policy0`) minimum frequency set to `691.2 MHz` across all governors at the flat `568 mV` voltage knee point (overridable via `scaling_min_freq`), alongside unlocked `blair.dtsi` Silver/Gold OPP filters
- **Charging & Thermal Controls** ([`Documentation/vector.md`](Documentation/vector.md)) — Charge limiter, bypass charging, USB ICL override, and vendor charging/CPU/GPU/backlight thermal bypass enabled by default with battery and skin temperature safety trips
- **[NoMount](https://github.com/maxsteeel/nomount)** — Integrated VFS path redirection subsystem (`CONFIG_NOMOUNT=y`)
- **Root (`-KSU` builds)** — [SukiSU Nova](https://github.com/SukiSU-Nova/SukiSU-Nova), [SUSFS](https://gitlab.com/simonpunk/susfs4ksu) (full feature support), and [KernelPatch (KPM)](https://github.com/bmax121/KernelPatch)
- **Compiler & Config Tuning** — Built with `-march=armv8.2-a -mtune=cortex-a55`, debug/UBSAN options disabled, 2 MB `ramoops` layout aligned, and watchdog / panic reboot enabled
- **Flashing & Companion App** — Universal dual-installer zip and Vector Link companion app
