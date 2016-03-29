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


#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <time.h>
#include <storage.h>
#include <tzplatform_config.h>

#include "device-node.h"
#include "core/log.h"
#include "core/devices.h"
#include "core/common.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"
#include "core/config-parser.h"
#include "apps/apps.h"

#define MEMORY_STATUS_TMP_PATH  "/tmp"
#define MEMNOTI_TMP_CRITICAL_VALUE (20)

#define MEMORY_MEGABYTE_VALUE   1048576

#define MEMNOTI_WARNING_VALUE  (5) /* 5% under */
#define MEMNOTI_CRITICAL_VALUE (0.1) /* 0.1% under */
#define MEMNOTI_FULL_VALUE     (0.0) /* 0.0% under */

#define SIGNAL_LOWMEM_STATE     "ChangeState"
#define SIGNAL_LOWMEM_FULL      "Full"
#define MEMNOTI_TIMER_INTERVAL  5

#define STORAGE_CONF_FILE       "/etc/deviced/storage.conf"

enum memnoti_level {
	MEMNOTI_LEVEL_CRITICAL = 0,
	MEMNOTI_LEVEL_WARNING,
	MEMNOTI_LEVEL_NORMAL,
	MEMNOTI_LEVEL_FULL,
};

enum memnoti_status {
	MEMNOTI_DISABLE,
	MEMNOTI_ENABLE,
};

struct storage_config_info {
	enum memnoti_level current_noti_level;
	double warning_level;
	double critical_level;
	double full_level;
};

static Ecore_Timer *memnoti_timer;

static struct storage_config_info storage_internal_info = {
	.current_noti_level = MEMNOTI_LEVEL_NORMAL,
	.warning_level      = MEMNOTI_WARNING_VALUE,
	.critical_level     = MEMNOTI_CRITICAL_VALUE,
	.full_level         = MEMNOTI_FULL_VALUE,
};

static struct storage_config_info storage_tmp_info = {
	.current_noti_level = MEMNOTI_LEVEL_NORMAL,
	.warning_level      = MEMNOTI_TMP_CRITICAL_VALUE,
	.critical_level     = MEMNOTI_TMP_CRITICAL_VALUE,
	.full_level         = MEMNOTI_FULL_VALUE,
};

static void memnoti_send_broadcast(char *signal, int status)
{
	char *arr[1];
	char str_status[32];

	_I("signal %s status %d", signal, status);
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;
	broadcast_edbus_signal(DEVICED_PATH_LOWMEM, DEVICED_INTERFACE_LOWMEM,
			signal, "i", arr);
}

static int memnoti_popup(enum memnoti_level level)
{
	int ret = -1;
	int val = -1;
	char *value = NULL;

	if (level != MEMNOTI_LEVEL_WARNING && level != MEMNOTI_LEVEL_CRITICAL) {
		_E("level check error : %d", level);
		return 0;
	}

	if (level == MEMNOTI_LEVEL_WARNING)
		value = "lowstorage_warning";
	else if (level == MEMNOTI_LEVEL_CRITICAL)
		value = "lowstorage_critical";

	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &val);
	if (val == 0 || ret != 0)
		goto out;

	if (value) {
		ret = launch_system_app(APP_DEFAULT,
				2, APP_KEY_TYPE, value);
		if (ret < 0)
			_E("Failed to launch (%s) popup", value);
	}

	return 0;
out:
	return -1;
}

static void storage_status_broadcast(struct storage_config_info *info, double total, double avail)
{
	double level = (avail/total)*100;
	int status = MEMNOTI_DISABLE;

	if (level <= info->full_level) {
		if (info->current_noti_level == MEMNOTI_LEVEL_FULL)
			return;
		info->current_noti_level = MEMNOTI_LEVEL_FULL;
		status = MEMNOTI_ENABLE;
		memnoti_send_broadcast(SIGNAL_LOWMEM_FULL, status);
		return;
	}

	if (level <= info->critical_level) {
		if (info->current_noti_level == MEMNOTI_LEVEL_CRITICAL)
			return;
		if (info->current_noti_level == MEMNOTI_LEVEL_FULL)
			memnoti_send_broadcast(SIGNAL_LOWMEM_FULL, status);
		info->current_noti_level = MEMNOTI_LEVEL_CRITICAL;
		status = MEMNOTI_ENABLE;
		memnoti_send_broadcast(SIGNAL_LOWMEM_STATE, status);
		return;
	}

	if (info->current_noti_level == MEMNOTI_LEVEL_FULL)
		memnoti_send_broadcast(SIGNAL_LOWMEM_FULL, status);
	if (info->current_noti_level == MEMNOTI_LEVEL_CRITICAL)
		memnoti_send_broadcast(SIGNAL_LOWMEM_STATE, status);
	if (level <= info->warning_level)
		info->current_noti_level = MEMNOTI_LEVEL_WARNING;
	else
		info->current_noti_level = MEMNOTI_LEVEL_NORMAL;
}

static int storage_get_memory_size(char *path, struct statvfs *s)
{
	int ret;

	if (!path) {
		_E("input param error");
		return -EINVAL;
	}

	ret = statvfs(path, s);
	if (ret) {
		_E("fail to get storage size");
		return -errno;
	}

	return 0;
}

static void get_storage_status(char *path, struct statvfs *s)
{
	if (strcmp(path, tzplatform_getenv(TZ_SYS_HOME)) == 0)
		storage_get_internal_memory_size(s);
	else
		storage_get_memory_size(path, s);
}

