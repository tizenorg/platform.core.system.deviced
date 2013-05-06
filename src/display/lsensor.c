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
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vconf.h>
#include <sensor.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "device-node.h"

#define SAMPLING_INTERVAL	2	/* 2 sec */
#define MAX_FAULT		5

static int (*prev_init_extention) (void *data);
static int (*_default_action) (int);
static Ecore_Timer *alc_timeout_id = 0;
static int sf_handle = -1;
static int fault_count = 0;
static int power_saving_display_stat = 0;

static bool alc_handler(void* data)
{
	int value = 0;
	static int cur_index = 1;
	static int old_index = 1;

	sensor_data_t light_data;
	if (pm_cur_state != S_NORMAL){
		if (alc_timeout_id > 0)
			ecore_timer_del(alc_timeout_id);
		alc_timeout_id = NULL;
	} else {
		if (sf_get_data(sf_handle, LIGHT_BASE_DATA_SET, &light_data) < 0) {
			fault_count++;
		} else {
			if (light_data.values[0] < 0.0 || light_data.values[0] > 10.0) {
				_I("fail to load light data : %d",	(int)light_data.values[0]);
				fault_count++;
			} else {
				int tmp_value;
				int cmd;
				int ret;
				value = PM_MAX_BRIGHTNESS * (int)light_data.values[0] / 10;
				cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
				ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &tmp_value);
				if (!ret && (tmp_value != value)) {
					backlight_ops.set_default_brt(value);
					backlight_ops.restore();
				}
				_I("load light data : %d, brightness : %d", (int)light_data.values[0], value);
			}
		}
	}

	if ((fault_count > MAX_FAULT) && !(pm_status_flag & PWROFF_FLAG)) {
		if (alc_timeout_id > 0)
			ecore_timer_del(alc_timeout_id);
		alc_timeout_id = NULL;
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
		    SETTING_BRIGHTNESS_AUTOMATIC_OFF);
		_E("Fault counts is over %d, disable automatic brightness",
		    MAX_FAULT);
		return EINA_FALSE;
	}

	if (alc_timeout_id != 0)
		return EINA_TRUE;

	return EINA_FALSE;
}

static int alc_action(int timeout)
{
	_I("alc action");
	/* sampling timer add */
	if (alc_timeout_id == 0 && !(pm_status_flag & PWRSV_FLAG))
		alc_timeout_id =
		    ecore_timer_add(SAMPLING_INTERVAL,
			    (Ecore_Task_Cb)alc_handler, NULL);

	if (_default_action != NULL)
		return _default_action(timeout);

	/* unreachable code */
	return -1;
}

static int connect_sfsvc(void)
{
	int sf_state = -1;
	/* connect with sensor fw */
	_I("connect with sensor fw");
	sf_handle = sf_connect(LIGHT_SENSOR);
	if (sf_handle < 0) {
		_E("sensor attach fail");
		return -1;
	}
	sf_state = sf_start(sf_handle, 0);
	if (sf_state < 0) {
		_E("sensor attach fail");
		sf_disconnect(sf_handle);
		sf_handle = -1;
		return -2;
	}
	fault_count = 0;
	return 0;
}

static int disconnect_sfsvc(void)
{
	_I("disconnect with sensor fw");
	if(sf_handle >= 0)
	{
		sf_stop(sf_handle);
		sf_disconnect(sf_handle);
		sf_handle = -1;
	}

	if (_default_action != NULL) {
		states[S_NORMAL].action = _default_action;
		_default_action = NULL;
	}
	if (alc_timeout_id > 0) {
		ecore_timer_del(alc_timeout_id);
		alc_timeout_id = NULL;
	}

	return 0;
}

static inline void set_brtch_state(void)
{
	if (pm_status_flag & PWRSV_FLAG) {
		pm_status_flag |= BRTCH_FLAG;
		vconf_set_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM, true);
		_I("brightness changed in low battery,"
		    "escape dim state (light)");
	}
}

static int set_alc_function(keynode_t *key_nodes, void *data)
{
	int onoff = 0;
	int ret = -1;
	int brt = -1;
	int default_brt = -1;
	int max_brt = -1;

	if (key_nodes == NULL) {
		_E("wrong parameter, key_nodes is null");
		return -1;
	}

	onoff = vconf_keynode_get_int(key_nodes);

	if (onoff == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		if(connect_sfsvc() < 0)
			return -1;
		/* escape dim state if it's in low battery.*/
		set_brtch_state();

		/* change alc action func */
		if (_default_action == NULL)
			_default_action = states[S_NORMAL].action;
		states[S_NORMAL].action = alc_action;
		alc_timeout_id =
		    ecore_timer_add(SAMPLING_INTERVAL,
			    (Ecore_Task_Cb)alc_handler, NULL);
	} else if (onoff == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		_I("auto brightness paused!");
		disconnect_sfsvc();
	} else {
		disconnect_sfsvc();
		/* escape dim state if it's in low battery.*/
		set_brtch_state();

		ret = get_setting_brightness(&default_brt);
		if (ret != 0 || (default_brt < PM_MIN_BRIGHTNESS || default_brt > PM_MAX_BRIGHTNESS)) {
			_I("fail to read vconf value for brightness");
			brt = PM_DEFAULT_BRIGHTNESS;
			if(default_brt < PM_MIN_BRIGHTNESS || default_brt > PM_MAX_BRIGHTNESS)
				vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt);
			default_brt = brt;
		}

		backlight_ops.set_default_brt(default_brt);
		backlight_ops.restore();
	}

	return 0;
}

static bool check_sfsvc(void* data)
{
	/* this function will return opposite value for re-callback in fail */
	int vconf_auto;
	int sf_state = 0;

	_I("register sfsvc");

	vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &vconf_auto);
	if (vconf_auto == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		if(connect_sfsvc() < 0)
			return EINA_TRUE;

		/* change alc action func */
		if (_default_action == NULL)
			_default_action = states[S_NORMAL].action;
		states[S_NORMAL].action = alc_action;
		alc_timeout_id =
		    ecore_timer_add(SAMPLING_INTERVAL,
			    (Ecore_Task_Cb)alc_handler, NULL);
		if (alc_timeout_id > 0)
			return EINA_FALSE;
		disconnect_sfsvc();
		return EINA_TRUE;
	}
	_I("change vconf value before registering sfsvc");
	return EINA_FALSE;
}

static int prepare_lsensor(void *data)
{
	int alc_conf;
	int sf_state = 0;

	vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &alc_conf);

	if (alc_conf == SETTING_BRIGHTNESS_AUTOMATIC_ON)
		ecore_timer_add(SAMPLING_INTERVAL, (Ecore_Task_Cb)check_sfsvc, NULL);

	/* add auto_brt_setting change handler */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
				 (void *)set_alc_function, NULL);

	if (prev_init_extention != NULL)
		return prev_init_extention(data);

	return 0;
}

void set_power_saving_display_stat(int stat)
{
	_I("stat = %d", stat);
	power_saving_display_stat = stat;
}

static void __attribute__ ((constructor)) pm_lsensor_init(void)
{
	_default_action = NULL;
	if (pm_init_extention != NULL)
		prev_init_extention = pm_init_extention;
	pm_init_extention = prepare_lsensor;
}

static void __attribute__ ((destructor)) pm_lsensor_fini(void)
{

}
