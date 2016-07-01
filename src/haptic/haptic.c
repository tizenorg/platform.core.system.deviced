/*
 * deviced-vibrator
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
#include <stdbool.h>
#include <dlfcn.h>
#include <assert.h>
#include <vconf.h>
#include <time.h>

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"
#include "core/config-parser.h"
#include "haptic.h"

#ifdef WEARABLE_CIRCLE
#include <Ecore.h>
#include <sys/types.h>
#include <fcntl.h>
#endif

#ifndef DATADIR
#define DATADIR		"/usr/share/deviced"
#endif

#define HAPTIC_CONF_PATH			"/etc/deviced/haptic.conf"

#ifdef WEARABLE_CIRCLE
#define CIRCLE_ON_PATH		"/sys/class/sec/motor/motor_on"
#define CIRCLE_OFF_PATH		"/sys/class/sec/motor/motor_off"
#endif

/* hardkey vibration variable */
#define HARDKEY_VIB_ITERATION		1
#define HARDKEY_VIB_FEEDBACK		3
#define HARDKEY_VIB_PRIORITY		2
#define HARDKEY_VIB_DURATION		30
#define HAPTIC_FEEDBACK_STEP		20
#define DEFAULT_FEEDBACK_LEVEL		3

/* power on, power off vibration variable */
#define POWER_ON_VIB_DURATION			300
#define POWER_OFF_VIB_DURATION			300
#define POWER_VIB_FEEDBACK			100

#define MAX_EFFECT_BUFFER			(64*1024)

#ifndef VCONFKEY_RECORDER_STATE
#define VCONFKEY_RECORDER_STATE "memory/recorder/state"
#define VCONFKEY_RECORDER_STATE_RECORDING	2
#endif

#define CHECK_VALID_OPS(ops, r)		((ops) ? true : !(r = -ENODEV))
#define RETRY_CNT	3

struct haptic_info {
	char *sender;
	dd_list *handle_list;
};

/* for playing */
static int g_handle;

/* haptic operation variable */
static dd_list *h_head;
static dd_list *haptic_handle_list;
static const struct haptic_plugin_ops *h_ops;
static enum haptic_type h_type;
static bool haptic_disabled;

struct haptic_config {
	int level;
	int *level_arr;
	int sound_capture;
};

static struct haptic_config haptic_conf;

static int haptic_start(void);
static int haptic_stop(void);
static int haptic_internal_init(void);
static int remove_haptic_info(struct haptic_info *info);

void add_haptic(const struct haptic_ops *ops)
{
	DD_LIST_APPEND(h_head, (void *)ops);
}

void remove_haptic(const struct haptic_ops *ops)
{
	DD_LIST_REMOVE(h_head, (void *)ops);
}

static int haptic_module_load(void)
{
	struct haptic_ops *ops;
	dd_list *elem;
	int r;

	/* find valid plugin */
	DD_LIST_FOREACH(h_head, elem, ops) {
		if (ops->is_valid && ops->is_valid()) {
			if (ops->load)
				h_ops = ops->load();
			h_type = ops->type;
			break;
		}
	}

	if (!CHECK_VALID_OPS(h_ops, r)) {
		_E("Can't find the valid haptic device");
		return r;
	}

	/* solution bug
	 * we do not use internal vibration except power off.
	 * if the last handle is closed during the playing of vibration,
	 * solution makes unlimited vibration.
	 * so we need at least one handle. */
	haptic_internal_init();

	return 0;
}

static int convert_magnitude_by_conf(int level)
{
	int i, step;

	assert(level >= 0 && level <= 100);

	step = 100 / (haptic_conf.level-1);
	for (i = 0; i < haptic_conf.level; ++i) {
		if (level <= i*step) {
			_D("level changed : %d -> %d", level, haptic_conf.level_arr[i]);
			return haptic_conf.level_arr[i];
		}
	}

	_D("play default level");
	return DEFAULT_FEEDBACK_LEVEL * HAPTIC_FEEDBACK_STEP;
}

#ifdef WEARABLE_CIRCLE
Ecore_Timer *timer;

static int circle_stop();
static Eina_Bool timer_cb(void *data)
{
	circle_stop();
	return ECORE_CALLBACK_CANCEL;
}

