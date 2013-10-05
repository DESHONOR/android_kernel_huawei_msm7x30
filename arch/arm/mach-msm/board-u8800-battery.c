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

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/platform_data/android_battery.h>
#include <linux/power/bq2415x_charger.h>
#include <linux/wakelock.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_rpcrouter.h>

#include "board-u8800.h"

#define BATTERY_LOW_UVOLTS	3400000
#define BATTERY_HIGH_UVOLTS	4200000

#define CHG_RPC_PROG				0x3000001a
#define CHG_RPC_VER_4_1				0x00040001
#define ONCRPC_CHG_GET_GENERAL_STATUS_PROC	12

/* The modem may be busy, let's ask for new information and wait for 5 seconds
 * before asking for new info. */
#define CHG_RPC_PULL_INTERVAL_NS		5000000000

static struct wake_lock charger_wakelock;
static struct msm_rpc_endpoint *chg_ep;

enum rpc_information_type {
	RPC_CHARGER_STATUS,
	RPC_CHARGER_TYPE,
	RPC_CHARGER_VALID,
	RPC_CHARGER_CHARGING,

	RPC_BATTERY_STATUS,
	RPC_BATTERY_LEVEL,
	RPC_BATTERY_VOLTAGE,
	RPC_BATTERY_TEMP,
	RPC_BATTERY_VALID,

	RPC_UI_EVENT
};

static int32_t rpc_get_information(enum rpc_information_type type)
{
	int32_t ret;

	struct rpc_req_batt_chg {
		struct rpc_request_hdr hdr;
		u32 more_data;
	} req_batt_chg;
	struct rpc_rep_batt_chg {
		struct rpc_reply_hdr hdr;
		u32 more_data;

		u32 charger_status;
		u32 charger_type;
		u32 battery_status;
		u32 battery_level;
		u32 battery_voltage;
		u32 battery_temp;

		u32 charger_valid;
		u32 charger_charging;
		u32 battery_valid;
		u32 ui_event;
	} rep_batt_chg;

	static struct rpc_rep_batt_chg last_rep_batt_chg;
	static ktime_t last_poll;

	memset(&req_batt_chg, 0, sizeof(req_batt_chg));
	req_batt_chg.more_data = cpu_to_be32(1);

	if (!chg_ep)
		return -ENODEV;

	/* Pull only if time has expired. */
	if ((ktime_to_ns(last_poll) + CHG_RPC_PULL_INTERVAL_NS)
		< ktime_to_ns(ktime_get_real())) {
		memset(&rep_batt_chg, 0, sizeof(rep_batt_chg));
		ret = msm_rpc_call_reply(chg_ep,
			ONCRPC_CHG_GET_GENERAL_STATUS_PROC,
			&req_batt_chg, sizeof(req_batt_chg),
			&rep_batt_chg, sizeof(rep_batt_chg),
			msecs_to_jiffies(1000));
		if (rep_batt_chg.more_data)
			last_rep_batt_chg = rep_batt_chg;
		last_poll = ktime_get_real();
	} else
		ret = 0;


	if (ret < 0)
		return ret;

	switch (type) {
	case RPC_CHARGER_STATUS:
		ret = last_rep_batt_chg.charger_status; break;
	case RPC_CHARGER_TYPE:
		ret = last_rep_batt_chg.charger_type; break;
	case RPC_CHARGER_VALID:
		ret = last_rep_batt_chg.charger_valid; break;
	case RPC_CHARGER_CHARGING:
		ret = last_rep_batt_chg.charger_charging; break;
	case RPC_BATTERY_STATUS:
		ret = last_rep_batt_chg.battery_status; break;
	case RPC_BATTERY_LEVEL:
		ret = last_rep_batt_chg.battery_level; break;
	case RPC_BATTERY_VOLTAGE:
		ret = last_rep_batt_chg.battery_voltage; break;
	case RPC_BATTERY_TEMP:
		ret = last_rep_batt_chg.battery_temp; break;
	case RPC_BATTERY_VALID:
		ret = last_rep_batt_chg.battery_valid; break;
	case RPC_UI_EVENT:
		ret = last_rep_batt_chg.ui_event; break;
	default:
		return -1;
	}

	return be32_to_cpu(ret);
}

