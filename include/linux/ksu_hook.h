/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shared declarations for symbols that are defined by KernelSU/SukiSU
 * (the KernelSU/ submodule) but used by the in-tree KSU/SUSFS hooks.
 *
 * Keeping these declarations in one place avoids the local "extern"
 * drift that caused ksu_is_init_rc_hook_enabled to be declared as a
 * static_key_false while the definition used DEFINE_STATIC_KEY_TRUE.
 *
 * This header is deliberately self-contained: it only needs forward
 * declarations and basic integer types.  drivers/Makefile force-includes
 * it into the KernelSU build so that every definition is checked against
 * these declarations and any type mismatch fails the build.
 */
#ifndef _LINUX_KSU_HOOK_H
#define _LINUX_KSU_HOOK_H

#include <linux/types.h>
#include <linux/compiler_types.h>
#include <linux/version.h>

struct cred;
struct file;
struct filename;
struct page;
struct static_key_false;
struct static_key_true;
struct user_arg_ptr;

#ifdef CONFIG_KSU

/* runtime/ksud.c */
extern bool ksu_vfs_read_hook;
extern bool ksu_execveat_hook;
extern bool ksu_input_hook;
extern struct static_key_true ksu_is_init_rc_hook_enabled;
extern struct static_key_true ksu_is_input_hook_enabled;
extern struct static_key_true is_init_second_stage_not_executed;
extern struct static_key_true is_first_zygote;
extern int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
			       size_t *count_ptr, loff_t **pos);
extern void ksu_handle_vfs_fstat(int fd, loff_t *kstat_size_ptr);

/* feature/sucompat.c */
extern bool ksu_su_compat_enabled;
extern int ksu_handle_execveat(int *fd, struct filename **filename_ptr,
			       void *argv, void *envp, int *flags);
extern int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
					void *argv_user, void *envp_user,
					int *flags);
extern int ksu_handle_post_execveat_sucompat(int *fd, struct filename **filename_ptr,
					     void *argv_user, void *envp_user,
					     int *flags, int *retval);
#ifdef CONFIG_KSU_SUSFS
extern int ksu_handle_faccessat(int *dfd, struct filename **filename,
				int *mode, int *__unused_flags);
extern int ksu_handle_stat(int *dfd, struct filename **filename, int *flags);
#else
extern int ksu_handle_faccessat(int *dfd, const char __user **filename_user,
				int *mode, int *__unused_flags);
extern int ksu_handle_stat(int *dfd, const char __user **filename_user,
			   int *flags);
extern int ksu_handle_execve_sucompat(int *fd, const char __user **filename_user,
				      void *argv, void *__never_use_envp,
				      int *__never_use_flags);
#endif

/* preinit/preinit.c */
#ifndef KSU_NO_PREINIT
extern int ksu_setup_preinit_file(struct filename **filename_ptr,
				  struct user_arg_ptr *argv_user);
extern int ksu_preinit_handle_post_execveat(int *fd, struct filename **filename_ptr,
					    void *argv_user, void *envp_user,
					    int *flags, int *retval, int *preinit_fd);
#endif

/* supercall/{dispatch,supercall}.c */
extern int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd,
				 void __user **arg);

/* hook/lsm_hook.c */
extern int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid);

/* policy/allowlist.c */
extern bool __ksu_is_allow_uid_for_current(uid_t uid);

/* ksu_main.c */
extern struct cred *ksu_cred;

/* selinux/selinux.c (KSU-defined SUSFS globals) */
#ifdef CONFIG_KSU_SUSFS
extern u32 susfs_ksu_sid;
extern u32 susfs_priv_app_sid;
#endif

/* feature/selinux_hide.c (only built on 5.10+) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
extern struct page *fake_status;
extern struct static_key_false fake_status_initialize_key;
extern bool ksu_selinux_hide_running;
extern bool ksu_selinux_hide_enabled;
extern void initialize_fake_status(void);
#endif

#endif /* CONFIG_KSU */

#endif /* _LINUX_KSU_HOOK_H */
