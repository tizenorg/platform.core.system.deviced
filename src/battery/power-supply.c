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


#include <stdio.h>
#include <stdbool.h>
#include <vconf.h>
#include <Ecore.h>
#include <device-node.h>
#include <bundle.h>
#include <eventsystem.h>
#include <hw/battery.h>

#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/udev.h"
#include "core/log.h"
#include "core/config-parser.h"
#include "display/poll.h"
#include "display/setting.h"
#include "apps/apps.h"
#include "power-supply.h"
#include "battery.h"

#define BATTERY_NAME        "battery"
#define CHARGEFULL_NAME     "Full"
#define CHARGENOW_NAME      "Charging"
#define DISCHARGE_NAME      "Discharging"
#define NOTCHARGE_NAME      "Not charging"
#define OVERHEAT_NAME       "Overheat"
#define TEMPCOLD_NAME       "Cold"
#define OVERVOLT_NAME       "Over voltage"

#define BUFF_MAX            255

#define SIGNAL_CHARGEERR_RESPONSE "ChargeErrResponse"
#define SIGNAL_TEMP_GOOD          "TempGood"

#define ABNORMAL_CHECK_TIMER_INTERVAL 60

#define METHOD_FULL_NOTI_ON   "BatteryFullNotiOn"
#define METHOD_FULL_NOTI_OFF  "BatteryFullNotiOff"
#define METHOD_CHARGE_NOTI_ON "BatteryChargeNotiOn"

#define RETRY_MAX 5
#define BATTERY_CHECK_TIMER_INTERVAL (0.5)

enum power_supply_init_type {
	POWER_SUPPLY_NOT_READY   = 0,
	POWER_SUPPLY_INITIALIZED = 1,
};

static void uevent_power_handler(struct udev_device *dev);
static const struct uevent_handler uh = {
	.subsystem   = POWER_SUBSYSTEM,
	.uevent_func = uevent_power_handler,
};

struct battery_status battery;
static int noti_id;
static Ecore_Timer *power_timer;
static Ecore_Timer *abnormal_timer;

static int booting_done(void *data);

static struct battery_device *battery_dev;

static void lowbat_execute(void *data)
{
	static const struct device_ops *lowbat_ops;

	FIND_DEVICE_VOID(lowbat_ops, "lowbat");
	device_execute(lowbat_ops, data);
}

static void pm_check_and_change(int bInserted)
{
	static int old = -1;

	if (old == bInserted)
		return;
	old = bInserted;
	pm_change_internal(getpid(), LCD_NORMAL);
}

static int changed_battery_cf(enum present_type status)
{
	char *value;

	if (status == PRESENT_ABNORMAL)
		value = "battdisconnect";
	else
		value = "remove_battery_popups";

	return launch_system_app(APP_DEFAULT,
			2, APP_KEY_TYPE, value);
}

static void abnormal_popup_timer_init(void)
{
	if (abnormal_timer == NULL)
		return;
	ecore_timer_del(abnormal_timer);
	abnormal_timer = NULL;
	_I("delete health timer");
}

static void health_status_broadcast(void)
{
	broadcast_edbus_signal(DEVICED_PATH_BATTERY, DEVICED_INTERFACE_BATTERY,
	    SIGNAL_TEMP_GOOD, NULL, NULL);
}


static void health_timer_reset(void)
{
	abnormal_timer = NULL;
}

static Eina_Bool health_timer_cb(void *data)
{
	health_timer_reset();

	if (battery.health == HEALTH_GOOD)
		return EINA_FALSE;

	_I("popup - Battery health status is not good");
	device_notify(DEVICE_NOTIFIER_BATTERY_HEALTH, (void *)HEALTH_BAD);
	pm_change_internal(getpid(), LCD_NORMAL);
	pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);
	if (battery.temp == TEMP_LOW)
		battery_charge_err_low_act(NULL);
	else if (battery.temp == TEMP_HIGH)
		battery_charge_err_high_act(NULL);
	return EINA_FALSE;
}

static void abnormal_popup_edbus_signal_handler(void *data, DBusMessage *msg)
{
	if (battery.health == HEALTH_GOOD)
		return;
	_I("restart health timer");
	abnormal_timer = ecore_timer_add(ABNORMAL_CHECK_TIMER_INTERVAL,
						health_timer_cb, NULL);
	if (abnormal_timer == NULL)
		_E("Fail to add abnormal check timer");
}

