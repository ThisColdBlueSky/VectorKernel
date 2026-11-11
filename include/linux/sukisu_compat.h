/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUKISU_COMPAT_H
#define _LINUX_SUKISU_COMPAT_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
/*
 * In Linux < 5.8, vdso clock_mode was arch-specific inside archdata:
 *   arch/arm64/include/asm/clocksource.h: struct arch_clocksource_data { enum vdso_clock_mode clock_mode; };
 * Upstream 5.8 (commit 24641f3074d2) promoted this to generic struct clocksource::vdso_clock_mode.
 * By aliasing vdso_clock_mode to archdata.clock_mode on < 5.8, we avoid modifying the struct
 * layout of struct clocksource, preserving 100% KABI/ABI compatibility while correctly writing
 * to the clock_mode checked by arm64 vDSO (__arm64_get_clock_mode in vsyscall.h).
 */
#define vdso_clock_mode archdata.clock_mode
#endif

#endif /* _LINUX_SUKISU_COMPAT_H */
