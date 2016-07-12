/*
 * deviced
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include <libkmod.h>
#include <usbg/usbg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>

#include "core/log.h"
#include "core/config-parser.h"
#include "core/device-idler.h"
#include "core/device-notifier.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "shared/deviced-systemd.h"

#define FFS_PATH "/tmp/usb-host-test-ffs"
#define GADGET_SCHEME_PATH "/etc/deviced/usb-host-test/test_gadget.gs"
#define FFS_INSTANCE_NAME "usb-host-test"
#define GADGET_NAME "g1"
#define SYSTEMD_UNIT_NAME "usb-host-test.socket"
#define SYSTEMD_SERVICE_NAME "usb-host-ffs-test-daemon.service"
#define UDC_NAME "dummy_udc.0"

static int load_module(const char *name)
{
	struct kmod_ctx *ctx;
	struct kmod_module *mod;
	const char *config = NULL;
	struct kmod_list *l, *list = NULL;
	int ret = 0;
	int i;

	ctx = kmod_new(NULL, &config);
	if (!ctx)
		return -1;

	ret = kmod_module_new_from_lookup(ctx, name, &list);
	if (ret < 0) {
		_E("Module %s not found", name);
		goto out;
	}

	kmod_list_foreach(l, list) {
		mod = kmod_module_get_module(l);
		if (!mod) {
			_E("Module %s load error", name);
			ret = -1;
			goto out;
		}

		ret = kmod_module_get_initstate(mod);
		if (ret >= 0)
			goto out; /* already loaded */

		ret = kmod_module_insert_module(mod, 0, NULL);
		if (ret < 0) {
			_E("Module %s insert error", name);
			goto out;
		}
	}

	_I("Module %s loaded\n", name);
	ret = 0;

out:
	kmod_module_unref_list(list);
	kmod_unref(ctx);
	return ret;
}

static int load_gadget()
{
	usbg_state *s;
	int ret = 0;
	FILE *fp;

	ret = usbg_init("/sys/kernel/config", &s);
	if (ret < 0) {
		_E("could not init libusbg");
		return ret;
	}

	fp = fopen(GADGET_SCHEME_PATH, "r");
	if (!fp) {
		_E("could not open gadget scheme");
		ret = -1;
		goto out;
	}

	ret = usbg_import_gadget(s, fp, GADGET_NAME, NULL);
	if (ret < 0) {
		_E("could not import gadget");
		goto out;
	}

out:
	usbg_cleanup(s);
	return ret;
}

int enable_gadget()
{
	int ret;
	usbg_gadget *g;
	usbg_udc *udc;
	usbg_state *s;

	ret = usbg_init("/sys/kernel/config", &s);
	if (ret < 0)
		return ret;

	g = usbg_get_gadget(s, GADGET_NAME);
	if (!g) {
		_E("could not find gadget");
		ret = -1;
		goto out;
	}

	udc = usbg_get_udc(s, UDC_NAME);
	if (!udc) {
		_E("could not find udc");
		ret = -1;
		goto out;
	}

	ret = usbg_enable_gadget(g, udc);

out:
	usbg_cleanup(s);
	return ret;
}

static void service_started_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	uint32_t id;
	const char *path, *unit, *result;
	int ret;
	pid_t pid;

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &id,
				DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_STRING, &unit,
				DBUS_TYPE_STRING, &result,
				DBUS_TYPE_INVALID)) {
		_E("%s", err.message);
		return;
	}

	if (strcmp(unit, SYSTEMD_UNIT_NAME) == 0) {
		ret = enable_gadget();
		if (ret < 0) {
			_E("Could not enable gadget");
			return;
		}

		unregister_edbus_signal_handler("/org/freedesktop/systemd1",
				"org.freedesktop.systemd1.Manager", "JobRemoved");

		_I("start");
	}
}

