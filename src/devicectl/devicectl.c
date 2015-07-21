/*
 * devicectl
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <shared/dbus.h>
#include <core/common.h>
#include "usb.h"

/*
 * devicectl [device] [action]
 * ex> devicectl display stop
 *     devicectl pass start
 */

enum device_type {
	DEVICE_CORE,
	DEVICE_DISPLAY,
	DEVICE_LED,
	DEVICE_PASS,
	DEVICE_USB,
	DEVICE_EXTCON,
	DEVICE_MAX,
	DEVICE_ALL,
};
static enum device_type arg_id;

static const struct device {
	const enum device_type id;
	const char *name;
	const char *path;
	const char *iface;
} devices[] = {
	{ DEVICE_CORE,    "core",    DEVICED_PATH_CORE,    DEVICED_INTERFACE_CORE    },
	{ DEVICE_DISPLAY, "display", DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY },
	{ DEVICE_LED,     "led",     DEVICED_PATH_LED,     DEVICED_INTERFACE_LED     },
	{ DEVICE_PASS,    "pass",    DEVICED_PATH_PASS,    DEVICED_INTERFACE_PASS    },
	{ DEVICE_USB,     "usb",     DEVICED_PATH_USB,     DEVICED_INTERFACE_USB     },
	{ DEVICE_EXTCON,  "extcon",  DEVICED_PATH_EXTCON,  DEVICED_INTERFACE_EXTCON  },
};

static int start_device(char **args)
{
	DBusMessage *msg;

	if (!args[1])
		return -EINVAL;

	printf("start %s device!\n", args[1]);

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		    devices[arg_id].path, devices[arg_id].iface,
		    "start", NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_message_unref(msg);

	return 0;
}

static int stop_device(char **args)
{
	DBusMessage *msg;

	if (!args[1])
		return -EINVAL;

	printf("stop %s device!\n", args[1]);

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		    devices[arg_id].path, devices[arg_id].iface,
		    "stop", NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_message_unref(msg);

	return 0;
}

static int dump_mode(char **args)
{
	DBusError err;
	DBusMessage *msg;
	int ret, val;
	char *arr[1];

	if (!args[1] || !args[2] || !args[3])
		return -EINVAL;

	printf("%s (%s %s)!\n", args[1], args[2], args[3]);

	arr[0] = args[3];
	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		    devices[arg_id].path, devices[arg_id].iface,
		    "Dumpmode", "s", arr);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		printf("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		val = -ENOMSG;
	}

	dbus_message_unref(msg);
	return val;
}

static int save_log(char **args)
{
	DBusMessage *msg;

	if (!args[1])
		return -EINVAL;

	printf("save log %s device!\n", args[1]);

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		    devices[arg_id].path, devices[arg_id].iface,
		    "SaveLog", NULL, NULL);
	if (!msg)
		return -EBADMSG;

	dbus_message_unref(msg);

	return 0;
}

static int set_usb_mode(char **args)
{
	return load_usb_mode(args[3]);
}

static int unset_usb_mode(char **args)
{
	return unload_usb_mode(args[3]);
}

static int enable_device(char **args)
{
	DBusMessage *msg;
	char *arr[1];

	if (!args[3])
		return -EINVAL;

	printf("enable %s device!\n", args[3]);

	arr[0] = args[3];

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		    devices[arg_id].path, devices[arg_id].iface,
		    "enable", "s", arr);
	if (!msg)
		return -EBADMSG;

	dbus_message_unref(msg);

	return 0;
}

static int disable_device(char **args)
{
	DBusMessage *msg;
	char *arr[1];

	if (!args[3])
		return -EINVAL;

	printf("disable %s device!\n", args[3]);

	arr[0] = args[3];

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
		    devices[arg_id].path, devices[arg_id].iface,
		    "disable", "s", arr);
	if (!msg)
		return -EBADMSG;

	dbus_message_unref(msg);

	return 0;
}

static const struct action {
	const enum device_type id;
	const char *action;
	const int argc;
	int (* const func)(char **args);
	const char *option;
} actions[] = {
	{ DEVICE_ALL,       "start",           3, start_device,      ""            },
	{ DEVICE_ALL,       "stop",            3, stop_device,       ""            },
	{ DEVICE_DISPLAY,   "dumpmode",        4, dump_mode,         "[on|off]"    },
	{ DEVICE_LED,       "dumpmode",        4, dump_mode,         "[on|off]"    },
	{ DEVICE_DISPLAY,   "savelog",         3, save_log,          ""            },
	{ DEVICE_USB,       "set",             4, set_usb_mode,      "[sdb|ssh]"   },
	{ DEVICE_USB,       "unset",           4, unset_usb_mode,    "[sdb|ssh]"   },
	{ DEVICE_EXTCON,    "enable",          4, enable_device,     "[USB|HEADPHONE|HDMI|DOCK]" },
	{ DEVICE_EXTCON,    "disable",         4, disable_device,    "[USB|HEADPHONE|HDMI|DOCK]" },
};

static inline void usage()
{
	printf("[usage] devicectl <device_name> <action>\n");
	printf("Please use option --help to check options\n");
}

static void help()
{
	int i;

	printf("[usage] devicectl <device_name> <action> <option>\n");
	printf("device name & action & option\n");
	for (i = 0; i < ARRAY_SIZE(actions); i++) {
		if (actions[i].id == DEVICE_ALL) {
			printf("    [all-device] %s %s\n", actions[i].action,
			    actions[i].option);
		} else {
			printf("    %s %s %s\n", devices[actions[i].id].name,
			    actions[i].action, actions[i].option);
		}
	}
}

int main(int argc, char *argv[])
{
	int i;

	if (argc == 2 && !strcmp(argv[1], "--help")) {
		help();
		return 0;
	}

	if (argc < 3) {
		usage();
		return -EINVAL;
	}

	for (i = 0; i < argc; i++)
		if (argv[i] == NULL) {
			usage();
			return -EINVAL;
		}

	for (i = 0; i < ARRAY_SIZE(devices); i++)
		if (!strcmp(argv[1], devices[i].name))
			break;

	if (i >= ARRAY_SIZE(devices)) {
		printf("invalid device name! %s\n", argv[1]);
		usage();
		return -EINVAL;
	}

	arg_id = devices[i].id;

	for (i = 0; i < ARRAY_SIZE(actions); i++)
		if (actions[i].id == arg_id || actions[i].id == DEVICE_ALL)
			if (!strcmp(argv[2], actions[i].action))
				break;

	if (i >= ARRAY_SIZE(actions)) {
		printf("invalid action name! %s\n", argv[2]);
		usage();
		return -EINVAL;
	}

	if (actions[i].argc != argc) {
		printf("invalid arg count!\n");
		usage();
		return -EINVAL;
	}

	return actions[i].func(argv);
}