static void full_noti_cb(void *data, DBusMessage *msg, DBusError *err)
{
	DBusError r_err;
	int ret, id;

	if (!msg)
		return;

	dbus_error_init(&r_err);
	ret = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &id, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		return;
	}

	noti_id = id;
	_D("Inserted battery full noti : %d", noti_id);
}

static int check_power_supply_noti(void)
{
#ifdef MICRO_DD
	int r_disturb, s_disturb, r_block, s_block;
	r_disturb = vconf_get_int("memory/shealth/sleep/do_not_disturb", &s_disturb);
	r_block = vconf_get_bool("db/setting/blockmode_wearable", &s_block);
	if ((r_disturb != 0 && r_block != 0) ||
	    (s_disturb == 0 && s_block == 0)) {
	    return 1;
	}
	return 0;
#else
	return 1;
#endif
}

static int send_full_noti(enum charge_full_type state)
{
	int ret = 0;
	int noti;
	int retry;
	char str_id[32];
	char *arr[1];

	noti = check_power_supply_noti();

	if (!noti)
		return noti;

	switch (state) {
	case CHARGING_FULL:
		for (retry = RETRY_MAX; retry > 0; retry--) {
			ret = dbus_method_async_with_reply(POPUP_BUS_NAME,
					POPUP_PATH_BATTERY,
					POPUP_INTERFACE_BATTERY,
					METHOD_FULL_NOTI_ON,
					NULL, NULL, full_noti_cb, -1, NULL);
			if (ret == 0) {
				_D("Created battery full noti");
				return ret;
			}
		}
		_E("Failed to call dbus method (err: %d)", ret);
	break;
	case CHARGING_NOT_FULL:
		if (noti_id <= 0)
			return -EPERM;
		snprintf(str_id, sizeof(str_id), "%d", noti_id);
		arr[0] = str_id;
		for (retry = RETRY_MAX; retry > 0; retry--) {
			ret = dbus_method_async(POPUP_BUS_NAME,
					POPUP_PATH_BATTERY,
					POPUP_INTERFACE_BATTERY,
					METHOD_FULL_NOTI_OFF,
					"i", arr);
			if (ret == 0) {
				_D("Deleted battery full noti");
				noti_id = 0;
				return ret;
			}
		}
		_E("Failed to call dbus method (err: %d)", ret);
	break;
	}
	return ret;
}

static int send_charge_noti(void)
{
	int ret = 0;
	int retry;

	for (retry = RETRY_MAX; retry > 0; retry--) {
		ret = dbus_method_async(POPUP_BUS_NAME,
				POPUP_PATH_BATTERY,
				POPUP_INTERFACE_BATTERY,
				METHOD_CHARGE_NOTI_ON,
				NULL, NULL);
		if (ret == 0) {
			_D("Created battery charge noti");
			return ret;
		}
	}
	_E("Failed to call dbus method (err: %d)", ret);
	return ret;
}

static void power_supply_noti(enum battery_noti_type type, enum battery_noti_status status)
{
	static int charger = CHARGER_DISCHARGING;
	static int full = CHARGING_NOT_FULL;
	int ret;

	if (type == DEVICE_NOTI_BATT_CHARGE) {
		if (status == DEVICE_NOTI_ON && charger == CHARGER_DISCHARGING) {
			send_charge_noti();
			charger = CHARGER_CHARGING;
		} else if (status == DEVICE_NOTI_OFF && charger == CHARGER_CHARGING) {
			charger = CHARGER_DISCHARGING;
		}
	} else if (type == DEVICE_NOTI_BATT_FULL) {
		if (status == DEVICE_NOTI_ON && full == CHARGING_NOT_FULL) {
			ret = send_full_noti(CHARGING_FULL);
			if (ret == 0)
				full = CHARGING_FULL;
		} else if (status == DEVICE_NOTI_OFF && full == CHARGING_FULL) {
			ret = send_full_noti(CHARGING_NOT_FULL);
			if (ret == 0)
				full = CHARGING_NOT_FULL;
		}
	}
}

void power_supply_broadcast(char *sig, int status)
{
	static int old;
	static char sig_old[32];
	char *arr[1];
	char str_status[32];

	if (strcmp(sig_old, sig) == 0 && old == status)
		return;

	_D("%s %d", sig, status);

	old = status;
	snprintf(sig_old, sizeof(sig_old), "%s", sig);
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_BATTERY, DEVICED_INTERFACE_BATTERY,
			sig, "i", arr);
}

