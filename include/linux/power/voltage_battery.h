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

struct voltage_battery_callbacks {
	uint8_t (*get_capacity)(struct voltage_battery_callbacks *callbacks);
	void (*unreliable_update)(struct voltage_battery_callbacks *callbacks,
		bool unreliable);
	void (*set_charging)(struct voltage_battery_callbacks *callbacks,
		bool charging);
};

struct voltage_battery_platform_data {
	/* Battery min/max voltage. */
	uint32_t voltage_low;
	uint32_t voltage_high;

	/* Discharge/Charge mappings. */
	uint8_t *discharge_map;
	uint8_t *charge_map;

	void (*register_callbacks)(struct voltage_battery_callbacks *callbacks);
	void (*unregister_callbacks)(void);

	uint32_t (*get_voltage)(void);
};

#endif /* __VOLTAGE_BATTERY_H__ */
