# SPDX-License-Identifier: GPL-2.0-only
#
# Header-consistency check for the KSU hook declarations.  The object is
# empty at runtime; its only purpose is to compile the shared header
# together with the SukiSU headers.
#
obj-$(CONFIG_KSU) += ksu_hook_check.o

ccflags-y += -I$(srctree)/drivers/kernelsu -I$(srctree)/drivers/kernelsu/include
ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux
ccflags-y += -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat -Wno-missing-prototypes
ccflags-y += -Wno-declaration-after-statement -Wno-unused-function