static void noti_batt_full(void)
{
	static int bat_full_noti;
	int noti;

	if (!battery.charge_full && bat_full_noti == 1) {
		power_supply_noti(DEVICE_NOTI_BATT_FULL, DEVICE_NOTI_OFF);
		bat_full_noti = 0;
		/* off the full charge state */
		device_notify(DEVICE_NOTIFIER_FULLBAT, (void *)false);
	}
	if (battery.charge_full && bat_full_noti == 0) {
		power_supply_noti(DEVICE_NOTI_BATT_FULL, DEVICE_NOTI_ON);
		bat_full_noti = 1;
		/* turn on LCD, if battery is full charged */
		noti = check_power_supply_noti();
		if (noti)
			pm_change_internal(INTERNAL_LOCK_BATTERY_FULL,
				LCD_NORMAL);
		else
			_I("block LCD");
		/* on the full charge state */
		device_notify(DEVICE_NOTIFIER_FULLBAT, (void *)true);
	}
}

static void check_power_supply(int state)
{
	pm_check_and_change(state);
	if (update_pm_setting)
		update_pm_setting(SETTING_CHARGING, state);
}

static void charger_state_send_system_event(int state)
{
	bundle *b;
	const char *str;

	switch (state) {
	case CHARGE_STATUS_CHARGING:
		str = EVT_VAL_BATTERY_CHARGER_CHARGING;
		break;
	case CHARGE_STATUS_FULL:
	case CHARGE_STATUS_DISCHARGING:
		str = EVT_VAL_BATTERY_CHARGER_DISCHARGING;
		break;
	case CHARGE_STATUS_CONNECTED:
		str = EVT_VAL_BATTERY_CHARGER_CONNECTED;
		break;
	case CHARGE_STATUS_DISCONNECTED:
		str = EVT_VAL_BATTERY_CHARGER_DISCONNECTED;
		break;
	default:
		_E("invalid parameter(%d)", state);
		return;
	}

	_D("system_event(%s)", str);

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_BATTERY_CHARGER_STATUS, str);
	eventsystem_send_system_event(SYS_EVENT_BATTERY_CHARGER_STATUS, b);
	bundle_free(b);
}

static void update_present(enum battery_noti_status status)
{
	static int old = DEVICE_NOTI_OFF;
	enum present_type present;

	if (old == status)
		return;
	_I("charge %d present %d", battery.charge_now, battery.present);
	old = status;
	pm_change_internal(getpid(), LCD_NORMAL);
	if (status == DEVICE_NOTI_ON) {
		present = PRESENT_ABNORMAL;
		device_notify(DEVICE_NOTIFIER_BATTERY_PRESENT, (void *)PRESENT_ABNORMAL);
		pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);
	} else {
		present = PRESENT_NORMAL;
		device_notify(DEVICE_NOTIFIER_BATTERY_PRESENT, (void *)PRESENT_NORMAL);
		pm_unlock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, PM_SLEEP_MARGIN);
	}
	changed_battery_cf(present);
}

static void update_health(enum battery_noti_status status)
{
	static int old = DEVICE_NOTI_OFF;

	if (old == status)
		return;
	_I("charge %d health %d", battery.charge_now, battery.health);
	old = status;

	pm_change_internal(getpid(), LCD_NORMAL);
	if (status == DEVICE_NOTI_ON) {
		_I("popup - Battery health status is not good");
		device_notify(DEVICE_NOTIFIER_BATTERY_HEALTH, (void *)HEALTH_BAD);
		pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);
		if (battery.temp == TEMP_LOW)
			battery_charge_err_low_act(NULL);
		else if (battery.temp == TEMP_HIGH)
			battery_charge_err_high_act(NULL);
	} else {
		device_notify(DEVICE_NOTIFIER_BATTERY_HEALTH, (void *)HEALTH_GOOD);
		pm_unlock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, PM_SLEEP_MARGIN);
		health_status_broadcast();
		abnormal_popup_timer_init();
	}
}

static void update_ovp(enum battery_noti_status status)
{
	static int old = DEVICE_NOTI_OFF;

	if (old == status)
		return;
	_I("charge %d ovp %d", battery.charge_now, battery.ovp);
	old = status;
	pm_change_internal(getpid(), LCD_NORMAL);
	if (status == DEVICE_NOTI_ON)
		device_notify(DEVICE_NOTIFIER_BATTERY_OVP, (void *)OVP_ABNORMAL);
	else
		device_notify(DEVICE_NOTIFIER_BATTERY_OVP, (void *)OVP_NORMAL);
}