static int rpc_initialize(bool init)
{
	if (init) {
		chg_ep = msm_rpc_connect_compatible(CHG_RPC_PROG, CHG_RPC_VER_4_1, 0);
		if (IS_ERR(chg_ep))
			return PTR_ERR(chg_ep);
	} else {
		msm_rpc_close(chg_ep);
		chg_ep = NULL;
	}

	return 0;
}

static int batt_get_voltage(void)
{
	return rpc_get_information(RPC_BATTERY_VOLTAGE) * 1000;
}

static int batt_get_temperature(void)
{
	return rpc_get_information(RPC_BATTERY_TEMP) * 10;
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
static struct bq2415x_callbacks *bq24152_callbacks;

static void bq24152_set_mode(enum bq2415x_mode mode)
{
	switch (mode) {
	case BQ2415X_MODE_OFF:
		wake_unlock(&charger_wakelock);
		break;
	case BQ2415X_MODE_CHARGE:
	case BQ2415X_MODE_BOOST:
		wake_lock(&charger_wakelock);
		break;
	}

	if (bq24152_callbacks)
		bq24152_callbacks->set_mode(bq24152_callbacks, mode);
}

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

	if (android_bat_cb)
		android_bat_cb->charge_source_changed(android_bat_cb, new_chg_source);

	chg_source = new_chg_source;
}

void batt_vbus_power(unsigned phy_info, int on)
{
	enum bq2415x_mode new_mode;

	if (on)
		new_mode = BQ2415X_MODE_BOOST;
	else
		new_mode = BQ2415X_MODE_OFF;

	bq24152_set_mode(new_mode);
}

static void android_bat_register_callbacks(
	struct android_bat_callbacks *callbacks)
{
	android_bat_cb = callbacks;
}

static void android_bat_unregister_callbacks(void)
{
	android_bat_cb = NULL;
}

static void android_bat_set_charging_current(int type)
{
	int mA = 0;

	switch (type) {
	case CHARGE_SOURCE_AC:
		mA = 1800;
		break;
	case CHARGE_SOURCE_USB:
		mA = 500;
		break;
	default:
		return;
	}

	if (bq24152_callbacks)
		bq24152_callbacks->set_current_limit(bq24152_callbacks, mA);
}

static void android_bat_set_charging_enable(int enable)
{
	enum bq2415x_mode new_mode;

	if (enable)
		new_mode = BQ2415X_MODE_CHARGE;
	else
		new_mode = BQ2415X_MODE_OFF;

	bq24152_set_mode(new_mode);
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

static void bq24152_status_changed(enum bq2415x_status status)
{
	if (status == BQ2415X_STATUS_CHARGE_DONE) {
		if (android_bat_cb)
			android_bat_cb->battery_set_full(android_bat_cb);
	}
}

static void bq24152_register_callbacks(struct bq2415x_callbacks *callbacks)
{
	bq24152_callbacks = callbacks;
}

static void bq24152_unregister_callbacks(void)
{
	bq24152_callbacks = NULL;
}

static struct bq2415x_platform_data bq24152_platform_data = {
	.current_limit = 100, /* mA */
	.weak_battery_voltage = 3400, /* mV */
	.battery_regulation_voltage = 4200, /* mV */
	.charge_current = 950, /* mA */
	.termination_current = 100, /* mA */
	.resistor_sense = 68, /* m ohm */
	.status_changed = bq24152_status_changed,
	.register_callbacks = bq24152_register_callbacks,
	.unregister_callbacks = bq24152_unregister_callbacks,
};

struct i2c_board_info bq24152_device = {
	I2C_BOARD_INFO("bq24152", 0x6b),
	.platform_data = &bq24152_platform_data,
};

static int __init battery_init(void)
{
	wake_lock_init(&charger_wakelock, WAKE_LOCK_SUSPEND, "charger");

	rpc_initialize(true);

	return 0;
}

static void __exit battery_exit(void)
{
	wake_lock_destroy(&charger_wakelock);

	rpc_initialize(false);
}

module_init(battery_init);
module_exit(battery_exit);
