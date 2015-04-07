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


#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <vconf.h>
#include <fcntl.h>

#include "battery.h"
#include "config.h"
#include "core/log.h"
#include "core/launch.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/common.h"
#include "core/list.h"
#include "core/udev.h"
#include "device-node.h"
#include "display/setting.h"
#include "display/poll.h"
#include "core/edbus-handler.h"
#include "power-supply.h"
#include "power/power-handler.h"

#define CHARGE_POWERSAVE_FREQ_ACT	"charge_powersave_freq_act"
#define CHARGE_RELEASE_FREQ_ACT		"charge_release_freq_act"


#define BATTERY_CHARGING	65535
#define BATTERY_UNKNOWN		-1

#define WARNING_LOW_BAT_ACT		"warning_low_bat_act"
#define CRITICAL_LOW_BAT_ACT		"critical_low_bat_act"
#define POWER_OFF_BAT_ACT		"power_off_bat_act"
#define CHARGE_BAT_ACT			"charge_bat_act"
#define CHARGE_CHECK_ACT			"charge_check_act"
#define CHARGE_ERROR_ACT			"charge_error_act"
#define CHARGE_ERROR_LOW_ACT			"charge_error_low_act"
#define CHARGE_ERROR_HIGH_ACT			"charge_error_high_act"
#define CHARGE_ERROR_OVP_ACT			"charge_error_ovp_act"
#define WAITING_INTERVAL	10

#define LOWBAT_CPU_CTRL_ID	"id6"
#define LOWBAT_CPU_FREQ_RATE	(0.7)

struct lowbat_process_entry {
	int old;
	int now;
	int (*func) (void *data);
};

static int cur_bat_state = BATTERY_UNKNOWN;
static int cur_bat_capacity = -1;

static int lowbat_freq = -1;
static struct battery_config_info battery_info = {
	.normal   = BATTERY_NORMAL,
	.warning  = BATTERY_WARNING,
	.critical = BATTERY_CRITICAL,
	.poweroff = BATTERY_POWEROFF,
	.realoff  = BATTERY_REALOFF,
};

static dd_list *lpe = NULL;
static int scenario_count = 0;

static int lowbat_initialized(void *data)
{
	static int status;

	if (!data)
		return status;

	status = *(int *)data;
	return status;
}

static int lowbat_scenario(int old, int now, void *data)
{
	dd_list *n;
	struct lowbat_process_entry *scenario;
	int found = 0;

	DD_LIST_FOREACH(lpe, n, scenario) {
		if (old != scenario->old || now != scenario->now)
			continue;
		if (!scenario->func)
			continue;
		scenario->func(data);
		found = 1;
		break;
	}
	return found;
}

static int lowbat_add_scenario(int old, int now, int (*func)(void *data))
{
	struct lowbat_process_entry *scenario;

	_I("%d %d, %x", old, now, func);

	if (!func) {
		_E("invalid func address!");
		return -EINVAL;
	}

	scenario = malloc(sizeof(struct lowbat_process_entry));
	if (!scenario) {
		_E("Fail to malloc for notifier!");
		return -ENOMEM;
	}

	scenario->old = old;
	scenario->now = now;
	scenario->func = func;

	DD_LIST_APPEND(lpe, scenario);
	scenario_count++;
	return 0;
}

static void print_lowbat_state(unsigned int bat_percent)
{
#if 0
	int i;
	for (i = 0; i < BAT_MON_SAMPLES; i++)
		_D("\t%d", recent_bat_percent[i]);
#endif
}

static int power_execute(void)
{
	static const struct device_ops *ops = NULL;

	FIND_DEVICE_INT(ops, POWER_OPS_NAME);

	return ops->execute(INTERNAL_PWROFF);
}

static int booting_done(void *data)
{
	static int done = 0;

	if (data == NULL)
		goto out;
	done = *(int*)data;
	if (!done)
		goto out;
	_I("booting done");

	power_supply_timer_stop();
	power_supply_init(NULL);
out:
	return done;
}

