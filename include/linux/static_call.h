/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_STATIC_CALL_H
#define _LINUX_STATIC_CALL_H

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include_next <linux/static_call.h>
#else
/*
 * 5.4 compatibility stub: static_call does not exist in Linux 5.4.
 * SukiSU kernel_includes.h includes <linux/static_call.h> but
 * does not invoke any static_call APIs.
 */
#include <linux/sukisu_compat.h>
#endif

#endif /* _LINUX_STATIC_CALL_H */
