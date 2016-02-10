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
#define _GNU_SOURCE

#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <tzplatform_config.h>

#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"
#include "core/udev.h"
#include "core/list.h"
#include "core/device-idler.h"

#define USB_INTERFACE_CLASS     "bInterfaceClass"
#define USB_INTERFACE_SUBCLASS  "bInterfaceSubClass"
#define USB_INTERFACE_PROTOCOL  "bInterfaceProtocol"
#define USB_VENDOR_ID           "idVendor"
#define USB_PRODUCT_ID          "idProduct"
#define USB_MANUFACTURER        "manufacturer"
#define USB_PRODUCT             "product"
#define USB_SERIAL              "serial"

#define SIGNAL_USB_HOST_CHANGED "ChangedDevice"
#define METHOD_GET_CONNECTION_CREDENTIALS "GetConnectionCredentials"

#define ROOTPATH tzplatform_getenv(TZ_SYS_VAR)
#define POLICY_FILENAME "usbhost-policy"

char *POLICY_FILEPATH;

/**
 * Below usb host class is defined by www.usb.org.
 * Please refer to below site.
 * http://www.usb.org/developers/defined_class
 * You can find the detail class codes in linux/usb/ch9.h.
 * Deviced uses kernel defines.
 */
#include <linux/usb/ch9.h>
#define USB_CLASS_ALL   0xffffffff
#define USB_DEVICE_MAJOR 189

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
	/* devpath is always valid */
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
	arr[7] = (!usbhost->manufacturer ? "" : usbhost->manufacturer);
	arr[8] = (!usbhost->product ? "" : usbhost->product);
	arr[9] = (!usbhost->serial ? "" : usbhost->serial);

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
		_E("fail to allocate usbhost memory : %d", errno);
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