static void init_storage_config_info(char *path, struct storage_config_info *info)
{
	struct statvfs s;
	double dAvail = 0.0;
	double dTotal = 0.0;

	get_storage_status(path, &s);

	dTotal = (double)(s.f_frsize * s.f_blocks);
	dAvail = (double)(s.f_bsize * s.f_bavail);

	info->full_level += (MEMORY_MEGABYTE_VALUE/dTotal)*100;

	_I("%s t: %4.0lf a: %4.0lf(%4.2lf) c:%4.4lf f:%4.4lf",
		path, dTotal, dAvail, (dAvail*100/dTotal), info->critical_level, info->full_level);
}

static void check_internal_storage_popup(struct storage_config_info *info)
{
	static enum memnoti_level old = MEMNOTI_LEVEL_NORMAL;
	int ret;

	if (info->current_noti_level < MEMNOTI_LEVEL_NORMAL && info->current_noti_level < old) {
		ret = memnoti_popup(info->current_noti_level);
		if (ret != 0)
			info->current_noti_level = MEMNOTI_LEVEL_NORMAL;
	}
	old = info->current_noti_level;
}

static Eina_Bool check_storage_status(void *data)
{
	struct statvfs s;
	double dAvail = 0.0;
	double dTotal = 0.0;

	/* check internal */
	storage_get_internal_memory_size(&s);
	dTotal = (double)s.f_frsize * s.f_blocks;
	dAvail = (double)s.f_bsize * s.f_bavail;
	storage_status_broadcast(&storage_internal_info, dTotal, dAvail);
	check_internal_storage_popup(&storage_internal_info);
	/* check tmp */
	storage_get_memory_size(MEMORY_STATUS_TMP_PATH, &s);
	dTotal = (double)s.f_frsize * s.f_blocks;
	dAvail = (double)s.f_bsize * s.f_bavail;
	storage_status_broadcast(&storage_tmp_info, dTotal, dAvail);

	if (memnoti_timer)
		ecore_timer_interval_set(memnoti_timer, MEMNOTI_TIMER_INTERVAL);

	return EINA_TRUE;
}

static int init_storage_config_info_all(void)
{
	init_storage_config_info(tzplatform_getenv(TZ_SYS_HOME), &storage_internal_info);
	init_storage_config_info(MEMORY_STATUS_TMP_PATH, &storage_tmp_info);
	memnoti_timer = ecore_timer_add(MEMNOTI_TIMER_INTERVAL,
				check_storage_status, NULL);
	if (memnoti_timer == NULL)
		_E("fail mem available noti timer add");
	return 0;
}

static DBusMessage *edbus_getstatus(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	struct statvfs s;
	unsigned long long dAvail = 0.0;
	unsigned long long dTotal = 0.0;

	storage_get_internal_memory_size(&s);
	dTotal = (unsigned long long)s.f_frsize * s.f_blocks;
	dAvail = (unsigned long long)s.f_bsize * s.f_bavail;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &dTotal);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &dAvail);
	return reply;
}

static DBusMessage *edbus_get_storage_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	char *path;
	struct statvfs s;
	pid_t pid;
	unsigned long long dAvail = 0.0;
	unsigned long long dTotal = 0.0;

	dbus_error_init(&err);
	if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID)) {
		_E("Bad message: [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		goto out;
	}

	if (!strcmp(path, tzplatform_getenv(TZ_SYS_HOME)))
		storage_get_internal_memory_size(&s);
	else
		storage_get_memory_size(path, &s);

	dTotal = (unsigned long long)s.f_frsize * s.f_blocks;
	dAvail = (unsigned long long)s.f_bsize * s.f_bavail;

	pid = get_edbus_sender_pid(msg);

	_D("[request %d] path %s total %4.0lf avail %4.0lf", pid, path, dTotal, dAvail);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &dTotal);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &dAvail);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "getstorage", NULL, "tt", edbus_getstatus },
	{ "GetStatus",   "s", "tt", edbus_get_storage_status},
	/* Add methods here */
};

static int booting_done(void *data)
{
	static int done;

	if (data == NULL)
		return done;
	done = *(int *)data;
	if (done == 0)
		return done;

	_I("booting done");

	if (init_storage_config_info_all() == -1)
		_E("fail remain mem noti control fd init");
	return done;
}

static int storage_poweroff(void *data)
{
	if (memnoti_timer) {
		ecore_timer_del(memnoti_timer);
		memnoti_timer = NULL;
	}
	return 0;
}

static int load_config(struct parse_result *result, void *user_data)
{
	struct storage_config_info *info = (struct storage_config_info *)user_data;
	char *name;
	char *value;

	if (!info)
		return -EINVAL;

	if (!MATCH(result->section, "LOWSTORAGE"))
		return -EINVAL;

	_D("%s,%s,%s", result->section, result->name, result->value);

	name = result->name;
	value = result->value;

	if (MATCH(name, "WARNING_LEVEL"))
		info->warning_level = (double)atof(value);
	else if (MATCH(name, "CRITICAL_LEVEL"))
		info->critical_level = (double)atof(value);
	else if (MATCH(name, "FULL_LEVEL"))
		info->full_level = (double)atof(value);

	return 0;
}

static void storage_config_load(struct storage_config_info *info)
{
	int ret;

	ret = config_parse(STORAGE_CONF_FILE, load_config, info);
	if (ret < 0)
		_E("Failed to load %s, %d Use default value!", STORAGE_CONF_FILE, ret);
}

static void storage_init(void *data)
{
	int ret;

	storage_config_load(&storage_internal_info);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_POWEROFF, storage_poweroff);
	ret = register_edbus_method(DEVICED_PATH_STORAGE, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static void storage_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF, storage_poweroff);
}

static const struct device_ops storage_device_ops = {
	.name     = "storage",
	.init     = storage_init,
	.exit	  = storage_exit,
};

DEVICE_OPS_REGISTER(&storage_device_ops)
