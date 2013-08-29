/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_MACH_MSM_BOARD_U8800_H__
#define __ARCH_ARM_MACH_MSM_BOARD_U8800_H__

#include "proccomm-regulator.h"

/* board-u8800-regulator.c */
extern struct proccomm_regulator_platform_data msm7x30_proccomm_regulator_data;

/* board-u8800-battery.c */
extern struct platform_device android_bat_device;
enum chg_type;
void batt_chg_connected(enum chg_type chg_type);
enum bq2415x_mode;
extern void (*bq24152_hook)(enum bq2415x_mode mode, void *data);
extern void *bq24152_data;

#endif