static int usbhost_init_from_udev_enumerate(void)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *list_entry;
	struct udev_device *dev;
	const char *syspath;
	const char *devpath;

	udev = udev_new();
	if (!udev) {
		_E("fail to create udev library context");
		return -EPERM;
	}

	/* create a list of the devices in the 'usb' subsystem */
	enumerate = udev_enumerate_new(udev);
	if (!enumerate) {
		_E("fail to create an enumeration context");
		return -EPERM;
	}

	udev_enumerate_add_match_subsystem(enumerate, USB_SUBSYSTEM);
	udev_enumerate_add_match_property(enumerate,
			UDEV_DEVTYPE, USB_INTERFACE_DEVTYPE);
	udev_enumerate_scan_devices(enumerate);

	udev_list_entry_foreach(list_entry,
			udev_enumerate_get_list_entry(enumerate)) {
		syspath = udev_list_entry_get_name(list_entry);
		if (!syspath)
			continue;

		dev = udev_device_new_from_syspath(udev_enumerate_get_udev(enumerate),
				syspath);
		if (!dev)
			continue;

		/* devpath is an unique information among usb host devices */
		devpath = udev_device_get_devpath(dev);
		if (!devpath) {
			_E("fail to get devpath from %s device", syspath);
			continue;
		}

		/* add usbhost list */
		add_usbhost_list(dev, devpath);

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return 0;
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
		str = (!usbhost->manufacturer ? "" : usbhost->manufacturer);
		dbus_message_iter_append_basic(&s, DBUS_TYPE_STRING, &str);
		str = (!usbhost->product ? "" : usbhost->product);
		dbus_message_iter_append_basic(&s, DBUS_TYPE_STRING, &str);
		str = (!usbhost->serial ? "" : usbhost->serial);
		dbus_message_iter_append_basic(&s, DBUS_TYPE_STRING, &str);
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

enum policy_value {
	POLICY_ALLOW,
	POLICY_DENY,
};

#define UID_KEY "UnixUserID"
#define SEC_LABEL_KEY "LinuxSecurityLabel"
#define ENTRY_LINE_SIZE 256

struct user_credentials {
	uint32_t uid;
	char *sec_label;
};

struct policy_entry {
	struct user_credentials creds;
	struct {
		uint16_t bcdUSB;
		uint8_t bDeviceClass;
		uint8_t bDeviceSubClass;
		uint8_t bDeviceProtocol;
		uint16_t idVendor;
		uint16_t idProduct;
		uint16_t bcdDevice;
	} device;
	enum policy_value value;
};

dd_list *access_list;

static const char *policy_value_str(enum policy_value value) {
	switch (value) {
	case POLICY_ALLOW:
		return "ALLOW";
	case POLICY_DENY:
		return "DENY";
	default:
		return "UNKNOWN";
	}
}

static int get_policy_value_from_str(const char *str) {
	if (strncmp("ALLOW", str, 5) == 0)
		return POLICY_ALLOW;
	if (strncmp("DENY", str, 4) == 0)
		return POLICY_DENY;
	return -1;
}

static inline int marshal_policy_entry(char *buf, int len, struct policy_entry *entry) {
	return snprintf(buf, len, "%d %s %04x %02x %02x %02x %04x %04x %04x %s\n",
			entry->creds.uid,
			entry->creds.sec_label,
			entry->device.bcdUSB,
			entry->device.bDeviceClass,
			entry->device.bDeviceSubClass,
			entry->device.bDeviceProtocol,
			entry->device.idVendor,
			entry->device.idProduct,
			entry->device.bcdDevice,
			policy_value_str(entry->value));
}

static DBusMessage *print_policy(E_DBus_Object *obj, DBusMessage *msg) {
	char line[ENTRY_LINE_SIZE];
	dd_list *elem;
	struct policy_entry *entry;
	int ret;

	_I("USB access policy:");
	DD_LIST_FOREACH(access_list, elem, entry) {
		ret = marshal_policy_entry(line, ENTRY_LINE_SIZE, entry);
		if (ret < 0)
			break;
		_I("\t%s", line);
	}

	return dbus_message_new_method_return(msg);
}

static int store_policy(void)
{
	int fd;
	dd_list *elem;
	struct policy_entry *entry;
	char line[256];
	int ret;

	fd = open(POLICY_FILEPATH, O_WRONLY | O_CREAT, 0664);
	if (fd < 0) {
		ret = -errno;
		_E("Could not open policy file for writing: %m");
		goto out;
	}

	DD_LIST_FOREACH(access_list, elem, entry) {
		ret = marshal_policy_entry(line, ENTRY_LINE_SIZE, entry);
		if (ret < 0) {
			_E("Serialization failed: %m");
			goto out;
		}

		ret = write(fd, line, ret);
		if (ret < 0) {
			ret = -errno;
			_E("Error writing policy entry: %m");
			goto out;
		}
	}

	_I("Policy stored in %s", POLICY_FILEPATH);

	ret = 0;

out:
	close(fd);
	return ret;
}

static int read_policy(void)
{
	FILE *fp;
	struct policy_entry *entry;
	char *line = NULL, value_str[256];
	int ret = -1;
	int count = 0;
	size_t len;

	fp = fopen(POLICY_FILEPATH, "r");
	if (!fp) {
		ret = -errno;
		_E("Could not open policy file for reading: %m");
		return ret;
	}

	while ((ret = getline(&line, &len, fp)) != -1) {
		entry = malloc(sizeof(*entry));
		if (!entry) {
			ret = -ENOMEM;
			_E("No memory: %m");
			goto out;
		}

		entry->creds.sec_label = calloc(ENTRY_LINE_SIZE, 1);
		if (!entry->creds.sec_label) {
			_E("No memory: %m");
			goto out;
		}

		ret = sscanf(line, "%d %s %04hx %02hhx %02hhx %02hhx %04hx %04hx %04hx %s\n",
				&entry->creds.uid,
				entry->creds.sec_label,
				&entry->device.bcdUSB,
				&entry->device.bDeviceClass,
				&entry->device.bDeviceSubClass,
				&entry->device.bDeviceProtocol,
				&entry->device.idVendor,
				&entry->device.idProduct,
				&entry->device.bcdDevice,
				value_str);
		if (ret == EOF) {
			_E("Error reading line: %m");
			free(entry);
			free(entry->creds.sec_label);
			goto out;
		}

		entry->value = get_policy_value_from_str(value_str);
		if (entry->value < 0) {
			_E("Invalid policy value: %s", value_str);
			ret = -EINVAL;
			goto out;
		}

		_I("%04x:%04x : %s", entry->device.idVendor, entry->device.idProduct,
				value_str);

		DD_LIST_APPEND(access_list, entry);
		count++;
	}

	_I("Found %d policy entries", count);
	ret = 0;

out:
	fclose(fp);
	free(line);

	return ret;
}

static int get_device_desc(const char *filepath, struct usb_device_descriptor *desc)
{
	char *path = NULL;
	struct stat st;
	int ret;
	int fd = -1;

	ret = stat(filepath, &st);
	if (ret < 0) {
		ret = -errno;
		_E("Could not stat %s: %m", filepath);
		goto out;
	}

	if (!S_ISCHR(st.st_mode) ||
	    major(st.st_rdev) != USB_DEVICE_MAJOR) {
		ret = -EINVAL;
		_E("Not an USB device");
		goto out;
	}

	ret = asprintf(&path, "/sys/dev/char/%d:%d/descriptors", major(st.st_rdev), minor(st.st_rdev));
	if (ret < 0) {
		ret = -ENOMEM;
		_E("asprintf failed");
		goto out;
	}

	_I("Opening descriptor at %s", path);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ret = -errno;
		_E("Failed to open %s: %m", path);
		goto out;
	}

	ret = read(fd, desc, sizeof(*desc));
	if (ret < 0) {
		ret = -errno;
		_E("Failed to read %s: %m", path);
		goto out;
	}

	ret = 0;

out:
	if (fd >= 0)
		close(fd);
	free(path);

	return ret;
}

