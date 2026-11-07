/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_THERMAL_OVERRIDE_H__
#define __LINUX_THERMAL_OVERRIDE_H__

#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/kconfig.h>

#if IS_ENABLED(CONFIG_WT_CHG)

bool vendor_thermal_override_safe(void);
bool vendor_thermal_cpu_override_active(unsigned long *max_state, struct cpufreq_policy *policy);
bool vendor_thermal_gpu_override_active(unsigned long *max_state);
bool vendor_thermal_backlight_override_active(void);

void vendor_thermal_reapply(void);

void vendor_thermal_register_cpu_reapply(void (*fn)(void));
void vendor_thermal_unregister_cpu_reapply(void);

void vendor_thermal_register_gpu_reapply(void (*fn)(void));
void vendor_thermal_unregister_gpu_reapply(void);

void vendor_thermal_register_backlight_reapply(void (*fn)(void));
void vendor_thermal_unregister_backlight_reapply(void);

#else /* !IS_ENABLED(CONFIG_WT_CHG) */

static inline bool vendor_thermal_override_safe(void)
{
	return false;
}

static inline bool vendor_thermal_cpu_override_active(unsigned long *max_state,
						      struct cpufreq_policy *policy)
{
	return false;
}

static inline bool vendor_thermal_gpu_override_active(unsigned long *max_state)
{
	return false;
}

static inline bool vendor_thermal_backlight_override_active(void)
{
	return false;
}

static inline void vendor_thermal_reapply(void) {}

static inline void vendor_thermal_register_cpu_reapply(void (*fn)(void)) {}
static inline void vendor_thermal_unregister_cpu_reapply(void) {}

static inline void vendor_thermal_register_gpu_reapply(void (*fn)(void)) {}
static inline void vendor_thermal_unregister_gpu_reapply(void) {}

static inline void vendor_thermal_register_backlight_reapply(void (*fn)(void)) {}
static inline void vendor_thermal_unregister_backlight_reapply(void) {}

#endif /* IS_ENABLED(CONFIG_WT_CHG) */

#endif /* __LINUX_THERMAL_OVERRIDE_H__ */
