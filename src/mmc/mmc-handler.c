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
#include <vconf.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/config-parser.h"
#include "block/block.h"
#include "config.h"

#define SMACK_LABELING_TIME (0.5)

static char *mmc_curpath;
static bool mmc_disabled = false;
static Ecore_Timer *smack_timer = NULL;

static void launch_syspopup(char *str)
{
	manage_notification("MMC", str);
}

static Eina_Bool smack_timer_cb(void *data)
{
	if (smack_timer) {
		ecore_timer_del(smack_timer);
		smack_timer = NULL;
	}
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	return EINA_FALSE;
}

static void mmc_mount_done(void)
{
	smack_timer = ecore_timer_add(SMACK_LABELING_TIME,
			smack_timer_cb, NULL);
	if (smack_timer) {
		_I("Wait to check");
		return;
	}
	_E("Fail to add abnormal check timer");
	smack_timer_cb(NULL);
}

static DBusMessage *edbus_request_mount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	ret = mount_block_device(mmc_curpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_unmount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	ret = unmount_block_device(mmc_curpath, opt);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_format(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	ret = format_block_device(mmc_curpath, opt);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_insert(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *devpath;
	int ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &devpath, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	ret = add_block_device(devpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_remove(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	ret = remove_block_device(mmc_curpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_change_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;
	char params[NAME_MAX];
	struct mmc_data *pdata;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}
	if (opt == VCONFKEY_SYSMAN_MMC_MOUNTED)
		mmc_mount_done();
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "RequestMount",         NULL, "i", edbus_request_mount },
	{ "RequestUnmount",        "i", "i", edbus_request_unmount },
	{ "RequestFormat",         "i", "i", edbus_request_format },
	{ "RequestInsert",         "s", "i", edbus_request_insert },
	{ "RequestRemove",        NULL, "i", edbus_request_remove },
	{ "ChangeStatus",          "i", "i", edbus_change_status },
};

static void mmc_init(void *data)
{
	int ret;

	mmc_load_config();

	ret = register_edbus_interface_and_method(DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static int mmc_mount(struct block_data *data, int result)
{
	if (result == -EROFS)
		launch_syspopup("mountrdonly");
	/* Do not need to show error popup, if mmc is disabled */
	else if (result == -EWOULDBLOCK)
		goto error_without_popup;
	else if (result < 0)
		goto error;

	/* save the current dev path */
	mmc_curpath = strdup(data->devpath);

	mmc_set_config(MAX_RATIO, mmc_curpath);

	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	return 0;

error:
	launch_syspopup("mounterr");

error_without_popup:
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_FAILED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);

	return 0;
}

static int mmc_unmount(struct block_data *data, int result)
{
	if (result < 0)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	else {
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
		free(mmc_curpath);
		mmc_curpath = NULL;
	}

	return 0;
}

static int mmc_format(struct block_data *data, int result)
{
	if (result < 0)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_FAILED);
	else
		vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED);
	return 0;
}

static int mmc_remove(struct block_data *data, int result)
{
	if (!result)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
				VCONFKEY_SYSMAN_MMC_REMOVED);
}

const struct block_dev_ops mmc_block_dev_ops = {
	.name    = "mmc",
	.type    = BLOCK_MMC_DEV,
	.init    = mmc_init,
	.mount   = mmc_mount,
	.unmount = mmc_unmount,
	.format  = mmc_format,
	.remove  = mmc_remove,
};

BLOCK_DEVICE_OPS_REGISTER(&mmc_block_dev_ops)
