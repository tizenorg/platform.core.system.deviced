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
#include <string.h>
#include <vconf.h>
#include <errno.h>
#include <device-node.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <linux/limits.h>

#include "log.h"
#include "dbus.h"
#include "dd-display.h"

#define DISPLAY_MAX_BRIGHTNESS  100
#define DISPLAY_MIN_BRIGHTNESS  1
#define DISPLAY_DIM_BRIGHTNESS  0

#define HOLDKEY_BLOCK_BIT		0x1
#define STANDBY_MODE_BIT		0x2

#define METHOD_SET_FRAME_RATE		"setframerate"
#define METHOD_LOCK_STATE		"lockstate"
#define METHOD_UNLOCK_STATE		"unlockstate"
#define METHOD_CHANGE_STATE		"changestate"
#define METHOD_GET_DISPLAY_COUNT	"GetDisplayCount"
#define METHOD_GET_BRIGHTNESS	"GetBrightness"
#define METHOD_SET_BRIGHTNESS	"SetBrightness"
#define METHOD_HOLD_BRIGHTNESS	"HoldBrightness"
#define METHOD_RELEASE_BRIGHTNESS	"ReleaseBrightness"
#define METHOD_GET_ACL_STATUS	"GetAclStatus"
#define METHOD_SET_ACL_STATUS	"SetAclStatus"

#define STR_LCD_OFF   "lcdoff"
#define STR_LCD_DIM   "lcddim"
#define STR_LCD_ON    "lcdon"
#define STR_SUSPEND   "suspend"

#define STR_STAYCURSTATE "staycurstate"
#define STR_GOTOSTATENOW "gotostatenow"

#define STR_HOLDKEYBLOCK "holdkeyblock"
#define STR_STANDBYMODE  "standbymode"
#define STR_NULL         "NULL"

#define STR_SLEEP_MARGIN "sleepmargin"
#define STR_RESET_TIMER  "resettimer"
#define STR_KEEP_TIMER   "keeptimer"

API int display_get_count(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_DISPLAY_COUNT, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_get_max_brightness(void)
{
	return DISPLAY_MAX_BRIGHTNESS;
}

API int display_get_min_brightness(void)
{
	return DISPLAY_MIN_BRIGHTNESS;
}

API int display_get_brightness(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_BRIGHTNESS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_set_brightness_with_setting(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_BRIGHTNESS, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_set_brightness(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_HOLD_BRIGHTNESS, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_release_brightness(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_RELEASE_BRIGHTNESS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_get_acl_status(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_GET_ACL_STATUS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_set_acl_status(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_ACL_STATUS, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

API int display_set_frame_rate(int val)
{
	DBusError err;
	DBusMessage *msg;
	char str_val[32];
	char *arr[1];
	int ret, ret_val;

	snprintf(str_val, sizeof(str_val), "%d", val);
	arr[0] = str_val;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_SET_FRAME_RATE, "i", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);
	return ret_val;
}

static inline char *get_lcd_str(unsigned int val)
{
	switch (val) {
	case LCD_NORMAL:
		return STR_LCD_ON;
	case LCD_DIM:
		return STR_LCD_DIM;
	case LCD_OFF:
		return STR_LCD_OFF;
	case SUSPEND:
		return STR_SUSPEND;
	default:
		return NULL;
	}
}

API int display_change_state(unsigned int s_bits)
{
	DBusError err;
	DBusMessage *msg;
	char *p, *pa[1];
	int ret, val;

	p = get_lcd_str(s_bits);
	if (!p)
		return -EINVAL;
	pa[0] = p;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_CHANGE_STATE, "s", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_CHANGE_STATE, val);
	return val;
}

API int display_lock_state(unsigned int s_bits, unsigned int flag,
		      unsigned int timeout)
{
	DBusError err;
	DBusMessage *msg;
	char *p, *pa[4];
	char str_timeout[32];
	int ret, val;

	p = get_lcd_str(s_bits);
	if (!p)
		return -EINVAL;
	pa[0] = p;

	if (flag & GOTO_STATE_NOW)
		/* if the flag is true, go to the locking state directly */
		p = STR_GOTOSTATENOW;
	else
		p = STR_STAYCURSTATE;
	pa[1] = p;

	if (flag & HOLD_KEY_BLOCK)
		p = STR_HOLDKEYBLOCK;
	else
		p = STR_NULL;
	pa[2] = p;

	snprintf(str_timeout, sizeof(str_timeout), "%d", timeout);
	pa[3] = str_timeout;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_LOCK_STATE, "sssi", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}
	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_LOCK_STATE, val);
	return val;
}

API int display_unlock_state(unsigned int s_bits, unsigned int flag)
{
	DBusError err;
	DBusMessage *msg;
	char *p, *pa[2];
	int ret, val;

	p = get_lcd_str(s_bits);
	if (!p)
		return -EINVAL;
	pa[0] = p;

	switch (flag) {
	case PM_SLEEP_MARGIN:
		p = STR_SLEEP_MARGIN;
		break;
	case PM_RESET_TIMER:
		p = STR_RESET_TIMER;
		break;
	case PM_KEEP_TIMER:
		p = STR_KEEP_TIMER;
		break;
	default:
		return -EINVAL;
	}
	pa[1] = p;

	msg = deviced_dbus_method_sync(BUS_NAME, DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			METHOD_UNLOCK_STATE, "ss", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_DISPLAY, METHOD_UNLOCK_STATE, val);
	return val;
}