static void check_battery_status(void)
{
	static int old = DEVICE_CHANGE_NORMAL;
	int status;

	if (battery.charge_now == CHARGER_ABNORMAL &&
	    (battery.health == HEALTH_BAD || battery.present == PRESENT_ABNORMAL))
		status = DEVICE_CHANGE_ABNORMAL;
	else if (battery.ovp == OVP_ABNORMAL)
		status = DEVICE_CHANGE_ABNORMAL;
	else
		status = DEVICE_CHANGE_NORMAL;
	if (old == status)
		return;
	old = status;

	if (battery.charge_now == CHARGER_ABNORMAL) {
		if (battery.health == HEALTH_BAD) {
			update_health(DEVICE_NOTI_ON);
			return;
		} else if (battery.present == PRESENT_ABNORMAL) {
			update_present(DEVICE_NOTI_ON);
			return;
		}
	}
	if (battery.ovp == OVP_ABNORMAL) {
		update_ovp(DEVICE_NOTI_ON);
		return;
	}

	if (battery.charge_now != CHARGER_ABNORMAL &&
	    status == DEVICE_CHANGE_NORMAL) {
		update_health(DEVICE_NOTI_OFF);
		update_ovp(DEVICE_NOTI_OFF);
		update_present(DEVICE_NOTI_OFF);
	}
}

static void check_online(void)
{
	static int old_online;
	static int old_charge_status;
	int charge_status;

	if (battery.charge_status == CHARGE_STATUS_FULL)
		charge_status = CHARGE_STATUS_DISCHARGING;
	else
		charge_status = battery.charge_status;

	if (battery.online > POWER_SUPPLY_TYPE_BATTERY &&
	    old_online == VCONFKEY_SYSMAN_CHARGER_DISCONNECTED) {
		old_online = VCONFKEY_SYSMAN_CHARGER_CONNECTED;
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, old_online);
		power_supply_broadcast(CHARGER_STATUS_SIGNAL, old_online);
		check_power_supply(old_online);
		charger_state_send_system_event(CHARGE_STATUS_CONNECTED);
		if (charge_status != old_charge_status)
			charger_state_send_system_event(charge_status);

	} else if (battery.online <= POWER_SUPPLY_TYPE_BATTERY &&
	    old_online == VCONFKEY_SYSMAN_CHARGER_CONNECTED) {
		old_online = VCONFKEY_SYSMAN_CHARGER_DISCONNECTED;
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, old_online);
		power_supply_broadcast(CHARGER_STATUS_SIGNAL, old_online);
		check_power_supply(old_online);
		if (charge_status != old_charge_status)
			charger_state_send_system_event(charge_status);
		charger_state_send_system_event(CHARGE_STATUS_DISCONNECTED);

	} else {
		if (charge_status != old_charge_status)
			charger_state_send_system_event(charge_status);
	}

	old_charge_status = charge_status;
}

static void check_charge_status(const char *env_value)
{
	if (env_value == NULL)
		return;

	_D("Charge Status(%s)", env_value);

	if (strncmp(env_value, CHARGEFULL_NAME,
				sizeof(CHARGEFULL_NAME)) == 0)
		battery.charge_status = CHARGE_STATUS_FULL;
	else if (strncmp(env_value, CHARGENOW_NAME,
				sizeof(CHARGENOW_NAME)) == 0)
		battery.charge_status = CHARGE_STATUS_CHARGING;
	else if (strncmp(env_value, DISCHARGE_NAME,
				sizeof(DISCHARGE_NAME)) == 0)
		battery.charge_status = CHARGE_STATUS_DISCHARGING;
	else if (strncmp(env_value, NOTCHARGE_NAME,
				sizeof(NOTCHARGE_NAME)) == 0)
		battery.charge_status = CHARGE_STATUS_NOT_CHARGING;
	else
		battery.charge_status = CHARGE_STATUS_UNKNOWN;

	if (battery.charge_status == CHARGE_STATUS_FULL) {
		battery.charge_full = CHARGING_FULL;
		battery.charge_now = CHARGER_DISCHARGING;
	} else if (battery.charge_status == CHARGE_STATUS_CHARGING) {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_CHARGING;
	} else if (battery.charge_status == CHARGE_STATUS_DISCHARGING) {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_DISCHARGING;
	} else if (battery.charge_status == CHARGE_STATUS_NOT_CHARGING) {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_ABNORMAL;
	} else {
		battery.charge_full = CHARGING_NOT_FULL;
		battery.charge_now = CHARGER_DISCHARGING;
	}
}

