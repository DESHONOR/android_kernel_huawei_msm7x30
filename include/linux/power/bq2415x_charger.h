/*
 * bq2415x charger driver
 *
 * Copyright (C) 2011-2012  Pali Rohár <pali.rohar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef BQ2415X_CHARGER_H
#define BQ2415X_CHARGER_H

/*
 * This is platform data for bq2415x chip. It contains default board
 * voltages and currents which can be also later configured via sysfs. If
 * value is -1 then default chip value (specified in datasheet) will be
 * used.
 *
 * Value resistor_sense is needed for for configuring charge and
 * termination current. It it is less or equal to zero, configuring charge
 * and termination current will not be possible.
 *
 * Callback register/unregister is used to modify the charge current & mode,
 * as described in bq2415x_mode.
 *
 */

enum bq2415x_status {
	BQ2415X_STATUS_READY,		/* ready */
	BQ2415X_STATUS_CHARGING,	/* charge in progress */
	BQ2415X_STATUS_CHARGE_DONE,	/* charge done */
	BQ2415X_STATUS_FAULT,		/* fault */
};

enum bq2415x_mode {
	BQ2415X_MODE_OFF,
	BQ2415X_MODE_CHARGE,
	BQ2415X_MODE_BOOST,
};

struct bq2415x_callbacks {
	void (*set_current_limit)(struct bq2415x_callbacks *ptr, int mA);
	void (*set_mode)
		(struct bq2415x_callbacks *ptr, enum bq2415x_mode mode);
};

struct bq2415x_platform_data {
	int current_limit;		/* mA */
	int weak_battery_voltage;	/* mV */
	int battery_regulation_voltage;	/* mV */
	int charge_current;		/* mA */
	int termination_current;	/* mA */
	int resistor_sense;		/* m ohm */
	void (*status_changed)(enum bq2415x_status status);
	void (*register_callbacks)(struct bq2415x_callbacks *ptr);
	void (*unregister_callbacks)(void);
};

#endif
