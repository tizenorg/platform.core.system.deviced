/*
 * deviced
 *
 * Copyright (c) 2012 - 2015 Samsung Electronics Co., Ltd.
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

#include "log.h"
#include "device-notifier.h"
#include "devices.h"
#include "udev.h"
#include "list.h"
#include "edbus-handler.h"

#define KERNEL          "kernel"
#define UDEV            "udev"

#define UDEV_MONITOR_SIZE   (10*1024)

struct uevent_info {
	struct udev_monitor *mon;
	Ecore_Fd_Handler *fdh;
	dd_list *event_list;
};

/* Uevent */
static struct udev *udev;

static struct uevent_info kevent; /* kernel */
static struct uevent_info uevent; /* udev */

static Eina_Bool uevent_control_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct uevent_info *info = data;
	struct udev_device *dev;
	struct uevent_handler *l;
	dd_list *elem;
	const char *subsystem;
	int len;

	assert(info);

	dev = udev_monitor_receive_device(info->mon);
	if (!dev)
		return ECORE_CALLBACK_RENEW;

	subsystem = udev_device_get_subsystem(dev);
	if (!subsystem)
		goto out;

	len = strlen(subsystem);
	DD_LIST_FOREACH(info->event_list, elem, l) {
		if (!strncmp(l->subsystem, subsystem, len) &&
		    l->uevent_func)
			l->uevent_func(dev);
	}

out:
	udev_device_unref(dev);
	return ECORE_CALLBACK_RENEW;
}

static int uevent_control_stop(struct uevent_info *info)
{
	struct udev_device *dev;

	if (!info)
		return -EINVAL;

	if (info->fdh) {
		ecore_main_fd_handler_del(info->fdh);
		info->fdh = NULL;
	}
	if (info->mon) {
		dev = udev_monitor_receive_device(info->mon);
		if (dev)
			udev_device_unref(dev);
		udev_monitor_unref(info->mon);
		info->mon = NULL;
	}
	if (udev)
		udev = udev_unref(udev);
	return 0;
}

static int uevent_control_start(const char *type,
		struct uevent_info *info)
{
	struct uevent_handler *l;
	dd_list *elem;
	int fd;
	int ret;

	if (!info)
		return -EINVAL;

	if (info->mon) {
		_E("%s uevent control routine is alreay started", type);
		return -EINVAL;
	}

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EINVAL;
		}
	} else
		udev = udev_ref(udev);

	info->mon = udev_monitor_new_from_netlink(udev, type);
	if (info->mon == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	ret = udev_monitor_set_receive_buffer_size(info->mon,
			UDEV_MONITOR_SIZE);
	if (ret != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	DD_LIST_FOREACH(info->event_list, elem, l) {
		ret = udev_monitor_filter_add_match_subsystem_devtype(
				info->mon,
				l->subsystem, NULL);
		if (ret < 0) {
			_E("error apply subsystem filter");
			goto stop;
		}
	}

	ret = udev_monitor_filter_update(info->mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	fd = udev_monitor_get_fd(info->mon);
	if (fd == -1) {
		_E("error udev_monitor_get_fd");
		goto stop;
	}

	info->fdh = ecore_main_fd_handler_add(fd, ECORE_FD_READ,
			uevent_control_cb, info, NULL, NULL);
	if (!info->fdh) {
		_E("error ecore_main_fd_handler_add");
		goto stop;
	}

	if (udev_monitor_enable_receiving(info->mon) < 0) {
		_E("error unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	uevent_control_stop(info);
	return -EINVAL;
}

static int register_uevent_control(struct uevent_info *info,
		const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *elem;
	int r;
	bool matched = false;
	int len;

	if (!info || !uh || !uh->subsystem)
		return -EINVAL;

	/* if udev is not initialized, it just will be added list */
	if (!udev || !info->mon)
		goto add_list;

	len = strlen(uh->subsystem);
	/* check if the same subsystem is already added */
	DD_LIST_FOREACH(info->event_list, elem, l) {
		if (!strncmp(l->subsystem, uh->subsystem, len)) {
			matched = true;
			break;
		}
	}

	/* the first request to add subsystem */
	if (!matched) {
		r = udev_monitor_filter_add_match_subsystem_devtype(info->mon,
				uh->subsystem, NULL);
		if (r < 0) {
			_E("fail to add %s subsystem : %d", uh->subsystem, r);
			return -EPERM;
		}
	}

	r = udev_monitor_filter_update(info->mon);
	if (r < 0)
		_E("fail to update udev monitor filter : %d", r);

add_list:
	DD_LIST_APPEND(info->event_list, uh);
	return 0;
}

static int unregister_uevent_control(struct uevent_info *info,
		const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *n, *next;
	int len;

	if (!info || !uh || !uh->subsystem)
		return -EINVAL;

	len = strlen(uh->subsystem);
	DD_LIST_FOREACH_SAFE(info->event_list, n, next, l) {
		if (!strncmp(l->subsystem, uh->subsystem, len) &&
		    l->uevent_func == uh->uevent_func) {
			DD_LIST_REMOVE(info->event_list, l);
			return 0;
		}
	}

	return -ENOENT;
}

int register_kernel_uevent_control(const struct uevent_handler *uh)
{
	return register_uevent_control(&kevent, uh);
}

int unregister_kernel_uevent_control(const struct uevent_handler *uh)
{
	return unregister_uevent_control(&kevent, uh);
}

int register_udev_uevent_control(const struct uevent_handler *uh)
{
	return register_uevent_control(&uevent, uh);
}

int unregister_udev_uevent_control(const struct uevent_handler *uh)
{
	return unregister_uevent_control(&uevent, uh);
}

static int device_change_poweroff(void *data)
{
	uevent_control_stop(&kevent);
	uevent_control_stop(&uevent);
	return 0;
}

static void udev_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);

	if (uevent_control_start(KERNEL, &kevent) != 0)
		_E("fail uevent kernel control init");

	if (uevent_control_start(UDEV, &uevent) != 0)
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
