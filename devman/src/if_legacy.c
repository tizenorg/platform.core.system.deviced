/*
 * devman
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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


#include <vconf.h>
#include <errno.h>
#include <device-node.h>

#include "devman.h"
#include "devman_internal.h"
#include "devlog.h"

#define DISPLAY_MAX_BRIGHTNESS  100
#define DISPLAY_MIN_BRIGHTNESS  0

#define DISP_INDEX_BIT                      4
#define COMBINE_DISP_CMD(cmd, prop, index)  (cmd = (prop | (index << DISP_INDEX_BIT)))

#define SET_FLAG(var, bit)              (var |= (1<<bit))
#define UNSET_FLAG(var, bit)            (var &= (~(1<<bit)))
#define BRT_BIT                 1
#define LED_BIT                 4

static unsigned int disp_flag = 0x0;

API int device_get_battery_pct(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY, &val);
	if (ret < 0)
		return ret;

	if (val < 0 || val > 100) {
		DEVERR("capacity value is wrong");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	return val;
}

API int device_is_battery_full(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_FULL, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1) {
		DEVERR("charge_full value is wrong");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	return val;
}

API int device_get_battery_health(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_HEALTH, &val);
	if (ret < 0)
		return ret;

	if (val < BAT_UNKNOWN || val > BAT_COLD) {
		DEVERR("battery health value is wrong");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	return val;
}

API int device_get_battery_pct_raw(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY_RAW, &val);
	if (ret < 0)
		return ret;

	if (val > 10000)
		return 10000;

	return val;
}

API int device_get_display_brt(display_num_t lcdnum)
{
	int val;
	int cmd;
	int ret;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, lcdnum);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_display_brt_with_settings(display_num_t lcdnum, int val)
{
	int auto_brt_state;
	int cmd;
	int ret;

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brt_state) != 0) {
		DEVERR("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		DEVLOG("auto_brightness state is ON, can not change the brightness value");
		return DEVMAN_ERROR_NONE;
	}

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, lcdnum);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, val);
	if (ret < 0)
		return ret;

	if (vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, val) != 0) {
		DEVERR("Failed to set VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");
	}

	if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, val) != 0) {
		DEVERR("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
	}

	return DEVMAN_ERROR_NONE;
}

API int device_set_display_brt(display_num_t lcdnum, int val)
{
	int auto_brt_state;
	int cmd;
	int ret;

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brt_state) != 0) {
		DEVERR("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_ON);
	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, lcdnum);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, val);
	if (ret < 0)
		return ret;

	if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		DEVLOG("Auto brightness will be paused");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_PAUSE);
	}

	if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, val) != 0) {
		DEVERR("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
	}

	if (!disp_flag)
		ret = display_register_postjob();
	if (ret == 0)
		SET_FLAG(disp_flag, BRT_BIT);
	return DEVMAN_ERROR_NONE;
}

API int device_release_brt_ctrl(display_num_t lcdnum)
{
	int bat_state;
	int setting_val;
	int auto_brt_state;
	int charger_state;
	int brt_changed_state;
	int cmd;
	int ret;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state) != 0) {
		DEVERR("Failed to get VCONFKEY_SYSMAN_BATTERY_STATUS_LOW value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &charger_state) != 0) {
		DEVERR("Failed to get VCONFKEY_SYSMAN_CHARGER_STATUS value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	if (vconf_get_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM, &brt_changed_state) != 0) {
		DEVERR("Failed to get VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &setting_val) != 0) {
		DEVERR("Failed to get VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brt_state) != 0) {
		DEVERR("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		return DEVMAN_ERROR_OPERATION_FAILED;
	}

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_OFF);

	// check dim state
	if (bat_state <= VCONFKEY_SYSMAN_BAT_WARNING_LOW &&
		charger_state == VCONFKEY_SYSMAN_CHARGER_DISCONNECTED && !brt_changed_state) {
		DEVLOG("batt warning low : brightness is not changed!");
		COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, lcdnum);
		device_set_property(DEVICE_TYPE_DISPLAY, cmd, 0);
		return DEVMAN_ERROR_NONE;
	}

	if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_OFF) {
		COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, lcdnum);
		device_set_property(DEVICE_TYPE_DISPLAY, cmd, setting_val);
		if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, setting_val) != 0) {
			DEVERR("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
		}
	} else if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		DEVLOG("Auto brightness will be enable");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_ON);
	}

	UNSET_FLAG(disp_flag, BRT_BIT);
	if (!disp_flag)
		display_cancel_postjob();
	return DEVMAN_ERROR_NONE;
}

API int device_get_max_brt(display_num_t lcdnum)
{
	return DISPLAY_MAX_BRIGHTNESS;
}

API int device_get_min_brt(display_num_t lcdnum)
{
	return DISPLAY_MIN_BRIGHTNESS;
}

API int device_get_display_gamma(display_num_t lcdnum)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_set_display_gamma(display_num_t lcdnum, display_gamma_t val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_release_gamma_ctrl(display_num_t lcdnum, display_gamma_t org_val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_get_display_count(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_DISPLAY_COUNT, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_get_image_enhance_mode(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_MODE, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_image_enhance_mode(int val)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_MODE, val);
	if (ret < 0)
		return ret;

	return DEVMAN_ERROR_NONE;
}

API int device_get_image_enhance_scenario(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_image_enhance_scenario(int val)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_SCENARIO, val);
	if (ret < 0)
		return ret;

	return DEVMAN_ERROR_NONE;
}

API int device_get_image_enhance_tone(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_TONE, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_image_enhance_tone(int val)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_TONE, val);
	if (ret < 0)
		return ret;

	return DEVMAN_ERROR_NONE;
}

API int device_get_image_enhance_outdoor(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_image_enhance_outdoor(int val)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_OUTDOOR, val);
	if (ret < 0)
		return ret;

	return DEVMAN_ERROR_NONE;
}

API int device_get_image_enhance_info(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_IMAGE_ENHANCE_INFO, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_get_led_brt(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_led_brt(int val)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, val);
	if (ret < 0)
		return ret;

	if (val == 0) {
		UNSET_FLAG(disp_flag, LED_BIT);
		if (!disp_flag)
			display_cancel_postjob();
	} else {
		if (!disp_flag)
			ret = display_register_postjob();
		if (ret == 0)
			SET_FLAG(disp_flag, LED_BIT);
	}

	return DEVMAN_ERROR_NONE;
}

API int device_set_led_brt_without_noti(int val)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, val);
	if (ret < 0)
		return ret;

	return DEVMAN_ERROR_NONE;
}

API int device_get_max_led(void)
{
	int val;
	int ret;

	ret = device_get_property(DEVICE_TYPE_LED, PROP_LED_MAX_BRIGHTNESS, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_get_acl_control_status(display_num_t num)
{
	int val;
	int cmd;
	int ret;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_ACL_CONTROL, num);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &val);
	if (ret < 0)
		return ret;

	return val;
}

API int device_set_acl_control_status(display_num_t num, int val)
{
	int cmd;
	int ret;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_ACL_CONTROL, num);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, val);
	if (ret < 0)
		return ret;

	return DEVMAN_ERROR_NONE;
}
