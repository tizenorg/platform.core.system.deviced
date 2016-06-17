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
#include <vconf.h>
#include <errno.h>
#include <stdbool.h>
#include <storage.h>
#include <glib.h>

#include "log.h"
#include "dbus.h"
#include "common.h"
#include "dd-mmc.h"

#define METHOD_REQUEST_SECURE_MOUNT		"RequestSecureMount"
#define METHOD_REQUEST_SECURE_UNMOUNT	"RequestSecureUnmount"

#define ODE_MOUNT_STATE 1

#define FORMAT_TIMEOUT	(120*1000)

API int mmc_secure_mount(const char *mount_point)
{
	if (mount_point == NULL) {
		return -EINVAL;
	}

	char *arr[1];
	arr[0] = (char *)mount_point;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC, METHOD_REQUEST_SECURE_MOUNT, "s", arr);
}

API int mmc_secure_unmount(const char *mount_point)
{
	if (mount_point == NULL) {
		return -EINVAL;
	}

	char *arr[1];
	arr[0] = (char *)mount_point;
	return dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC, METHOD_REQUEST_SECURE_UNMOUNT, "s", arr);
}

static bool storage_list_cb(int storage_id, storage_type_e type,
		storage_state_e state, const char *path, void *user_data)
{
	int *id = user_data;

	if (type != STORAGE_TYPE_EXTERNAL)
		return true;
	if (!strstr(path, "SDCard"))
		return true;

	*id = storage_id;
	return false; /* Stop the iteration */
}

static int get_object_path(int id, char **objectpath)
{
	char *param[1];
	char storage_id[8];
	char object[128];
	char *node;
	int ret;
	DBusMessage *msg;
	DBusError err;
	int type;
	char *devnode, syspath, *fsusage, *fstype, *fsversion, *fsuuid;
	int readonly;
	char *mountpath;
	int state;
	bool primary;
	int flags, sid;

	if (!mountpath)
		return -EINVAL;

	snprintf(storage_id, sizeof(storage_id), "%d", id);
	param[0] = storage_id;

	msg = dbus_method_sync_with_reply(STORAGE_BUS_NAME,
			DEVICED_PATH_BLOCK_MANAGER, DEVICED_INTERFACE_BLOCK_MANAGER,
			"GetDeviceInfoByID", "i", param);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err,
			DBUS_TYPE_INT32, &type,
			DBUS_TYPE_STRING, &devnode,
			DBUS_TYPE_STRING, &syspath,
			DBUS_TYPE_STRING, &fsusage,
			DBUS_TYPE_STRING, &fstype,
			DBUS_TYPE_STRING, &fsversion,
			DBUS_TYPE_STRING, &fsuuid,
			DBUS_TYPE_INT32, &readonly,
			DBUS_TYPE_STRING, &mountpath,
			DBUS_TYPE_INT32, &state,
			DBUS_TYPE_BOOLEAN, &primary,
			DBUS_TYPE_INT32, &flags,
			DBUS_TYPE_INT32, &sid,
			DBUS_TYPE_INVALID);
	dbus_message_unref(msg);
	dbus_error_free(&err);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		return -EBADMSG;
	}

	node = strstr(devnode, "mmcblk");
	if (!node)
		return -EINVAL;

	snprintf(object, sizeof(object), "%s/%s",
			DEVICED_PATH_BLOCK_DEVICES, node);

	*objectpath = strdup(object);
	return 0;
}

static void mount_mmc_cb(void *data, DBusMessage *msg, DBusError *err)
{
	struct mmc_contents *mmc_data = (struct mmc_contents*)data;
	DBusError r_err;
	int r, mmc_ret;

	_D("mount_mmc_cb called");

	if (!msg) {
		_E("no message [%s:%s]", err->name, err->message);
		mmc_ret = -EBADMSG;
		goto exit;
	}

	dbus_error_init(&r_err);
	r = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &mmc_ret, DBUS_TYPE_INVALID);
	if (!r) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		mmc_ret = -EBADMSG;
		goto exit;
	}

	_I("Mount State : %d", mmc_ret);

exit:
	(mmc_data->mmc_cb)(mmc_ret, mmc_data->user_data);
}

API int deviced_request_mount_mmc(struct mmc_contents *mmc_data)
{
	void (*mount_cb)(void*, DBusMessage*, DBusError*) = NULL;
	void *data = NULL;
	int ret;
	int id = -1;
	char *objectpath = NULL;
	char *param[1];

	ret = storage_foreach_device_supported(storage_list_cb, &id);
	if (ret != STORAGE_ERROR_NONE) {
		_E("Failed to get mmc information (%d)", ret);
		ret = -ENODEV;
		goto out;
	}
	if (id < 0) {
		_E("There is no mmc device connected");
		ret = -ENODEV;
		goto out;
	}

	ret = get_object_path(id, &objectpath);
	if (ret < 0) {
		_E("Failed to get object path");
		goto out;
	}
	if (!objectpath) {
		_E("Failed to get object path");
		ret = -ENODEV;
		goto out;
	}

	if (mmc_data && mmc_data->mmc_cb) {
		_I("Mount callback exists");
		mount_cb = mount_mmc_cb;
		data = mmc_data;
	}

	param[0] = "";
	ret = dbus_method_async_with_reply(STORAGE_BUS_NAME, objectpath,
			DEVICED_INTERFACE_BLOCK, "Mount", "s", param,
			mount_cb, -1, data);

	_I("Mount Request Success");

out:
	free(objectpath);

	return ret;
}