static void check_health_status(const char *env_value)
{
	if (env_value == NULL) {
		battery.health = HEALTH_GOOD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_NORMAL;
		return;
	}
	if (strncmp(env_value, OVERHEAT_NAME,
				sizeof(OVERHEAT_NAME)) == 0) {
		battery.health = HEALTH_BAD;
		battery.temp = TEMP_HIGH;
		battery.ovp = OVP_NORMAL;
	} else if (strncmp(env_value, TEMPCOLD_NAME,
				sizeof(TEMPCOLD_NAME)) == 0) {
		battery.health = HEALTH_BAD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_NORMAL;
	} else if (strncmp(env_value, OVERVOLT_NAME,
				sizeof(OVERVOLT_NAME)) == 0) {
		battery.health = HEALTH_GOOD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_ABNORMAL;
	} else {
		battery.health = HEALTH_GOOD;
		battery.temp = TEMP_LOW;
		battery.ovp = OVP_NORMAL;
	}
}

static void check_online_status(const char *env_value)
{
	if (env_value == NULL)
		return;
	battery.online = atoi(env_value);
}

static void check_present_status(const char *env_value)
{
	if (env_value == NULL) {
		battery.present = PRESENT_NORMAL;
		return;
	}
	battery.present = atoi(env_value);
}

static void check_capacity_status(const char *env_value)
{
	if (env_value == NULL)
		return;
	battery.capacity = atoi(env_value);
}

static void process_power_supply(void *data)
{
	static struct battery_status old;

	if (old.charge_now != battery.charge_now || battery.charge_now == CHARGER_ABNORMAL) {
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, battery.charge_now);
		power_supply_broadcast(CHARGE_NOW_SIGNAL, battery.charge_now);
	}

	lowbat_execute(data);
	check_online();
	if (old.charge_full != battery.charge_full)
		noti_batt_full();

	old.capacity = battery.capacity;
	old.online = battery.online;
	old.charge_status = battery.charge_status;
	old.charge_now = battery.charge_now;
	old.charge_full = battery.charge_full;

	check_battery_status();
	device_notify(DEVICE_NOTIFIER_POWER_SUPPLY, NULL);
	device_notify(DEVICE_NOTIFIER_BATTERY_CHARGING, &battery.charge_now);
}

static void uevent_power_handler(struct udev_device *dev)
{
	struct udev_list_entry *list_entry;
	const char *env_name;
	const char *env_value;
	bool matched = false;
	int ret;

	udev_list_entry_foreach(list_entry,
			udev_device_get_properties_list_entry(dev)) {
		env_name = udev_list_entry_get_name(list_entry);
		if (!env_name)
			continue;

		if (!strncmp(env_name, CHARGE_NAME, sizeof(CHARGE_NAME))) {
			env_value = udev_list_entry_get_value(list_entry);
			if (!env_value)
				continue;
			if (!strncmp(env_value, BATTERY_NAME,
						sizeof(BATTERY_NAME))) {
				matched = true;
				break;
			}
		}
	}

	if (!matched)
		return;

	env_value = udev_device_get_property_value(dev, CHARGE_STATUS);
	check_charge_status(env_value);
	env_value = udev_device_get_property_value(dev, CHARGE_ONLINE);
	check_online_status(env_value);
	env_value = udev_device_get_property_value(dev, CHARGE_HEALTH);
	check_health_status(env_value);
	env_value = udev_device_get_property_value(dev, CHARGE_PRESENT);
	check_present_status(env_value);
	env_value = udev_device_get_property_value(dev, CAPACITY);
	check_capacity_status(env_value);

	ret = booting_done(NULL);
	if (ret) {
		if (battery.online > POWER_SUPPLY_TYPE_BATTERY)
			power_supply_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
		else
			power_supply_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_OFF);
	}

	process_power_supply(&battery.capacity);
}

