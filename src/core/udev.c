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

#define PREDEF_UDEV_CONTROL		KERNEL

/* Uevent */
static struct udev *udev;
/* Kernel Uevent */
static struct udev_monitor *mon;
static Ecore_Fd_Handler *ufdh;
static int ufd = -1;
static dd_list *event_list;

/* Udev Uevent */
static struct udev_monitor *udev_mon;
static Ecore_Fd_Handler *udev_ufdh;
static int udev_ufd = -1;
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

	DD_LIST_FOREACH(event_list, elem, l) {
		if (!strncmp(subsystem, l->subsystem, strlen(subsystem)) &&
		    l->uevent_func)
			l->uevent_func(dev);
	}

	udev_device_unref(dev);
	return EINA_TRUE;
}

static int uevent_kernel_control_stop(void)
{
	struct udev_device *dev;

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
	if (udev && !udev_mon) {
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

	mon = udev_monitor_new_from_netlink(udev, KERNEL);
	if (mon == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	if (udev_monitor_set_receive_buffer_size(mon, UDEV_MONITOR_SIZE) != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	DD_LIST_FOREACH(event_list, elem, l) {
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

static Eina_Bool uevent_udev_control_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct udev_device *dev;
	struct uevent_handler *l;
	dd_list *elem;
	const char *subsystem;

	if ((dev = udev_monitor_receive_device(udev_mon)) == NULL)
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

static int uevent_udev_control_stop(void)
{
	struct udev_device *dev;

	if (udev_ufdh) {
		ecore_main_fd_handler_del(udev_ufdh);
		udev_ufdh = NULL;
	}
	if (udev_ufd >= 0) {
		close(udev_ufd);
		udev_ufd = -1;
	}
	if (udev_mon) {
		dev = udev_monitor_receive_device(udev_mon);
		if (dev) {
			udev_device_unref(dev);
			dev = NULL;
		}
		udev_monitor_unref(udev_mon);
		udev_mon = NULL;
	}
	if (udev && !mon) {
		udev_unref(udev);
		udev = NULL;
	}
	return 0;
}

static int uevent_udev_control_start(void)
{
	struct uevent_handler *l;
	dd_list *elem;
	int i, ret;

	if (udev && udev_mon) {
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

	udev_mon = udev_monitor_new_from_netlink(udev, UDEV);
	if (mon == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	if (udev_monitor_set_receive_buffer_size(udev_mon, UDEV_MONITOR_SIZE) != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	DD_LIST_FOREACH(udev_event_list, elem, l) {
		ret = udev_monitor_filter_add_match_subsystem_devtype(udev_mon,
				l->subsystem, NULL);
		if (ret < 0) {
			_E("error apply subsystem filter");
			goto stop;
		}
	}

	ret = udev_monitor_filter_update(udev_mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	udev_ufd = udev_monitor_get_fd(udev_mon);
	if (udev_ufd == -1) {
		_E("error udev_monitor_get_fd");
		goto stop;
	}

	udev_ufdh = ecore_main_fd_handler_add(udev_ufd, ECORE_FD_READ,
			uevent_udev_control_cb, NULL, NULL, NULL);
	if (!udev_ufdh) {
		_E("error ecore_main_fd_handler_add");
		goto stop;
	}

	if (udev_monitor_enable_receiving(udev_mon) < 0) {
		_E("error unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	uevent_udev_control_stop();
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
	DD_LIST_FOREACH(event_list, elem, l) {
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
	DD_LIST_APPEND(event_list, uh);
	return 0;
}

int unregister_kernel_uevent_control(const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *n, *next;

	DD_LIST_FOREACH_SAFE(event_list, n, next, l) {
		if (!strncmp(l->subsystem, uh->subsystem, strlen(l->subsystem)) &&
		    l->uevent_func == uh->uevent_func) {
			DD_LIST_REMOVE(event_list, l);
			return 0;
		}
	}

	return -ENOENT;
}

int register_udev_uevent_control(const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *elem;
	int r;
	bool matched = false;

	if (!uh)
		return -EINVAL;

	/* if udev is not initialized, it just will be added list */
	if (!udev || !udev_mon)
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
		r = udev_monitor_filter_add_match_subsystem_devtype(udev_mon,
				uh->subsystem, NULL);
		if (r < 0) {
			_E("fail to add %s subsystem : %d", uh->subsystem, r);
			return -EPERM;
		}
	}

	r = udev_monitor_filter_update(udev_mon);
	if (r < 0)
		_E("fail to update udev monitor filter : %d", r);

add_list:
	DD_LIST_APPEND(udev_event_list, uh);
	return 0;
}

int unregister_udev_uevent_control(const struct uevent_handler *uh)
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

static DBusMessage *dbus_start_kernel_monitor(E_DBus_Object *obj, DBusMessage *msg)
{
	uevent_kernel_control_start();
	return dbus_message_new_method_return(msg);
}

static DBusMessage *dbus_stop_kernel_monitor(E_DBus_Object *obj, DBusMessage *msg)
{
	uevent_kernel_control_stop();
	return dbus_message_new_method_return(msg);
}

static DBusMessage *dbus_start_udev_monitor(E_DBus_Object *obj, DBusMessage *msg)
{
	uevent_udev_control_start();
	return dbus_message_new_method_return(msg);
}

static DBusMessage *dbus_stop_udev_monitor(E_DBus_Object *obj, DBusMessage *msg)
{
	uevent_udev_control_stop();
	return dbus_message_new_method_return(msg);
}

static const struct edbus_method edbus_methods[] = {
	{ "StartKernelMonitor", NULL, NULL, dbus_start_kernel_monitor },
	{ "StopKernelMonitor",  NULL, NULL, dbus_stop_kernel_monitor },
	{ "StartUdevMonitor",   NULL, NULL, dbus_start_udev_monitor },
	{ "StopUdevMonitor",    NULL, NULL, dbus_stop_udev_monitor },
};

static int device_change_poweroff(void *data)
{
	uevent_kernel_control_stop();
	uevent_udev_control_stop();
	return 0;
}

static void udev_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);

	ret = register_edbus_method(DEVICED_PATH_CORE,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	if (uevent_kernel_control_start() != 0)
		_E("fail uevent kernel control init");

	if (uevent_udev_control_start() != 0)
		_E("fail uevent udev control init");
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
