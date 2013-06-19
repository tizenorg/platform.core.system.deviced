/*
 * power-manager
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


#include <stdio.h>
#include <stdlib.h>
#include <util.h>
#include "setting.h"
#include "conf.h"

static const char *setting_keys[SETTING_GET_END] = {
	[SETTING_TO_NORMAL] = VCONFKEY_SETAPPL_LCD_TIMEOUT_NORMAL,
	[SETTING_BRT_LEVEL] = VCONFKEY_SETAPPL_LCD_BRIGHTNESS,
	[SETTING_LOCK_SCREEN] = VCONFKEY_IDLE_LOCK_STATE,
	[SETTING_POWER_SAVING] = VCONFKEY_SETAPPL_PWRSV_SYSMODE_STATUS,
	[SETTING_POWER_SAVING_DISPLAY] = VCONFKEY_SETAPPL_PWRSV_CUSTMODE_DISPLAY,
};

static int lock_screen_state = VCONFKEY_IDLE_UNLOCK;

int (*update_pm_setting) (int key_idx, int val);

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
	return vconf_set_int(VCONFKEY_PM_STATE, val);
}

int get_setting_brightness(int *level)
{
	return vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, level);
}

int get_run_timeout(int *timeout)
{
	int dim_timeout = -1, vconf_timeout = -1, ret;
	get_dim_timeout(&dim_timeout);

	if(dim_timeout < 0) {
		LOGERR("Can not get dim timeout. set default 5 seconds");
		dim_timeout = 5;
	}

	ret = vconf_get_int(setting_keys[SETTING_TO_NORMAL], &vconf_timeout);

	if(vconf_timeout == 0)
		*timeout = 0; //timeout 0 : Always ON (Do not apply dim_timeout)
	else
		*timeout = vconf_timeout - dim_timeout;
	return ret;

}

int get_dim_timeout(int *timeout)
{
	char buf[255];
	/* TODO if needed */
	*timeout = 5;		/* default timeout */
	get_env("PM_TO_LCDDIM", buf, sizeof(buf));
	LOGINFO("Get lcddim timeout [%s]", buf);
	*timeout = atoi(buf);
	return 0;
}

int get_off_timeout(int *timeout)
{
	char buf[255];
	/* TODO if needed */
	*timeout = 5;		/* default timeout */
	get_env("PM_TO_LCDOFF", buf, sizeof(buf));
	LOGINFO("Get lcdoff timeout [%s]", buf);
	*timeout = atoi(buf);
	return 0;
}

static int setting_cb(keynode_t *key_nodes, void *data)
{
	keynode_t *tmp = key_nodes;

	if ((int)data > SETTING_END) {
		LOGERR("Unknown setting key: %s, idx=%d",
		       vconf_keynode_get_name(tmp), (int)data);
		return -1;
	}
	if (update_pm_setting != NULL) {
		switch((int)data) {
			case SETTING_POWER_SAVING:
			case SETTING_POWER_SAVING_DISPLAY:
				update_pm_setting((int)data, vconf_keynode_get_bool(tmp));
				break;
			default:
				update_pm_setting((int)data, vconf_keynode_get_int(tmp));
				break;
		}
	}

	return 0;
}

int init_setting(int (*func) (int key_idx, int val))
{
	int i;

	if (func != NULL)
		update_pm_setting = func;

	for (i = SETTING_BEGIN; i < SETTING_GET_END; i++) {
		vconf_notify_key_changed(setting_keys[i], (void *)setting_cb,
					 (void *)i);
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