static int lowbat_popup(char *option)
{
	int ret;
	int r_disturb, s_disturb, r_block, s_block;
	int lowbat_popup_option = -1;
	char *value;

	if (!option)
		return -1;

	if (!strcmp(option, CRITICAL_LOW_BAT_ACT)) {
		value = "Critical";
	} else if (!strcmp(option, WARNING_LOW_BAT_ACT)) {
		value = "Warning";
	} else if (!strcmp(option, POWER_OFF_BAT_ACT)) {
		value = "Poweroff";
	} else if (!strcmp(option, CHARGE_ERROR_ACT)) {
		value = "Charger error";
		lowbat_popup_option = LOWBAT_OPT_CHARGEERR;
	} else if (!strcmp(option, CHARGE_ERROR_LOW_ACT)) {
		value = "Charger low temperature error";
		lowbat_popup_option = LOWBAT_OPT_CHARGEERR;
	} else if (!strcmp(option, CHARGE_ERROR_HIGH_ACT)) {
		value = "Charger high temperature error";
		lowbat_popup_option = LOWBAT_OPT_CHARGEERR;
	} else if (!strcmp(option, CHARGE_ERROR_OVP_ACT)) {
		value = "Charger ovp error";
		lowbat_popup_option = LOWBAT_OPT_CHARGEERR;
	} else if (!strcmp(option, CHARGE_CHECK_ACT)) {
		return 0;
	} else
		return -1;

	_D("%s", value);
	if (booting_done(NULL)) {
		r_disturb = vconf_get_int("memory/shealth/sleep/do_not_disturb", &s_disturb);
		r_block = vconf_get_bool("db/setting/blockmode_wearable", &s_block);
		if ((r_disturb != 0 && r_block != 0) ||
		    (s_disturb == 0 && s_block == 0) ||
		    lowbat_popup_option == LOWBAT_OPT_CHARGEERR)
			pm_change_internal(getpid(), LCD_NORMAL);
		else
			_I("block LCD");
		ret = manage_notification("Low battery", value);
		if (ret == -1)
			return -1;
	} else {
		_D("boot-animation running yet");
	}

	return 0;
}

static int battery_check_act(void *data)
{
	lowbat_popup(CHARGE_CHECK_ACT);
	return 0;
}

static int battery_warning_low_act(void *data)
{
	lowbat_popup(WARNING_LOW_BAT_ACT);
	return 0;
}

static int battery_critical_low_act(void *data)
{
	lowbat_popup(CRITICAL_LOW_BAT_ACT);
	return 0;
}

int battery_power_off_act(void *data)
{
	poweroff_request_internal(POWER_OFF_DIRECT);
	return 0;
}

int battery_charge_err_act(void *data)
{
	lowbat_popup(CHARGE_ERROR_ACT);
	return 0;
}

int battery_charge_err_low_act(void *data)
{
	lowbat_popup(CHARGE_ERROR_LOW_ACT);
	return 0;
}

int battery_charge_err_high_act(void *data)
{
	lowbat_popup(CHARGE_ERROR_HIGH_ACT);
	return 0;
}

int battery_charge_err_ovp_act(void *data)
{
	lowbat_popup(CHARGE_ERROR_OVP_ACT);
	return 0;
}

static int battery_charge_act(void *data)
{
	return 0;
}

static void lowbat_scenario_init(void)
{
	lowbat_add_scenario(battery_info.normal, battery_info.warning, battery_warning_low_act);
	lowbat_add_scenario(battery_info.warning, battery_info.critical, battery_critical_low_act);
	lowbat_add_scenario(battery_info.poweroff, battery_info.realoff, battery_power_off_act);
	lowbat_add_scenario(battery_info.normal, battery_info.critical, battery_critical_low_act);
	lowbat_add_scenario(battery_info.warning, battery_info.poweroff, battery_critical_low_act);
	lowbat_add_scenario(battery_info.critical, battery_info.realoff, battery_power_off_act);
	lowbat_add_scenario(battery_info.normal, battery_info.poweroff, battery_critical_low_act);
	lowbat_add_scenario(battery_info.warning, battery_info.realoff, battery_power_off_act);
	lowbat_add_scenario(battery_info.normal, battery_info.realoff, battery_power_off_act);
	lowbat_add_scenario(battery_info.realoff, battery_info.realoff, battery_power_off_act);
	lowbat_add_scenario(battery_info.realoff, battery_info.normal, battery_check_act);
	lowbat_add_scenario(battery_info.realoff, battery_info.warning, battery_check_act);
	lowbat_add_scenario(battery_info.realoff, battery_info.critical, battery_check_act);
	lowbat_add_scenario(battery_info.realoff, battery_info.poweroff, battery_check_act);
}

