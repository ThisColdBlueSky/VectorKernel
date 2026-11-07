/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_THERMAL_OVERRIDE_H__
#define __LINUX_THERMAL_OVERRIDE_H__

#include <linux/types.h>
#include <linux/cpufreq.h>

bool vendor_thermal_override_safe(void);
bool vendor_thermal_cpu_override_active(unsigned long *max_state, struct cpufreq_policy *policy);
bool vendor_thermal_gpu_override_active(unsigned long *max_state);

void vendor_thermal_reapply(void);

void vendor_thermal_register_cpu_reapply(void (*fn)(void));
void vendor_thermal_unregister_cpu_reapply(void);

void vendor_thermal_register_gpu_reapply(void (*fn)(void));
void vendor_thermal_unregister_gpu_reapply(void);

#endif /* __LINUX_THERMAL_OVERRIDE_H__ */
