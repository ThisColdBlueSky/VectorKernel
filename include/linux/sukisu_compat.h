/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUKISU_COMPAT_H
#define _LINUX_SUKISU_COMPAT_H

#include <linux/version.h>

/*
 * 5.4 compatibility definitions for SukiSU-Ultra.
 * Note: vdso_clock_mode in struct clocksource is aliased to archdata.clock_mode
 * via an anonymous union in include/linux/clocksource.h to preserve exact struct
 * size and layout without corrupting the 'enum vdso_clock_mode' type token.
 */

#endif /* _LINUX_SUKISU_COMPAT_H */
