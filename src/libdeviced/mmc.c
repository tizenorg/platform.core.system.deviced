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

#include "log.h"
#include "dbus.h"
#include "common.h"
#include "dd-mmc.h"

#define METHOD_REQUEST_SECURE_MOUNT		"RequestSecureMount"
#define METHOD_REQUEST_SECURE_UNMOUNT	"RequestSecureUnmount"
#define METHOD_REQUEST_MOUNT		"RequestMount"
#define METHOD_REQUEST_UNMOUNT		"RequestUnmount"
#define METHOD_REQUEST_FORMAT		"RequestFormat"

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

//	if (mmc_ret == ODE_MOUNT_STATE)
//		return;

exit:
	(mmc_data->mmc_cb)(mmc_ret, mmc_data->user_data);
}

API int deviced_request_mount_mmc(struct mmc_contents *mmc_data)
{
	void (*mount_cb)(void*, DBusMessage*, DBusError*) = NULL;
	void *data = NULL;

	if (mmc_data != NULL && mmc_data->mmc_cb != NULL) {
		mount_cb = mount_mmc_cb;
		data = mmc_data;
	}

	return dbus_method_async_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			METHOD_REQUEST_MOUNT,
			NULL, NULL, mount_cb, -1, data);
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
	char *arr[1];
	char buf_opt[32];
	void (*unmount_cb)(void*, DBusMessage*, DBusError*) = NULL;
	void *data = NULL;

	if (option < 0 || option > 1)
		return -EINVAL;

	if (mmc_data != NULL && mmc_data->mmc_cb != NULL) {
		unmount_cb = unmount_mmc_cb;
		data = mmc_data;
	}

	snprintf(buf_opt, sizeof(buf_opt), "%d", option);
	arr[0] = buf_opt;
	return dbus_method_async_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			METHOD_REQUEST_UNMOUNT,
			"i", arr, unmount_cb, -1, data);
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
	char *arr[1];
	char buf_opt[32];
	void (*format_cb)(void*, DBusMessage*, DBusError*) = NULL;
	void *data = NULL;

	if (option < 0 || option > 1)
		return -EINVAL;

	if (mmc_data != NULL && mmc_data->mmc_cb != NULL) {
		format_cb = format_mmc_cb;
		data = mmc_data;
	}

	snprintf(buf_opt, sizeof(buf_opt), "%d", option);
	arr[0] = buf_opt;
	return dbus_method_async_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			METHOD_REQUEST_FORMAT,
			"i", arr, format_cb, FORMAT_TIMEOUT, data);
}
