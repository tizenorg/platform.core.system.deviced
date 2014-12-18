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
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <vconf.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "brightness.h"
#include "display-ops.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "shared/dbus.h"

#define KEY_WAITING_TIME		3	/* 3 seconds */

#define SIGNAL_BRIGHTNESS_READY		"BrightnessReady"
#define SIGNAL_BRIGHTNESS_CHANGED	"BrightnessChanged"

#define METHOD_BRIGHTNESS		"BrightnessPopupLaunch"
#define POPUP_TYPE_LAUNCH		"launch"
#define POPUP_TYPE_TERMINATE		"terminate"

static Ecore_Timer *popup_timer = NULL;
static int popup_pid = -1;
static bool brightness_ready = false;

static void broadcast_brightness_changed(int val)
{
	char *arr[1];
	char str[32];

	snprintf(str, sizeof(str), "%d", val);
	arr[0] = str;

	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_BRIGHTNESS_CHANGED, "i", arr);
}

static int process_syspopup(char *type)
{
	int pid;
	char *pa[4];
	pa[0] = "_SYSPOPUP_CONTENT_";
	pa[1] = "brightness";
	pa[2] = "_TYPE_";
	pa[3] = type;

	pid = dbus_method_sync(POPUP_BUS_NAME, POPUP_PATH_SYSTEM,
	    POPUP_INTERFACE_SYSTEM, METHOD_BRIGHTNESS, "ssss", pa);

	if (pid < 0)
		_E("Failed to syspopup(%d)", pid);

	return pid;
}

static inline int calculate_brightness(int val, int action)
{
	if (action == BRIGHTNESS_UP)
		val += BRIGHTNESS_CHANGE;
	else if (action == BRIGHTNESS_DOWN)
		val -= BRIGHTNESS_CHANGE;

	val = clamp(val, PM_MIN_BRIGHTNESS, PM_MAX_BRIGHTNESS);

	return val;
}

static void update_brightness(int action)
{
	int ret, val, new_val;

	ret = vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &val);
	if (ret < 0) {
		_E("Fail to get brightness!");
		return;
	}

	new_val = calculate_brightness(val, action);

	if (new_val == val)
		return;

	broadcast_brightness_changed(new_val);

	ret = vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, new_val);
	if (!ret) {
		backlight_ops.update();
		_I("brightness is changed! (%d)", new_val);
	} else {
		_E("Fail to set brightness!");
	}
}

static Eina_Bool key_waiting_expired(void *data)
{
	int ret;
	char name[PATH_MAX];

	popup_timer = NULL;
	brightness_ready = false;

	get_pname(popup_pid, name);
	if (!name[0])
		return EINA_FALSE;

	ret = process_syspopup(POPUP_TYPE_TERMINATE);

	if (ret < 0)
		_E("Failed to terminate syspopup!(%d:%d)", popup_pid, ret);
	else
		_D("syspopup is terminated!(%d:%d)", popup_pid, ret);

	popup_pid = -1;

	return EINA_FALSE;
}

int control_brightness_key(int action)
{
	int ret, val;
	char name[PATH_MAX];

	ret = vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &val);
	if (!ret && val == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
		    SETTING_BRIGHTNESS_AUTOMATIC_OFF);
	}

	get_pname(popup_pid, name);
	if (popup_pid < 0 || !name[0]) {
		brightness_ready = false;
		popup_pid = process_syspopup(POPUP_TYPE_LAUNCH);

		if (popup_pid > 0)
			_D("popup is launched! (%d)", popup_pid);
		else
			_E("Failed to launch popup! (%d)", popup_pid);
	}

	if (!brightness_ready)
		return false;

	if (popup_timer)
		ecore_timer_reset(popup_timer);
	else
		popup_timer = ecore_timer_add(KEY_WAITING_TIME,
		    key_waiting_expired, NULL);

	update_brightness(action);
	return false;
}

static int lcd_changed_cb(void *data)
{
	int lcd_state;

	if (!data)
		return 0;

	lcd_state = *(int*)data;
	if (lcd_state == S_LCDOFF && popup_pid > 0) {
		if (popup_timer)
			ecore_timer_del(popup_timer);

		key_waiting_expired(NULL);
	}
	return 0;
}

static void brightness_ready_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret, state;

	ret = dbus_message_is_signal(msg, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_BRIGHTNESS_READY);
	if (!ret) {
		_E("there is no brightness ready signal");
		return;
	}

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &state,
	    DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}

	if (state) {
		_D("brightness popup is ready!");
		brightness_ready = true;
	} else {
		_D("brightness popup is not ready! ");
		brightness_ready = false;

		if (popup_timer)
			ecore_timer_del(popup_timer);

		key_waiting_expired(NULL);
	}
}

static void brightness_init(void *data)
{
	int ret;

	/* register signal handler to get brightness popup state */
	ret = register_edbus_signal_handler(DEVICED_PATH_DISPLAY,
		    DEVICED_INTERFACE_DISPLAY, SIGNAL_BRIGHTNESS_READY,
		    brightness_ready_handler);
	if (ret < 0)
		_E("Failed to register signal handler! %d", ret);

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_LCD, lcd_changed_cb);
}

static void brightness_exit(void *data)
{
	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_LCD, lcd_changed_cb);
}

static const struct display_ops display_brightness_ops = {
	.name     = "brightness",
	.init     = brightness_init,
	.exit     = brightness_exit,
};

DISPLAY_OPS_REGISTER(&display_brightness_ops)

