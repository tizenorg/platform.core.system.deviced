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
#include <dlfcn.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "haptic-module.h"
#include "haptic-plugin-intf.h"

#define HAPTIC_MODULE_PATH			"/usr/lib/libhaptic-module.so"

/* Haptic Plugin Interface */
static void *dlopen_handle;
static const struct haptic_ops *plugin_intf;

static DBusMessage *edbus_get_count(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;

	if (!plugin_intf || !plugin_intf->get_device_count) {
		ret = -EFAULT;
		goto exit;
	}

	ret = plugin_intf->get_device_count(&val);
	if (ret < 0)
		_E("fail to get device count : %d", ret);
	else
		ret = val;

	_D("get haptic count %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_open_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	int index, handle, ret;

	if (!plugin_intf || !plugin_intf->open_device) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &index, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->open_device(index, &handle);
	if (ret < 0)
		_E("fail to open device : %d", ret);
	else
		ret = handle;

	_D("haptic open %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_close_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned int handle;
	int ret;

	if (!plugin_intf || !plugin_intf->close_device) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->close_device(handle);
	if (ret < 0)
		_E("fail to close device : %d", ret);

	_D("haptic close %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_stop_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned int handle;
	int ret;

	if (!plugin_intf || !plugin_intf->stop_device) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->stop_device(handle);
	if (ret < 0)
		_E("fail to stop device : %d", ret);

	_D("haptic close %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_vibrate_monotone(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned int handle;
	int duration, feedback, priority, e_handle, ret;

	if (!plugin_intf || !plugin_intf->vibrate_monotone) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_INT32, &duration,
				DBUS_TYPE_INT32, &feedback,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->vibrate_monotone(handle, duration, feedback, priority, &e_handle);
	if (ret < 0)
		_E("fail to vibrate monotone : %d", ret);
	else
		ret = e_handle;

	_D("haptic vibrate monotone %d(h:%d, d:%d, f:%d, p:%d)", ret, handle, duration, feedback, priority);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;

}

static DBusMessage *edbus_vibrate_buffer(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned int handle;
	unsigned char *data;
	int size, iteration, feedback, priority, e_handle, ret;

	if (!plugin_intf || !plugin_intf->vibrate_buffer) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_INT32, &iteration,
				DBUS_TYPE_INT32, &feedback,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->vibrate_buffer(handle, data, iteration, feedback, priority, &e_handle);
	if (ret < 0)
		_E("fail to vibrate buffer : %d", ret);
	else
		ret = e_handle;

	_D("haptic vibrate buffer %d(h:%d, s:%d, i:%d, f:%d, p:%d)",
			ret, handle, size, iteration, feedback, priority);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned int handle;
	int state, ret;

	if (!plugin_intf || !plugin_intf->get_device_state) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->get_device_state(handle, &state);
	if (ret < 0)
		_E("fail to get device state : %d", ret);
	else
		ret = state;

	_D("get haptic state %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_create_effect(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	haptic_module_effect_element *elem_arr;
	unsigned char *data;
	unsigned char *elem;
	unsigned char *p;
	int i, size, cnt, ret;

	if (!plugin_intf || !plugin_intf->create_effect) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &data,
				DBUS_TYPE_INT32, &size,
				DBUS_TYPE_STRING, &elem,
				DBUS_TYPE_INT32, &cnt, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	elem_arr = (haptic_module_effect_element*)malloc(sizeof(haptic_module_effect_element)*cnt);
	for (p = elem_arr, i = 0; i < cnt; i++, p+=9) {
		sscanf(p, "%6d%3d", &elem_arr[i].haptic_duration, &elem_arr[i].haptic_level);
		_D("[%2d] duration : %d, level : %d", i, elem_arr[i].haptic_duration, elem_arr[i].haptic_level);
	}

	ret = plugin_intf->create_effect(data, size, elem_arr, cnt);
	if (ret < 0)
		_E("fail to create haptic effect : %d", ret);

	_D("create haptic effect %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_duration(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned int handle;
	unsigned char *data;
	int duration, ret;

	if (!plugin_intf || !plugin_intf->get_buffer_duration) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_STRING, &data, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = plugin_intf->get_buffer_duration(handle, data, &duration);
	if (ret < 0)
		_E("fail to get buffer duration : %d", ret);
	else
		ret = duration;

	_D("haptic get buffer duration %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_save_binary(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	unsigned char *data;
	unsigned char *path;
	int size, ret;

	if (!plugin_intf || !plugin_intf->convert_binary) {
		ret = -EFAULT;
		goto exit;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &data,
				DBUS_TYPE_INT32, &size,
				DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	_D("file path : %s", path);
	ret = plugin_intf->convert_binary(data, size, path);
	if (ret < 0)
		_E("fail to save binary data : %d", ret);

	_D("haptic save binary data %d", ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static int load_module(void)
{
	struct stat buf;
	const struct haptic_ops *(*get_haptic_plugin_interface) () = NULL;

	if (stat(HAPTIC_MODULE_PATH, &buf)) {
		_E("file(%s) is not presents", HAPTIC_MODULE_PATH);
		goto error;
	}

	dlopen_handle = dlopen(HAPTIC_MODULE_PATH, RTLD_NOW);
	if (!dlopen_handle) {
		_E("dlopen failed: %s", dlerror());
		goto error;
	}

	get_haptic_plugin_interface = dlsym(dlopen_handle, "get_haptic_plugin_interface");
	if (!get_haptic_plugin_interface) {
		_E("dlsym failed : %s", dlerror());
		goto error;
	}

	plugin_intf = get_haptic_plugin_interface();
	if (!plugin_intf) {
		_E("get_haptic_plugin_interface() failed");
		goto error;
	}

	_D("This device can vibe");
	return 0;

error:
	if (dlopen_handle) {
		dlclose(dlopen_handle);
		dlopen_handle = NULL;
	}

	_D("This device can not vibe");
	return -EPERM;
}

static void release_module(void)
{
	if (dlopen_handle) {
		dlclose(dlopen_handle);
		dlopen_handle = NULL;
	}

	_D("haptic module is released");
}

static struct edbus_method {
	const char *member;
	const char *signature;
	const char *reply_signature;
	E_DBus_Method_Cb func;
} edbus_methods[] = {
	{ "GetCount",          NULL,   "i", edbus_get_count },
	{ "OpenDevice",         "i",   "i", edbus_open_device },
	{ "CloseDevice",        "u",   "i", edbus_close_device },
	{ "StopDevice",         "u",   "i", edbus_stop_device },
	{ "VibrateMonotone", "uiii",   "i", edbus_vibrate_monotone },
	{ "VibrateBuffer", "uayiii",   "i", edbus_vibrate_buffer },
	{ "GetState",           "u",   "i", edbus_get_state },
	{ "GetDuration",       "us",   "i", edbus_get_duration },
	{ "CreateEffect",    "sisi",   "i", edbus_create_effect },
	{ "SaveBinary",       "sis",   "i", edbus_save_binary },
	/* Add methods here */
};

static int haptic_dbus_init(void)
{
	E_DBus_Interface *iface;
	int ret, i;

	iface = get_edbus_interface(DEVICED_PATH_HAPTIC);

	_D("%s, %x", DEVICED_PATH_HAPTIC, iface);

	if (!iface) {
		_E("fail to get edbus interface!");
		return -EPERM;
	}

	for (i = 0; i < ARRAY_SIZE(edbus_methods); i++) {
		ret = e_dbus_interface_method_add(iface,
				    edbus_methods[i].member,
				    edbus_methods[i].signature,
				    edbus_methods[i].reply_signature,
				    edbus_methods[i].func);
		if (!ret) {
			_E("fail to add method %s!", edbus_methods[i].member);
			return -EPERM;
		}
	}

	return 0;
}

static void haptic_init(void *data)
{
	int r;

	/* init dbus interface */
	haptic_dbus_init();

	/* load haptic plugin module */
	r = load_module();
	if (r < 0)
		_E("fail to load haptic plugin module");
}

static void haptic_exit(void *data)
{
	/* release haptic plugin module */
	release_module();
}

const struct device_ops haptic_device_ops = {
	.init = haptic_init,
	.exit = haptic_exit,
};
