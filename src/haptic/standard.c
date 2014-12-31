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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <Ecore.h>

#include "core/log.h"
#include "core/list.h"
#include "haptic.h"

#define MAX_MAGNITUDE			0xFFFF
#define PERIODIC_MAX_MAGNITUDE	0x7FFF	/* 0.5 * MAX_MAGNITUDE */

#define DEV_INPUT   "/dev/input"
#define EVENT		"event"

#define BITS_PER_LONG (sizeof(long) * 8)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)


struct ff_info {
	unsigned int id;
	Ecore_Timer *timer;
	struct ff_effect effect;
};

static int ff_fd;
static dd_list *ff_list;
static char ff_path[PATH_MAX];

/* for debug */
static void print_list(void)
{
	struct ff_info *temp;
	dd_list *elem;
	int i = 0;

	DD_LIST_FOREACH(ff_list, elem, temp)
		_D("[%d] %x", i++, temp);
}

static bool check_valid_handle(struct ff_info *info)
{
	struct ff_info *temp;
	dd_list *elem;

	DD_LIST_FOREACH(ff_list, elem, temp) {
		if (temp == info)
			break;
	}

	if (!temp)
		return false;
	return true;
}

static int ff_stop(int fd, struct ff_effect *effect);
static Eina_Bool timer_cb(void *data)
{
	struct ff_info *info = (struct ff_info *)data;

	if (!info)
		return ECORE_CALLBACK_CANCEL;

	if (!check_valid_handle(info))
		return ECORE_CALLBACK_CANCEL;

	_I("stop vibration by timer : id(%d)", info->effect.id);

	/* stop previous vibration */
	ff_stop(ff_fd, &info->effect);

	/* reset timer */
	info->timer = NULL;

	return ECORE_CALLBACK_CANCEL;
}

static int ff_find_device(void)
{
	DIR *dir;
	struct dirent *dent;
	char ev_path[PATH_MAX];
	unsigned long features[1+FF_MAX/sizeof(unsigned long)];
	int fd, ret;

	dir = opendir(DEV_INPUT);
	if (!dir)
		return -errno;

	while ((dent = readdir(dir))) {
		if (dent->d_type == DT_DIR ||
			!strstr(dent->d_name, "event"))
			continue;

		snprintf(ev_path, sizeof(ev_path), "%s/%s", DEV_INPUT, dent->d_name);

		fd = open(ev_path, O_RDWR);
		if (fd < 0)
			continue;

		/* get force feedback device */
		memset(features, 0, sizeof(features));
		ret = ioctl(fd, EVIOCGBIT(EV_FF, sizeof(features)), features);
		if (ret == -1) {
			close(fd);
			continue;
		}

		if (test_bit(FF_CONSTANT, features))
			_D("%s type : constant", ev_path);
		if (test_bit(FF_PERIODIC, features))
			_D("%s type : periodic", ev_path);
		if (test_bit(FF_SPRING, features))
			_D("%s type : spring", ev_path);
		if (test_bit(FF_FRICTION, features))
			_D("%s type : friction", ev_path);
		if (test_bit(FF_RUMBLE, features))
			_D("%s type : rumble", ev_path);

		if (test_bit(FF_PERIODIC, features)) {
			memcpy(ff_path, ev_path, strlen(ev_path));
			close(fd);
			closedir(dir);
			return 0;
		}

		close(fd);
	}

	closedir(dir);
	return -1;
}

static int ff_init_effect(struct ff_effect *effect)
{
	if (!effect)
		return -EINVAL;

	/* initialize member variables in effect struct */
	effect->type = FF_PERIODIC;
	effect->id = -1;
	effect->u.periodic.waveform = FF_SQUARE;
	effect->u.periodic.period = 0.1*0x100;	/* 0.1 second */
	effect->u.periodic.magnitude = 0;	/* temporary value */
	effect->u.periodic.offset = 0;
	effect->u.periodic.phase = 0;
	effect->direction = 0x4000;	/* Along X axis */
	effect->u.periodic.envelope.attack_length = 0;
	effect->u.periodic.envelope.attack_level = 0;
	effect->u.periodic.envelope.fade_length = 0;
	effect->u.periodic.envelope.fade_level = 0;
	effect->trigger.button = 0;
	effect->trigger.interval = 0;
	effect->replay.length = 0;		/* temporary value */
	effect->replay.delay = 0;

	return 0;
}

