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
#include <errno.h>
#include <assert.h>
#include <device-node.h>

#include "deviced/dd-led.h"
#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"

#define PREDEF_LED			"led"
#define PREDEF_LED_CMD         		"ledcmd"
#define PREDEF_LED_BRT         		"ledbrt"
#define PREDEF_LED_MODE        		"ledmode"

enum {
	SET_BRT = 0,
};

static int set_brightness(int val)
{
	int r;

	r = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, val);
	if (r < 0)
		return r;

	return 0;
}

static int predefine_action(int argc, char **argv)
{
	int prop;

	if (argc > 4) {
		_E("Invalid argument");
		errno = EINVAL;
		return -1;
	}

	prop = atoi(argv[1]);
	switch(prop) {
	case SET_BRT:
		return set_brightness(atoi(argv[2]));
	default:
		break;
	}

	return -1;
}

static DBusMessage *dbus_led_cmd_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[3];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1],
		    DBUS_TYPE_STRING, &argv[2], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	predefine_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_led_brt_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[4];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1],
		    DBUS_TYPE_STRING, &argv[2],
		    DBUS_TYPE_STRING, &argv[3], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	predefine_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_led_mode_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[5];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1],
		    DBUS_TYPE_STRING, &argv[2],
		    DBUS_TYPE_STRING, &argv[3],
		    DBUS_TYPE_STRING, &argv[4], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	predefine_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_LED_CMD,       "sisss",   "i", dbus_led_cmd_handler },
	{ PREDEF_LED_BRT,       "sissss",   "i", dbus_led_brt_handler },
	{ PREDEF_LED_MODE,       "sisssss",   "i", dbus_led_mode_handler },
	/* Add methods here */
};

static void led_init(void *data)
{
	int ret;
	ret = register_edbus_method(DEVICED_PATH_LED, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		 _E("fail to init edbus method(%d)", ret);

	action_entry_add_internal(PREDEF_LED, predefine_action, NULL, NULL);
}

static const struct device_ops led_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "led",
	.init = led_init,
};

DEVICE_OPS_REGISTER(&led_device_ops)
