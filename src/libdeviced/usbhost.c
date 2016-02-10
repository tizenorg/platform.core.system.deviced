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
#include <errno.h>
#include <E_DBus.h>

#include "log.h"
#include "common.h"
#include "dbus.h"
#include "dd-usbhost.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define METHOD_OPEN_DEVICE                "OpenDevice"
#define METHOD_REQUEST_STORAGE_INFO_ALL   "StorageInfoAll"
#define METHOD_REQUEST_STORAGE_MOUNT      "StorageMount"
#define METHOD_REQUEST_STORAGE_UNMOUNT    "StorageUnmount"
#define METHOD_REQUEST_STORAGE_FORMAT     "StorageFormat"
#define SIGNAL_NAME_USB_STORAGE_CHANGED   "usb_storage_changed"
#define SIGNAL_NAME_USB_DEVICE_CHANGED    "ChangedDevice"
#define RETRY_MAX 5

union action {
	void (*storage)(char *type, char *path, int mount, void *param);
	void (*device)(struct usbhost_device *device, int state);
};

struct signal_handler {
	char *name;
	E_DBus_Signal_Handler *handler;
	union action action;
	void *data;
};

static struct signal_handler handlers[] = {
	{ SIGNAL_NAME_USB_STORAGE_CHANGED    , NULL, NULL, NULL },
	{ SIGNAL_NAME_USB_DEVICE_CHANGED     , NULL, NULL, NULL },
};

static E_DBus_Connection *conn = NULL;

static int register_edbus_signal_handler(const char *path, const char *interface,
		const char *name, E_DBus_Signal_Cb cb,
		union action action,
		void *data)
{
	int i, ret;

	if (!conn) {
		_E("Use init_usbhost_signal() first to use this function");
		ret = -1;
		goto out;
	}

	for (i = 0 ; i < ARRAY_SIZE(handlers) ; i++) {
		if (strncmp(handlers[i].name, name, strlen(name)))
			continue;
		break;
	}
	if (i >= ARRAY_SIZE(handlers)) {
		_E("Failed to find \"%s\" signal", name);
		ret = -1;
		goto out;
	}

	if (handlers[i].handler) {
		_E("The handler is already registered");
		ret = -1;
		goto out;
	}

	handlers[i].handler = e_dbus_signal_handler_add(conn, NULL, path,
					interface, name, cb, NULL);
	if (!(handlers[i].handler)) {
		_E("fail to add edbus handler");
		ret = -1;
		goto out;
	}
	handlers[i].action = action;
	handlers[i].data = data;

	return 0;

out:
	return ret;
}

static void storage_signal_handler(void *data, DBusMessage *msg)
{
	int i;
	char *type, *path;
	int mount;
	DBusError err;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_USBHOST, SIGNAL_NAME_USB_STORAGE_CHANGED) == 0) {
		_E("The signal is not for storage changed");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err,
				DBUS_TYPE_STRING, &type,
				DBUS_TYPE_STRING, &path,
				DBUS_TYPE_INT32, &mount,
				DBUS_TYPE_INVALID) == 0) {
		_E("Failed to get storage info");
		return;
	}

	for (i = 0 ; i < ARRAY_SIZE(handlers) ; i++) {
		if (strcmp(handlers[i].name, SIGNAL_NAME_USB_STORAGE_CHANGED))
			continue;
		break;
	}
	if (i >= ARRAY_SIZE(handlers)) {
		_E("Failed to find \"storage_changed\" signal");
		return;
	}

	if (handlers[i].action.storage)
		handlers[i].action.storage(type, path, mount, handlers[i].data);
}

static void device_signal_handler(void *data, DBusMessage *msg)
{
	int i;
	char *path;
	struct usbhost_device device;
	int state;
	DBusError err;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_USBHOST, SIGNAL_NAME_USB_DEVICE_CHANGED) == 0) {
		_E("The signal is not ChangedDevice");
		return;
	}

	dbus_error_init(&err);
	if (dbus_message_get_args(msg, &err,
				DBUS_TYPE_INT32, &state,
				DBUS_TYPE_STRING, &path,
				DBUS_TYPE_INT32, &device.baseclass,
				DBUS_TYPE_INT32, &device.subclass,
				DBUS_TYPE_INT32, &device.protocol,
				DBUS_TYPE_INT32, &device.vendorid,
				DBUS_TYPE_INT32, &device.productid,
				DBUS_TYPE_STRING, &device.manufacturer,
				DBUS_TYPE_STRING, &device.product,
				DBUS_TYPE_STRING, &device.serial,
				DBUS_TYPE_INVALID) == 0) {
		_E("Failed to get device info");
		return;
	}

	for (i = 0 ; i < ARRAY_SIZE(handlers) ; i++) {
		if (strcmp(handlers[i].name, SIGNAL_NAME_USB_DEVICE_CHANGED))
			continue;
		break;
	}

	if (i >= ARRAY_SIZE(handlers)) {
		_E("Failed to find ChangedDevice signal");
		return;
	}

	strncpy(device.devpath, path, PATH_MAX);

	if (handlers[i].action.device)
		handlers[i].action.device(&device, state);
}

