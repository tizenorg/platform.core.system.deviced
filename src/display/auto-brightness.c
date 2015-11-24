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
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vconf.h>
#include <sensor_internal.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "display-ops.h"
#include "device-node.h"
#include "core/device-notifier.h"
#include "core/config-parser.h"

#define DISP_FORCE_SHIFT	12
#define DISP_FORCE_CMD(prop, force)	(((force) << DISP_FORCE_SHIFT) | prop)

#define SAMPLING_INTERVAL	1	/* 1 sec */
#define MAX_SAMPLING_COUNT	3
#define MAX_FAULT		5
#define DEFAULT_AUTOMATIC_BRT	5
#define MAX_AUTOMATIC_COUNT	11
#define AUTOMATIC_DEVIDE_VAL	10
#define AUTOMATIC_DELAY_TIME	0.5	/* 0.5 sec */

#define RADIAN_VALUE 		(57.2957)
#define ROTATION_90		90
#define WORKING_ANGLE_MIN	0
#define WORKING_ANGLE_MAX	20

#define ISVALID_AUTOMATIC_INDEX(index) (index >= 0 && index < MAX_AUTOMATIC_COUNT)
#define BOARD_CONF_FILE "/etc/deviced/display.conf"

#define ON_LUX		-1
#define OFF_LUX		-1
#define ON_COUNT	1
#define OFF_COUNT	1

struct lbm_config {
	int on;
	int off;
	int on_count;
	int off_count;
};

struct lbm_config lbm_conf = {
	.on		= ON_LUX,
	.off		= OFF_LUX,
	.on_count	= ON_COUNT,
	.off_count	= OFF_COUNT,
};

static int (*_default_action) (int);
static Ecore_Timer *alc_timeout_id = 0;
static Ecore_Timer *update_timeout;
static int light_handle = -1;
static int accel_handle = -1;
static int fault_count = 0;
static int automatic_brt = DEFAULT_AUTOMATIC_BRT;
static int min_brightness = PM_MIN_BRIGHTNESS;
static char *min_brightness_name = 0;
static int value_table[MAX_AUTOMATIC_COUNT];
static int lbm_state = -1;

static bool update_working_position(void)
{
	sensor_data_t data;
	int ret;
	float x, y, z, pitch, realg;

	if (!display_conf.accel_sensor_on)
		return false;

	ret = sf_get_data(accel_handle, ACCELEROMETER_BASE_DATA_SET, &data);
	if (ret < 0) {
		_E("Fail to get accelerometer data! %d", ret);
		return true;
	}

	x = data.values[0];
	y = data.values[1];
	z = data.values[2];

	realg = (float)sqrt((x * x) + (y * y) + (z * z));
	pitch = ROTATION_90 - abs((int) (asin(z / realg) * RADIAN_VALUE));

	_D("accel data [%f, %f, %f] - %f", x, y, z, pitch);

	if (pitch >= WORKING_ANGLE_MIN && pitch <= WORKING_ANGLE_MAX)
		return true;
	return false;
}

static int get_siop_brightness(int value)
{
	int brt;

	brt = DEFAULT_DISPLAY_MAX_BRIGHTNESS;
	if (value > brt)
		return brt;

	return value;
}

static void alc_set_brightness(int setting, int value, int lux)
{
	static int old;
	int position, tmp_value = 0, ret;

	ret = backlight_ops.get_brightness(&tmp_value);
	if (ret < 0) {
		_E("Fail to get display brightness!");
		return;
	}

	if (value < min_brightness)
		value = min_brightness;

	if (tmp_value != value) {
		if (!setting && min_brightness == PM_MIN_BRIGHTNESS &&
		    display_conf.accel_sensor_on == true) {
			position = update_working_position();
			if (!position && (old > lux)) {
				_D("It's not working position, "
				    "LCD isn't getting dark!");
				return;
			}
		}
		int diff, step;

		diff = value - tmp_value;
		if (abs(diff) < display_conf.brightness_change_step)
			step = (diff > 0 ? 1 : -1);
		else
			step = (int)ceil(diff /
			    (float)display_conf.brightness_change_step);

		_D("%d", step);
		while (tmp_value != value) {
			if (step == 0) break;

			tmp_value += step;
			if ((step > 0 && tmp_value > value) ||
			    (step < 0 && tmp_value < value))
				tmp_value = value;

			backlight_ops.set_default_brt(tmp_value);
			backlight_ops.update();
		}
		_I("load light data:%d lux,auto brt %d,min brightness %d,"
		    "brightness %d", lux, automatic_brt, min_brightness, value);
		old = lux;
	}
}