static void battery_print_info(struct battery_info *info)
{
	if (!info)
		return;
	_I("Battery state updated:");
	_I("   Status(%s)", info->status);
	_I("   Health(%s)", info->health);
	_I("   Power Source(%s)", info->power_source);
	_I("   Online(%d)", info->online);
	_I("   Present(%d)", info->present);
	_I("   Capacity(%d)", info->capacity);
	_I("   Current Now(%d)", info->current_now);
	_I("   Current Average(%d)", info->current_average);
	_I("   Charge Full (%s)", battery.charge_full == CHARGING_FULL ? "Full" : "Not Full");
	switch (battery.charge_now) {
	case CHARGER_CHARGING:
		_I("   Charge Now(Charging)");
		break;
	case CHARGER_DISCHARGING:
		_I("   Charge Now(Discharging)");
		break;
	case CHARGER_ABNORMAL:
		_I("   Charge Now(Abnormal)");
		break;
	default:
		break;
	}
	_I("   BatPresent(%s)", battery.present == PRESENT_NORMAL ? "Normal" : "Abnormal");
	_I("   Temperature(%s)", battery.temp == TEMP_LOW ? "Low" : "High");
	_I("   OVP(%s)", battery.ovp == OVP_NORMAL ? "Normal": "Abnormal");
}

static void battery_changed(struct battery_info *info, void *data)
{
	int ret;

	if (!info)
		return;

	if (info->status) {
		snprintf(battery.status_s, sizeof(battery.status_s),
				"%s", info->status);
		check_charge_status(info->status);
	} else
		battery.status_s[0] = '\0';

	if (info->health) {
		snprintf(battery.health_s, sizeof(battery.health_s),
				"%s", info->health);
		check_health_status(info->health);
	} else
		battery.health_s[0] = '\0';

	if (info->power_source)
		snprintf(battery.power_source_s, sizeof(battery.power_source_s),
				"%s", info->power_source);
	else
		battery.power_source_s[0] = '\0';

	battery.online = info->online;
	battery.present = info->present;
	battery.capacity = info->capacity;
	battery.current_now = info->current_now;
	battery.current_average = info->current_average;

	battery_print_info(info);

	ret = booting_done(NULL);
	if (ret) {
		if (battery.online > POWER_SUPPLY_TYPE_BATTERY)
			power_supply_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
		else
			power_supply_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_OFF);
	}

	process_power_supply(&battery.capacity);

}

static int lowbat_read(int *val)
{
	int r;

	if (!val)
		return -EINVAL;

	r = sys_get_int("/sys/class/power_supply/battery/capacity", val);
	if (r < 0)
		return r;

	return 0;
}

static void battery_get_capacity(struct battery_info *info, void *data)
{
	int *capa = data;

	if (info && info->capacity >= 0)
		*capa = info->capacity;
}

static void power_supply_status_init(void)
{
	static int charge_now = -1;
	static int charge_full = -1;
	static int capacity = -1;
	int pct;
	int r;

	if (battery_dev && battery_dev->get_current_state) {
		pct = -1;
		r = battery_dev->get_current_state(battery_get_capacity, &pct);
		if (r < 0 || pct < 0) {
			_E("Failed to get battery capacity (capa:%d, ret:%d)", pct, r);
			return;
		}
	} else {
		r = lowbat_read(&pct);
		if (r < 0) {
			_E("fail to read capacity data : %d", r);
			return;
		}
	}

	battery.capacity = pct;
	battery.health = HEALTH_GOOD;
	battery.ovp = OVP_NORMAL;
	battery.present = PRESENT_NORMAL;
	battery.temp = TEMP_LOW;

	if (charge_now == battery.charge_now &&
	    charge_full == battery.charge_full &&
	    capacity == battery.capacity)
		return;

	_I("charging %d full %d capacity %d", battery.charge_now, battery.charge_full, battery.capacity);

	if (charge_now != battery.charge_now) {
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, battery.charge_now);
		power_supply_broadcast(CHARGE_NOW_SIGNAL, battery.charge_now);
	}
	if (capacity != battery.capacity) {
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CAPACITY, battery.capacity);
		power_supply_broadcast(CHARGE_CAPACITY_SIGNAL, battery.capacity);
	}

	charge_now = battery.charge_now;
	charge_full = battery.charge_full;
	capacity =  battery.capacity;
}

static Eina_Bool power_supply_update(void *data)
{
	power_supply_status_init();
	return EINA_TRUE;
}

static void power_supply_timer_start(void)
{
	_D("battery init timer during booting");
	power_timer = ecore_timer_add(BATTERY_CHECK_TIMER_INTERVAL,
				power_supply_update, NULL);
	if (power_timer == NULL)
		_E("fail to add battery init timer during booting");
}

static void power_supply_timer_stop(void)
{
	_D("battery init timer during booting");
	if (!power_timer)
		return;
	ecore_timer_del(power_timer);
	power_timer = NULL;
}