static int ff_set_effect(struct ff_effect *effect, int length, int level)
{
	double magnitude;

	if (!effect)
		return -EINVAL;

	magnitude = (double)level/HAPTIC_MODULE_FEEDBACK_MAX;
	magnitude *= PERIODIC_MAX_MAGNITUDE;

	_I("info : magnitude(%d) length(%d)", (int)magnitude, length);

	/* set member variables in effect struct */
	effect->u.periodic.magnitude = (int)magnitude;
	effect->replay.length = length;		/* length millisecond */

	return 0;
}

static int ff_play(int fd, struct ff_effect *effect)
{
	struct input_event play;

	if (fd < 0 || !effect)
		return -EINVAL;

	/* upload an effect */
	if (ioctl(fd, EVIOCSFF, effect) == -1)
		return -errno;

	/* play vibration*/
	play.type = EV_FF;
	play.code = effect->id;
	play.value = 1; /* 1 : PLAY, 0 : STOP */

	if (write(fd, (const void *)&play, sizeof(play)) == -1)
		return -errno;

	return 0;
}

static int ff_stop(int fd, struct ff_effect *effect)
{
	struct input_event stop;

	if (fd < 0)
		return -EINVAL;

	/* Stop vibration */
	stop.type = EV_FF;
	stop.code = effect->id;
	stop.value = 0; /* 1 : PLAY, 0 : STOP */

	if (write(fd, (const void *)&stop, sizeof(stop)) == -1)
		return -errno;

	/* removing an effect from the device */
	if (ioctl(fd, EVIOCRMFF, effect->id) == -1)
		return -errno;

	/* reset effect id */
	effect->id = -1;

	return 0;
}

static int create_unique_id(void)
{
	static int i;
	return i++;		/* TODO: overflow */
}

static struct ff_info *get_element_from_list(int id)
{
	dd_list *elem;
	struct ff_info *info;

	if (id < 0)
		return NULL;

	/* find matched element */
	DD_LIST_FOREACH(ff_list, elem, info) {
		if (info->id == id)
			return info;
	}

	return NULL;
}

/* START: Haptic Module APIs */
static int get_device_count(int *count)
{
	/* suppose there is just one haptic device */
	if (count)
		*count = 1;

	return 0;
}

static int open_device(int device_index, int *device_handle)
{
	struct ff_info *info;
	int n;

	if (!device_handle)
		return -EINVAL;

	/* if it is the first element */
	n = DD_LIST_LENGTH(ff_list);
	if (n == 0 && !ff_fd) {
		_I("First element: open ff driver");
		/* open ff driver */
		ff_fd = open(ff_path, O_RDWR);
		if (!ff_fd) {
			_E("Failed to open %s : %s", ff_path, strerror(errno));
			return -errno;
		}
	}

	/* allocate memory */
	info = calloc(sizeof(struct ff_info), 1);
	if (!info) {
		_E("Failed to allocate memory : %s", strerror(errno));
		return -errno;
	}

	/* create unique id */
	info->id = create_unique_id();

	/* initialize ff_effect structure */
	ff_init_effect(&info->effect);

	/* add info to local list */
	DD_LIST_APPEND(ff_list, info);

	*device_handle = info->id;
	return 0;
}

static int close_device(int device_handle)
{
	struct ff_info *info;
	int r, n;

	info = get_element_from_list(device_handle);
	if (!info)
		return -EINVAL;

	/* stop vibration */
	r = ff_stop(ff_fd, &info->effect);
	if (r < 0)
		_I("already stopped or failed to stop effect : %d", r);

	/* unregister existing timer */
	if (r >= 0 && info->timer) {
		ecore_timer_del(info->timer);
		info->timer = NULL;
	}

	/* remove info from local list */
	DD_LIST_REMOVE(ff_list, info);
	safe_free(info);

	/* if it is the last element */
	n = DD_LIST_LENGTH(ff_list);
	if (n == 0 && ff_fd) {
		_I("Last element: close ff driver");
		/* close ff driver */
		close(ff_fd);
		ff_fd = 0;
	}

	return 0;
}

