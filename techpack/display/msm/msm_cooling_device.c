// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/thermal_override.h>
#include "msm_cooling_device.h"

#define BRIGHTNESS_CDEV_MAX 255

static DEFINE_MUTEX(sde_cdev_list_lock);
static LIST_HEAD(sde_cdev_list);

static int sde_cdev_get_max_brightness(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;

	*state = disp_cdev->bd->props.max_brightness / disp_cdev->cdev_sf;

	return 0;
}

static int sde_cdev_get_cur_brightness(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;

	*state = ((disp_cdev->bd->props.max_brightness -
			disp_cdev->thermal_state) / disp_cdev->cdev_sf);

	return 0;
}

static int sde_cdev_set_cur_brightness(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;
	unsigned long brightness_lvl = 0;
	unsigned long effective_state;

	if (state > disp_cdev->bd->props.max_brightness / disp_cdev->cdev_sf)
		return -EINVAL;

	mutex_lock(&disp_cdev->lock);

	/* Store daemon's requested state separately */
	disp_cdev->requested_state = state;

	if (vendor_thermal_backlight_override_active())
		effective_state = 0;
	else
		effective_state = state;

	brightness_lvl = disp_cdev->bd->props.max_brightness -
				(effective_state * disp_cdev->cdev_sf);
	if (brightness_lvl == disp_cdev->thermal_state) {
		mutex_unlock(&disp_cdev->lock);
		return 0;
	}
	disp_cdev->thermal_state = brightness_lvl;
	blocking_notifier_call_chain(&disp_cdev->notifier_head,
					brightness_lvl, (void *)disp_cdev->bd);
	mutex_unlock(&disp_cdev->lock);

	return 0;
}

static void backlight_cdev_thermal_override_reapply(void)
{
	struct sde_cdev *disp_cdev;
	unsigned long effective_state;
	unsigned long brightness_lvl;

	mutex_lock(&sde_cdev_list_lock);
	list_for_each_entry(disp_cdev, &sde_cdev_list, node) {
		mutex_lock(&disp_cdev->lock);
		if (disp_cdev->requested_state > disp_cdev->bd->props.max_brightness / disp_cdev->cdev_sf) {
			mutex_unlock(&disp_cdev->lock);
			continue;
		}

		if (vendor_thermal_backlight_override_active())
			effective_state = 0;
		else
			effective_state = disp_cdev->requested_state;

		if (effective_state <= disp_cdev->bd->props.max_brightness / disp_cdev->cdev_sf) {
			brightness_lvl = disp_cdev->bd->props.max_brightness -
						(effective_state * disp_cdev->cdev_sf);
			if (brightness_lvl != disp_cdev->thermal_state) {
				disp_cdev->thermal_state = brightness_lvl;
				blocking_notifier_call_chain(&disp_cdev->notifier_head,
						brightness_lvl, (void *)disp_cdev->bd);
			}
		}
		mutex_unlock(&disp_cdev->lock);
	}
	mutex_unlock(&sde_cdev_list_lock);
}

static struct thermal_cooling_device_ops sde_cdev_ops = {
	.get_max_state = sde_cdev_get_max_brightness,
	.get_cur_state = sde_cdev_get_cur_brightness,
	.set_cur_state = sde_cdev_set_cur_brightness,
};

struct sde_cdev *backlight_cdev_register(struct device *dev,
					struct backlight_device *bd,
					struct notifier_block *n)
{
	struct sde_cdev *disp_cdev = NULL;

	if (!dev || !bd || !n)
		return ERR_PTR(-EINVAL);
	if (!of_find_property(dev->of_node, "#cooling-cells", NULL))
		return ERR_PTR(-ENODEV);

	disp_cdev = devm_kzalloc(dev, sizeof(*disp_cdev), GFP_KERNEL);
	if (!disp_cdev)
		return ERR_PTR(-ENOMEM);
	mutex_init(&disp_cdev->lock);
	disp_cdev->thermal_state = 0;
	disp_cdev->bd = bd;

	if (bd->props.max_brightness > BRIGHTNESS_CDEV_MAX)
		disp_cdev->cdev_sf = (bd->props.max_brightness /
						BRIGHTNESS_CDEV_MAX);
	else
		disp_cdev->cdev_sf = 1;

	disp_cdev->cdev = thermal_of_cooling_device_register(dev->of_node,
				(char *)dev_name(&bd->dev), disp_cdev,
				&sde_cdev_ops);
	if (IS_ERR_OR_NULL(disp_cdev->cdev)) {
		pr_err("cooling device register failed\n");
		return (void *)disp_cdev->cdev;
	}
	BLOCKING_INIT_NOTIFIER_HEAD(&disp_cdev->notifier_head);
	blocking_notifier_chain_register(&disp_cdev->notifier_head, n);

	mutex_lock(&sde_cdev_list_lock);
	if (list_empty(&sde_cdev_list))
		vendor_thermal_register_backlight_reapply(backlight_cdev_thermal_override_reapply);
	list_add(&disp_cdev->node, &sde_cdev_list);
	mutex_unlock(&sde_cdev_list_lock);

	return disp_cdev;
}

void backlight_cdev_unregister(struct sde_cdev *cdev)
{
	if (!cdev)
		return;

	mutex_lock(&sde_cdev_list_lock);
	list_del(&cdev->node);
	if (list_empty(&sde_cdev_list))
		vendor_thermal_unregister_backlight_reapply();
	mutex_unlock(&sde_cdev_list_lock);

	thermal_cooling_device_unregister(cdev->cdev);
}