static DBusMessage *dbus_get_charger_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &ret) < 0) {
		_E("vconf_get_int() failed");
		ret = -EIO;
	}
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_get_charge_now(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.charge_now;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_get_charge_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &ret) < 0) {
		_E("vconf_get_int() failed");
		ret = -EIO;
	}

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_get_percent(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.capacity;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_get_percent_raw(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = -ENOTSUP;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_is_full(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.charge_full;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_get_health(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = battery.health;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_power_supply_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret = 0;
	int argc;
	char *type_str;
	char *argv[5];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1],
		    DBUS_TYPE_STRING, &argv[2],
		    DBUS_TYPE_STRING, &argv[3],
		    DBUS_TYPE_STRING, &argv[4], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}
	check_capacity_status(argv[0]);
	check_charge_status(argv[1]);
	check_health_status(argv[2]);
	check_online_status(argv[3]);
	check_present_status(argv[4]);
	_I("%d %d %d %d %d %d %d %d",
		battery.capacity,
		battery.charge_full,
		battery.charge_now,
		battery.health,
		battery.online,
		battery.ovp,
		battery.present,
		battery.temp);

	if (battery.online > POWER_SUPPLY_TYPE_BATTERY)
		power_supply_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_ON);
	else
		power_supply_noti(DEVICE_NOTI_BATT_CHARGE, DEVICE_NOTI_OFF);

	process_power_supply(&battery.capacity);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static void battery_get_info(struct battery_info *info, void *data)
{
	struct battery_info *bat = data;

	if (!info || !bat)
		return;

	bat->status = strdup(info->status);
	bat->health = strdup(info->health);
	bat->power_source = strdup(info->power_source);
	bat->online = info->online;
	bat->present = info->present;
	bat->capacity = info->capacity;
	bat->current_now = info->current_now;
	bat->current_average = info->current_average;
}

static DBusMessage *dbus_get_battery_info(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret, val;
	const char *str;
	struct battery_info info = { 0, };

	if (battery_dev && battery_dev->get_current_state) {
		ret = battery_dev->get_current_state(battery_get_info, &info);
		if (ret < 0)
			_E("Failed to get battery info (%d)", ret);

		battery_changed(&info, NULL);
	} else {
		if (battery.charge_status == CHARGE_STATUS_FULL)
			str = CHARGEFULL_NAME;
		else if (battery.charge_status == CHARGE_STATUS_CHARGING)
			str = CHARGENOW_NAME;
		else if (battery.charge_status == CHARGE_STATUS_DISCHARGING)
			str = DISCHARGE_NAME;
		else if (battery.charge_status == CHARGE_STATUS_NOT_CHARGING)
			str = NOTCHARGE_NAME;
		else
			str = "Unknown";
		snprintf(battery.status_s, sizeof(battery.status_s), "%s", str);

		if (battery.health == HEALTH_GOOD) {
			if (battery.temp == TEMP_LOW && battery.ovp == OVP_ABNORMAL)
				str = OVERVOLT_NAME;
			else
				str = "Good";
		} else { /* HEALTH_BAD */
			if (battery.temp == TEMP_HIGH)
				str = OVERHEAT_NAME;
			else /* TEMP_LOW */
				str = TEMPCOLD_NAME;
		}
		snprintf(battery.health_s, sizeof(battery.health_s), "%s", str);

		if (vconf_get_int(VCONFKEY_SYSMAN_USB_STATUS, &val) == 0 &&
			val != VCONFKEY_SYSMAN_USB_DISCONNECTED)
			str = POWER_SOURCE_USB;
		else if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &val) == 0 &&
			val == VCONFKEY_SYSMAN_CHARGER_CONNECTED)
			str = POWER_SOURCE_AC;
		else
			str = POWER_SOURCE_NONE;
		snprintf(battery.power_source_s, sizeof(battery.power_source_s), "%s", str);

		battery.current_now = -1; /* Not supported */
		battery.current_average = -1; /* Not supported */
		ret = 0;
	}

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	str = battery.status_s;
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str);
	str = battery.health_s;
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str);
	str = battery.power_source_s;
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &(battery.online));
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &(battery.present));
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &(battery.capacity));
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &(battery.current_now));
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &(battery.current_average));
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ CHARGER_STATUS_SIGNAL,      NULL, "i", dbus_get_charger_status },
	{ CHARGE_NOW_SIGNAL,          NULL, "i", dbus_get_charge_now },
	{ CHARGE_LEVEL_SIGNAL,        NULL, "i", dbus_get_charge_level },
	{ CHARGE_CAPACITY_SIGNAL,     NULL, "i", dbus_get_percent },
	{ CHARGE_CAPACITY_LAW_SIGNAL, NULL, "i", dbus_get_percent_raw },
	{ CHARGE_FULL_SIGNAL,         NULL, "i", dbus_is_full },
	{ CHARGE_HEALTH_SIGNAL,       NULL, "i", dbus_get_health },
	{ POWER_SUBSYSTEM,       "sisssss", "i", dbus_power_supply_handler },
	{ "GetBatteryInfo",           NULL, "isssiiiii", dbus_get_battery_info },
};

