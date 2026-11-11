# Vector Kernel for POCO X4 Pro 5G / Redmi Note 11 Pro 5G (`veux` / `peux`)

Fork of [positron kernel](https://github.com/dereference23/kernel_xiaomi_sm6375.git).

## SukiSU & SuSFS on Linux 5.4

### SELinux-Hiding Architectural Limits on Linux 5.4
On Linux 5.4, the upstream SELinux implementation lacks the RCU-based `struct selinux_policy` duplicate and pointer swap infrastructure introduced in Linux 5.10 (`selinux_state.policy`). As a result, the following features are intentionally compiled out or bypassed on 5.4 kernels:

1. **`ksu_selinux_hide_*` Live Policy Swapping**:
   - `selinux_hide.c` is compiled out on `< 5.10` in SukiSU builtin. Linux 5.4 uses in-place `selinux_state.ss->policydb` protected by `policy_rwlock`, not dynamic RCU swapping.
2. **`setprocattr` Spoofing**:
   - `security/selinux/hooks.c:6759` is version-guarded (`>= 5.10`) because it depends on `ksu_selinux_hide_running`. On 5.4, it passes through to native `selinux_setprocattr()`.
3. **Fake Status Page**:
   - `fake_status` and `initialize_fake_status()` in `security/selinux/selinuxfs.c:242, 257` are version-guarded (`>= 5.10`). On 5.4, `sel_open_handle_status()` falls through to the native kernel status page.
4. **`selinuxfs` Context/Access Spoofing**:
   - `handle_sel_write_context()` and `handle_sel_write_access()` in `security/selinux/selinuxfs.c:712, 1020` are version-guarded (`>= 5.10`). On 5.4, writes fall through to native `sel_write_context()` and `sel_write_access()`.

### Active In-Kernel SELinux Stealth Hooks on Linux 5.4
The following in-kernel stealth mechanisms remain fully active on Linux 5.4:
- **Status Page Sequence Spoofing** (`security/selinux/ss/status.c:93, 129`): When `susfs_is_current_ksu_domain()` is true, updates `status->policyload` without incrementing `status->sequence`, preventing userspace `libselinux` / AVC from observing policy loads.
- **Policy Capability Reload Sequence Spoofing** (`security/selinux/ss/services.c:2305, 3018`): Bypasses sequence number increments (`latest_granting`) during KSU policy reload.
- **AVC Log Spoofing** (`security/selinux/avc.c:728`): Gated by runtime static key `susfs_is_avc_log_spoofing_enabled` via `ksud susfs config avc_log_spoofing 1`.

