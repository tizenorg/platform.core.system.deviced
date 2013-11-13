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
#include <heynoti.h>
#include <vconf.h>
#include <fcntl.h>
#include <bundle.h>

#include "core/log.h"
#include "core/launch.h"
#include "core/noti.h"
#include "core/queue.h"
#include "core/data.h"
#include "core/devices.h"
#include "core/device-handler.h"
#include "device-node.h"
#include "display/setting.h"

#define PREDEF_LOWBAT			"lowbat"

#define BAT_MON_INTERVAL		30
#define BAT_MON_INTERVAL_MIN	2

#define BATTERY_CHARGING		65535
#define BATTERY_UNKNOWN		-1
#define BATTERY_FULL			100
#define BATTERY_NORMAL			100
#define BATTERY_WARNING_LOW	15
#define BATTERY_CRITICAL_LOW	5
#define BATTERY_POWER_OFF		1
#define BATTERY_REAL_POWER_OFF	0

#define MAX_BATTERY_ERROR		10
#define RESET_RETRY_COUNT		3

#define BATTERY_LEVEL_CHECK_FULL	95
#define BATTERY_LEVEL_CHECK_HIGH	15
#define BATTERY_LEVEL_CHECK_LOW		5
#define BATTERY_LEVEL_CHECK_CRITICAL	1

#define LOWBAT_OPT_WARNING		1
#define LOWBAT_OPT_POWEROFF		2
#define LOWBAT_OPT_CHARGEERR		3
#define LOWBAT_OPT_CHECK		4

#define WARNING_LOW_BAT_ACT		"warning_low_bat_act"
#define CRITICAL_LOW_BAT_ACT		"critical_low_bat_act"
#define POWER_OFF_BAT_ACT		"power_off_bat_act"
#define CHARGE_BAT_ACT			"charge_bat_act"
#define CHARGE_CHECK_ACT			"charge_check_act"
#define CHARGE_ERROR_ACT			"charge_error_act"

#define _SYS_LOW_POWER "LOW_POWER"

#define WAITING_INTERVAL	10

struct lowbat_process_entry {
	unsigned cur_bat_state;
	unsigned new_bat_state;
	int (*action) (void *);
};

static Ecore_Timer *lowbat_timer;
static int cur_bat_state = BATTERY_UNKNOWN;
static int cur_bat_capacity = -1;

static int bat_err_count = 0;

static Ecore_Timer *lowbat_popup_id = NULL;
static int lowbat_popup_option = 0;

int battery_warning_low_act(void *ad);
int battery_critical_low_act(void *ad);
int battery_power_off_act(void *ad);

static struct lowbat_process_entry lpe[] = {
	{BATTERY_NORMAL, BATTERY_WARNING_LOW, battery_warning_low_act},
	{BATTERY_WARNING_LOW, BATTERY_CRITICAL_LOW, battery_critical_low_act},
	{BATTERY_CRITICAL_LOW,	BATTERY_POWER_OFF,		battery_critical_low_act},
	{BATTERY_POWER_OFF,		BATTERY_REAL_POWER_OFF,	battery_power_off_act},
	{BATTERY_NORMAL, BATTERY_CRITICAL_LOW, battery_critical_low_act},
	{BATTERY_WARNING_LOW,	BATTERY_POWER_OFF,		battery_critical_low_act},
	{BATTERY_CRITICAL_LOW,	BATTERY_REAL_POWER_OFF,	battery_power_off_act},
	{BATTERY_NORMAL,		BATTERY_POWER_OFF,		battery_critical_low_act},
	{BATTERY_WARNING_LOW,	BATTERY_REAL_POWER_OFF,	battery_power_off_act},
	{BATTERY_NORMAL,		BATTERY_REAL_POWER_OFF,	battery_power_off_act},
};

/*
 * TODO: remove this function
 */
static void print_lowbat_state(unsigned int bat_percent)
{
#if 0
	int i;
	for (i = 0; i < BAT_MON_SAMPLES; i++)
		_D("\t%d", recent_bat_percent[i]);
#endif
}

int battery_warning_low_act(void *data)
{
	char lowbat_noti_name[NAME_MAX];

	heynoti_get_snoti_name(_SYS_LOW_POWER, lowbat_noti_name, NAME_MAX);
	ss_noti_send(lowbat_noti_name);

	action_entry_call_internal(PREDEF_LOWBAT, 1, WARNING_LOW_BAT_ACT);
	return 0;
}

int battery_critical_low_act(void *data)
{
	action_entry_call_internal(PREDEF_LOWBAT, 1, CRITICAL_LOW_BAT_ACT);
	return 0;
}

int battery_power_off_act(void *data)
{
	action_entry_call_internal(PREDEF_LOWBAT, 1,	POWER_OFF_BAT_ACT);
	return 0;
}

int battery_charge_err_act(void *data)
{
	action_entry_call_internal(PREDEF_LOWBAT, 1, CHARGE_ERROR_ACT);
	return 0;
}


static int battery_charge_act(void *data)
{
	return 0;
}