static int booting_done(void *data)
{
	static int done;

	if (data == NULL)
		return done;
	done = *(int *)data;
	if (done == 0)
		return done;

	_I("booting done");

	power_supply_timer_stop();

	/* for simple noti change cb */
	power_supply_status_init();
	process_power_supply(NULL);

	return done;
}

static int display_changed(void *data)
{
	if (battery.charge_now != CHARGER_ABNORMAL)
		return 0;
	if (battery.health != HEALTH_BAD && battery.present != PRESENT_ABNORMAL)
		return 0;
	pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);
	return 0;
}

static int load_uevent(struct parse_result *result, void *user_data)
{
	struct battery_status *info = user_data;

	if (!info)
		return -EINVAL;

	if (MATCH(result->name, CHARGE_STATUS)) {
		if (strstr(result->value, "Charging")) {
			info->charge_now = CHARGER_CHARGING;
			info->charge_full = CHARGING_NOT_FULL;
		} else if (strstr(result->value, "Discharging")) {
			info->charge_now = CHARGER_DISCHARGING;
			info->charge_full = CHARGING_NOT_FULL;
		} else if (strstr(result->value, "Full")) {
			info->charge_now = CHARGER_DISCHARGING;
			info->charge_full = CHARGING_FULL;
		} else if (strstr(result->value, "Not charging")) {
			info->charge_now = CHARGER_ABNORMAL;
			info->charge_full = CHARGING_NOT_FULL;
		}
	} else if (MATCH(result->name, CAPACITY))
		info->capacity = atoi(result->value);
	return 0;
}

static int power_supply_probe(void *data)
{
	struct hw_info *info;
	int ret;

	if (battery_dev)
		return 0;

	ret = hw_get_info(BATTERY_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);

	if (ret < 0) {
		/**
		 * Set battery vconf as -ENOTSUP
		 * These vconf key used by runtime-info and capi-system-device.
		 */
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, -ENOTSUP);
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, -ENOTSUP);
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_LEVEL_STATUS, -ENOTSUP);
		_I("There is no battery HAL(%d)", ret);
		return -ENODEV;
	}

	if (!info->open) {
		_E("Failed to open battery device; open(NULL)");
		return -ENODEV;
	}

	ret = info->open(info, NULL, (struct hw_common**)&battery_dev);
	if (ret < 0) {
		_E("Failed to get battery device structure (%d)", ret);
		return ret;
	}

	_I("battery device structure load success");
	return 0;
}

static void power_supply_init(void *data)
{
	int ret;

	if (battery_dev) {
		if (battery_dev->register_changed_event)
			battery_dev->register_changed_event(battery_changed, NULL);

		if (battery_dev->get_current_state)
			battery_dev->get_current_state(battery_changed, NULL);
	} else {
		ret = config_parse(POWER_SUPPLY_UEVENT, load_uevent, &battery);
		if (ret < 0)
			_E("Failed to load %s, %d Use default value!",
					POWER_SUPPLY_UEVENT, ret);

		/* register power subsystem */
		register_kernel_uevent_control(&uh);
	}

	/* process check battery timer until booting done */
	power_supply_timer_start();

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);

	ret = register_edbus_interface_and_method(DEVICED_PATH_BATTERY,
			DEVICED_INTERFACE_BATTERY,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	ret = register_edbus_signal_handler(DEVICED_PATH_SYSNOTI,
			DEVICED_INTERFACE_SYSNOTI, SIGNAL_CHARGEERR_RESPONSE,
			abnormal_popup_edbus_signal_handler);
	if (ret < 0)
		_E("fail to init edbus signal(%d)", ret);
}

static void power_supply_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);

	/* unregister power subsystem */
	unregister_kernel_uevent_control(&uh);
}

static const struct device_ops power_supply_ops = {
	.name     = "power_supply",
	.probe    = power_supply_probe,
	.init     = power_supply_init,
	.exit     = power_supply_exit,
};

DEVICE_OPS_REGISTER(&power_supply_ops)