int start()
{
	struct stat st;
	int ret;

	ret = load_module("dummy_hcd");
	if (ret < 0) {
		_E("Error loading module: %d", ret);
		return ret;
	}

	ret = load_module("usb_f_fs");
	if (ret < 0) {
		_E("Error loading module: %d", ret);
		return ret;
	}

	ret = load_gadget();
	if (ret < 0) {
		_E("Error loading gadget: %d", ret);
		return ret;
	}

	/* TODO make it recusrsive? */
	if (stat(FFS_PATH, &st) < 0) {
		ret = mkdir(FFS_PATH, S_IRWXU | S_IRWXG | S_IROTH);
		if (ret < 0) {
			_E("Error creating ffs directory");
			return ret;
		}
	}

	ret = mount(FFS_INSTANCE_NAME, FFS_PATH, "functionfs", 0, NULL);
	if (ret < 0) {
		_E("Error mounting ffs");
		return ret;
	}

	ret = register_edbus_signal_handler("/org/freedesktop/systemd1",
				"org.freedesktop.systemd1.Manager", "JobRemoved",
				service_started_handler);
	if (ret < 0) {
		_E("could not register signal handler");
		return ret;
	}

	ret = deviced_systemd_start_unit(SYSTEMD_UNIT_NAME);
	if (ret < 0) {
		_E("Error starting daemon");
		return ret;
	}

	return 0;
}

static int stop()
{
	usbg_state *s;
	usbg_gadget *g;
	int ret = 0;

	ret = deviced_systemd_stop_unit(SYSTEMD_UNIT_NAME);
	if (ret < 0) {
		_E("could not stop socket unit");
		return ret;
	}

	ret = deviced_systemd_stop_unit(SYSTEMD_SERVICE_NAME);
	if (ret < 0) {
		_E("could not stop service unit");
		return ret;
	}

	ret = usbg_init("/sys/kernel/config", &s);
	if (ret < 0) {
		_E("could not init libusbg");
		return ret;
	}

	g = usbg_get_gadget(s, GADGET_NAME);
	if (!g) {
		_E("could not find gadget");
		ret = -1;
		goto out;
	}

	ret = usbg_rm_gadget(g, USBG_RM_RECURSE);
	if (ret < 0) {
		_E("could not remove gadget");
		goto out;
	}

	ret = umount(FFS_PATH);
	if (ret < 0) {
		_E("could not umount functionfs");
		goto out;
	}

	unregister_edbus_signal_handler("/org/freedesktop/systemd1",
				"org.freedesktop.systemd1.Manager", "JobRemoved");

	_I("stop");

out:
	usbg_cleanup(s);
	return ret;
}

static DBusMessage *edbus_start(E_DBus_Object *obj, DBusMessage *msg)
{
	int ret;

	ret = start();

	if (ret < 0)
		return NULL;

	return dbus_message_new_method_return(msg);
}

static DBusMessage *edbus_stop(E_DBus_Object *obj, DBusMessage *msg)
{
	int ret;

	ret = stop();

	if (ret < 0)
		return NULL;

	return dbus_message_new_method_return(msg);
}

static int usb_host_test_start(void *data)
{
	return start();
}

static int usb_host_test_stop(void *data)
{
	return stop();
}

static const struct edbus_method edbus_methods[] = {
	{ "start", NULL, NULL, edbus_start },  /* for devicectl */
	{ "stop",  NULL, NULL, edbus_stop }, /* for devicectl */
};

static void usb_host_test_init(void *data)
{
	int ret;

	ret = register_edbus_interface_and_method(DEVICED_PATH_USB_HOST_TEST,
			DEVICED_INTERFACE_USB_HOST_TEST,
			edbus_methods, ARRAY_SIZE(edbus_methods));

	if (ret < 0) {
		_E("Failed to register edbus method! %d", ret);
		return;
	}

	_I("initialized");
}

static void usb_host_test_exit(void *data)
{
	_I("exited");
}

static const struct device_ops usb_host_test_device_ops = {
	.name	= "usb-host-test",
	.init	= usb_host_test_init,
	.exit	= usb_host_test_exit,
	.start	= usb_host_test_start,
	.stop	= usb_host_test_stop,
};

DEVICE_OPS_REGISTER(&usb_host_test_device_ops)
