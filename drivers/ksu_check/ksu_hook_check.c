/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Compile-time guard for the KSU hook declarations.
 *
 * <linux/ksu_hook.h> is the single place for the declarations of symbols
 * that are defined by the KernelSU/SukiSU submodule but used by the
 * in-tree KSU/SUSFS hooks.  drivers/Makefile force-includes that header
 * into the KernelSU build, so every actual definition is checked against
 * it as well.
 *
 * This file additionally pulls in the SukiSU headers that declare the
 * same symbols.  If one of those headers disagrees with the shared
 * header, the build fails here before the kernel is linked.
 */
#include <linux/ksu_hook.h>

#include "../kernelsu/kernel_includes.h"

#include "../kernelsu/runtime/ksud.h"
#include "../kernelsu/feature/sucompat.h"
#include "../kernelsu/include/ksu.h"
#include "../kernelsu/policy/app_profile.h"
#include "../kernelsu/policy/allowlist.h"
#include "../kernelsu/feature/selinux_hide.h"
#include "../kernelsu/preinit/preinit.h"
#include "../kernelsu/supercall/supercall.h"
#include "../kernelsu/hook/lsm_hook.h"

/*
 * Keep the translation unit non-empty.  The check is the include set
 * above; this object is not meant to do anything at runtime.
 */
static int __maybe_unused ksu_hook_check_placeholder;