static void change_lowbat_level(int bat_percent)
{
	int prev, now;

	if (cur_bat_capacity == bat_percent)
		return;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_LEVEL_STATUS, &prev) < 0) {
		_E("vconf_get_int() failed");
		return;
	}


	if (bat_percent > BATTERY_LEVEL_CHECK_FULL) {
		now = VCONFKEY_SYSMAN_BAT_LEVEL_FULL;
	} else if (bat_percent > BATTERY_LEVEL_CHECK_HIGH) {
		now = VCONFKEY_SYSMAN_BAT_LEVEL_HIGH;
	} else if (bat_percent > BATTERY_LEVEL_CHECK_LOW) {
		now = VCONFKEY_SYSMAN_BAT_LEVEL_LOW;
	} else if (bat_percent > BATTERY_LEVEL_CHECK_CRITICAL) {
		now = VCONFKEY_SYSMAN_BAT_LEVEL_CRITICAL;
	} else {
		now = VCONFKEY_SYSMAN_BAT_LEVEL_EMPTY;
	}

	if (prev != now)
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_LEVEL_STATUS, now);
}

static int lowbat_process(int bat_percent, void *ad)
{
	int new_bat_capacity;
	int new_bat_state;
	int vconf_state = -1;
	int i, ret = 0;
	int status = -1;
	bool low_bat = false;
	bool full_bat = false;
#ifdef MICRO_DD
	int extreme = 0;
#endif
	int result = 0;
	int lock = -1;

	new_bat_capacity = bat_percent;
	if (new_bat_capacity < 0)
		return -EINVAL;
	change_lowbat_level(new_bat_capacity);

	if (new_bat_capacity != cur_bat_capacity) {
		_D("[BAT_MON] cur = %d new = %d", cur_bat_capacity, new_bat_capacity);
		if (vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CAPACITY, new_bat_capacity) == 0)
			cur_bat_capacity = new_bat_capacity;
	}

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &vconf_state) < 0) {
		_E("vconf_get_int() failed");
		result = -EIO;
		goto exit;
	}

	if (new_bat_capacity <= battery_info.realoff) {
		if (battery.charge_now) {
			new_bat_state = battery_info.poweroff;
			if (vconf_state != VCONFKEY_SYSMAN_BAT_POWER_OFF)
				status = VCONFKEY_SYSMAN_BAT_POWER_OFF;
		} else {
			new_bat_state = battery_info.realoff;
			if (vconf_state != VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF)
				status = VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF;
		}
	} else if (new_bat_capacity <= battery_info.poweroff) {
		new_bat_state = battery_info.poweroff;
		if (vconf_state != VCONFKEY_SYSMAN_BAT_POWER_OFF)
			status = VCONFKEY_SYSMAN_BAT_POWER_OFF;
	} else if (new_bat_capacity <= battery_info.critical) {
		new_bat_state = battery_info.critical;
		if (vconf_state != VCONFKEY_SYSMAN_BAT_CRITICAL_LOW)
			status = VCONFKEY_SYSMAN_BAT_CRITICAL_LOW;
	} else if (new_bat_capacity <= battery_info.warning) {
		new_bat_state = battery_info.warning;
		if (vconf_state != VCONFKEY_SYSMAN_BAT_WARNING_LOW)
			status = VCONFKEY_SYSMAN_BAT_WARNING_LOW;
	} else {
		new_bat_state = battery_info.normal;
		if (new_bat_capacity == BATTERY_FULL) {
			if (battery.charge_full) {
				if (vconf_state != VCONFKEY_SYSMAN_BAT_FULL)
					status = VCONFKEY_SYSMAN_BAT_FULL;
				full_bat = true;
			} else {
				if (vconf_state != VCONFKEY_SYSMAN_BAT_NORMAL)
					status = VCONFKEY_SYSMAN_BAT_NORMAL;
			}
		} else {
			if (vconf_state != VCONFKEY_SYSMAN_BAT_NORMAL)
				status = VCONFKEY_SYSMAN_BAT_NORMAL;
		}
	}

	if (status != -1) {
		lock = pm_lock_internal(INTERNAL_LOCK_BATTERY, LCD_OFF, STAY_CUR_STATE, 0);
		ret = vconf_set_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, status);
		power_supply_broadcast(CHARGE_LEVEL_SIGNAL, status);
		if (update_pm_setting)
			update_pm_setting(SETTING_LOW_BATT, status);
	}

	if (ret < 0) {
		result = -EIO;
		goto exit;
	}
