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
#include "core/edbus-handler.h"
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

#define SIGNAL_USB_HOST_CHANGED "ChangedDevice"

/**
 * Below usb host class is defined by www.usb.org.
 * Please refer to below site.
 * http://www.usb.org/developers/defined_class
 * You can find the detail class codes in linux/usb/ch9.h.
 * Deviced uses kernel defines.
 */
#include <linux/usb/ch9.h>
#define USB_CLASS_ALL   0xffffffff

/**
 * HID Standard protocol information.
 * Please refer to below site.
 * http://www.usb.org/developers/hidpage/HID1_11.pdf
 * Below protocol only has meaning
 * if the subclass is a boot interface subclass,
 * otherwise it is 0.
 */
enum usbhost_hid_protocol {
	USB_HOST_HID_KEYBOARD = 1,
	USB_HOST_HID_MOUSE    = 2,
};

enum usbhost_state {
	USB_HOST_REMOVED,
	USB_HOST_ADDED,
};

struct usbhost_device {
	char devpath[PATH_MAX]; /* unique info. */
	int baseclass;
	int subclass;
	int protocol;
	int vendorid;
	int productid;
	char *manufacturer;
	char *product;
	char *serial;
};

static dd_list *usbhost_list;

static void print_usbhost(struct usbhost_device *usbhost)
{
	if (!usbhost)
		return;

	_I("devpath : %s", usbhost->devpath);
	_I("interface baseclass : %xh", usbhost->baseclass);
	_I("interface subclass : %xh", usbhost->subclass);
	_I("interface protocol : %xh", usbhost->protocol);
	_I("vendor id : %xh", usbhost->vendorid);
	_I("product id : %xh", usbhost->productid);
	_I("manufacturer : %s", usbhost->manufacturer);
	_I("product : %s", usbhost->product);
	_I("serial : %s", usbhost->serial);
}

static void broadcast_usbhost_signal(enum usbhost_state state,
		struct usbhost_device *usbhost)
{
	char *arr[10];
	char str_state[32];
	char str_baseclass[32];
	char str_subclass[32];
	char str_protocol[32];
	char str_vendorid[32];
	char str_productid[32];

	if (!usbhost)
		return;

	snprintf(str_state, sizeof(str_state), "%d", state);
	arr[0] = str_state;
	arr[1] = usbhost->devpath;
	snprintf(str_baseclass, sizeof(str_baseclass),
			"%d", usbhost->baseclass);
	arr[2] = str_baseclass;
	snprintf(str_subclass, sizeof(str_subclass),
			"%d", usbhost->subclass);
	arr[3] = str_subclass;
	snprintf(str_protocol, sizeof(str_protocol),
			"%d", usbhost->protocol);
	arr[4] = str_protocol;
	snprintf(str_vendorid, sizeof(str_vendorid),
			"%d", usbhost->vendorid);
	arr[5] = str_vendorid;
	snprintf(str_productid, sizeof(str_productid),
			"%d", usbhost->productid);
	arr[6] = str_productid;
	arr[7] = usbhost->manufacturer;
	arr[8] = usbhost->product;
	arr[9] = usbhost->serial;

	broadcast_edbus_signal(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			SIGNAL_USB_HOST_CHANGED,
			"isiiiiisss", arr);
}

static int add_usbhost_list(struct udev_device *dev, const char *devpath)
{
	struct usbhost_device *usbhost;
	const char *str;
	struct udev_device *parent;

	/* allocate new usbhost device */
	usbhost = calloc(1, sizeof(struct usbhost_device));
	if (!usbhost) {
		_E("fail to allocate usbhost memory : %s", strerror(errno));
		return -errno;
	}

	/* save the devnode */
	snprintf(usbhost->devpath, sizeof(usbhost->devpath),
			"%s", devpath);

	/* get usb interface informations */
	str = udev_device_get_sysattr_value(dev, USB_INTERFACE_CLASS);
	if (str)
		usbhost->baseclass = (int)strtol(str, NULL, 16);
	str = udev_device_get_sysattr_value(dev, USB_INTERFACE_SUBCLASS);
	if (str)
		usbhost->subclass = (int)strtol(str, NULL, 16);
	str = udev_device_get_sysattr_value(dev, USB_INTERFACE_PROTOCOL);
	if (str)
		usbhost->protocol = (int)strtol(str, NULL, 16);

	/* parent has a lot of information about usb_interface */
	parent = udev_device_get_parent(dev);
	if (!parent) {
		_E("fail to get parent");
		free(usbhost);
		return -EPERM;
	}

	/* get usb device informations */
	str = udev_device_get_sysattr_value(parent, USB_VENDOR_ID);
	if (str)
		usbhost->vendorid = (int)strtol(str, NULL, 16);
	str = udev_device_get_sysattr_value(parent, USB_PRODUCT_ID);
	if (str)
		usbhost->productid = (int)strtol(str, NULL, 16);
	str = udev_device_get_sysattr_value(parent, USB_MANUFACTURER);
	if (str)
		usbhost->manufacturer = strdup(str);
	str = udev_device_get_sysattr_value(parent, USB_PRODUCT);
	if (str)
		usbhost->product = strdup(str);
	str = udev_device_get_sysattr_value(parent, USB_SERIAL);
	if (str)
		usbhost->serial = strdup(str);

	DD_LIST_APPEND(usbhost_list, usbhost);

	broadcast_usbhost_signal(USB_HOST_ADDED, usbhost);

	/* for debugging */
	_I("USB HOST Added");
	print_usbhost(usbhost);

	return 0;
}