static void store_idler_cb(void *data)
{
	store_policy();
}

static int get_policy_value(const char *path, struct user_credentials *cred)
{
	struct usb_device_descriptor desc;
	int ret;
	dd_list *elem;
	struct policy_entry *entry;

	memset(&desc, 0, sizeof(desc));

	_I("Requested access from user %d to %s", cred->uid, path);
	ret = get_device_desc(path, &desc);
	if (ret < 0) {
		_E("Could not get device descriptor");
		return ret;
	}

	DD_LIST_FOREACH(access_list, elem, entry) {
		if (entry->creds.uid != cred->uid
		 || strncmp(entry->creds.sec_label, cred->sec_label, strlen(cred->sec_label)) != 0
		 || entry->device.bcdUSB != le16toh(desc.bcdUSB)
		 || entry->device.bDeviceClass != desc.bDeviceClass
		 || entry->device.bDeviceSubClass != desc.bDeviceSubClass
		 || entry->device.bDeviceProtocol != desc.bDeviceProtocol
		 || entry->device.idVendor != le16toh(desc.idVendor)
		 || entry->device.idProduct != le16toh(desc.idProduct)
		 || entry->device.bcdDevice != le16toh(desc.bcdDevice))
			continue;

		_I("Found matching policy entry: %s", policy_value_str(entry->value));

		return entry->value;
	}

	/* TODO ask user for policy */

	/* Allow always */
	entry = calloc(sizeof(*entry), 1);
	if (!entry) {
		_E("No memory");
		return -ENOMEM;
	}

	entry->creds.uid = cred->uid;
	entry->creds.sec_label = calloc(strlen(cred->sec_label)+1, 1);
	if (!entry->creds.sec_label) {
		_E("No memory");
		return -ENOMEM;
	}

	strncpy(entry->creds.sec_label, cred->sec_label, strlen(cred->sec_label));
	entry->device.bcdUSB = le16toh(desc.bcdUSB);
	entry->device.bDeviceClass = desc.bDeviceClass;
	entry->device.bDeviceSubClass = desc.bDeviceSubClass;
	entry->device.bDeviceProtocol = desc.bDeviceProtocol;
	entry->device.idVendor = le16toh(desc.idVendor);
	entry->device.idProduct = le16toh(desc.idProduct);
	entry->device.bcdDevice = le16toh(desc.bcdDevice);
	entry->value = POLICY_ALLOW;

	_I("Added policy entry: %d %s %04x %02x %02x %02x %04x %04x %04x %s",
		entry->creds.uid,
		entry->creds.sec_label,
		entry->device.bcdUSB,
		entry->device.bDeviceClass,
		entry->device.bDeviceSubClass,
		entry->device.bDeviceProtocol,
		entry->device.idVendor,
		entry->device.idProduct,
		entry->device.bcdDevice,
		policy_value_str(entry->value));
	DD_LIST_APPEND(access_list, entry);

	ret = add_idle_request(store_idler_cb, NULL);
	if (ret < 0) {
		_E("fail to add store idle request : %d", ret);
		return ret;
	}

	return entry->value;
}

static int creds_read_uid(DBusMessageIter *iter, uint32_t *dest)
{
	int type;
	DBusMessageIter sub;

	dbus_message_iter_next(iter);

	dbus_message_iter_recurse(iter, &sub);
	type = dbus_message_iter_get_arg_type(&sub);
	if (type != DBUS_TYPE_UINT32) {
		_E("expected uint32");
		return -EINVAL;
	}

	dbus_message_iter_get_basic(&sub, dest);

	return 0;
}

static int creds_read_label(DBusMessageIter *iter, char **dest)
{
	int type;
	DBusMessageIter sub, byte;
	int n;

	dbus_message_iter_next(iter);

	dbus_message_iter_recurse(iter, &sub);
	type = dbus_message_iter_get_arg_type(&sub);
	if (type != DBUS_TYPE_ARRAY) {
		_E("expected array of bytes");
		return -EINVAL;
	}

	dbus_message_iter_recurse(&sub, &byte);
	dbus_message_iter_get_fixed_array(&byte, dest, &n);

	return n;
}

