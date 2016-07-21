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
#define STORAGE_MMC 1
#define BUF_MAX 256

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

static int get_mmc_primary_id()
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter aiter, piter;
	char *param[1];
	int type;
	char *devnode = NULL;
	bool primary;
	int ret;
	int id;

	param[0] = "mmc";
	reply = dbus_method_sync_with_reply(
			STORAGE_BUS_NAME,
			DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER,
			"GetDeviceList", "s", param);
	if (!reply) {
		_E("Failed to get mmc storage list");
		return -EPERM;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &aiter);

	while (dbus_message_iter_get_arg_type(&aiter) != DBUS_TYPE_INVALID) {
		devnode = NULL;
		dbus_message_iter_recurse(&aiter, &piter); /*type*/
		dbus_message_iter_get_basic(&piter, &type);
		dbus_message_iter_next(&piter); /* devnode */
		dbus_message_iter_get_basic(&piter, &devnode);
		dbus_message_iter_next(&piter); /* syspath */
		dbus_message_iter_next(&piter); /* fsusage */
		dbus_message_iter_next(&piter); /* fstype */
		dbus_message_iter_next(&piter); /* fsversion */
		dbus_message_iter_next(&piter); /* fsuuid */
		dbus_message_iter_next(&piter); /* readonly */
		dbus_message_iter_next(&piter); /* mountpath */
		dbus_message_iter_next(&piter); /* state */
		dbus_message_iter_next(&piter); /* primary */
		dbus_message_iter_get_basic(&piter, &primary);
		dbus_message_iter_next(&piter); /* flags */
		dbus_message_iter_get_basic(&piter, &id); /* storage id */
		dbus_message_iter_next(&aiter);

		if (type == STORAGE_MMC && primary && devnode)
			break;
	}

	if (devnode)
		ret = id;
	else
		ret = -ENODEV;

	dbus_message_unref(reply);
	return ret;
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
	int id;
	char buf_id[32];
	char *param[2];

	if (mmc_data && mmc_data->mmc_cb) {
		_I("Mount callback exists");
		mount_cb = mount_mmc_cb;
		data = mmc_data;
	}

	id = get_mmc_primary_id();
	if (id < 0)
		return id;

	snprintf(buf_id, sizeof(buf_id), "%d", id);
	param[0] = buf_id;
	param[1] = "";
	ret = dbus_method_async_with_reply(STORAGE_BUS_NAME, DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER, "Mount", "is", param,
			mount_cb, -1, data);

	_I("Mount Request %s", ret == 0 ? "Success" : "Failed");

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
	char buf_id[32];
	int ret;
	int id;
	char *param[2];

	if (option < 0 || option > 1)
		return -EINVAL;

	if (mmc_data && mmc_data->mmc_cb) {
		_I("Mount callback exists");
		unmount_cb = unmount_mmc_cb;
		data = mmc_data;
	}

	id = get_mmc_primary_id();
	if (id < 0)
		return id;

	snprintf(buf_id, sizeof(buf_id), "%d", id);
	param[0] = buf_id;
	snprintf(buf_opt, sizeof(buf_opt), "%d", option);
	param[1] = buf_opt;
	ret = dbus_method_async_with_reply(STORAGE_BUS_NAME, DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER, "Unmount", "ii", param,
			unmount_cb, -1, data);

	_I("Unmount Request %s", ret == 0 ? "Success" : "Failed");

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
	char buf_id[32];
	int ret;
	int id;
	char *param[2];

	if (option < 0 || option > 1)
		return -EINVAL;

	if (mmc_data && mmc_data->mmc_cb) {
		_I("Mount callback exists");
		format_cb = format_mmc_cb;
		data = mmc_data;
	}

	id = get_mmc_primary_id();
	if (id < 0)
		return id;

	snprintf(buf_id, sizeof(buf_id), "%d", id);
	param[0] = buf_id;
	snprintf(buf_opt, sizeof(buf_opt), "%d", option);
	param[1] = buf_opt;
	ret = dbus_method_async_with_reply(STORAGE_BUS_NAME, DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER, "Format", "ii", param,
			format_cb, FORMAT_TIMEOUT, data);

	_I("Format Request %s", ret == 0 ? "Success" : "Failed");

	return ret;
}
