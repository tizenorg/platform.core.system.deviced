/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __POWER_SUPPLY_H__
#define __POWER_SUPPLY_H__

enum device_change_type {
	DEVICE_CHANGE_ABNORMAL	= 0,
	DEVICE_CHANGE_NORMAL	= 1,
};

enum charge_full_type {
	CHARGING_NOT_FULL	= 0,
	CHARGING_FULL		= 1,
};
enum charge_now_type {
	CHARGER_ABNORMAL	= -1,
	CHARGER_DISCHARGING	= 0,
	CHARGER_CHARGING	= 1,
};
enum health_type {
	HEALTH_BAD		= 0,
	HEALTH_GOOD		= 1,
};

enum temp_type {
	TEMP_LOW		= 0,
	TEMP_HIGH		= 1,
};

enum present_type {
	PRESENT_ABNORMAL	= 0,
	PRESENT_NORMAL		= 1,
};

enum ovp_type {
	OVP_NORMAL		= 0,
	OVP_ABNORMAL		= 1,
};

enum battery_noti_type {
	DEVICE_NOTI_BATT_CHARGE = 0,
	DEVICE_NOTI_BATT_LOW,
	DEVICE_NOTI_BATT_FULL,
	DEVICE_NOTI_MAX,
};

enum battery_noti_status {
	DEVICE_NOTI_OFF = 0,
	DEVICE_NOTI_ON  = 1,
};

struct battery_status {
	int capacity;
	int charge_full;
	int charge_now;
	int health;
	int present;
	int online;
	int temp;
	int ovp;
};

extern struct battery_status battery;

void power_supply_broadcast(char *sig, int status);

/* Battery functions */
void lowbat_monitor(void *data);

#define CHARGER_STATUS_SIGNAL      "ChargerStatus"
#define CHARGE_NOW_SIGNAL          "ChargeNow"
#define CHARGE_LEVEL_SIGNAL        "BatteryStatusLow"
#define CHARGE_CAPACITY_SIGNAL     "GetPercent"
#define CHARGE_CAPACITY_LAW_SIGNAL "GetPercentRaw"
#define CHARGE_HEALTH_SIGNAL       "GetHealth"
#define CHARGE_FULL_SIGNAL         "IsFull"

#endif /* __POWER_SUPPLY_H__ */