int ss_lowbat_set_charge_on(int on)
{
	static bool state = -1;

	if (state == on)
		return 0;
	if (CONNECTED(on))
		extcon_set_count(EXTCON_TA);
	if (vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, on) != 0) {
		_E("fail to set charge vconf value");
		return -EPERM;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_CHARGING, on);

	state = on;
	return 0;
}

int ss_lowbat_is_charge_in_now(void)
{
	int val = 0;
	if (device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_NOW, &val) < 0) {
		_E("fail to read charge now from kernel");
		ss_lowbat_set_charge_on(0);
		return 0;
	}

	if (val == 1) {
		ss_lowbat_set_charge_on(1);
		return 1;
	} else {
		ss_lowbat_set_charge_on(0);
		return 0;
	}
}

static int lowbat_process(int bat_percent, void *ad)
{
	int new_bat_capacity;
	int new_bat_state;
	int vconf_state = -1;
	int bat_full = -1;
	int i, ret = 0;
	int val = 0;
	int status = -1;

	new_bat_capacity = bat_percent;
	if (new_bat_capacity < 0)
		return -1;

	if (new_bat_capacity != cur_bat_capacity) {
		_D("[BAT_MON] cur = %d new = %d", cur_bat_capacity, new_bat_capacity);
		if (vconf_set_int(VCONFKEY_SYSMAN_BATTERY_CAPACITY, new_bat_capacity) == 0)
			cur_bat_capacity = new_bat_capacity;
	}

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &vconf_state) < 0) {
		_E("vconf_get_int() failed");
		return -1;
	}

	if (new_bat_capacity <= BATTERY_REAL_POWER_OFF) {
		if (device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_NOW, &val) < 0) {
			_E("fail to read charge now from kernel");
		}
		_D("charge_now status %d",val);
		if (val == 1) {
			new_bat_state = BATTERY_POWER_OFF;
			if (vconf_state != VCONFKEY_SYSMAN_BAT_POWER_OFF)
				status = VCONFKEY_SYSMAN_BAT_POWER_OFF;
		} else {
			new_bat_state = BATTERY_REAL_POWER_OFF;
			if (vconf_state != VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF)
				status = VCONFKEY_SYSMAN_BAT_REAL_POWER_OFF;
		}
	} else if (new_bat_capacity <= BATTERY_POWER_OFF) {
		new_bat_state = BATTERY_POWER_OFF;
		if (vconf_state != VCONFKEY_SYSMAN_BAT_POWER_OFF)
			status = VCONFKEY_SYSMAN_BAT_POWER_OFF;
	} else if (new_bat_capacity <= BATTERY_CRITICAL_LOW) {
		new_bat_state = BATTERY_CRITICAL_LOW;
		if (vconf_state != VCONFKEY_SYSMAN_BAT_CRITICAL_LOW)
			status = VCONFKEY_SYSMAN_BAT_CRITICAL_LOW;
	} else if (new_bat_capacity <= BATTERY_WARNING_LOW) {
		new_bat_state = BATTERY_WARNING_LOW;
		if (vconf_state != VCONFKEY_SYSMAN_BAT_WARNING_LOW)
			status = VCONFKEY_SYSMAN_BAT_WARNING_LOW;
	} else {
		new_bat_state = BATTERY_NORMAL;
		if (new_bat_capacity == BATTERY_FULL) {
			device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_FULL, &bat_full);
			if (bat_full == 1) {
				if (vconf_state != VCONFKEY_SYSMAN_BAT_FULL)
					status = VCONFKEY_SYSMAN_BAT_FULL;
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
		ret = vconf_set_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, status);
		if (update_pm_setting)
			update_pm_setting(SETTING_LOW_BATT, status);
	}

	if (ret < 0)
		return -1;

	ss_lowbat_is_charge_in_now();

	if (cur_bat_state == new_bat_state) {
		return 0;
	}

	if (cur_bat_state == BATTERY_UNKNOWN) {
		for (i = 0;
		     i < sizeof(lpe) / sizeof(struct lowbat_process_entry);
		     i++) {
			if (new_bat_state == lpe[i].new_bat_state) {
				lpe[i].action(ad);
				cur_bat_state = new_bat_state;
				return 0;
			}
		}
	} else {
		for (i = 0;
		     i < sizeof(lpe) / sizeof(struct lowbat_process_entry);
		     i++) {
			if ((cur_bat_state == lpe[i].cur_bat_state)
			    && (new_bat_state == lpe[i].new_bat_state)) {
				lpe[i].action(ad);
				cur_bat_state = new_bat_state;
				return 0;
			}
		}
	}
	_D("[BATMON] Unknown battery state cur:%d new:%d", cur_bat_state, new_bat_state);
	cur_bat_state = new_bat_state;
	return -1;
}

static int lowbat_read()
{
	int bat_percent;

	device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY, &bat_percent);

	return bat_percent;
}

static void __ss_change_lowbat_level(int bat_percent)
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

