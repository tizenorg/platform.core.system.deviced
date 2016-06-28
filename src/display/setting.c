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
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <bundle.h>
#include <eventsystem.h>

#include "core.h"
#include "util.h"
#include "setting.h"

#define LCD_DIM_RATIO		0.3
#define LCD_MAX_DIM_TIMEOUT	7000
#define LCD_MIN_DIM_TIMEOUT	500

static const char *setting_keys[SETTING_GET_END] = {
	[SETTING_TO_NORMAL] = VCONFKEY_SETAPPL_LCD_TIMEOUT_NORMAL,
	[SETTING_BRT_LEVEL] = VCONFKEY_SETAPPL_LCD_BRIGHTNESS,
	[SETTING_LOCK_SCREEN] = VCONFKEY_IDLE_LOCK_STATE,
	[SETTING_POWER_CUSTOM_BRIGHTNESS] = VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS,
};

static int lock_screen_state = VCONFKEY_IDLE_UNLOCK;
static bool lock_screen_bg_state = false;
static int force_lcdtimeout = 0;
static int custom_on_timeout = 0;
static int custom_normal_timeout = 0;
static int custom_dim_timeout = 0;

int (*update_pm_setting) (int key_idx, int val);

static void display_state_send_system_event(int state)
{
	bundle *b;
	const char *str;

	if (state == S_NORMAL)
		str = EVT_VAL_DISPLAY_NORMAL;
	else if (state == S_LCDDIM)
		str = EVT_VAL_DISPLAY_DIM;
	else if (state == S_LCDOFF)
		str = EVT_VAL_DISPLAY_OFF;
	else
		return;

	_I("eventsystem (%s)", str);

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_DISPLAY_STATE, str);
	eventsystem_send_system_event(SYS_EVENT_DISPLAY_STATE, b);
	bundle_free(b);
}

int set_force_lcdtimeout(int timeout)
{
	if (timeout < 0)
		return -EINVAL;

	force_lcdtimeout = timeout;

	return 0;
}

int get_lock_screen_state(void)
{
	return lock_screen_state;
}

void set_lock_screen_state(int state)
{
	switch (state) {
	case VCONFKEY_IDLE_LOCK:
	case VCONFKEY_IDLE_UNLOCK:
		lock_screen_state = state;
		break;
	default:
		lock_screen_state = VCONFKEY_IDLE_UNLOCK;
	}
}

int get_lock_screen_bg_state(void)
{
	return lock_screen_bg_state;
}

void set_lock_screen_bg_state(bool state)
{
	_I("state is %d", state);
	lock_screen_bg_state = state;
}

int get_charging_status(int *val)
{
	return vconf_get_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, val);
}

int get_lowbatt_status(int *val)
{
	return vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, val);
}

int get_usb_status(int *val)
{
	return vconf_get_int(VCONFKEY_SYSMAN_USB_STATUS, val);
}

int set_setting_pmstate(int val)
{
	display_state_send_system_event(val);
	return vconf_set_int(VCONFKEY_PM_STATE, val);
}

int get_setting_brightness(int *level)
{
	return vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, level);
}

void get_dim_timeout(int *dim_timeout)
{
	int vconf_timeout, on_timeout, val, ret;

	if (custom_dim_timeout > 0) {
		*dim_timeout = custom_dim_timeout;
		return;
	}

	ret = vconf_get_int(setting_keys[SETTING_TO_NORMAL], &vconf_timeout);
	if (ret != 0) {
		_E("Failed ro get setting timeout!");
		vconf_timeout = DEFAULT_NORMAL_TIMEOUT;
	}

	if (force_lcdtimeout > 0)
		on_timeout = SEC_TO_MSEC(force_lcdtimeout);
	else
		on_timeout = SEC_TO_MSEC(vconf_timeout);

	val = (double)on_timeout * LCD_DIM_RATIO;
	if (val > LCD_MAX_DIM_TIMEOUT)
		val = LCD_MAX_DIM_TIMEOUT;

	*dim_timeout = val;
}

void get_run_timeout(int *timeout)
{
	int dim_timeout = -1;
	int vconf_timeout = -1;
	int on_timeout;
	int ret;

	if (custom_normal_timeout > 0) {
		*timeout = custom_normal_timeout;
		return;
	}

	ret = vconf_get_int(setting_keys[SETTING_TO_NORMAL], &vconf_timeout);
	if (ret != 0) {
		_E("Failed ro get setting timeout!");
		vconf_timeout = DEFAULT_NORMAL_TIMEOUT;
	}

	if (force_lcdtimeout > 0)
		on_timeout = SEC_TO_MSEC(force_lcdtimeout);
	else
		on_timeout = SEC_TO_MSEC(vconf_timeout);

	if (on_timeout == 0) {
		*timeout = on_timeout;
		return;
	}

	get_dim_timeout(&dim_timeout);
	*timeout = on_timeout - dim_timeout;
}

int set_custom_lcdon_timeout(int timeout)
{
	int changed = (custom_on_timeout == timeout ? false : true);

	custom_on_timeout = timeout;

	if (timeout <= 0) {
		custom_normal_timeout = 0;
		custom_dim_timeout = 0;
		return changed;
	}

	custom_dim_timeout = (double)timeout * LCD_DIM_RATIO;
	custom_normal_timeout = timeout - custom_dim_timeout;

	_I("custom normal(%d), dim(%d)", custom_normal_timeout,
	    custom_dim_timeout);

	return changed;
}

static int setting_cb(keynode_t *key_nodes, void *data)
{
	keynode_t *tmp = key_nodes;
	int index;

	index = (int)((intptr_t)data);
	if (index > SETTING_END) {
		_E("Unknown setting key: %s, idx=%d",
		       vconf_keynode_get_name(tmp), index);
		return -1;
	}
	if (update_pm_setting != NULL) {
		update_pm_setting(index, vconf_keynode_get_int(tmp));
	}

	return 0;
}

int init_setting(int (*func) (int key_idx, int val))
{
	int i;

	if (func != NULL)
		update_pm_setting = func;

	for (i = SETTING_BEGIN; i < SETTING_GET_END; i++) {
		/*
		 * To pass an index data through the vconf infratstructure
		 * without memory allocation, an index data becomes typecast
		 * to proper pointer size on each architecture.
		 */
		vconf_notify_key_changed(setting_keys[i], (void *)setting_cb,
					 (void *)((intptr_t)i));
	}

	return 0;
}

int exit_setting(void)
{
	int i;
	for (i = SETTING_BEGIN; i < SETTING_GET_END; i++) {
		vconf_ignore_key_changed(setting_keys[i], (void *)setting_cb);
	}

	return 0;
}