static bool check_lbm(int lux, int setting)
{
	static int on_count, off_count;

	if (lux < 0)
		return false;

	if (lbm_conf.on < 0 || lbm_conf.off < 0)
		return true;

	if (lux <= lbm_conf.on && lbm_state != true) {
		off_count = 0;
		on_count++;
		if (on_count >= lbm_conf.on_count || setting) {
			on_count = 0;
			lbm_state = true;
			_D("LBM is on");
			return true;
		}
	} else if (lux >= lbm_conf.off && lbm_state != false) {
		on_count = 0;
		off_count++;
		if (off_count >= lbm_conf.off_count || setting) {
			off_count = 0;
			lbm_state = false;
			_D("LBM is off");
			return true;
		}
	} else if (lux > lbm_conf.on && lux < lbm_conf.off) {
		off_count = 0;
		on_count = 0;
	}

	if (setting)
		return true;

	return false;
}

static bool check_brightness_changed(int value)
{
	int i;
	static int values[MAX_SAMPLING_COUNT], count = 0;

	if (!get_hallic_open())
		return false;

	if (count >= MAX_SAMPLING_COUNT || count < 0)
		count = 0;

	values[count++] = value;

	for (i = 0; i < MAX_SAMPLING_COUNT - 1; i++)
		if (values[i] != values[i+1])
			return false;
	return true;
}

static bool alc_update_brt(bool setting)
{
	int value = 0;
	int cal_value = 0;
	int ret = -1;
	int cmd;
	sensor_data_t light_data;

	ret = sf_get_data(light_handle, LIGHT_LUX_DATA_SET, &light_data);
	if (ret < 0 || (int)light_data.values[0] < 0) {
		fault_count++;
	} else {
		int force = (setting ? 1 : 0);
		cmd = DISP_FORCE_CMD(PROP_DISPLAY_BRIGHTNESS_BY_LUX, force);
		cmd = DISP_CMD(cmd, (int)light_data.values[0]);
		ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, value_table);

		value = (ISVALID_AUTOMATIC_INDEX(automatic_brt) ?
		    value_table[automatic_brt] : value_table[DEFAULT_AUTOMATIC_BRT]);

		if (ret < 0 || value < PM_MIN_BRIGHTNESS ||
		    value > PM_MAX_BRIGHTNESS) {
			_E("fail to load light data : %d lux, %d",
				(int)light_data.values[0], value);
			fault_count++;
		} else {
			fault_count = 0;

			if (!check_lbm((int)light_data.values[0], setting))
				return EINA_TRUE;

			if (display_conf.continuous_sampling &&
			    !check_brightness_changed(value) && !setting)
				return EINA_TRUE;

			alc_set_brightness(setting, value, (int)light_data.values[0]);
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
	return EINA_TRUE;
}

static bool alc_handler(void* data)
{
	if (pm_cur_state != S_NORMAL || !get_hallic_open()){
		if (alc_timeout_id > 0)
			ecore_timer_del(alc_timeout_id);
		alc_timeout_id = NULL;
		return EINA_FALSE;
	}

	if (alc_update_brt(false) == EINA_FALSE)
		return EINA_FALSE;

	if (alc_timeout_id != 0)
		return EINA_TRUE;

	return EINA_FALSE;
}

static int alc_action(int timeout)
{
	/* sampling timer add */
	if (alc_timeout_id == 0 && !(pm_status_flag & PWRSV_FLAG)) {
		display_info.update_auto_brightness(true);

		alc_timeout_id =
		    ecore_timer_add(display_conf.lightsensor_interval,
			    (Ecore_Task_Cb)alc_handler, NULL);
	}

	if (_default_action != NULL)
		return _default_action(timeout);

	/* unreachable code */
	return -1;
}

static int connect_sfsvc(void)
{
	int sf_state = -1;

	_I("connect with sensor fw");
	/* light sensor */
	light_handle = sf_connect(LIGHT_SENSOR);
	if (light_handle < 0) {
		_E("light sensor attach fail");
		goto error;
	}
	sf_state = sf_start(light_handle, 0);
	if (sf_state < 0) {
		_E("light sensor attach fail");
		sf_disconnect(light_handle);
		light_handle = -1;
		goto error;
	}
	sf_change_sensor_option(light_handle, 1);

	if (!display_conf.accel_sensor_on)
		goto success;

	/* accelerometer sensor */
	accel_handle = sf_connect(ACCELEROMETER_SENSOR);
	if (accel_handle < 0) {
		_E("accelerometer sensor attach fail");
		goto error;
	}
	sf_state = sf_start(accel_handle, 0);
	if (sf_state < 0) {
		_E("accelerometer sensor attach fail");
		sf_disconnect(accel_handle);
		accel_handle = -1;
		goto error;
	}
	sf_change_sensor_option(accel_handle, 1);

success:
	fault_count = 0;
	return 0;

error:
	if (light_handle >= 0) {
		sf_stop(light_handle);
		sf_disconnect(light_handle);
		light_handle = -1;
	}
	if (display_conf.accel_sensor_on && accel_handle >= 0) {
		sf_stop(accel_handle);
		sf_disconnect(accel_handle);
		accel_handle = -1;
	}
	return -EIO;
}

static int disconnect_sfsvc(void)
{
	_I("disconnect with sensor fw");
	/* light sensor*/
	if(light_handle >= 0) {
		sf_stop(light_handle);
		sf_disconnect(light_handle);
		light_handle = -1;
	}
	/* accelerometer sensor*/
	if (display_conf.accel_sensor_on && accel_handle >= 0) {
		sf_stop(accel_handle);
		sf_disconnect(accel_handle);
		accel_handle = -1;
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
		_D("brightness changed in low battery,"
		    "escape dim state (light)");
	}
}

static int set_autobrightness_state(int status)
{
	int ret = -1;
	int brt = -1;
	int default_brt = -1;
	int max_brt = -1;

	if (status == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		if(connect_sfsvc() < 0)
			return -1;

		/* escape dim state if it's in low battery.*/
		set_brtch_state();

		/* change alc action func */
		if (_default_action == NULL)
			_default_action = states[S_NORMAL].action;
		states[S_NORMAL].action = alc_action;

		display_info.update_auto_brightness(true);

		alc_timeout_id =
		    ecore_timer_add(display_conf.lightsensor_interval,
			    (Ecore_Task_Cb)alc_handler, NULL);
	} else if (status == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		_I("auto brightness paused!");
		disconnect_sfsvc();
		lbm_state = 0;
	} else {
		disconnect_sfsvc();
		lbm_state = 0;
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
		backlight_ops.update();
	}

	return 0;
}

static void set_alc_function(keynode_t *key_nodes, void *data)
{
	int status, ret;

	if (key_nodes == NULL) {
		_E("wrong parameter, key_nodes is null");
		return;
	}

	status = vconf_keynode_get_int(key_nodes);

	switch (status) {
	case SETTING_BRIGHTNESS_AUTOMATIC_OFF:
	case SETTING_BRIGHTNESS_AUTOMATIC_ON:
	case SETTING_BRIGHTNESS_AUTOMATIC_PAUSE:
		ret = set_autobrightness_state(status);
		_D("set auto brightness : %d", ret);
		break;
	default:
		_E("invalid value! %d", status);
	}
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
		    ecore_timer_add(display_conf.lightsensor_interval,
			    (Ecore_Task_Cb)alc_handler, NULL);
		if (alc_timeout_id > 0)
			return EINA_FALSE;
		disconnect_sfsvc();
		return EINA_TRUE;
	}
	_I("change vconf value before registering sfsvc");
	return EINA_FALSE;
}

