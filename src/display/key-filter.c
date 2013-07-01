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
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include <vconf.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "poll.h"
#include "core/queue.h"
#include "core/data.h"

#include <linux/input.h>
#ifndef KEY_SCREENLOCK
#define KEY_SCREENLOCK		0x98
#endif

#define PREDEF_LEAVESLEEP	"leavesleep"
#define POWEROFF_ACT			"poweroff"
#define PWROFF_POPUP_ACT		"pwroff-popup"
#define USEC_PER_SEC			1000000
#define LONG_PRESS_INTERVAL		1	/* 1 second */
#define COMBINATION_INTERVAL		0.3	/* 0.3 second */
#define POWER_KEY_PRESS_IGNORE_TIME	0.7	/* 0.7 second */

#define KEY_RELEASED		0
#define KEY_PRESSED		1
#define KEY_BEING_PRESSED	2

#define KEY_COMBINATION_STOP		0
#define KEY_COMBINATION_START		1
#define KEY_COMBINATION_SCREENCAPTURE	2

static struct timeval pressed_time;
static Ecore_Timer *longkey_timeout_id = NULL;
static Ecore_Timer *combination_timeout_id = NULL;
static int cancel_lcdoff;
static int key_combination = KEY_COMBINATION_STOP;
static int powerkey_ignored = false;

static inline int current_state_in_on(void)
{
	return (pm_cur_state == S_LCDDIM || pm_cur_state == S_NORMAL);
}

static void longkey_pressed()
{
	int val = 0;
	char *opt;
	_I("Power key long pressed!");
	cancel_lcdoff = 1;

	/* change state - LCD on */
	recv_data.pid = -1;
	recv_data.cond = 0x100;
	(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);

	(*g_pm_callback) (INPUT_POLL_EVENT, NULL);

	if (vconf_get_int(VCONFKEY_TESTMODE_POWER_OFF_POPUP, &val) == 0 && val == 1)
		opt = POWEROFF_ACT;
	else
		opt = PWROFF_POPUP_ACT;

	ss_action_entry_call_internal(opt, 0);
}

static Eina_Bool longkey_pressed_cb(void *data)
{
	longkey_pressed();
	longkey_timeout_id = NULL;

	return EINA_FALSE;
}

static Eina_Bool combination_failed_cb(void *data)
{
	key_combination = KEY_COMBINATION_STOP;
	combination_timeout_id = NULL;

	return EINA_FALSE;
}

static unsigned long timediff_usec(struct timeval t1, struct timeval t2)
{
	unsigned long udiff;

	udiff = (t2.tv_sec - t1.tv_sec) * USEC_PER_SEC;
	udiff += (t2.tv_usec - t1.tv_usec);

	return udiff;
}

static void stop_key_combination(void)
{
	key_combination = KEY_COMBINATION_STOP;
	if (combination_timeout_id > 0) {
		g_source_remove(combination_timeout_id);
		combination_timeout_id = NULL;
	}
}

