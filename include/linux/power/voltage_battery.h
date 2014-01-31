/*
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

#ifndef __VOLTAGE_BATTERY_H__
#define __VOLTAGE_BATTERY_H__

#include <linux/types.h>

struct voltage_mapping {
	/* Range 0-100 ordered from top to down. */
	uint8_t capacity;
	/* OCV voltage. */
	int32_t voltage;
};

struct voltage_battery_callbacks {
	uint8_t (*get_capacity)(struct voltage_battery_callbacks *callbacks);
	void (*unreliable_update)(struct voltage_battery_callbacks *callbacks,
		bool unreliable);
	void (*set_charging)(struct voltage_battery_callbacks *callbacks,
		bool charging);
};

struct voltage_battery_platform_data {
	/* Battery min/max voltage. */
	int32_t voltage_low;
	int32_t voltage_high;

	/* Discharge/Charge mappings. */
	const struct voltage_mapping *discharge_map;
	const struct voltage_mapping *charge_map;
	int discharge_map_size;
	int charge_map_size;

	void (*register_callbacks)(struct voltage_battery_callbacks *callbacks);
	void (*unregister_callbacks)(void);

	uint32_t (*get_voltage)(void);
};

#endif /* __VOLTAGE_BATTERY_H__ */
