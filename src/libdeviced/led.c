/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "log.h"
#include "dbus.h"
#include "common.h"

#define METHOD_GET_BRIGHTNESS		"GetBrightness"
#define METHOD_GET_MAX_BRIGHTNESS	"GetMaxBrightness"
#define METHOD_SET_BRIGHTNESS		"SetBrightness"
#define METHOD_SET_IR_COMMAND		"SetIrCommand"

API int led_get_brightness(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_GET_BRIGHTNESS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int led_get_max_brightness(void)
{
	DBusError err;
	DBusMessage *msg;
	int ret, ret_val;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_GET_MAX_BRIGHTNESS, NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int led_set_brightness_with_noti(int val, bool enable)
{
	DBusError err;
	DBusMessage *msg;
	char *arr[2];
	char buf_val[32];
	char buf_noti[32];
	int ret, ret_val;

	snprintf(buf_val, sizeof(buf_val), "%d", val);
	arr[0] = buf_val;
	snprintf(buf_noti, sizeof(buf_noti), "%d", enable);
	arr[1] = buf_noti;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_SET_BRIGHTNESS, "ii", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}

API int led_set_ir_command(char *value)
{
	if (value == NULL) {
		return -EINVAL;
	}

	DBusError err;
	DBusMessage *msg;
	char *arr[1];
	int ret, ret_val;

	arr[0] = value;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			METHOD_SET_IR_COMMAND, "s", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		ret_val = -EBADMSG;
	}

	dbus_message_unref(msg);
	return ret_val;
}
