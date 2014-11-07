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

#include "device-node.h"
#include "core/log.h"
#include "core/devices.h"
#include "core/common.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"
#include "core/config-parser.h"

#define MEMNOTIFY_NORMAL	0x0000
#define MEMNOTIFY_LOW		0xfaac
#define MEMNOTIFY_CRITICAL	0xdead
#define MEMNOTIFY_REBOOT	0xb00f

#define MEMORY_STATUS_USR_PATH  "/opt/usr"
#define MEMORY_MEGABYTE_VALUE   1048576

#define MEMNOTI_WARNING_VALUE  (5) // 5% under
#define MEMNOTI_CRITICAL_VALUE (0.1) // 0.1% under
#define MEMNOTI_FULL_VALUE     (0.0) // 0.0% under

#define SIGNAL_LOWMEM_STATE     "ChangeState"
#define SIGNAL_LOWMEM_FULL      "Full"

#define POPUP_KEY_MEMNOTI       "_MEM_NOTI_"
#define POPUP_KEY_APPNAME       "_APP_NAME_"

#define LOWMEM_POPUP_NAME       "lowmem-syspopup"

#define MEMNOTI_TIMER_INTERVAL  5
#define MEM_TRIM_TIMER_INTERVAL 86400 /* 24 hour */
#define MEM_FSTRIM_PATH         "/sbin/fstrim"

#define MEM_TRIM_START_TIME     2 // AM 02:00:00
#define MIN_SEC                 (60)
#define HOUR_SEC                (MIN_SEC * MIN_SEC)

#define BUF_MAX                 1024

#define STORAGE_CONF_FILE       "/etc/deviced/storage.conf"

enum memnoti_level {
	MEMNOTI_LEVEL_CRITICAL = 0,
	MEMNOTI_LEVEL_WARNING,
	MEMNOTI_LEVEL_NORMAL,
} ;

struct popup_data {
	char *name;
	char *key;
	char *value;
};

struct storage_config_info {
	double warning_level;
	double critical_level;
	double full_level;
};

static Ecore_Fd_Handler *lowmem_efd = NULL;
static int lowmem_fd;
static int cur_mem_state = MEMNOTIFY_NORMAL;

static Ecore_Timer *memnoti_timer = NULL;
static Ecore_Timer *mem_trim_timer = NULL;

static struct storage_config_info storage_info = {
	.warning_level   = MEMNOTI_WARNING_VALUE,
	.critical_level = MEMNOTI_CRITICAL_VALUE,
	.full_level      = MEMNOTI_FULL_VALUE,
};

static void memnoti_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;
	broadcast_edbus_signal(DEVICED_PATH_LOWMEM, DEVICED_INTERFACE_LOWMEM,
			SIGNAL_LOWMEM_STATE, "i", arr);
}

static void memnoti_level_broadcast(enum memnoti_level level)
{
	static int status = 0;
	if (level == MEMNOTI_LEVEL_CRITICAL && status == 0)
		status = 1;
	else if (level != MEMNOTI_LEVEL_CRITICAL && status == 1)
		status = 0;
	else
		return;
	_D("send user mem noti : %d %d", level, status);
	memnoti_send_broadcast(status);
}

static int memnoti_popup(enum memnoti_level level)
{
	int ret = -1;
	int val = -1;
	char *value = NULL;
	struct popup_data *params;
	static const struct device_ops *apps = NULL;

	if (level != MEMNOTI_LEVEL_WARNING && level != MEMNOTI_LEVEL_CRITICAL) {
		_E("level check error : %d",level);
		return 0;
	}

	if (level == MEMNOTI_LEVEL_WARNING) {
		value = "warning";
	} else if (level == MEMNOTI_LEVEL_CRITICAL) {
		value = "critical";
	}

	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &val);
	if (val == 0 || ret != 0)
		goto out;

	FIND_DEVICE_INT(apps, "apps");

	params = malloc(sizeof(struct popup_data));
	if (params == NULL) {
		_E("Malloc failed");
		return -1;
	}
	params->name = LOWMEM_POPUP_NAME;
	params->key = POPUP_KEY_MEMNOTI;
	params->value = strdup(value);
	apps->init((void *)params);
	free(params);
	return 0;
out:
	return -1;
}

static enum memnoti_level check_memnoti_level(double total, double avail)
{
	double tmp_size = (avail/total)*100;

	if (tmp_size > storage_info.warning_level)
		return MEMNOTI_LEVEL_NORMAL;
	if (tmp_size > storage_info.critical_level)
		return MEMNOTI_LEVEL_WARNING;
	return MEMNOTI_LEVEL_CRITICAL;
}

static void memnoti_full_broadcast(double total, double avail)
{
	static int status = 0;
	int tmp = 0;
	double tmp_size = (avail/total)*100;
	char *arr[1];
	char str_status[32];

	tmp = status;
	if (tmp_size <= storage_info.full_level && status == 0)
		status = 1;
	else if (tmp_size > storage_info.full_level && status == 1)
		status = 0;
	if (status == tmp)
		return;

	_D("send memory full noti : %d (total: %4.4lf avail: %4.4lf)", status, total, avail);
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;
	broadcast_edbus_signal(DEVICED_PATH_LOWMEM, DEVICED_INTERFACE_LOWMEM,
			SIGNAL_LOWMEM_FULL, "i", arr);
}