static int __check_lowbat_percent(void)
{
	int bat_percent;

	bat_percent = lowbat_read();
	if (bat_percent < 0) {
		ecore_timer_interval_set(lowbat_timer, BAT_MON_INTERVAL_MIN);
		bat_err_count++;
		if (bat_err_count > MAX_BATTERY_ERROR) {
			_E("[BATMON] Cannot read battery gage. stop read fuel gage");
			return 0;
		}
		return 1;
	}
	if (bat_percent > 100)
		bat_percent = 100;
	__ss_change_lowbat_level(bat_percent);
	return bat_percent;
}

Eina_Bool ss_lowbat_monitor(void *data)
{
	int bat_percent;
	struct ss_main_data *ad = (struct ss_main_data *)data;

	bat_percent = __check_lowbat_percent();
	print_lowbat_state(bat_percent);

	if (lowbat_process(bat_percent, ad) < 0)
		ecore_timer_interval_set(lowbat_timer, BAT_MON_INTERVAL_MIN);
	else
		ecore_timer_interval_set(lowbat_timer, BAT_MON_INTERVAL);

	return EINA_TRUE;
}

static int wakeup_cb(keynode_t *key_nodes, void *data)
{
	int pm_state = 0;

	if ((pm_state =
	     vconf_keynode_get_int(key_nodes)) == VCONFKEY_PM_STATE_LCDOFF)
		ss_lowbat_monitor(NULL);

	return 0;
}

/* for debugging (request by kernel) */
static int check_battery()
{
	int r;
	int ret = -1;

	if (device_get_property(DEVICE_TYPE_POWER, PROP_POWER_PRESENT, &ret) < 0) {
		_E("FAIL: device_get_property(): [BATMON] battery check : %d", ret);
	}
	_D("[BATMON] battery check : %d", ret);

	return ret;
}

static Eina_Bool lowbat_popup(void *data)
{
	int ret = -1, state = 0;
	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &state);
	if (state == 1 || ret != 0) {
		bundle *b = NULL;
		b = bundle_create();
		if (lowbat_popup_option == LOWBAT_OPT_WARNING || lowbat_popup_option == LOWBAT_OPT_CHECK) {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "warning");
		} else if(lowbat_popup_option == LOWBAT_OPT_POWEROFF) {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "poweroff");
		} else if(lowbat_popup_option == LOWBAT_OPT_CHARGEERR) {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "chargeerr");
		} else
			goto out;

		ret = syspopup_launch("lowbat-syspopup", b);
		if (ret < 0) {
			_E("popup lauch failed");
			bundle_free(b);
			return EINA_TRUE;
		}
out:
		if (lowbat_popup_id != NULL) {
			ecore_timer_del(lowbat_popup_id);
			lowbat_popup_id = NULL;
		}
		lowbat_popup_option = 0;
		bundle_free(b);
		return EINA_FALSE;
	}
	_D("boot-animation running yet");
	return EINA_FALSE;
}

int lowbat_def_predefine_action(int argc, char **argv)
{
	int ret, state=0;
	char argstr[128];
	char* option = NULL;

	if (argc < 1)
		return -1;

	if (lowbat_popup_id != NULL) {
		ecore_timer_del(lowbat_popup_id);
		lowbat_popup_id = NULL;
	}

	bundle *b = NULL;
	b = bundle_create();
	if (!strcmp(argv[0],CRITICAL_LOW_BAT_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "warning");
		lowbat_popup_option = LOWBAT_OPT_CHECK;
	} else if(!strcmp(argv[0],WARNING_LOW_BAT_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "warning");
		lowbat_popup_option = LOWBAT_OPT_WARNING;
	} else if(!strcmp(argv[0],POWER_OFF_BAT_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "poweroff");
		lowbat_popup_option = LOWBAT_OPT_POWEROFF;
	} else if(!strcmp(argv[0],CHARGE_ERROR_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "chargeerr");
		lowbat_popup_option = LOWBAT_OPT_CHARGEERR;
	}

	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &state);
	if (state == 1 || ret != 0) {
		if (predefine_control_launch("lowbat-syspopup", b, lowbat_popup_option) < 0) {
				_E("popup lauch failed\n");
				bundle_free(b);
				lowbat_popup_option = 0;
				return -1;
		}
	} else {
		_D("boot-animation running yet");
		lowbat_popup_id = ecore_timer_add(WAITING_INTERVAL, lowbat_popup, NULL);
	}
	bundle_free(b);
	return 0;
}

static void lowbat_init(void *data)
{
	struct ss_main_data *ad = (struct ss_main_data*)data;

	action_entry_add_internal(PREDEF_LOWBAT, lowbat_def_predefine_action,
				     NULL, NULL);

	/* need check battery */
	lowbat_timer =
		ecore_timer_add(BAT_MON_INTERVAL_MIN, ss_lowbat_monitor, ad);

	__check_lowbat_percent();

	ss_lowbat_is_charge_in_now();

	vconf_notify_key_changed(VCONFKEY_PM_STATE, (void *)wakeup_cb, NULL);
}

const struct device_ops lowbat_device_ops = {
	.init = lowbat_init,
};
