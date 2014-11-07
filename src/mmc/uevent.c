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
#include <Ecore.h>
#include "core/udev.h"
#include "core/device-notifier.h"
#include "core/log.h"

/* block device */
#define BLOCK_SUBSYSTEM		"block"
#define MMC_PATH			"*/mmcblk[0-9]"
#define BLOCK_DEVPATH			"disk"

static struct udev_monitor *mon;
static struct udev *udev;
static Ecore_Fd_Handler *ufdh;
static int ufd;

static Eina_Bool mmc_uevent_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct udev_device *dev;
	const char *subsystem, *devpath, *action, *devnode;

	dev = udev_monitor_receive_device(mon);
	if (!dev)
		return EINA_TRUE;

	subsystem = udev_device_get_subsystem(dev);
	if (strcmp(subsystem, BLOCK_SUBSYSTEM))
		goto out;

	devpath = udev_device_get_devpath(dev);
	if (fnmatch(MMC_PATH, devpath, 0))
		goto out;

	_D("mmc uevent occurs!");

	action = udev_device_get_action(dev);
	devnode = udev_device_get_devnode(dev);
	if (!action || !devnode)
		goto out;

	if (!strcmp(action, UDEV_ADD))
		device_notify(DEVICE_NOTIFIER_MMC, (void *)devnode);
	else if (!strcmp(action, UDEV_REMOVE))
		device_notify(DEVICE_NOTIFIER_MMC, NULL);

out:
	udev_device_unref(dev);
	return EINA_TRUE;
}

int mmc_uevent_start(void)
{
	int r;

	if (udev) {
		_D("uevent control is already started");
		return -EPERM;
	}

	udev = udev_new();
	if (!udev)
		return -EPERM;

	mon = udev_monitor_new_from_netlink(udev, UDEV);
	if (!mon)
		goto stop;

	r = udev_monitor_set_receive_buffer_size(mon, UDEV_MONITOR_SIZE);
	if (r < 0)
		goto stop;

	r = udev_monitor_filter_add_match_subsystem_devtype(mon, BLOCK_SUBSYSTEM, NULL);
	if (r < 0)
		goto stop;

	r = udev_monitor_filter_update(mon);
	if (r < 0)
		_E("error udev_monitor_filter_update");

	ufd = udev_monitor_get_fd(mon);
	if (ufd < 0)
		goto stop;

	ufdh = ecore_main_fd_handler_add(ufd, ECORE_FD_READ, mmc_uevent_cb, NULL, NULL, NULL);
	if (!ufdh)
		goto stop;

	r = udev_monitor_enable_receiving(mon);
	if (r < 0)
		goto stop;

	return 0;

stop:
	mmc_uevent_stop();
	return -EPERM;
}

int mmc_uevent_stop(void)
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
