/*
 * Voltage Battery Driver
 * Copyright (C) 2013  Rudolf Tammekivi <rtammekivi@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#define DEBUG 1

#define STANDARD_UPDATE_MS (60 * 1000)
#define UNRELIABLE_UPDATE_MS (90 * 1000)

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/power/voltage_battery.h>

struct voltage_bat_data {
	const struct voltage_battery_platform_data *pdata;
	struct device *dev;
	struct voltage_battery_callbacks callbacks;
	struct delayed_work monitor_work;

	struct {
		uint8_t stable;
		uint8_t relative;
		uint8_t direct;
	} capacity;

	struct {
		bool unreliable;
		bool use_relative;
		int8_t diff_relative;
	} monitor;
	ktime_t last_work;
	bool charging;
};

static void voltage_bat_update(struct voltage_bat_data *data);

static uint8_t voltage_bat_calculate_capacity(struct voltage_bat_data *data,
	uint32_t voltage)
{
	uint32_t voltage_low = data->pdata->voltage_low;
	uint32_t voltage_high = data->pdata->voltage_high;

	if (voltage <= voltage_low)
		return 0;
	else if (voltage >= voltage_high)
		return 100;
	else
		return (voltage - voltage_low) * 100 /
			(voltage_high - voltage_low);
}

static uint8_t voltage_bat_get_direct_capacity(struct voltage_bat_data *data)
{
	uint32_t voltage = data->pdata->get_voltage();
	return voltage_bat_calculate_capacity(data, voltage);
}

static uint8_t voltage_bat_cb_get_capacity(
	struct voltage_battery_callbacks *callbacks)
{
	struct voltage_bat_data *data = container_of(callbacks,
		struct voltage_bat_data, callbacks);

	if (ktime_us_delta(ktime_get_real(), data->last_work) >
		(STANDARD_UPDATE_MS * USEC_PER_MSEC) + USEC_PER_SEC &&
		!data->monitor.unreliable) {
		dev_vdbg(data->dev, "manual update triggered\n");
		voltage_bat_update(data);
	}
	return data->capacity.relative;
}

static void voltage_bat_cb_unreliable_update(
	struct voltage_battery_callbacks *callbacks, bool unreliable)
{
	struct voltage_bat_data *data = container_of(callbacks,
		struct voltage_bat_data, callbacks);

	data->monitor.unreliable = true;
	data->monitor.use_relative = unreliable;

	cancel_delayed_work(&data->monitor_work);
	schedule_delayed_work(&data->monitor_work,
		msecs_to_jiffies(UNRELIABLE_UPDATE_MS));
}

static void voltage_bat_cb_set_charging(
	struct voltage_battery_callbacks *callbacks, bool charging)
{
	struct voltage_bat_data *data = container_of(callbacks,
		struct voltage_bat_data, callbacks);

	data->charging = charging;
}

static void voltage_bat_update(struct voltage_bat_data *data)
{
	uint8_t direct = voltage_bat_get_direct_capacity(data);

	int8_t diff_direct = 0, diff_relative = 0;

	if (data->monitor.unreliable)
		goto norelative;

	diff_direct = direct - data->capacity.direct;

	data->capacity.relative += diff_direct;

	if (!data->monitor.use_relative)
		data->capacity.relative = direct;

	if (data->capacity.relative > 100 || direct == 100)
		data->capacity.relative = 100;
	else if (data->capacity.relative < 0 || direct == 0)
		data->capacity.relative = 0;

norelative:
	diff_relative = data->capacity.relative - data->capacity.stable;

	if (data->capacity.stable > 0 && diff_relative < 0/* && !data->charging*/)
		data->capacity.stable--;
	else if (data->capacity.stable < 100 && diff_relative > 0/* && data->charging*/)
		data->capacity.stable++;

	dev_vdbg(data->dev,
		"Capacity: %d Relative: %d LastDirect: %d "
		"Direct: %d Unreliable: %d Use Relative: %d Charging %d\n",
		data->capacity.stable,
		data->capacity.relative,
		data->capacity.direct,
		direct,
		data->monitor.unreliable,
		data->monitor.use_relative,
		data->charging);

	data->capacity.direct = direct;
	data->monitor.diff_relative = diff_relative;
	data->monitor.unreliable = false;
	data->last_work = ktime_get_real();
}

static void voltage_bat_monitor_work(struct work_struct *work)
{
	struct voltage_bat_data *data = container_of(to_delayed_work(work),
		struct voltage_bat_data, monitor_work);

	voltage_bat_update(data);

	schedule_delayed_work(&data->monitor_work,
		msecs_to_jiffies(STANDARD_UPDATE_MS));
}

static void voltage_bat_set_defaults(struct voltage_bat_data *data)
{
	uint8_t direct_capacity = voltage_bat_get_direct_capacity(data);

	data->capacity.stable = direct_capacity;
	data->capacity.relative = direct_capacity;
	data->capacity.direct = direct_capacity;
	data->monitor.unreliable = false;
	data->monitor.use_relative = false;
}

static int voltage_bat_probe(struct platform_device *pdev)
{
	struct voltage_battery_platform_data *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct voltage_bat_data *data;
	int ret = 0;

	if (pdev->id != -1) {
		dev_err(dev, "multiple devices not supported\n");
		goto err_exit;
	}

	if (!pdata) {
		dev_err(dev, "platform data not supplied\n");
		ret = -EINVAL;
		goto err_exit;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	platform_set_drvdata(pdev, data);

	data->pdata = pdata;
	data->dev = dev;

	/* Register callbacks. */
	data->callbacks.get_capacity = voltage_bat_cb_get_capacity;
	data->callbacks.unreliable_update = voltage_bat_cb_unreliable_update;
	data->callbacks.set_charging = voltage_bat_cb_set_charging;
	if (pdata->register_callbacks)
		pdata->register_callbacks(&data->callbacks);

	voltage_bat_set_defaults(data);

	INIT_DELAYED_WORK(&data->monitor_work, voltage_bat_monitor_work);

	schedule_delayed_work(&data->monitor_work,
		msecs_to_jiffies(STANDARD_UPDATE_MS));

	return 0;

err_exit:
	return ret;
}

static int voltage_bat_remove(struct platform_device *pdev)
{
	struct voltage_bat_data *data = platform_get_drvdata(pdev);
	const struct voltage_battery_platform_data *pdata = data->pdata;

	if (pdata->unregister_callbacks)
		pdata->unregister_callbacks();
	data->callbacks.get_capacity = NULL;

	kfree(data);

	return 0;
}

static int voltage_bat_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct voltage_bat_data *data = platform_get_drvdata(pdev);

	data->monitor.unreliable = false;
	cancel_delayed_work(&data->monitor_work);

	return 0;
}

static int voltage_bat_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver voltage_bat_driver = {
	.probe		= voltage_bat_probe,
	.remove		= voltage_bat_remove,
	.suspend	= voltage_bat_suspend,
	.resume		= voltage_bat_resume,
	.driver		= {
		.name	= "voltage_battery",
		.owner	= THIS_MODULE,
	},
};

static int __init voltage_bat_init(void)
{
	return platform_driver_register(&voltage_bat_driver);
}

static void __exit voltage_bat_exit(void)
{
	platform_driver_unregister(&voltage_bat_driver);
}

device_initcall_sync(voltage_bat_init);
module_exit(voltage_bat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rudolf Tammekivi <rtammekivi@gmail.com>");
MODULE_DESCRIPTION("Generic Voltage Battery");
MODULE_VERSION("1.0.0");
