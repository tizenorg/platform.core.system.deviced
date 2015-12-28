/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <libinput.h>
#include <Ecore.h>
#include "util.h"
#include "core.h"
#include "poll.h"

#define SEAT_NAME   "seat0"

static struct udev *udev;
static struct libinput *li;
static Ecore_Fd_Handler *efd;

int (*pm_callback) (int, PMMsg *);

static inline void process_event(struct libinput_event *ev)
{
	static const struct device_ops *display_device_ops;
	struct input_event input;
	struct libinput *li;
	struct libinput_event_keyboard *k;
	unsigned int time;
	int fd;

	if (!pm_callback)
		return;

	if (!display_device_ops) {
		display_device_ops = find_device("display");
		if (!display_device_ops)
			return;
	}

	/* do not operate when display stops */
	if (device_get_status(display_device_ops)
			!= DEVICE_OPS_STATUS_START) {
		_E("display status is stop");
		return;
	}

	switch (libinput_event_get_type(ev)) {
	case LIBINPUT_EVENT_DEVICE_ADDED:
		return;
	case LIBINPUT_EVENT_KEYBOARD_KEY:
		k = libinput_event_get_keyboard_event(ev);
		time = libinput_event_keyboard_get_time(k);
		li = libinput_event_get_context(ev);

		input.time.tv_sec = MSEC_TO_SEC(time);
		input.time.tv_usec = MSEC_TO_USEC(time % 1000);
		input.type = EV_KEY;
		input.code = libinput_event_keyboard_get_key(k);
		input.value = libinput_event_keyboard_get_key_state(k);

		fd = libinput_get_fd(li);
		_D("time %d.%d type %d code %d value %d fd %d",
				input.time.tv_sec, input.time.tv_usec, input.type,
				input.code, input.value, fd);

		if (CHECK_OPS(keyfilter_ops, check) &&
		    keyfilter_ops->check(&input, fd) != 0)
			return;
		break;
	case LIBINPUT_EVENT_POINTER_MOTION:
	case LIBINPUT_EVENT_POINTER_BUTTON:
	case LIBINPUT_EVENT_POINTER_AXIS:
		li = libinput_event_get_context(ev);
		input.type = EV_REL;

		fd = libinput_get_fd(li);
		_D("time %d.%d type %d code %d value %d fd %d",
				input.time.tv_sec, input.time.tv_usec, input.type,
				input.code, input.value, fd);

		if (CHECK_OPS(keyfilter_ops, check) &&
		    keyfilter_ops->check(&input, fd) != 0)
			return;
		break;
	case LIBINPUT_EVENT_TOUCH_DOWN:
	case LIBINPUT_EVENT_TOUCH_UP:
	case LIBINPUT_EVENT_TOUCH_MOTION:
	case LIBINPUT_EVENT_TOUCH_FRAME:
		break;
	default:
		break;
	}

	/* lcd on or update lcd timeout */
	(*pm_callback) (INPUT_POLL_EVENT, NULL);
}

static Eina_Bool input_handler(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct libinput_event *ev;
	struct libinput *input = (struct libinput *)data;

	if (!input)
		return ECORE_CALLBACK_RENEW;

	libinput_dispatch(input);

	while ((ev = libinput_get_event(input))) {
		process_event(ev);

		libinput_event_destroy(ev);
		libinput_dispatch(input);
	}

	return ECORE_CALLBACK_RENEW;
}

static int open_restricted(const char *path, int flags, void *user_data)
{
	int fd;
	unsigned int clockid = CLOCK_MONOTONIC;

	if (!path)
		return -EINVAL;

	fd = open(path, flags);
	if (fd >= 0) {
		/* TODO Why does fd change the clock? */
		if (ioctl(fd, EVIOCSCLOCKID, &clockid) < 0)
			_E("fail to change clock %s", path);
	}

	return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
	close(fd);
}

static const struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

int init_input(int (*callback)(int , PMMsg * ))
{
	int ret;
	int fd;

	if (!callback) {
		_E("invalid parameter : callback(NULL)");
		return -EINVAL;
	}

	pm_callback = callback;

	udev = udev_new();
	if (!udev) {
		_E("fail to create udev library context");
		return -EPERM;
	}

	li = libinput_udev_create_context(&interface, NULL, udev);
	if (!li) {
		_E("fail to create a new libinput context from udev");
		return -EPERM;
	}

	ret = libinput_udev_assign_seat(li, SEAT_NAME);
	if (ret < 0) {
		_E("fail to assign a seat");
		return -EPERM;
	}

	fd = libinput_get_fd(li);
	if (fd < 0) {
		_E("fail to get file descriptor from libinput context");
		return -EPERM;
	}

	/* add to poll handler */
	efd = ecore_main_fd_handler_add(fd, ECORE_FD_READ,
			input_handler,
			(void *)((intptr_t)li), NULL, NULL);
	if (!efd) {
		_E("fail to add fd handler");
		/* TODO Does it really need close()? */
		close(fd);
		return -EPERM;
	}

	return 0;
}

int exit_input(void)
{
	if (efd)
		ecore_main_fd_handler_del(efd);

	if (li)
		libinput_unref(li);

	if (udev)
		udev_unref(udev);

	return 0;
}