static int circle_stop()
{
	int fd, ret;
	char buf[8];

	fd = open(CIRCLE_OFF_PATH, O_RDONLY);
	if (fd < 0)
		return -ENODEV;
	ret = read(fd, buf, 8);
	if (ret < 0) {
		_E("Failed to stop");
		close(fd);
		return -1;
	}
	close(fd);
	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}
	return 0;
}

static int circle_play(int duration)
{
	int fd, ret;
	char buf[8];

	if (timer) {
		ecore_timer_del(timer);
		timer = NULL;
	}

	fd = open(CIRCLE_ON_PATH, O_RDONLY);
	if (fd < 0)
		return -ENODEV;
	ret = read(fd, buf, 8);
	if (ret < 0) {
		_E("Failed to play");
		close(fd);
		return -1;
	}
	close(fd);
	ecore_timer_add(duration/1000.f, timer_cb, timer);

	return 0;
}
#endif

static DBusMessage *edbus_get_count(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret, val;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	ret = h_ops->get_device_count(&val);
	if (ret >= 0)
		ret = val;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static void haptic_name_owner_changed(const char *sender, void *data)
{
	dd_list *n;
	struct haptic_info *info = data;
	int handle;

	_I("%s (sender:%s)", __func__, sender);

	if (!info)
		return;

	for (n = info->handle_list; n; n = n->next) {
		handle = (int)(long)n->data;
		h_ops->stop_device(handle);
		h_ops->close_device(handle);
	}

	remove_haptic_info(info);
}

static struct haptic_info *add_haptic_info(const char *sender)
{
	struct haptic_info *info;

	assert(sender);

	info = calloc(1, sizeof(struct haptic_info));
	if (!info)
		return NULL;

	info->sender = strdup(sender);
	DD_LIST_APPEND(haptic_handle_list, info);

	register_edbus_watch(sender, haptic_name_owner_changed, info);

	return info;
}

static int remove_haptic_info(struct haptic_info *info)
{
	assert(info);

	unregister_edbus_watch(info->sender, haptic_name_owner_changed);

	DD_LIST_REMOVE(haptic_handle_list, info);
	DD_LIST_FREE_LIST(info->handle_list);
	free(info->sender);
	free(info);

	return 0;
}

static struct haptic_info *get_matched_haptic_info(const char *sender)
{
	dd_list *n;
	struct haptic_info *info;
	int len;

	assert(sender);

	len = strlen(sender) + 1;
	DD_LIST_FOREACH(haptic_handle_list, n, info) {
		if (!strncmp(info->sender, sender, len))
			return info;
	}

	return NULL;
}

static DBusMessage *edbus_open_device(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int index, handle, ret;
	struct haptic_info *info;
	const char *sender;

	/* Load haptic module before booting done */
	if (!CHECK_VALID_OPS(h_ops, ret)) {
		ret = haptic_module_load();
		if (ret < 0)
			goto exit;
	}

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &index, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

#ifdef WEARABLE_CIRCLE
	ret = 1;
	goto exit;
#endif
	ret = h_ops->open_device(index, &handle);
	if (ret < 0)
		goto exit;


	sender = dbus_message_get_sender(msg);
	if (!sender) {
		ret = -EPERM;
		h_ops->close_device(handle);
		goto exit;
	}

	info = get_matched_haptic_info(sender);
	if (!info) {
		info = add_haptic_info(sender);
		if (!info) {
			_E("fail to create haptic information");
			ret = -EPERM;
			h_ops->close_device(handle);
			goto exit;
		}
	}

	DD_LIST_APPEND(info->handle_list, (gpointer)(long)handle);

	ret = handle;

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
	unsigned int handle;
	int ret;
	struct haptic_info *info;
	const char *sender;
	int cnt;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("fail to get sender from dbus message");
		ret = -EPERM;
		goto exit;
	}

#ifdef WEARABLE_CIRCLE
	goto exit;
#endif

	ret = h_ops->close_device(handle);
	if (ret < 0)
		goto exit;

	info = get_matched_haptic_info(sender);
	if (!info) {
		_E("fail to find the matched haptic info.");
		goto exit;
	}

	DD_LIST_REMOVE(info->handle_list, (gpointer)(long)handle);
	cnt = DD_LIST_LENGTH(info->handle_list);
	if (cnt == 0)
		remove_haptic_info(info);

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
	unsigned int handle;
	int duration, level, priority, e_handle, ret = 0;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled)
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_INT32, &duration,
				DBUS_TYPE_INT32, &level,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	/* convert as per conf value */
	level = convert_magnitude_by_conf(level);
	if (level < 0) {
		ret = -EINVAL;
		goto exit;
	}