static void set_alc_automatic_brt(keynode_t *key_nodes, void *data)
{
	if (key_nodes == NULL) {
		_E("wrong parameter, key_nodes is null");
		return;
	}
	automatic_brt = vconf_keynode_get_int(key_nodes) / AUTOMATIC_DEVIDE_VAL;
	_D("automatic brt : %d", automatic_brt);

	alc_update_brt(true);
}

static Eina_Bool update_handler(void* data)
{
	int ret, on;

	update_timeout = NULL;

	if (pm_cur_state != S_NORMAL)
		return EINA_FALSE;

	if (!get_hallic_open())
		return EINA_FALSE;

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &on);
	if (ret < 0 || on != SETTING_BRIGHTNESS_AUTOMATIC_ON)
		return EINA_FALSE;

	_D("auto brightness is working!");
	alc_update_brt(true);

	return EINA_FALSE;
}

static void update_auto_brightness(bool update)
{
	if (update_timeout) {
		ecore_timer_del(update_timeout);
		update_timeout = NULL;
	}

	if (update) {
		update_timeout = ecore_timer_add(AUTOMATIC_DELAY_TIME,
		    update_handler, NULL);
	}
}

static int prepare_lsensor(void *data)
{
	int status, ret;
	int sf_state = 0;
	int brt = -1;

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &status);

	if (ret == 0 && status == SETTING_BRIGHTNESS_AUTOMATIC_ON)
		set_autobrightness_state(status);

	/* add auto_brt_setting change handler */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
				 set_alc_function, NULL);

	vconf_get_int(VCONFKEY_SETAPPL_LCD_AUTOMATIC_BRIGHTNESS, &brt);
	if (brt < PM_MIN_BRIGHTNESS || brt > PM_MAX_BRIGHTNESS) {
		_E("Failed to get automatic brightness!");
	} else {
		automatic_brt = brt / AUTOMATIC_DEVIDE_VAL;
		_I("automatic brt init success %d", automatic_brt);
	}

	vconf_notify_key_changed(VCONFKEY_SETAPPL_LCD_AUTOMATIC_BRIGHTNESS,
				set_alc_automatic_brt, NULL);

	return 0;
}

