/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <vconf.h>
#include <hw/led.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "display/core.h"
#include "display/setting.h"

#define KEYBACKLIGHT_TIME_90            90	/* 1.5 second */
#define KEYBACKLIGHT_TIME_360           360	/* 6 second */
#define KEYBACKLIGHT_TIME_ALWAYS_ON     -1	/* always on */
#define KEYBACKLIGHT_TIME_ALWAYS_OFF    0	/* always off */
#define KEYBACKLIGHT_BASE_TIME          60	/* 1min = 60sec */
#define KEYBACKLIGHT_PRESSED_TIME       15	/*  15 second */

#ifndef VCONFKEY_SETAPPL_TOUCHKEY_LIGHT_DURATION
#define VCONFKEY_SETAPPL_TOUCHKEY_LIGHT_DURATION VCONFKEY_SETAPPL_PREFIX"/display/touchkey_light_duration"
#endif

#define GET_BRIGHTNESS(val)     (((val) >> 24) & 0xFF)
#define SET_BRIGHTNESS(val)     (((val) & 0xFF) << 24)

struct led_device *touchled_dev;
static Ecore_Timer *hardkey_timeout_id;
static int hardkey_duration;

static int touchled_set_state(bool on)
{
	struct led_state tmp = {0,};
	int r;

	if (!touchled_dev || !touchled_dev->set_state) {
		_E("there is no led device");
		return -ENOENT;
	}

	if (on)
		tmp.color = SET_BRIGHTNESS(255);
	else
		tmp.color = SET_BRIGHTNESS(0);

	r = touchled_dev->set_state(&tmp);
	if (r < 0) {
		_E("fail to set touch led state : %d", r);
		return r;
	}

	return 0;
}

static Eina_Bool key_backlight_expired(void *data)
{
	hardkey_timeout_id = NULL;

	touchled_set_state(false);

	return ECORE_CALLBACK_CANCEL;
}

void process_touchkey_press(void)
{
	/* release existing timer */
	if (hardkey_timeout_id > 0) {
		ecore_timer_del(hardkey_timeout_id);
		hardkey_timeout_id = NULL;
	}
	/* if hardkey option is always off */
	if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_OFF)
		return;
	/* turn on hardkey backlight */
	touchled_set_state(true);
	/* start timer */
	hardkey_timeout_id = ecore_timer_add(
		    KEYBACKLIGHT_PRESSED_TIME,
		    key_backlight_expired, NULL);
}

void process_touchkey_release(void)
{
	float fduration;

	/* release existing timer */
	if (hardkey_timeout_id > 0) {
		ecore_timer_del(hardkey_timeout_id);
		hardkey_timeout_id = NULL;
	}
	/* if hardkey option is always on or off */
	if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_ON ||
		hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_OFF)
		return;
	/* start timer */
	fduration = (float)hardkey_duration / KEYBACKLIGHT_BASE_TIME;
	hardkey_timeout_id = ecore_timer_add(
		    fduration,
		    key_backlight_expired, NULL);
}

void process_touchkey_enable(bool enable)
{
	/* release existing timer */
	if (hardkey_timeout_id > 0) {
		ecore_timer_del(hardkey_timeout_id);
		hardkey_timeout_id = NULL;
	}

	/* start timer in case of backlight enabled */
	if (enable) {
		/* if hardkey option is always off */
		if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_OFF)
			return;

		/* turn on hardkey backlight */
		touchled_set_state(true);

		/* do not create turnoff timer in case of idle lock state */
		if (get_lock_screen_state() == VCONFKEY_IDLE_LOCK)
			return;

		/* start timer */
		hardkey_timeout_id = ecore_timer_add(
			    KEYBACKLIGHT_PRESSED_TIME,
				key_backlight_expired, NULL);
	} else {
		/* if hardkey option is always on */
		if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_ON)
			return;

		/* turn off hardkey backlight */
		touchled_set_state(false);
	}
}

static void hardkey_duration_cb(keynode_t *key, void *data)
{
	float duration;

	hardkey_duration = vconf_keynode_get_int(key);

	/* release existing timer */
	if (hardkey_timeout_id > 0) {
		ecore_timer_del(hardkey_timeout_id);
		hardkey_timeout_id = NULL;
	}

	/* if hardkey option is always off */
	if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_OFF) {
		/* turn off hardkey backlight */
		touchled_set_state(false);
		return;
	}

	/* turn on hardkey backlight */
	touchled_set_state(true);

	/* if hardkey option is always on */
	if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_ON)
		return;

	/* start timer */
	duration = (float)hardkey_duration / KEYBACKLIGHT_BASE_TIME;
	hardkey_timeout_id = ecore_timer_add(
		    duration,
		    key_backlight_expired, NULL);
}

static int hardkey_lcd_changed_cb(void *data)
{
	int lcd_state;

	if (!data)
		return 0;

	lcd_state = *(int*)data;
	if (lcd_state == S_NORMAL
	    && hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_ON) {
		touchled_set_state(true);
		return 0;
	}

	return 0;
}

static int touchled_service_load(void)
{
	struct hw_info *info;
	int r;

	r = hw_get_info(LED_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);
	if (r < 0) {
		_E("fail to load led shared library : %d", r);
		return -ENOENT;
	}

	if (!info->open) {
		_E("fail to open touch led device : open(NULL)");
		return -EPERM;
	}

	r = info->open(info, LED_ID_TOUCH_KEY,
			(struct hw_common **)&touchled_dev);
	if (r < 0) {
		_E("fail to get touch led device : %d", r);
		return -EPERM;
	}

	_D("touch led device structure load success");
	return 0;
}

static int touchled_service_free(void)
{
	struct hw_info *info;

	if (!touchled_dev)
		return -ENOENT;

	info = touchled_dev->common.info;
	if (!info)
		return -EPERM;

	info->close((struct hw_common *)touchled_dev);

	return 0;
}

static int touchled_probe(void *data)
{
	/* load led device */
	return touchled_service_load();
}

static void touchled_init(void *data)
{
	/* get touchkey light duration setting */
	if (vconf_get_int(VCONFKEY_SETAPPL_TOUCHKEY_LIGHT_DURATION, &hardkey_duration) < 0) {
		_W("Fail to get VCONFKEY_SETAPPL_TOUCHKEY_LIGHT_DURATION!!");
		hardkey_duration = KEYBACKLIGHT_TIME_90;
	}

	vconf_notify_key_changed(VCONFKEY_SETAPPL_TOUCHKEY_LIGHT_DURATION, hardkey_duration_cb, NULL);

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_LCD, hardkey_lcd_changed_cb);

	/* update touchkey light duration right now */
	if (hardkey_duration == KEYBACKLIGHT_TIME_ALWAYS_ON)
		touchled_set_state(true);
}

static void touchled_exit(void *data)
{
	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_LCD, hardkey_lcd_changed_cb);

	vconf_ignore_key_changed(VCONFKEY_SETAPPL_TOUCHKEY_LIGHT_DURATION, hardkey_duration_cb);

	/* free led device */
	touchled_service_free();
}

static const struct device_ops touchled_device_ops = {
	.name     = "touchled",
	.probe    = touchled_probe,
	.init     = touchled_init,
	.exit     = touchled_exit,
};

DEVICE_OPS_REGISTER(&touchled_device_ops)
