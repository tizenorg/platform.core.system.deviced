/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <limits.h>

#include "core/log.h"
#include "core/devices.h"
#include "core/udev.h"
#include "core/list.h"

#define USB_INTERFACE_CLASS     "bInterfaceClass"
#define USB_INTERFACE_SUBCLASS  "bInterfaceSubClass"
#define USB_INTERFACE_PROTOCOL  "bInterfaceProtocol"
#define USB_VENDOR_ID           "idVendor"
#define USB_PRODUCT_ID          "idProduct"
#define USB_MANUFACTURER        "manufacturer"
#define USB_PRODUCT             "product"
#define USB_SERIAL              "serial"

#define BUF_MAX 255

struct usb_host_device {
	char devpath[PATH_MAX]; /* unique info. */
	int class;
	int subclass;
	int protocol;
	int vendorid;
	int productid;
	char manufacturer[NAME_MAX];
	char product[NAME_MAX];
	char serial[BUF_MAX];
};

static dd_list *usb_host_list;

static void uevent_usb_handler(struct udev_device *dev)
{
	const char *subsystem;
	const char *devtype;
	const char *devpath;
	const char *action;
	const char *str;
	struct udev_device *parent;
	struct usb_host_device *usb_host;
	dd_list *n, *next;

	/**
	 * Usb host device must have at least one interface.
	 * An interface is matched with a specific usb class.
	 */
	subsystem = udev_device_get_subsystem(dev);
	devtype = udev_device_get_devtype(dev);
	_I("subsystem : %s, devtype : %s", subsystem, devtype);

	/* devpath is an unique information among usb host devices */
	devpath = udev_device_get_devpath(dev);
	if (!devpath) {
		_E("fail to get devpath from udev_device");
		return;
	}

	/**
	 * if devtype is not matched with usb subsystem
	 * and usb_interface devtype, skip.
	 */
	if (strncmp(subsystem, USB_SUBSYSTEM, sizeof(USB_SUBSYSTEM)) ||
	    strncmp(devtype, USB_INTERFACE_DEVTYPE, sizeof(USB_INTERFACE_DEVTYPE)))
		return;

	action = udev_device_get_action(dev);
	if (!strncmp(action, UDEV_ADD, sizeof(UDEV_ADD))) {

		/* allocate new usb_host device */
		usb_host = calloc(1, sizeof(struct usb_host_device));
		if (!usb_host) {
			_E("fail to allocate usb_host memory : %s", strerror(errno));
			return;
		}

		/* save the devnode */
		strncpy(usb_host->devpath, devpath, sizeof(usb_host->devpath));

		/* get usb interface informations */
		str = udev_device_get_sysattr_value(dev, USB_INTERFACE_CLASS);
		if (str)
			usb_host->class = (int)strtol(str, NULL, 16);
		str = udev_device_get_sysattr_value(dev, USB_INTERFACE_SUBCLASS);
		if (str)
			usb_host->subclass = (int)strtol(str, NULL, 16);
		str = udev_device_get_sysattr_value(dev, USB_INTERFACE_PROTOCOL);
		if (str)
			usb_host->protocol = (int)strtol(str, NULL, 16);

		/* parent has a lot of information about usb_interface */
		parent = udev_device_get_parent(dev);
		if (!parent) {
			_E("fail to get parent");
			free(usb_host);
			return;
		}

		/* get usb device informations */
		str = udev_device_get_sysattr_value(parent, USB_VENDOR_ID);
		if (str)
			usb_host->vendorid = (int)strtol(str, NULL, 16);
		str = udev_device_get_sysattr_value(parent, USB_PRODUCT_ID);
		if (str)
			usb_host->productid = (int)strtol(str, NULL, 16);
		str = udev_device_get_sysattr_value(parent, USB_MANUFACTURER);
		if (str)
			strncpy(usb_host->manufacturer,
					str, sizeof(usb_host->manufacturer));
		str = udev_device_get_sysattr_value(parent, USB_PRODUCT);
		if (str)
			strncpy(usb_host->product, str, sizeof(usb_host->product));
		str = udev_device_get_sysattr_value(parent, USB_SERIAL);
		if (str)
			strncpy(usb_host->serial, str, sizeof(usb_host->serial));

		DD_LIST_APPEND(usb_host_list, usb_host);

		/* for debugging */
		_I("USB HOST Added");
		_I("devpath : %s", usb_host->devpath);
		_I("interface class : %xh", usb_host->class);
		_I("interface subclass : %xh", usb_host->subclass);
		_I("interface protocol : %xh", usb_host->protocol);
		_I("vendor id : %xh", usb_host->vendorid);
		_I("product id : %xh", usb_host->productid);
		_I("manufacturer : %s", usb_host->manufacturer);
		_I("product : %s", usb_host->product);
		_I("serial : %s", usb_host->serial);

	} else if (!strncmp(action, UDEV_REMOVE, sizeof(UDEV_REMOVE))) {

		/* find the matched item */
		DD_LIST_FOREACH_SAFE(usb_host_list, n, next, usb_host) {
			if (!strncmp(usb_host->devpath,
						devpath, sizeof(usb_host->devpath))) {

				/* for debugging */
				_I("USB HOST Removed");
				_I("devpath : %s", usb_host->devpath);

				DD_LIST_REMOVE(usb_host_list, usb_host);
				free(usb_host);
				break;
			}
		}
	}
}

static struct uevent_handler uh = {
	.subsystem = USB_SUBSYSTEM,
	.uevent_func = uevent_usb_handler,
};

static void usbhost_init(void *data)
{
	int ret;

	/* register usbhost uevent */
	ret = register_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register usb uevent : %d", ret);
}

static void usbhost_exit(void *data)
{
	int ret;

	/* unreigset usbhost uevent */
	ret = unregister_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister usb uevent : %d", ret);
}

static const struct device_ops usbhost_device_ops = {
	.name	= "usbhost",
	.init	= usbhost_init,
	.exit	= usbhost_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_ops)