#ifdef WEARABLE_CIRCLE
	ret = circle_play(duration);
	goto exit;
#endif

	ret = h_ops->vibrate_monotone(handle, duration, level, priority, &e_handle);
	if (ret >= 0)
		ret = e_handle;

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
	unsigned int handle;
	unsigned char *data;
	int size, iteration, level, priority, e_handle, ret = 0;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled)
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_INT32, &iteration,
				DBUS_TYPE_INT32, &level,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	/* convert as per conf value */
	level = convert_magnitude_by_conf(level);
	if (level < 0) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->vibrate_buffer(handle, data, iteration, level, priority, &e_handle);
	if (ret >= 0)
		ret = e_handle;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_vibrate_effect(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	char *pattern;
	int level, priority, ret = 0;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled)
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_STRING, &pattern,
				DBUS_TYPE_INT32, &level,
				DBUS_TYPE_INT32, &priority, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	/* convert as per conf value */
	level = convert_magnitude_by_conf(level);
	if (level < 0) {
		ret = -EINVAL;
		goto exit;
	}

#ifdef WEARABLE_CIRCLE
	ret = circle_play(100);
	goto exit;
#endif

	ret = h_ops->vibrate_effect(handle, pattern,
			level, priority);

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
	unsigned int handle;
	int ret = 0;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled)
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

#ifdef WEARABLE_CIRCLE
	ret = circle_stop();
	goto exit;
