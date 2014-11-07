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
#include "devices.h"
#include "device-handler.h"
#include "device-notifier.h"
#include "udev.h"
#include "log.h"
#include "display/poll.h"
#include "display/setting.h"
#include "proc/proc-handler.h"
#include "config-parser.h"
#include "power-supply.h"

#define BUFF_MAX		255
#define POPUP_KEY_CONTENT		"_SYSPOPUP_CONTENT_"

#define SIGNAL_CHARGEERR_RESPONSE	"ChargeErrResponse"
#define SIGNAL_TEMP_GOOD		"TempGood"

#define ABNORMAL_CHECK_TIMER_INTERVAL	60

#define METHOD_FULL_NOTI_ON		"BatteryFullNotiOn"
#define METHOD_FULL_NOTI_OFF	"BatteryFullNotiOff"
#define METHOD_CHARGE_NOTI_ON	"BatteryChargeNotiOn"

#define SIOP_DISABLE	"memory/private/sysman/siop_disable"

#define RETRY_MAX 5
#define BATTERY_CHECK_TIMER_INTERVAL	(0.5)

#ifdef MICRO_DD
#define DEVICE_NOTIFIER "/usr/bin/sys_device_noti"
#define BATT_CHARGE_NOTI "0"
#define BATT_FULL_NOTI   "2"
#endif

enum power_supply_init_type {
	POWER_SUPPLY_NOT_READY = 0,
	POWER_SUPPLY_INITIALIZED = 1,
};

struct popup_data {
	char *name;
	char *key;
	char *value;
};

static Ecore_Timer *power_timer = NULL;
static Ecore_Timer *abnormal_timer = NULL;
extern int battery_power_off_act(void *data);

static void pm_check_and_change(int bInserted)
{
	static int old = -1;
	if (old != bInserted) {
		old = bInserted;
		internal_pm_change_state(LCD_NORMAL);
	}
}

int check_lowbat_charge_device(int bInserted)
{
	static int bChargeDeviceInserted = 0;
	int val = -1;
	int bat_state = -1;
	int ret = -1;
	char *value;
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	pm_check_and_change(bInserted);
	if (bInserted == 1) {
		if (battery.charge_now)
			bChargeDeviceInserted = 1;
		return 0;
	} else if (bInserted == 0) {
		if (!battery.charge_now && bChargeDeviceInserted == 1) {
			bChargeDeviceInserted = 0;
			//low bat popup during charging device removing
			if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state) == 0) {
				if(bat_state < VCONFKEY_SYSMAN_BAT_NORMAL
						|| bat_state == VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF) {
					FIND_DEVICE_INT(apps, "apps");

					if(bat_state == VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF)
						value = "poweroff";
					else
						value = "warning";
					params = malloc(sizeof(struct popup_data));
					if (params == NULL) {
						_E("Malloc failed");
						return -1;
					}
					params->name = "lowbat-syspopup";
					params->key = POPUP_KEY_CONTENT;
					params->value = value;
					_I("%s %s %s(%x)", params->name, params->key, params->value, params);
					if (apps->init)
						apps->init((void *)params);
					free(params);
				}
			} else {
				_E("failed to get vconf key");
				return -1;
			}
		}
		return 0;
	}
	return -1;
}

static int changed_battery_cf(enum present_type status)
{
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	FIND_DEVICE_INT(apps, "apps");
	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return -ENOMEM;
	}
	params->name = "lowbat-syspopup";
	params->key = POPUP_KEY_CONTENT;
	params->value = "battdisconnect";
	if (apps->init == NULL || apps->exit == NULL)
		goto out;
	if (status == PRESENT_ABNORMAL)
		apps->init((void *)params);
	else
		apps->exit((void *)params);
out:
	free(params);
	return 0;
}