static void update_brightness_direct(void)
{
	int ret, status;

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &status);
	if (ret == 0 && status == SETTING_BRIGHTNESS_AUTOMATIC_ON)
		alc_update_brt(true);
}

static int set_autobrightness_min(int val, char *name)
{
	if (!name)
		return -EINVAL;

	if (val < PM_MIN_BRIGHTNESS || val > PM_MAX_BRIGHTNESS)
		return -EINVAL;

	min_brightness = val;

	if (min_brightness_name) {
		free(min_brightness_name);
		min_brightness_name = 0;
	}
	min_brightness_name = strndup(name, strlen(name));

	update_brightness_direct();

	_I("auto brightness min value changed! (%d, %s)",
	    min_brightness, min_brightness_name);

	return 0;
}

static void reset_autobrightness_min(const char *sender, void *data)
{
	if (!sender)
		return;

	if (!min_brightness_name)
		return;

	if (strcmp(sender, min_brightness_name))
		return;

	_I("change to default %d -> %d, %s", min_brightness,
	    PM_MIN_BRIGHTNESS, min_brightness_name);
	min_brightness = PM_MIN_BRIGHTNESS;
	if (min_brightness_name) {
		free(min_brightness_name);
		min_brightness_name = 0;
	}

	update_brightness_direct();
}

static int lcd_changed_cb(void *data)
{
	int lcd_state;

	if (!data)
		return 0;

	lcd_state = *(int*)data;
	if (lcd_state == S_LCDOFF && alc_timeout_id > 0) {
		ecore_timer_del(alc_timeout_id);
		alc_timeout_id = NULL;
	}

	return 0;
}

static int lbm_load_config(struct parse_result *result, void *user_data)
{
	struct lbm_config *c = user_data;

	_D("%s,%s,%s", result->section, result->name, result->value);

	if (!c)
		return -EINVAL;

	if (!MATCH(result->section, "LBM"))
		return 0;

	if (MATCH(result->name, "on")) {
		SET_CONF(c->on, atoi(result->value));
		_D("on lux is %d", c->on);
	} else if (MATCH(result->name, "off")) {
		SET_CONF(c->off, atoi(result->value));
		_D("off lux is %d", c->off);
	} else if (MATCH(result->name, "on_count")) {
		SET_CONF(c->on_count, atoi(result->value));
		_D("on count is %d", c->on_count);
	} else if (MATCH(result->name, "off_count")) {
		SET_CONF(c->off_count, atoi(result->value));
		_D("off count is %d", c->off_count);
	}

	return 0;
}

static void auto_brightness_init(void *data)
{
	int ret;

	display_info.update_auto_brightness = update_auto_brightness;
	display_info.set_autobrightness_min = set_autobrightness_min;
	display_info.reset_autobrightness_min = reset_autobrightness_min;

	/* load configutation */
	ret = config_parse(BOARD_CONF_FILE, lbm_load_config, &lbm_conf);
	if (ret < 0)
		_W("Failed to load %s, %d Use default value!",
		    BOARD_CONF_FILE, ret);

	register_notifier(DEVICE_NOTIFIER_LCD, lcd_changed_cb);

	prepare_lsensor(NULL);
}

static void auto_brightness_exit(void *data)
{
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
	    set_alc_function);

	vconf_ignore_key_changed(VCONFKEY_SETAPPL_LCD_AUTOMATIC_BRIGHTNESS,
	    set_alc_automatic_brt);

	unregister_notifier(DEVICE_NOTIFIER_LCD, lcd_changed_cb);

	set_autobrightness_state(SETTING_BRIGHTNESS_AUTOMATIC_OFF);
}

static const struct display_ops display_autobrightness_ops = {
	.name     = "auto-brightness",
	.init     = auto_brightness_init,
	.exit     = auto_brightness_exit,
};

DISPLAY_OPS_REGISTER(&display_autobrightness_ops)