#ifdef MICRO_DD
	if (vconf_get_int(VCONFKEY_PM_KEY_IGNORE, &extreme) == 0 && extreme == TRUE &&
	    (status > VCONFKEY_SYSMAN_BAT_POWER_OFF && status <= VCONFKEY_SYSMAN_BAT_FULL))
		if (vconf_set_int(VCONFKEY_PM_KEY_IGNORE, FALSE) == 0)
			_I("release key ignore");
#endif
	if (new_bat_capacity <= battery_info.warning)
		low_bat = true;

	device_notify(DEVICE_NOTIFIER_LOWBAT, (void*)low_bat);

	if (cur_bat_state == new_bat_state && new_bat_state > battery_info.realoff)
		goto exit;

	if (cur_bat_state == BATTERY_UNKNOWN)
		cur_bat_state = battery_info.normal;
	result = lowbat_scenario(cur_bat_state, new_bat_state, NULL);
	if (result)
		_I("cur %d, new %d(capacity %d)",
		cur_bat_state, new_bat_state, bat_percent);
	cur_bat_state = new_bat_state;
exit:
	if (lock == 0)
		pm_unlock_internal(INTERNAL_LOCK_BATTERY, LCD_OFF, PM_SLEEP_MARGIN);

	return result;
}

static int check_lowbat_percent(int *pct)
{
	int bat_percent;

	bat_percent = battery.capacity;
	if (bat_percent < 0) {
		_E("[BATMON] Cannot read battery gage. stop read fuel gage");
		return -ENODEV;
	}
	if (bat_percent > 100)
		bat_percent = 100;
	change_lowbat_level(bat_percent);
	*pct = bat_percent;
	return 0;
}

void lowbat_monitor(void *data)
{
	int bat_percent, r;

	r = lowbat_initialized(NULL);
	if (!r)
		return;

	if (data == NULL) {
		r = check_lowbat_percent(&bat_percent);
		if (r < 0)
			return;
	} else
		bat_percent = *(int *)data;
	print_lowbat_state(bat_percent);
	lowbat_process(bat_percent, NULL);
}

static int lowbat_monitor_init(void *data)
{
	int status = 1;

	lowbat_initialized(&status);
	unregister_notifier(DEVICE_NOTIFIER_POWER_SUPPLY, lowbat_monitor_init);
	battery_config_load(&battery_info);
	_I("%d %d %d %d %d", battery_info.normal, battery_info.warning,
		battery_info.critical, battery_info.poweroff, battery_info.realoff);
	lowbat_scenario_init();
	check_lowbat_percent(&battery.capacity);
	lowbat_process(battery.capacity, NULL);
	return 0;
}

static int display_changed(void *data)
{
	if (battery.charge_now == CHARGER_ABNORMAL &&
	    battery.health == HEALTH_BAD)
		pm_lock_internal(INTERNAL_LOCK_POPUP, LCD_DIM, STAY_CUR_STATE, 0);

	return 0;
}

static int lowbat_probe(void *data)
{
	/**
	 * find power-supply class.
	 * if there is no power-supply class,
	 * deviced does not activate a battery module.
	 */
	if (access(POWER_PATH, R_OK) != 0) {
		/**
		 * Set battery vconf as -ENOTSUP
		 * These vconf key used by runtime-info and capi-system-device.
		 */
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS, -ENOTSUP);
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, -ENOTSUP);
		vconf_set_int(VCONFKEY_SYSMAN_BATTERY_LEVEL_STATUS, -ENOTSUP);

		_E("there is no power-supply class");
		return -ENODEV;
	}

	return 0;
}

static void lowbat_init(void *data)
{
	/* process check battery timer until booting done */
	power_supply_timer_start();

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_POWER_SUPPLY, lowbat_monitor_init);
	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);
}

static void lowbat_exit(void *data)
{
	int status = 0;

	lowbat_initialized(&status);
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);

	/* exit power supply */
	power_supply_exit(NULL);
}

static const struct device_ops lowbat_device_ops = {
	.name     = "lowbat",
	.probe    = lowbat_probe,
	.init     = lowbat_init,
	.exit     = lowbat_exit,
};

DEVICE_OPS_REGISTER(&lowbat_device_ops)
