/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include "devices.h"

static struct gpiomux_setting ts_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ts_resout_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm7x30_ts_configs[] __initdata = {
	{	/* TOUCH_INT */
		.gpio = 148,
		.settings = {
			[GPIOMUX_ACTIVE]    = &ts_int_act_cfg,
		},
	},
	{	/* TOUCH_RESET */
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &ts_resout_act_cfg,
		},
	},
};

static struct gpiomux_setting i2c_dcdc_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm7x30_i2c_dcdc_configs[] __initdata = {
	{	/* I2C_SCL_DCDC */
		.gpio = 149,
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2c_dcdc_act_cfg,
		},
	},
	{	/* I2C_SDA_DCDC */
		.gpio = 150,
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2c_dcdc_act_cfg,
		},
	},
};

int __init msm7x30_init_gpiomux(void)
{
	int rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}

	msm_gpiomux_install(msm7x30_ts_configs,
			ARRAY_SIZE(msm7x30_ts_configs));

	msm_gpiomux_install(msm7x30_i2c_dcdc_configs,
			ARRAY_SIZE(msm7x30_i2c_dcdc_configs));

	return rc;
}

postcore_initcall(msm7x30_init_gpiomux);
