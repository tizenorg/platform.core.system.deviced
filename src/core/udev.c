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

#include "log.h"
#include "device-notifier.h"
#include "devices.h"
#include "udev.h"
#include "list.h"
#include "edbus-handler.h"

#define PREDEF_UDEV_CONTROL		UDEV

/* Uevent */
static struct udev *udev;
/* Kernel Uevent */
static struct udev_monitor *mon;
static Ecore_Fd_Handler *ufdh;
static int ufd = -1;

static dd_list *udev_event_list;

static Eina_Bool uevent_kernel_control_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct udev_device *dev;
	struct uevent_handler *l;
	dd_list *elem;
	const char *subsystem;

	if ((dev = udev_monitor_receive_device(mon)) == NULL)
		return EINA_TRUE;

	subsystem = udev_device_get_subsystem(dev);

	DD_LIST_FOREACH(udev_event_list, elem, l) {
		if (!strncmp(subsystem, l->subsystem, strlen(subsystem)) &&
		    l->uevent_func)
			l->uevent_func(dev);
	}

	udev_device_unref(dev);
	return EINA_TRUE;
}

static int uevent_kernel_control_stop(void)
{
	struct udev_device *dev = NULL;

	if (ufdh) {
		ecore_main_fd_handler_del(ufdh);
		ufdh = NULL;
	}
	if (ufd >= 0) {
		close(ufd);
		ufd = -1;
	}
	if (mon) {
		dev = udev_monitor_receive_device(mon);
		if (dev) {
			udev_device_unref(dev);
			dev = NULL;
		}
		udev_monitor_unref(mon);
		mon = NULL;
	}
	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}
	return 0;
}

static int uevent_kernel_control_start(void)
{
	struct uevent_handler *l;
	dd_list *elem;
	int i, ret;

	if (udev && mon) {
		_E("uevent control routine is alreay started");
		return -EINVAL;
	}

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EINVAL;
		}
	}

	mon = udev_monitor_new_from_netlink(udev, UDEV);
	if (mon == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	if (udev_monitor_set_receive_buffer_size(mon, UDEV_MONITOR_SIZE) != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	DD_LIST_FOREACH(udev_event_list, elem, l) {
		ret = udev_monitor_filter_add_match_subsystem_devtype(mon,
				l->subsystem, NULL);
		if (ret < 0) {
			_E("error apply subsystem filter");
			goto stop;
		}
	}

	ret = udev_monitor_filter_update(mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	ufd = udev_monitor_get_fd(mon);
	if (ufd == -1) {
		_E("error udev_monitor_get_fd");
		goto stop;
	}

	ufdh = ecore_main_fd_handler_add(ufd, ECORE_FD_READ,
			uevent_kernel_control_cb, NULL, NULL, NULL);
	if (!ufdh) {
		_E("error ecore_main_fd_handler_add");
		goto stop;
	}

	if (udev_monitor_enable_receiving(mon) < 0) {
		_E("error unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	uevent_kernel_control_stop();
	return -EINVAL;

}

int register_kernel_uevent_control(const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *elem;
	int r;
	bool matched = false;

	if (!uh)
		return -EINVAL;

	/* if udev is not initialized, it just will be added list */
	if (!udev || !mon)
		goto add_list;

	/* check if the same subsystem is already added */
	DD_LIST_FOREACH(udev_event_list, elem, l) {
		if (!strncmp(l->subsystem, uh->subsystem, strlen(l->subsystem))) {
			matched = true;
			break;
		}
	}

	/* the first request to add subsystem */
	if (!matched) {
		r = udev_monitor_filter_add_match_subsystem_devtype(mon,
				uh->subsystem, NULL);
		if (r < 0) {
			_E("fail to add %s subsystem : %d", uh->subsystem, r);
			return -EPERM;
		}
	}

	r = udev_monitor_filter_update(mon);
	if (r < 0)
		_E("fail to update udev monitor filter : %d", r);

add_list:
	DD_LIST_APPEND(udev_event_list, uh);
	return 0;
}

int unregister_kernel_uevent_control(const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *n, *next;

	DD_LIST_FOREACH_SAFE(udev_event_list, n, next, l) {
		if (!strncmp(l->subsystem, uh->subsystem, strlen(l->subsystem)) &&
		    l->uevent_func == uh->uevent_func) {
			DD_LIST_REMOVE(udev_event_list, l);
			return 0;
		}
	}

	return -ENOENT;
}

int uevent_udev_get_path(const char *subsystem, dd_list **list)
{
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices, *dev_list_entry;
	int ret;

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EIO;
		}
	}

	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
		return -EIO;

	ret = udev_enumerate_add_match_subsystem(enumerate, subsystem);
	if (ret < 0)
		return -EIO;

	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0)
		return -EIO;

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		path = udev_list_entry_get_name(dev_list_entry);
		_D("subsystem : %s, path : %s", subsystem, path);
		DD_LIST_APPEND(*list, (void*)path);
	}

	return 0;
}

static DBusMessage *dbus_udev_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
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

	if (strncmp(argv, "start", strlen("start")) == 0) {
		ret = uevent_kernel_control_start();
	} else if (strncmp(argv, "stop", strlen("stop")) == 0) {
		ret = uevent_kernel_control_stop();
	}

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_UDEV_CONTROL,   "sis","i", dbus_udev_handler },
};

static int device_change_poweroff(void *data)
{
	uevent_kernel_control_stop();
	return 0;
}

static void udev_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	if (uevent_kernel_control_start() != 0) {
		_E("fail uevent control init");
		return;
	}
}

static void udev_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);
}

static const struct device_ops udev_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "udev",
	.init     = udev_init,
	.exit     = udev_exit,
};

DEVICE_OPS_REGISTER(&udev_device_ops)