API int init_usbhost_signal(void)
{
	int retry;

	if (conn)
		return 0;

	retry = 0;
	do {
		if (e_dbus_init() > 0)
			break;
		if (retry >= RETRY_MAX)
			return -1;
	} while (retry++ < RETRY_MAX);

	conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!conn) {
		_E("Failed to get edbus bus");
		e_dbus_shutdown();
		return -1;
	}

	return 0;
}

API void deinit_usbhost_signal(void)
{
	int i;

	if (!conn)
		return;

	for (i = 0 ; i < ARRAY_SIZE(handlers) ; i++) {
		if (handlers[i].handler) {
			e_dbus_signal_handler_del(conn, handlers[i].handler);
			handlers[i].handler = NULL;
		}
		handlers[i].action.storage = NULL;
		handlers[i].data = NULL;
	}

	e_dbus_connection_close(conn);
	conn = NULL;

	e_dbus_shutdown();
}

API int request_usb_storage_info(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST, METHOD_REQUEST_STORAGE_INFO_ALL, NULL, NULL);
}

API int register_usb_storage_change_handler(
		void (*storage_changed)(char *type, char *path, int mount, void *param),
		void *data)
{
	union action action;
	if (!storage_changed)
		return -EINVAL;

	action.storage = storage_changed;
	return register_edbus_signal_handler(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			SIGNAL_NAME_USB_STORAGE_CHANGED,
			storage_signal_handler,
			action,
			data);
}

API int register_usb_device_change_handler(
		void (*device_changed)(struct usbhost_device *device, int state),
		void *data)
{
	union action action;
	if (!device_changed)
		return -EINVAL;

	action.device = device_changed;
	return register_edbus_signal_handler(DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			SIGNAL_NAME_USB_DEVICE_CHANGED,
			device_signal_handler,
			action,
			data);
}

API int unregister_usb_storage_change_handler(void)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(handlers) ; i++) {
		if (strcmp(handlers[i].name, SIGNAL_NAME_USB_STORAGE_CHANGED))
			continue;
		if (handlers[i].handler == NULL)
			continue;

		e_dbus_signal_handler_del(conn, handlers[i].handler);
		handlers[i].handler = NULL;
		handlers[i].action.storage = NULL;
		handlers[i].data = NULL;
		return 0;
	}
	return -1;
}

API int unregister_usb_device_change_handler(void)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(handlers) ; i++) {
		if (strcmp(handlers[i].name, SIGNAL_NAME_USB_DEVICE_CHANGED))
			continue;
		if (handlers[i].handler == NULL)
			continue;

		e_dbus_signal_handler_del(conn, handlers[i].handler);
		handlers[i].handler = NULL;
		handlers[i].action.storage = NULL;
		handlers[i].data = NULL;
		return 0;
	}

	return -1;
}

API int mount_usb_storage(char *path)
{
	char *param[1];

	if (!path)
		return -1;

	param[0] = path;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST, METHOD_REQUEST_STORAGE_MOUNT, "s", param);
}

API int unmount_usb_storage(char *path)
{
	char *param[1];

	if (!path)
		return -1;

	param[0] = path;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST, METHOD_REQUEST_STORAGE_UNMOUNT, "s", param);
}

API int format_usb_storage(char *path)
{
	char *param[1];

	if (!path)
		return -1;

	param[0] = path;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST, METHOD_REQUEST_STORAGE_FORMAT, "s", param);
}

API int open_usb_device(char *path, int *fd)
{
	DBusMessage *reply;
	DBusError err;
	int ret, rfd;
	dbus_bool_t result;

	if (!fd || !path)
		return -EINVAL;

	dbus_error_init(&err);

	reply = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_USBHOST,
			DEVICED_INTERFACE_USBHOST,
			METHOD_OPEN_DEVICE,
			"s", &path);

	if (!reply) {
		_E("Unable to send USB device request");
		return -1;
	}

	result = dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &ret, DBUS_TYPE_UNIX_FD, &rfd, DBUS_TYPE_INVALID);
	if (!result) {
		_E("Failed to get arguments: %s", err.message);
		return -1;
	}

	if (ret >= 0)
		*fd = rfd;

	return ret;
}