#endif

	ret = h_ops->stop_device(handle);

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
	int index, state, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &index, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->get_device_state(index, &state);
	if (ret >= 0)
		ret = state;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_create_effect(E_DBus_Object *obj, DBusMessage *msg)
{
	static unsigned char data[MAX_EFFECT_BUFFER];
	static unsigned char *p = data;
	DBusMessageIter iter, arr;
	DBusMessage *reply;
	haptic_module_effect_element *elem_arr;
	int i, size, cnt, ret, bufsize = sizeof(data);

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &bufsize,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &elem_arr, &size,
				DBUS_TYPE_INT32, &cnt, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	if (bufsize > MAX_EFFECT_BUFFER) {
		ret = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < cnt; ++i)
		_D("[%2d] %d %d", i, elem_arr[i].haptic_duration, elem_arr[i].haptic_level);

	memset(data, 0, MAX_EFFECT_BUFFER);
	ret = h_ops->create_effect(data, bufsize, elem_arr, cnt);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arr);
	dbus_message_iter_append_fixed_array(&arr, DBUS_TYPE_BYTE, &p, bufsize);
	dbus_message_iter_close_container(&iter, &arr);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_duration(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int handle;
	unsigned char *data;
	int size, duration, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->get_buffer_duration(handle, data, &duration);
	if (ret >= 0)
		ret = duration;

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
	unsigned char *data;
	char *path;
	int size, ret;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
				DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	_D("file path : %s", path);
	ret = h_ops->convert_binary(data, size, path);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_show_handle_list(E_DBus_Object *obj, DBusMessage *msg)
{
	dd_list *n;
	dd_list *elem;
	struct haptic_info *info;
	int cnt = 0;

	_D("    sender    handle");
	DD_LIST_FOREACH(haptic_handle_list, n, info) {
		for (elem = info->handle_list; elem; elem = elem->next)
			_D("[%2d]%s    %d", cnt++, info->sender, (int)(long)elem->data);
	}

	return dbus_message_new_method_return(msg);
}

static DBusMessage *edbus_pattern_is_supported(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *data;
	int ret = 0;

	if (!CHECK_VALID_OPS(h_ops, ret))
		goto exit;

	if (haptic_disabled)
		goto exit;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &data,
				DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	ret = h_ops->is_supported(data);

	_I("%s is supported : %d", data, ret);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static int haptic_internal_init(void)
{
	int r;
	if (!CHECK_VALID_OPS(h_ops, r))
		return r;
	return h_ops->open_device(HAPTIC_MODULE_DEVICE_ALL, &g_handle);
}

static int haptic_internal_exit(void)
{
	int r;
	if (!CHECK_VALID_OPS(h_ops, r))
		return r;
	return h_ops->close_device(g_handle);
}

static int haptic_hardkey_changed_cb(void *data)
{
	int level, status, e_handle, ret;

	if (!CHECK_VALID_OPS(h_ops, ret)) {
		ret = haptic_module_load();
		if (ret < 0)
			return ret;
	}

	if (!g_handle)
		haptic_internal_init();

	/* if haptic is stopped, do not play vibration */
	if (haptic_disabled)
		return 0;

	if (vconf_get_bool(VCONFKEY_SETAPPL_VIBRATION_STATUS_BOOL, &status) < 0) {
		_E("fail to get VCONFKEY_SETAPPL_VIBRATION_STATUS_BOOL");
		status = 1;
	}

	/* when turn off haptic feedback option */
	if (!status)
		return 0;

	ret = vconf_get_int(VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT, &level);
	if (ret < 0) {
		_E("fail to get VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT");
		level = HARDKEY_VIB_FEEDBACK;
	}

	ret = h_ops->vibrate_monotone(g_handle, HARDKEY_VIB_DURATION,
			level*HAPTIC_FEEDBACK_STEP, HARDKEY_VIB_PRIORITY, &e_handle);
	if (ret < 0)
		_E("fail to vibrate buffer : %d", ret);

	return ret;
}

static int haptic_poweroff_cb(void *data)
{
	int e_handle, ret;
	struct timespec time = {0,};

	if (!CHECK_VALID_OPS(h_ops, ret)) {
		ret = haptic_module_load();
		if (ret < 0)
			return ret;
	}

	if (!g_handle)
		haptic_internal_init();

	/* power off vibration */
	ret = h_ops->vibrate_monotone(g_handle, POWER_OFF_VIB_DURATION,
			POWER_VIB_FEEDBACK, HARDKEY_VIB_PRIORITY, &e_handle);
	if (ret < 0) {
		_E("fail to vibrate_monotone : %d", ret);
		return ret;
	}

	/* sleep for vibration */
	time.tv_nsec = POWER_OFF_VIB_DURATION * NANO_SECOND_MULTIPLIER;
	nanosleep(&time, NULL);
	return 0;
}

static void sound_capturing_cb(keynode_t *key, void *data)
{
	int status;

	status = vconf_keynode_get_int(key);

	/* if sound capture is in use, this value is 1(true). */
	if (status == VCONFKEY_RECORDER_STATE_RECORDING)
		haptic_stop();
	else
		haptic_start();
}

static int parse_section(struct parse_result *result, void *user_data, int index)
{
	struct haptic_config *conf = (struct haptic_config *)user_data;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	if (MATCH(result->name, "sound_capture")) {
		conf->sound_capture = atoi(result->value);
	} else if (MATCH(result->name, "level")) {
		conf->level = atoi(result->value);
		if (conf->level < 0) {
			_E("You must set level with positive number");
			return -EINVAL;
		}
		conf->level_arr = calloc(sizeof(int), conf->level);
		if (!conf->level_arr) {
			_E("failed to allocate memory for level");
			return -errno;
		}
	} else if (MATCH(result->name, "value")) {
		if (index < 0)
			return -EINVAL;
		conf->level_arr[index] = atoi(result->value);
	}

	return 0;
}

static int haptic_load_config(struct parse_result *result, void *user_data)
{
	struct haptic_config *conf = (struct haptic_config *)user_data;
	char name[NAME_MAX];
	int ret;
	static int index;

	if (!result)
		return 0;

	if (!result->section || !result->name || !result->value)
		return 0;

	/* Parsing 'Haptic' section */
	if (MATCH(result->section, "Haptic")) {
		ret = parse_section(result, user_data, -1);
		if (ret < 0) {
			_E("failed to parse [Haptic] section : %d", ret);
			return ret;
		}
		goto out;
	}

	/* Parsing 'Level' section */
	for (index = 0; index < conf->level; ++index) {
		snprintf(name, sizeof(name), "level%d", index);
		if (MATCH(result->section, name)) {
			ret = parse_section(result, user_data, index);
			if (ret < 0) {
				_E("failed to parse [level] section : %d", ret);
				return ret;
			}
			goto out;
		}
	}

out:
	return 0;
}

static const struct edbus_method edbus_methods[] = {
	{ "GetCount",          NULL,   "i", edbus_get_count },
	{ "OpenDevice",         "i",   "i", edbus_open_device },
	{ "CloseDevice",        "u",   "i", edbus_close_device },
	{ "StopDevice",         "u",   "i", edbus_stop_device },
	{ "VibrateMonotone", "uiii",   "i", edbus_vibrate_monotone },
	{ "VibrateBuffer", "uayiii",   "i", edbus_vibrate_buffer },
	{ "VibrateEffect",   "usii",   "i", edbus_vibrate_effect },
	{ "GetState",           "i",   "i", edbus_get_state },
	{ "GetDuration",      "uay",   "i", edbus_get_duration },
	{ "CreateEffect",    "iayi", "ayi", edbus_create_effect },
	{ "SaveBinary",       "ays",   "i", edbus_save_binary },
	{ "ShowHandleList",    NULL,  NULL, edbus_show_handle_list },
	{ "IsSupported",        "s",   "i", edbus_pattern_is_supported },
	/* Add methods here */
};

int haptic_probe(void)
{
	/**
	 * load haptic module.
	 * if there is no haptic module,
	 * deviced does not activate a haptic interface.
	 */
	return haptic_module_load();
}

void haptic_init(void)
{
	int r;

	/* get haptic data from configuration file */
	r = config_parse(HAPTIC_CONF_PATH, haptic_load_config, &haptic_conf);
	if (r < 0) {
		_E("failed to load configuration file(%s) : %d", HAPTIC_CONF_PATH, r);
		safe_free(haptic_conf.level_arr);
	}

	/* init dbus interface */
	r = register_edbus_interface_and_method(VIBRATOR_PATH_HAPTIC,
			VIBRATOR_INTERFACE_HAPTIC,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (r < 0)
		_E("fail to init edbus interface and method(%d)", r);

	/* register notifier for below each event */
	register_notifier(DEVICE_NOTIFIER_TOUCH_HARDKEY, haptic_hardkey_changed_cb);
	register_notifier(DEVICE_NOTIFIER_POWEROFF_HAPTIC, haptic_poweroff_cb);

	/* add watch for sound capturing value */
	if (haptic_conf.sound_capture)
		vconf_notify_key_changed(VCONFKEY_RECORDER_STATE, sound_capturing_cb, NULL);
}

void haptic_exit(void)
{
	struct haptic_ops *ops;
	dd_list *elem;
	int r;

	/* remove watch */
	if (haptic_conf.sound_capture)
		vconf_ignore_key_changed(VCONFKEY_RECORDER_STATE, sound_capturing_cb);

	/* unregister notifier for below each event */
	unregister_notifier(DEVICE_NOTIFIER_TOUCH_HARDKEY, haptic_hardkey_changed_cb);
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF_HAPTIC, haptic_poweroff_cb);

	/* release haptic data memory */
	safe_free(haptic_conf.level_arr);

	if (!CHECK_VALID_OPS(h_ops, r))
		return;

	/* haptic exit for deviced */
	haptic_internal_exit();

	/* release plugin */
	DD_LIST_FOREACH(h_head, elem, ops) {
		if (ops->is_valid && ops->is_valid()) {
			if (ops->release)
				ops->release();
			h_ops = NULL;
			break;
		}
	}
}

static int haptic_start(void)
{
	_I("start");
	haptic_disabled = false;
	return 0;
}

static int haptic_stop(void)
{
	_I("stop");
	haptic_disabled = true;
	return 0;
}