static int remove_usbhost_list(const char *devpath)
{
	struct usbhost_device *usbhost;
	dd_list *n, *next;

	/* find the matched item */
	DD_LIST_FOREACH_SAFE(usbhost_list, n, next, usbhost) {
		if (!strncmp(usbhost->devpath,
					devpath, sizeof(usbhost->devpath)))
			break;
	}

	if (!usbhost) {
		_E("fail to find the matched usbhost device");
		return -ENODEV;
	}

	broadcast_usbhost_signal(USB_HOST_REMOVED, usbhost);

	/* for debugging */
	_I("USB HOST Removed");
	_I("devpath : %s", usbhost->devpath);

	DD_LIST_REMOVE(usbhost_list, usbhost);
	free(usbhost->manufacturer);
	free(usbhost->product);
	free(usbhost->serial);
	free(usbhost);

	return 0;
}

static void remove_all_usbhost_list(void)
{
	struct usbhost_device *usbhost;
	dd_list *n, *next;

	DD_LIST_FOREACH_SAFE(usbhost_list, n, next, usbhost) {

		/* for debugging */
		_I("USB HOST Removed");
		_I("devpath : %s", usbhost->devpath);

		DD_LIST_REMOVE(usbhost_list, usbhost);
		free(usbhost->manufacturer);
		free(usbhost->product);
		free(usbhost->serial);
		free(usbhost);
	}
}

static void uevent_usbhost_handler(struct udev_device *dev)
{
	const char *subsystem;
	const char *devtype;
	const char *devpath;
	const char *action;

	/**
	 * Usb host device must have at least one interface.
	 * An interface is matched with a specific usb class.
	 */
	subsystem = udev_device_get_subsystem(dev);
	devtype = udev_device_get_devtype(dev);
	if (!subsystem || !devtype) {
		_E("fail to get subsystem or devtype");
		return;
	}

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
	_I("subsystem : %s, devtype : %s", subsystem, devtype);
	if (strncmp(subsystem, USB_SUBSYSTEM, sizeof(USB_SUBSYSTEM)) ||
	    strncmp(devtype, USB_INTERFACE_DEVTYPE, sizeof(USB_INTERFACE_DEVTYPE)))
		return;

	action = udev_device_get_action(dev);
	if (!strncmp(action, UDEV_ADD, sizeof(UDEV_ADD))) 
		add_usbhost_list(dev, devpath);
	else if (!strncmp(action, UDEV_REMOVE, sizeof(UDEV_REMOVE)))
		remove_usbhost_list(devpath);
}

static DBusMessage *print_device_list(E_DBus_Object *obj, DBusMessage *msg)
{
	dd_list *elem;
	struct usbhost_device *usbhost;
	int cnt = 0;

	DD_LIST_FOREACH(usbhost_list, elem, usbhost) {
		_I("== [%2d USB HOST DEVICE] ===============", cnt++);
		print_usbhost(usbhost);
	}

	return dbus_message_new_method_return(msg);
}

static DBusMessage *get_device_list(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessageIter arr;
	DBusMessageIter s;
	DBusMessage *reply;
	dd_list *elem;
	struct usbhost_device *usbhost;
	const char *str;
	int baseclass;

	reply = dbus_message_new_method_return(msg);

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &baseclass, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		goto out;
	}

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter,
			DBUS_TYPE_ARRAY, "(siiiiisss)", &arr);

	DD_LIST_FOREACH(usbhost_list, elem, usbhost) {
		if (baseclass != USB_CLASS_ALL && usbhost->baseclass != baseclass)
			continue;
		dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, NULL, &s);
		str = usbhost->devpath;
		dbus_message_iter_append_basic(&s, DBUS_TYPE_STRING, &str);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_INT32, &usbhost->baseclass);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_INT32, &usbhost->subclass);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_INT32, &usbhost->protocol);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_INT32, &usbhost->vendorid);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_INT32, &usbhost->productid);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_STRING, &usbhost->manufacturer);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_STRING, &usbhost->product);
		dbus_message_iter_append_basic(&s,
				DBUS_TYPE_STRING, &usbhost->serial);
		dbus_message_iter_close_container(&arr, &s);
	}

	dbus_message_iter_close_container(&iter, &arr);

out:
	return reply;
}

static DBusMessage *get_device_list_count(E_DBus_Object *obj, DBusMessage *msg)
{
	dd_list *elem;
	struct usbhost_device *usbhost;
	int baseclass;
	int ret = 0;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &baseclass, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	DD_LIST_FOREACH(usbhost_list, elem, usbhost) {
		if (baseclass != USB_CLASS_ALL && usbhost->baseclass != baseclass)
			continue;
		ret++;
	}

out:
	return make_reply_message(msg, ret);
}

static struct uevent_handler uh = {
	.subsystem = USB_SUBSYSTEM,
	.uevent_func = uevent_usbhost_handler,
};

static const struct edbus_method edbus_methods[] = {
	{ "PrintDeviceList",   NULL,           NULL, print_device_list }, /* for debugging */
	{ "GetDeviceList",      "i", "a(siiiiisss)", get_device_list },
	{ "GetDeviceListCount", "i",            "i", get_device_list_count },
};

static void usbhost_init(void *data)
{
	int ret;

	/* register usbhost uevent */
	ret = register_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register usb uevent : %d", ret);

	/* register usbhost interface and method */
	ret = register_edbus_interface_and_method(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to register edbus interface and method! %d", ret);
}

static void usbhost_exit(void *data)
{
	int ret;

	/* unreigset usbhost uevent */
	ret = unregister_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister usb uevent : %d", ret);

	/* remove all usbhost list */
	remove_all_usbhost_list();
}

static const struct device_ops usbhost_device_ops = {
	.name	= "usbhost",
	.init	= usbhost_init,
	.exit	= usbhost_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_ops)