int check_abnormal_popup(void)
{
	int ret = HEALTH_BAD;

	if (abnormal_timer)
		ret =  HEALTH_GOOD;
	return ret;
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

static int clean_health_popup(void)
{
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	FIND_DEVICE_INT(apps, "apps");
	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return -ENOMEM;
	}
	params->name = "lowbat-syspopup";
	params->key = POPUP_KEY_CONTENT;
	if (apps->exit)
		apps->exit((void *)params);
	health_status_broadcast();
out:
	free(params);
	return 0;

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
	vconf_set_int(SIOP_DISABLE, 1);
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

static int noti_id;

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

static int check_noti(void)
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

	noti = check_noti();

	if (!noti)
		return noti;

	switch (state) {
	case CHARGING_FULL:
		for (retry = RETRY_MAX; retry > 0 ;retry--) {
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
		for (retry = RETRY_MAX; retry > 0 ;retry--) {
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

int send_charge_noti(void)
{
	int ret = 0;
	int retry;

	for (retry = RETRY_MAX; retry > 0 ;retry--) {
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

void battery_noti(enum battery_noti_type type, enum battery_noti_status status)
{
	static int charge = CHARGER_DISCHARGING;
	static int full = CHARGING_NOT_FULL;
	int ret, i;

	if (type == DEVICE_NOTI_BATT_FULL && status == DEVICE_NOTI_ON &&
	    full == CHARGING_NOT_FULL) {
		if (charge == CHARGER_DISCHARGING)
			send_charge_noti();
		ret = send_full_noti(CHARGING_FULL);
		if (ret == 0)
			full = CHARGING_FULL;
	} else if (type == DEVICE_NOTI_BATT_FULL && status == DEVICE_NOTI_OFF &&
	    full == CHARGING_FULL) {
		ret = send_full_noti(CHARGING_NOT_FULL);
		if (ret == 0)
			full = CHARGING_NOT_FULL;
	} else if (type == DEVICE_NOTI_BATT_CHARGE &&
	    battery.charge_now == CHARGER_CHARGING &&
	    charge == CHARGER_DISCHARGING) {
		if (full == CHARGING_FULL) {
			ret = send_full_noti(CHARGING_NOT_FULL);
			if (ret == 0)
				full = CHARGING_NOT_FULL;
		}
		send_charge_noti();
	}
	charge = battery.charge_now;
}

static void noti_batt_full(void)
{
	char params[BUFF_MAX];
	static int bat_full_noti = 0;
	int r_disturb, s_disturb, r_block, s_block;

	r_disturb = vconf_get_int("memory/shealth/sleep/do_not_disturb", &s_disturb);
	r_block = vconf_get_bool("db/setting/blockmode_wearable", &s_block);
	if (!battery.charge_full && bat_full_noti == 1) {
		battery_noti(DEVICE_NOTI_BATT_FULL, DEVICE_NOTI_OFF);
		bat_full_noti = 0;
	}
	if (battery.charge_full && bat_full_noti == 0) {
		battery_noti(DEVICE_NOTI_BATT_FULL, DEVICE_NOTI_ON);
		bat_full_noti = 1;
		/* turn on LCD, if battery is full charged */
		if ((r_disturb != 0 && r_block != 0) ||
		    (s_disturb == 0 && s_block == 0))
			pm_change_internal(getpid(), LCD_NORMAL);
		else
			_I("block LCD");
	}
}

static void check_power_supply(int state)
{
	int ret = -1;
	int val = 0;
	char params[BUFF_MAX];

	check_lowbat_charge_device(state);
	if (update_pm_setting)
		update_pm_setting(SETTING_CHARGING, state);

	ret = device_get_property(DEVICE_TYPE_POWER,
		PROP_POWER_INSUSPEND_CHARGING_SUPPORT, &val);

	if (ret != 0 || val == 1) {
		_D("fail to check charger insuspend");
		goto out;
	}

	if (state == 0)
		pm_unlock_internal(INTERNAL_LOCK_TA, LCD_OFF, STAY_CUR_STATE);
	else
		pm_lock_internal(INTERNAL_LOCK_TA, LCD_OFF, STAY_CUR_STATE, 0);
out:
	_I("ta device %d(capacity %d)", state, battery.capacity);

	sync_cradle_status();
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
	if (status == DEVICE_NOTI_ON)
		present = PRESENT_ABNORMAL;
	else
		present = PRESENT_NORMAL;
	changed_battery_cf(present);
}

static void update_health(enum battery_noti_status status)
{
	static int old = DEVICE_NOTI_OFF;

	if (old == status)
		return;
	_I("charge %d health %d", battery.charge_now, battery.health);
	old = status;

	if (status == DEVICE_NOTI_ON) {
		_I("silent health popup");
		return;
	}

	pm_change_internal(getpid(), LCD_NORMAL);
	if (status == DEVICE_NOTI_ON) {
		_I("popup - Battery health status is not good");
		vconf_set_int(SIOP_DISABLE, 1);
		device_notify(DEVICE_NOTIFIER_BATTERY_HEALTH, (void *)HEALTH_BAD);
		pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);
		if (battery.temp == TEMP_LOW)
			battery_charge_err_low_act(NULL);
		else if (battery.temp == TEMP_HIGH)
			battery_charge_err_high_act(NULL);
	} else {
		vconf_set_int(SIOP_DISABLE, 0);
		device_notify(DEVICE_NOTIFIER_BATTERY_HEALTH, (void *)HEALTH_GOOD);
		pm_unlock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, PM_SLEEP_MARGIN);
		clean_health_popup();
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

	if (battery.online > POWER_SUPPLY_TYPE_BATTERY &&
	    old_online == VCONFKEY_SYSMAN_CHARGER_DISCONNECTED) {
		old_online = VCONFKEY_SYSMAN_CHARGER_CONNECTED;
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, old_online);
		power_supply_broadcast(CHARGER_STATUS_SIGNAL, old_online);
		extcon_set_count(EXTCON_TA);
		check_power_supply(old_online);
	} else if (battery.online <= POWER_SUPPLY_TYPE_BATTERY &&
	    old_online == VCONFKEY_SYSMAN_CHARGER_CONNECTED) {
		old_online = VCONFKEY_SYSMAN_CHARGER_DISCONNECTED;
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, old_online);
		power_supply_broadcast(CHARGER_STATUS_SIGNAL, old_online);
		check_power_supply(old_online);
	}
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
	}
	else if (MATCH(result->name, CAPACITY))
		info->capacity = atoi(result->value);
	return 0;
}

static void power_load_uevent(void)
{
	int ret;
	static int initialized = POWER_SUPPLY_NOT_READY;

	if (initialized == POWER_SUPPLY_INITIALIZED)
		return;
	ret = config_parse(POWER_SUPPLY_UEVENT, load_uevent, &battery);
	if (ret < 0)
		_E("Failed to load %s, %d Use default value!", POWER_SUPPLY_UEVENT, ret);
	else
		initialized = POWER_SUPPLY_INITIALIZED;
}

void power_supply(void *data)
{
	int ret;
	int status = POWER_SUPPLY_STATUS_DISCHARGING;
	static struct battery_status old;

	if (old.charge_now != battery.charge_now || battery.charge_now == CHARGER_ABNORMAL) {
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, battery.charge_now);
		power_supply_broadcast(CHARGE_NOW_SIGNAL, battery.charge_now);
	}

	if (old.online != battery.online ||
	    old.charge_now != battery.charge_now ||
	    old.charge_full != battery.charge_full) {
		switch (battery.charge_now)
		{
		case CHARGER_ABNORMAL:
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case CHARGER_DISCHARGING:
			if (battery.charge_full == CHARGING_FULL)
				status = POWER_SUPPLY_STATUS_FULL;
			else
				status = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case CHARGER_CHARGING:
			status = POWER_SUPPLY_STATUS_CHARGING;
			break;
		}
	}

	lowbat_monitor(data);
	check_online();
	if (old.charge_full != battery.charge_full) {
		noti_batt_full();
	}

	old.capacity = battery.capacity;
	old.online = battery.online;
	old.charge_now = battery.charge_now;
	old.charge_full = battery.charge_full;

	check_battery_status();
	device_notify(DEVICE_NOTIFIER_POWER_SUPPLY, NULL);
}

void power_supply_status_init(void)
{
	int ret, val;
	static int charge_now = -1;
	static int charge_full = -1;
	static int capacity = -1;

	power_load_uevent();
	battery.health = HEALTH_GOOD;
	battery.ovp = OVP_NORMAL;
	battery.present = PRESENT_NORMAL;
	battery.temp = TEMP_LOW;

	if (charge_now == battery.charge_now &&
	    charge_full == battery.charge_full &&
	    capacity == battery.capacity)
		return;

	if (charge_now != battery.charge_now ||
	    charge_full != battery.charge_full ||
	    capacity != battery.capacity)
		_I("charging %d full %d capacity %d", battery.charge_now, battery.charge_full, battery.capacity);

	if (charge_now != battery.charge_now) {
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, battery.charge_now);
		power_supply_broadcast(CHARGE_NOW_SIGNAL, battery.charge_now);
	}
	if (capacity != battery.capacity)
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CAPACITY, battery.capacity);

	charge_now = battery.charge_now;
	charge_full = battery.charge_full;
	capacity =  battery.capacity;
}