static int process_power_key(struct input_event *pinput)
{
	int ignore = true;

	switch (pinput->value) {
	case KEY_RELEASED:
		if (current_state_in_on() && !cancel_lcdoff &&
		    !(key_combination == KEY_COMBINATION_SCREENCAPTURE)) {
			check_processes(S_LCDOFF);
			check_processes(S_LCDDIM);

			if (!check_holdkey_block(S_LCDOFF) &&
			    !check_holdkey_block(S_LCDDIM)) {
				delete_condition(S_LCDOFF);
				delete_condition(S_LCDDIM);
				/* LCD off forcly */
				recv_data.pid = -1;
				recv_data.cond = 0x400;
				(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);
			}
		} else {
			if (!powerkey_ignored)
				ignore = false;
		}

		stop_key_combination();
		cancel_lcdoff = 0;
		if (longkey_timeout_id > 0) {
			ecore_timer_del(longkey_timeout_id);
			longkey_timeout_id = NULL;
		}
		break;
	case KEY_PRESSED:
		if (timediff_usec(pressed_time, pinput->time) <
		    (POWER_KEY_PRESS_IGNORE_TIME * USEC_PER_SEC)) {
			_I("power key double pressed ignored");
			powerkey_ignored = true;
			break;
		} else {
			powerkey_ignored = false;
		}
		_I("power key pressed");
		pressed_time.tv_sec = (pinput->time).tv_sec;
		pressed_time.tv_usec = (pinput->time).tv_usec;
		if (key_combination == KEY_COMBINATION_STOP) {
			/* add long key timer */
			longkey_timeout_id = ecore_timer_add(
				    LONG_PRESS_INTERVAL,
				    (Ecore_Task_Cb)longkey_pressed_cb, NULL);
			key_combination = KEY_COMBINATION_START;
			combination_timeout_id = ecore_timer_add(
				    COMBINATION_INTERVAL,
				    (Ecore_Task_Cb)combination_failed_cb, NULL);
		} else if (key_combination == KEY_COMBINATION_START) {
			if (combination_timeout_id > 0) {
				ecore_timer_del(combination_timeout_id);
				combination_timeout_id = NULL;
			}
			_I("capture mode");
			key_combination = KEY_COMBINATION_SCREENCAPTURE;
			ignore = false;
		}
		break;
	case KEY_BEING_PRESSED:
		if (timediff_usec(pressed_time, pinput->time) >
		    (LONG_PRESS_INTERVAL * USEC_PER_SEC))
			longkey_pressed();
		break;
	}

	return ignore;
}

static int process_volumedown_key(struct input_event *pinput)
{
	int ignore = true;

	if (pinput->value == KEY_PRESSED) {
		if (key_combination == KEY_COMBINATION_STOP) {
			key_combination = KEY_COMBINATION_START;
			combination_timeout_id = ecore_timer_add(
				    COMBINATION_INTERVAL,
				    (Ecore_Task_Cb)combination_failed_cb, NULL);
		} else if (key_combination == KEY_COMBINATION_START) {
			if (combination_timeout_id > 0) {
				ecore_timer_del(combination_timeout_id);
				combination_timeout_id = NULL;
			}
			_I("capture mode");
			key_combination = KEY_COMBINATION_SCREENCAPTURE;
			ignore = false;
		}
	} else if (pinput->value == KEY_RELEASED) {
		if (key_combination != KEY_COMBINATION_SCREENCAPTURE) {
			stop_key_combination();
			if (current_state_in_on())
				ignore = false;
		}
	}

	return ignore;
}

static int check_key(struct input_event *pinput)
{
	int ignore = true;

	switch (pinput->code) {
	case KEY_POWER:
		ignore = process_power_key(pinput);
		break;
	case KEY_VOLUMEDOWN:
		ignore = process_volumedown_key(pinput);
		break;
	case KEY_VOLUMEUP:
	case KEY_CAMERA:
	case KEY_EXIT:
	case KEY_PHONE:
	case KEY_CONFIG:
	case KEY_SEARCH:
	case KEY_MEDIA:
		stop_key_combination();
		if (current_state_in_on())
			ignore = false;
		break;
	case KEY_SCREENLOCK:
	case 0x1DB:
	case 0x1DC:
	case 0x1DD:
	case 0x1DE:
		stop_key_combination();
		break;
	default:
		stop_key_combination();
		ignore = false;
	}
#ifdef ENABLE_PM_LOG
	if (pinput->value == KEY_PRESSED)
		pm_history_save(PM_LOG_KEY_PRESS, pinput->code);
	else if (pinput->value == KEY_RELEASED)
		pm_history_save(PM_LOG_KEY_RELEASE, pinput->code);
#endif
	return ignore;
}

int check_key_filter(int length, char buf[])
{
	struct input_event *pinput;
	int ignore = true;
	int idx = 0;

	do {
		pinput = (struct input_event *)&buf[idx];
		switch (pinput->type) {
		case EV_KEY:
			ignore = check_key(pinput);
			break;
		case EV_REL:
			ignore = false;
			break;
		case EV_ABS:
			if (current_state_in_on())
				ignore = false;
			break;
		}
		idx += sizeof(struct input_event);
		if (ignore == true && length <= idx)
			return 1;
	} while (length > idx);

	return 0;
}