static void unmount_mmc_cb(void *data, DBusMessage *msg, DBusError *err)
{
	struct mmc_contents *mmc_data = (struct mmc_contents*)data;
	DBusError r_err;
	int r, mmc_ret;

	_D("unmount_mmc_cb called");

	if (!msg) {
		_E("no message [%s:%s]", err->name, err->message);
		mmc_ret = -EBADMSG;
		goto exit;
	}

	dbus_error_init(&r_err);
	r = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &mmc_ret, DBUS_TYPE_INVALID);
	if (!r) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		mmc_ret = -EBADMSG;
		goto exit;
	}

	_I("Unmount State : %d", mmc_ret);

exit:
	(mmc_data->mmc_cb)(mmc_ret, mmc_data->user_data);
}

API int deviced_request_unmount_mmc(struct mmc_contents *mmc_data, int option)
{
	void (*unmount_cb)(void*, DBusMessage*, DBusError*) = NULL;
	void *data = NULL;
	char buf_opt[32];
	int ret;
	char *objectpath = NULL;
	char *param[1];
	int id;

	if (option < 0 || option > 1)
		return -EINVAL;

	ret = storage_foreach_device_supported(storage_list_cb, &id);
	if (ret != STORAGE_ERROR_NONE) {
		_E("Failed to get mmc information (%d)", ret);
		ret = -ENODEV;
		goto out;
	}
	if (id < 0) {
		_E("There is no mmc device connected");
		ret = -ENODEV;
		goto out;
	}

	ret = get_object_path(id, &objectpath);
	if (ret < 0) {
		_E("Failed to get object path");
		goto out;
	}
	if (!objectpath) {
		_E("Failed to get object path");
		ret = -ENODEV;
		goto out;
	}

	if (mmc_data && mmc_data->mmc_cb) {
		_I("Mount callback exists");
		unmount_cb = unmount_mmc_cb;
		data = mmc_data;
	}

	snprintf(buf_opt, sizeof(buf_opt), "%d", option);
	param[0] = buf_opt;
	ret = dbus_method_async_with_reply(STORAGE_BUS_NAME, objectpath,
			DEVICED_INTERFACE_BLOCK, "Unmount", "i", param,
			unmount_cb, -1, data);

	_I("Unmount Request Success");

out:
	free(objectpath);

	return ret;
}

static void format_mmc_cb(void *data, DBusMessage *msg, DBusError *err)
{
	struct mmc_contents *mmc_data = (struct mmc_contents*)data;
	DBusError r_err;
	int r, mmc_ret;

	_D("format_mmc_cb called");

	if (!msg) {
		_E("no message [%s:%s]", err->name, err->message);
		mmc_ret = -EBADMSG;
		goto exit;
	}

	dbus_error_init(&r_err);
	r = dbus_message_get_args(msg, &r_err, DBUS_TYPE_INT32, &mmc_ret, DBUS_TYPE_INVALID);
	if (!r) {
		_E("no message [%s:%s]", r_err.name, r_err.message);
		dbus_error_free(&r_err);
		mmc_ret = -EBADMSG;
		goto exit;
	}

	_I("Format State : %d", mmc_ret);

exit:
	(mmc_data->mmc_cb)(mmc_ret, mmc_data->user_data);
}

API int deviced_request_format_mmc(struct mmc_contents *mmc_data)
{
	return deviced_format_mmc(mmc_data, 1);
}

API int deviced_format_mmc(struct mmc_contents *mmc_data, int option)
{
	void (*format_cb)(void*, DBusMessage*, DBusError*) = NULL;
	void *data = NULL;
	char buf_opt[32];
	int ret;
	char *objectpath = NULL;
	char *param[1];
	int id;

	if (option < 0 || option > 1)
		return -EINVAL;

	ret = storage_foreach_device_supported(storage_list_cb, &id);
	if (ret != STORAGE_ERROR_NONE) {
		_E("Failed to get mmc information (%d)", ret);
		ret = -ENODEV;
		goto out;
	}
	if (id < 0) {
		_E("There is no mmc device connected");
		ret = -ENODEV;
		goto out;
	}

	ret = get_object_path(id, &objectpath);
	if (ret < 0) {
		_E("Failed to get object path");
		goto out;
	}
	if (!objectpath) {
		_E("Failed to get object path");
		ret = -ENODEV;
		goto out;
	}

	if (mmc_data && mmc_data->mmc_cb) {
		_I("Mount callback exists");
		format_cb = format_mmc_cb;
		data = mmc_data;
	}

	snprintf(buf_opt, sizeof(buf_opt), "%d", option);
	param[0] = buf_opt;
	ret = dbus_method_async_with_reply(STORAGE_BUS_NAME, objectpath,
			DEVICED_INTERFACE_BLOCK, "Format", "i", param,
			format_cb, -1, data);

	_I("Format Request Success");
out:
	free(objectpath);

	return ret;
}
