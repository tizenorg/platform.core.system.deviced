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
#include <sys/types.h>
#include <dd-control.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"

#define CONTROL_HANDLER_NAME		"control"
#define CONTROL_GETTER_NAME			"getcontrol"

static const struct control_device {
	const int id;
	const char *name;
} devices[] = {
	/* Add id & ops to provide start/stop control */
	{ DEVICE_CONTROL_MMC,               "mmc" },
	{ DEVICE_CONTROL_USBCLIENT,   "usbclient" },
};

static int control_handler(int argc, char **argv)
{
	int i;
	int pid;
	int device;
	bool enable;
	int ret;
	const struct device_ops *dev_ops = NULL;

	_I("argc : %d", argc);
	for (i = 0; i < argc; ++i)
		_I("[%2d] %s", i, argv[i]);

	if (argc > 5) {
		_E("Invalid argument");
		errno = EINVAL;
		return -1;
	}

	pid = atoi(argv[0]);
	device = atoi(argv[1]);
	enable = atoi(argv[2]);
	_I("pid : %d, device : %d, enable :%d", pid, device, enable);

	for (i = 0; i < ARRAY_SIZE(devices); i++)
		if (devices[i].id == device)
			break;

	if (i >= ARRAY_SIZE(devices))
		return -EINVAL;
	FIND_DEVICE_INT(dev_ops, devices[i].name);
	if (enable)
		ret = device_start(dev_ops);
	else
		ret = device_stop(dev_ops);

	return ret;
}

static int get_control_handler(int argc, char **argv)
{
	int i;
	int pid;
	int device;
	bool enable;
	int ret;
	const struct device_ops *dev_ops = NULL;

	_I("argc : %d", argc);
	for (i = 0; i < argc; ++i)
		_I("[%2d] %s", i, argv[i]);

	if (argc > 4) {
		_E("Invalid argument");
		errno = EINVAL;
		return -1;
	}

	pid = atoi(argv[0]);
	device = atoi(argv[1]);
	_I("pid : %d, device : %d", pid, device);

	for (i = 0; i < ARRAY_SIZE(devices); i++)
		if (devices[i].id == device)
			break;

	if (i >= ARRAY_SIZE(devices))
		return -EINVAL;

	FIND_DEVICE_INT(dev_ops, devices[i].name);

	return device_get_status(dev_ops);
}


static DBusMessage *dbus_control_handler(E_DBus_Object *obj, DBusMessage *msg)
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

	ret = control_handler(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_get_control_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
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

	ret = get_control_handler(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ CONTROL_HANDLER_NAME, "sisss", "i", dbus_control_handler     },
	{ CONTROL_GETTER_NAME ,  "siss", "i", dbus_get_control_handler },
};

static void control_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static const struct device_ops control_device_ops = {
	.name     = "control",
	.init     = control_init,
};

DEVICE_OPS_REGISTER(&control_device_ops)
