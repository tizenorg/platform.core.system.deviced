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


#include <dd-display.h>
#include <dd-battery.h>
#include <dd-led.h>

#include "devman.h"
#include "devlog.h"

#ifndef API
#define API __attribute__ ((visibility("default")))
#endif

API int device_get_battery_pct(void)
{
	return battery_get_percent();
}

API int device_is_battery_full(void)
{
	return battery_is_full();
}

API int device_get_battery_health(void)
{
	return battery_get_health();
}

API int device_get_battery_pct_raw(void)
{
	return battery_get_percent_raw();
}

API int device_get_display_brt(display_num_t lcdnum)
{
	return display_get_brightness();
}

API int device_set_display_brt_with_settings(display_num_t lcdnum, int val)
{
	return display_set_brightness_with_setting(val);
}

API int device_set_display_brt(display_num_t lcdnum, int val)
{
	return display_set_brightness(val);
}

API int device_release_brt_ctrl(display_num_t lcdnum)
{
	return display_release_brightness();
}

API int device_get_max_brt(display_num_t lcdnum)
{
	return display_get_max_brightness();
}

API int device_get_min_brt(display_num_t lcdnum)
{
	return display_get_min_brightness();
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
	return display_get_count();
}

API int device_get_image_enhance_mode(void)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_set_image_enhance_mode(int val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_get_image_enhance_scenario(void)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_set_image_enhance_scenario(int val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_get_image_enhance_tone(void)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_set_image_enhance_tone(int val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_get_image_enhance_outdoor(void)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_set_image_enhance_outdoor(int val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_get_image_enhance_info(void)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_get_led_brt(void)
{
	return led_get_brightness();
}

API int device_set_led_brt(int val)
{
	DEVERR("Not support this api");
	return DEVMAN_ERROR_NOT_SUPPORTED;
}

API int device_set_led_brt_without_noti(int val)
{
	return led_set_brightness(val);
}

API int device_get_max_led(void)
{
	return led_get_max_brightness();
}

API int device_get_acl_control_status(display_num_t num)
{
	return display_get_acl_status();
}

API int device_set_acl_control_status(display_num_t num, int val)
{
	return display_set_acl_status(val);
}