static Eina_Bool power_supply_update(void *data)
{
	power_supply_status_init();
	return EINA_TRUE;
}

void power_supply_timer_start(void)
{
	_D("battery init timer during booting");
	power_timer = ecore_timer_add(BATTERY_CHECK_TIMER_INTERVAL,
				power_supply_update, NULL);
	if (power_timer == NULL)
	    _E("fail to add battery init timer during booting");
}

void power_supply_timer_stop(void)
{
    _D("battery init timer during booting");
	if (!power_timer)
		return;
	ecore_timer_del(power_timer);
	power_timer = NULL;
}

void power_supply_broadcast(char *sig, int status)
{
	static int old = 0;
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
	int ret, val;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY_RAW, &val);
	if (ret < 0)
		goto out;

	if (val > 10000)
		val = 10000;

	ret = val;

out:
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

static const struct edbus_method edbus_methods[] = {
	{ CHARGER_STATUS_SIGNAL,      NULL, "i", dbus_get_charger_status },
	{ CHARGE_NOW_SIGNAL,          NULL, "i", dbus_get_charge_now },
	{ CHARGE_LEVEL_SIGNAL,        NULL, "i", dbus_get_charge_level },
	{ CHARGE_CAPACITY_SIGNAL,     NULL, "i", dbus_get_percent },
	{ CHARGE_CAPACITY_LAW_SIGNAL, NULL, "i", dbus_get_percent_raw },
	{ CHARGE_FULL_SIGNAL,         NULL, "i", dbus_is_full },
	{ CHARGE_HEALTH_SIGNAL,       NULL, "i", dbus_get_health },
};

int power_supply_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_BATTERY, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
	ret = register_edbus_signal_handler(DEVICED_PATH_SYSNOTI,
			DEVICED_INTERFACE_SYSNOTI, SIGNAL_CHARGEERR_RESPONSE,
			abnormal_popup_edbus_signal_handler);
	if (ret < 0)
		_E("fail to init edbus signal(%d)", ret);

	/* for simple noti change cb */
	power_supply_status_init();
	power_supply(NULL);
	return ret;
}