static int get_caller_credentials(DBusMessage *msg, struct user_credentials *cred)
{
	const char *cid;
	char *key;
	char *arr[1];
	DBusMessage *reply;
	DBusMessageIter iter, dict, entry;
	dbus_bool_t ret;
	int type;
	int reti;

	cid = dbus_message_get_sender(msg);
	if (!cid) {
		_E("Unable to identify client");
		return -1;
	}

	arr[0] = (char *)cid;
	reply = dbus_method_sync_with_reply(DBUS_BUS_NAME,
			DBUS_OBJECT_PATH,
			DBUS_INTERFACE_NAME,
			METHOD_GET_CONNECTION_CREDENTIALS,
			"s", arr);

	if (!reply) {
		_E("Cannot get connection credentials for %s", cid);
		return -1;
	}

	ret = dbus_message_iter_init(reply, &iter);
	if (!ret) {
		_E("could not init msg iterator");
		reti = -1;
		goto out;
	}

	type = dbus_message_iter_get_arg_type(&iter);
	if (type != DBUS_TYPE_ARRAY) {
		_E("Expected array (%s)", cid);
		reti = -EINVAL;
		goto out;
	}

	dbus_message_iter_recurse(&iter, &dict);

	while ((type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
		if (type != DBUS_TYPE_DICT_ENTRY) {
			_E("Expected dict entry (%s)", cid);
			reti = -EINVAL;
			goto out;
		}

		dbus_message_iter_recurse(&dict, &entry);
		type = dbus_message_iter_get_arg_type(&entry);
		if (type != DBUS_TYPE_STRING) {
			_E("Expected string (%s)", cid);
			reti = -EINVAL;
			goto out;
		}

		dbus_message_iter_get_basic(&entry, &key);
		if (strncmp(key, UID_KEY, sizeof(UID_KEY)) == 0) {
			reti = creds_read_uid(&entry, &cred->uid);
			if (reti < 0)
				goto out;
		} else if (strncmp(key, SEC_LABEL_KEY, sizeof(SEC_LABEL_KEY)) == 0) {
			reti = creds_read_label(&entry, &cred->sec_label);
			if (reti < 0)
				goto out;
		}

		dbus_message_iter_next(&dict);
	}

	reti = 0;

out:
	dbus_message_unref(reply);
	return reti;
}

static void remove_all_access_list(void)
{
	struct policy_entry *entry;
	dd_list *n, *next;

	DD_LIST_FOREACH_SAFE(access_list, n, next, entry) {
		DD_LIST_REMOVE(access_list, entry);
		free(entry->creds.sec_label);
		free(entry);
	}
}

static DBusMessage *open_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	dbus_bool_t dbus_ret;
	int ret = 0, fd = -1;
	char *path;
	struct user_credentials cred;

	dbus_error_init(&err);

	reply = dbus_message_new_method_return(msg);
	if (!reply) {
		_E("Cannot allocate memory for message");
		return reply;
	}

	ret = get_caller_credentials(msg, &cred);
	if (ret < 0) {
		_E("Unable to get credentials for caller : %s", strerror(-ret));
		goto out;
	}

	dbus_ret = dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID);
	if (!dbus_ret) {
		_E("Unable to get arguments from message: %s", err.message);
		ret = -EINVAL;
		goto out;
	}

	ret = get_policy_value(path, &cred);
	switch (ret) {
	case POLICY_ALLOW:
		fd = open(path, O_RDWR);
		if (fd < 0) {
			ret = -errno;
			_E("Unable to open file (%s): %m", path);
		} else
			ret = 0;
		break;
	case POLICY_DENY:
		ret = -EACCES;
		break;
	default:
		/* ret has error code */
		break;
	}

out:
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UNIX_FD, &fd);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "PrintDeviceList",   NULL,           NULL, print_device_list }, /* for debugging */
	{ "PrintPolicy",       NULL,           NULL, print_policy }, /* for debugging */
	{ "GetDeviceList",      "i", "a(siiiiisss)", get_device_list },
	{ "GetDeviceListCount", "i",            "i", get_device_list_count },
	{ "OpenDevice",         "s",           "ih", open_device },
};

static int booting_done(void *data)
{
	/**
	 * To search the attched usb host device is not an argent task.
	 * So deviced does not load while booting time.
	 * After booting task is done, it tries to find the attached devices.
	 */
	usbhost_init_from_udev_enumerate();

	/* unregister booting done notifier */
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);

	return 0;
}

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

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);

	ret = asprintf(&POLICY_FILEPATH, "%s/%s", ROOTPATH, POLICY_FILENAME);
	if (ret < 0) {
		_E("no memory for policy path");
		return;
	}

	read_policy();
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

	store_policy();
	remove_all_access_list();

	free(POLICY_FILEPATH);
}

static const struct device_ops usbhost_device_ops = {
	.name	= "usbhost",
	.init	= usbhost_init,
	.exit	= usbhost_exit,
};

DEVICE_OPS_REGISTER(&usbhost_device_ops)