static void memory_status_set_full_mem_size(void)
{
	struct statvfs s;
	double dTotal = 0.0;
	double dAvail = 0.0;

	storage_get_internal_memory_size(&s);
	dTotal = (double)s.f_frsize * s.f_blocks;
	dAvail = (double)s.f_bsize * s.f_bavail;

	storage_info.full_level += (MEMORY_MEGABYTE_VALUE/dTotal)*100;
	_I("full : %4.4lf avail : %4.4lf warning : %4.4lf critical : %4.4lf",
		storage_info.full_level, (dAvail*100/dTotal),
		storage_info.warning_level, storage_info.critical_level);
}

static Eina_Bool memory_status_get_available_size(void *data)
{
	static enum memnoti_level old = MEMNOTI_LEVEL_NORMAL;
	enum memnoti_level now;
	int ret;
	struct statvfs s;
	double dAvail = 0.0;
	double dTotal = 0.0;

	storage_get_internal_memory_size(&s);
	dTotal = (double)s.f_frsize * s.f_blocks;
	dAvail = (double)s.f_bsize * s.f_bavail;

	memnoti_full_broadcast(dTotal, dAvail);

	now = check_memnoti_level(dTotal, dAvail);

	memnoti_level_broadcast(now);

	if (now < MEMNOTI_LEVEL_NORMAL && now < old) {
		ret = memnoti_popup(now);
		if (ret != 0)
			now = MEMNOTI_LEVEL_NORMAL;
	}
	old = now;
	if (memnoti_timer)
		ecore_timer_interval_set(memnoti_timer, MEMNOTI_TIMER_INTERVAL);
out:
	return EINA_TRUE;
}

static int __memnoti_fd_init(void)
{
	memory_status_set_full_mem_size();
	memory_status_get_available_size(NULL);
	memnoti_timer = ecore_timer_add(MEMNOTI_TIMER_INTERVAL,
				memory_status_get_available_size, NULL);
	if (memnoti_timer == NULL)
	    _E("fail mem available noti timer add");
	return 0;
}

static Eina_Bool memory_trim_cb(void *data)
{
	ecore_timer_interval_set(memnoti_timer, MEM_TRIM_TIMER_INTERVAL);
	if (launch_if_noexist(MEM_FSTRIM_PATH, MEMORY_STATUS_USR_PATH) == -1) {
		_E("fail to launch fstrim");
	} else {
		_D("fs memory trim is operated");
	}
	return EINA_TRUE;
}

static int __mem_trim_delta(struct tm *cur_tm)
{
	int delta = 0;
	int sign_val;

	if (cur_tm->tm_hour < MEM_TRIM_START_TIME)
		sign_val = 1;
	else
		sign_val = -1;
	delta += ((sign_val) * (MEM_TRIM_START_TIME - cur_tm->tm_hour) * HOUR_SEC);
	delta -= ((sign_val) * (cur_tm->tm_min * MIN_SEC + cur_tm->tm_sec));
	return delta;
}

static int __run_mem_trim(void)
{
	time_t now;
	struct tm *cur_tm;
	int mem_trim_time;

	now = time(NULL);
	cur_tm = (struct tm *)malloc(sizeof(struct tm));
	if (cur_tm == NULL) {
		_E("Fail to memory allocation");
		return -1;
	}

	if (localtime_r(&now, cur_tm) == NULL) {
		_E("Fail to get localtime");
		free(cur_tm);
		return -1;
	}

	mem_trim_time = MEM_TRIM_TIMER_INTERVAL + __mem_trim_delta(cur_tm);
	_D("start mem trim timer", mem_trim_time);
	mem_trim_timer = ecore_timer_add(mem_trim_time, memory_trim_cb, NULL);
	if (mem_trim_timer == NULL) {
		_E("Fail to add mem trim timer");
		free(cur_tm);
		return -1;
	}
	free(cur_tm);
	return 0;
}

static DBusMessage *edbus_getstatus(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	struct statvfs s;
	double dAvail = 0.0;
	double dTotal = 0.0;

	storage_get_internal_memory_size(&s);
	dTotal = (double)s.f_frsize * s.f_blocks;
	dAvail = (double)s.f_bsize * s.f_bavail;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &dTotal);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &dAvail);
	return reply;
}

static DBusMessage *edbus_memtrim(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = launch_if_noexist(MEM_FSTRIM_PATH, MEMORY_STATUS_USR_PATH);
	if (ret == -1) {
		_E("fail to launch fstrim");
	} else {
		_D("fs memory trim is operated");
	}

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "getstorage",       NULL,   "i", edbus_getstatus },
	{ "MemTrim",       NULL,   "i", edbus_memtrim },
	/* Add methods here */
};

static int booting_done(void *data)
{
	static int done = 0;

	if (data != NULL) {
		done = (int)data;
		if (done)
			_I("booting done");
		if (__memnoti_fd_init() == -1)
			_E("fail remain mem noti control fd init");
	}
	return done;
}

static int lowmem_poweroff(void *data)
{
	if (memnoti_timer) {
		ecore_timer_del(memnoti_timer);
		memnoti_timer = NULL;
	}
	if (mem_trim_timer) {
		ecore_timer_del(mem_trim_timer);
		mem_trim_timer = NULL;
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

static void lowmem_init(void *data)
{
	int ret;

	storage_config_load(&storage_info);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_POWEROFF, lowmem_poweroff);
	ret = register_edbus_method(DEVICED_PATH_STORAGE, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	if (__run_mem_trim() < 0) {
		_E("fail mem trim timer start");
	}
}

static void lowmem_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF, lowmem_poweroff);
}

static const struct device_ops lowmem_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "lowmem",
	.init     = lowmem_init,
	.exit	  = lowmem_exit,
};

DEVICE_OPS_REGISTER(&lowmem_device_ops)
