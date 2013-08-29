/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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

#include <linux/msm_adc.h>
#include <linux/platform_device.h>
#include <linux/platform_data/android_battery.h>
#include <linux/power/bq2415x_charger.h>
#include <linux/wakelock.h>
#include <mach/msm_hsusb.h>

#include "board-u8800.h"

#define VBAT_ADC_CHANNEL	1
#define BATT_TEMP_CHANNEL	5 /* MPP_07 */

#define BATTERY_LOW_UVOLTS	3400000
#define BATTERY_HIGH_UVOLTS	4200000

static struct wake_lock charger_wakelock;

static int batt_read_adc(int32_t channel, int32_t *reading)
{
	int ret = 0;
	struct adc_chan_result chan_result;

	ret = adc_rpc_read_result(channel, &chan_result);
	if (!ret)
		*reading = chan_result.physical;

	return ret;
}

static int batt_get_voltage(void)
{
	int32_t voltage = 0;
	batt_read_adc(VBAT_ADC_CHANNEL, &voltage);
	return voltage * 1000;
}

static int batt_get_temperature(void)
{
	int32_t temp = 0;
	batt_read_adc(BATT_TEMP_CHANNEL, &temp);
	if (temp == 80) /* TODO: Fix. */
		temp = 35;
	return temp * 10;
}

static int batt_get_capacity(void)
{
	int32_t voltage = batt_get_voltage();

	if (voltage <= BATTERY_LOW_UVOLTS)
		return 0;
	else if (voltage >= BATTERY_HIGH_UVOLTS)
		return 100;
	else
		return (voltage - BATTERY_LOW_UVOLTS) * 100 /
			(BATTERY_HIGH_UVOLTS - BATTERY_LOW_UVOLTS);
}

static struct android_bat_callbacks *android_bat_cb;

static int chg_source;

void batt_chg_connected(enum chg_type chg_type)
{
	int new_chg_source;

	switch (chg_type) {
	case USB_CHG_TYPE__SDP:
	case USB_CHG_TYPE__CARKIT:
		new_chg_source = CHARGE_SOURCE_USB;
		break;
	case USB_CHG_TYPE__WALLCHARGER:
		new_chg_source = CHARGE_SOURCE_AC;
		break;
	default:
		new_chg_source = CHARGE_SOURCE_NONE;
		break;
	}

	if (!android_bat_cb || chg_source == new_chg_source)
		return;

	android_bat_cb->charge_source_changed(android_bat_cb, new_chg_source);
	chg_source = new_chg_source;
}

static void android_bat_register_callbacks(
	struct android_bat_callbacks *callbacks)
{
	android_bat_cb = callbacks;
	wake_lock_init(&charger_wakelock, WAKE_LOCK_SUSPEND, "charger");
}

static void android_bat_unregister_callbacks(void)
{
	android_bat_cb = NULL;
	wake_lock_destroy(&charger_wakelock);
}

static void android_bat_set_charging_current(int type)
{
	enum bq2415x_mode new_mode = BQ2415X_MODE_NONE;

	switch (type) {
	case CHARGE_SOURCE_AC:
		new_mode = BQ2415X_MODE_DEDICATED_CHARGER;
		break;
	case CHARGE_SOURCE_USB:
		new_mode = BQ2415X_MODE_HOST_CHARGER;
		break;
	default:
		return;
	}

	bq24152_hook(new_mode, bq24152_data);
}

static void android_bat_set_charging_enable(int enable)
{
	if (enable)
		wake_lock(&charger_wakelock);
	else {
		bq24152_hook(BQ2415X_MODE_OFF, bq24152_data);
		wake_unlock(&charger_wakelock);
	}
}

static int android_bat_poll_charge_source(void)
{
	/* No need to poll, just return last state. */
	return chg_source;
}

static int android_bat_get_capacity(void)
{
	return batt_get_capacity();
}

static int android_bat_get_temperature(int *t)
{
	*t = batt_get_temperature();
	return 0;
}

static int android_bat_get_voltage_now(void)
{
	return batt_get_voltage();
}

static struct android_bat_platform_data android_bat_pdata = {
	.register_callbacks	= android_bat_register_callbacks,
	.unregister_callbacks	= android_bat_unregister_callbacks,
	.set_charging_current	= android_bat_set_charging_current,
	.set_charging_enable	= android_bat_set_charging_enable,
	.poll_charge_source	= android_bat_poll_charge_source,
	.get_capacity		= android_bat_get_capacity,
	.get_temperature	= android_bat_get_temperature,
	.get_voltage_now	= android_bat_get_voltage_now,

	.temp_high_threshold	= 450,
	.temp_high_recovery	= 400,
	.temp_low_recovery	= 50,
	.temp_low_threshold	= 0,
};

struct platform_device android_bat_device = {
	.name	= "android-battery",
	.id	= 0,
	.dev	= {
		.platform_data = &android_bat_pdata,
	},
};