static int vibrate_monotone(int device_handle, int duration, int feedback, int priority, int *effect_handle)
{
	struct ff_info *info;
	int ret;

	info = get_element_from_list(device_handle);
	if (!info)
		return -EINVAL;

	/* Zero(0) is the infinitely vibration value */
	if (duration == HAPTIC_MODULE_DURATION_UNLIMITED)
		duration = 0;

	/* set effect as per arguments */
	ret = ff_set_effect(&info->effect, duration, feedback);
	if (ret < 0) {
		_E("failed to set effect(duration:%d, feedback:%d) : %d",
				duration, feedback, ret);
		return ret;
	}

	/* play effect as per arguments */
	ret = ff_play(ff_fd, &info->effect);
	if (ret < 0) {
		_E("failed to play haptic effect(id:%d) : %d",
				info->effect.id, ret);
		return ret;
	}

	/* unregister existing timer */
	if (info->timer) {
		ecore_timer_del(info->timer);
		info->timer = NULL;
	}

	/* register timer */
	if (duration) {
		info->timer = ecore_timer_add(duration/1000.f, timer_cb, info);
		if (!info->timer)
			_E("Failed to add timer callback");
	}

	_D("effect id : %d", info->effect.id);
	if (effect_handle)
		*effect_handle = info->effect.id;

	return 0;
}

static int vibrate_buffer(int device_handle, const unsigned char *vibe_buffer, int iteration, int feedback, int priority, int *effect_handle)
{
	/* temporary code */
	return vibrate_monotone(device_handle, 300, feedback, priority, effect_handle);
}

static int stop_device(int device_handle)
{
	struct ff_info *info;
	int r;

	info = get_element_from_list(device_handle);
	if (!info)
		return -EINVAL;

	/* stop effect */
	r = ff_stop(ff_fd, &info->effect);
	if (r < 0)
		_E("failed to stop effect(id:%d) : %d", info->effect.id, r);

	/* unregister existing timer */
	if (r >= 0 && info->timer) {
		ecore_timer_del(info->timer);
		info->timer = NULL;
	}

	return 0;
}

static int get_device_state(int device_index, int *effect_state)
{
	struct ff_info *info;
	dd_list *elem;
	int n, status = false;

	if (!effect_state)
		return -EINVAL;

	/* suppose there is just one haptic device */
	DD_LIST_FOREACH(ff_list, elem, info) {
		if (info->effect.id >= 0) {
			status = true;
			break;
		}
	}

	*effect_state = status;
	return 0;
}

static int create_effect(unsigned char *vibe_buffer, int max_bufsize, haptic_module_effect_element *elem_arr, int max_elemcnt)
{
	_E("Not support feature");
	return -EACCES;
}

static int get_buffer_duration(int device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	_E("Not support feature");
	return -EACCES;
}

static int convert_binary(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	_E("Not support feature");
	return -EACCES;
}
/* END: Haptic Module APIs */

static const struct haptic_plugin_ops default_plugin = {
	.get_device_count    = get_device_count,
	.open_device         = open_device,
	.close_device        = close_device,
	.vibrate_monotone    = vibrate_monotone,
	.vibrate_buffer      = vibrate_buffer,
	.stop_device         = stop_device,
	.get_device_state    = get_device_state,
	.create_effect       = create_effect,
	.get_buffer_duration = get_buffer_duration,
	.convert_binary      = convert_binary,
};

static bool is_valid(void)
{
	int ret;

	ret = ff_find_device();
	if (ret < 0) {
		_I("Do not support standard haptic device");
		return false;
	}

	_I("Support standard haptic device");
	return true;
}

static const struct haptic_plugin_ops *load(void)
{
	return &default_plugin;
}

static const struct haptic_ops std_ops = {
	.type     = HAPTIC_STANDARD,
	.is_valid = is_valid,
	.load     = load,
};

HAPTIC_OPS_REGISTER(&std_ops)
