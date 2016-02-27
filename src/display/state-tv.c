/*
 * deviced
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "core/common.h"
#include "core/log.h"
#include "core/device-notifier.h"
#include "core/devices.h"
#include "power/power-handler.h"
#include "core.h"
#include "poll.h"
#include "device-interface.h"
#include "util.h"

#define PRE_STATE_CHANGE_TIMEOUT 500*1000 /* 500ms */
#define SIGNAL_CHANGE_STATE     "ChangeState"
#define SIGNAL_PRE_CHANGE_STATE "PreChangeState"
#define SIGNAL_WAKEUP           "WakeUp"
#define SIGNAL_PRE_WAKEUP       "PreWakeUp"
#define SIGNAL_POST_WAKEUP      "PostWakeUp"
#define SIGNAL_EARLY_WAKEUP     "EarlyWakeUp"


static bool pre_suspend_flag = false;
static unsigned long long resumetime;
static int wakeupcnt;

static int change_state(pid_t pid, int type, enum state_t st)
{
	int ret;

	if (type == PM_CONTROL_EVENT && states[st].check) {
		ret = states[pm_cur_state].check(pm_cur_state, st);
		if (ret != 0) {
			_E("(%s) State Locked. Cannot be changed to (%s)",
					states[pm_cur_state].name, states[st].name);
			return ret;
		}
	}

	if (states[st].trans) {
		ret = states[st].trans(type);
		if (ret < 0) {
			_E("Failed to trans state (%s, ret:%d)", states[st].name, ret);
			return ret;
		}
	}

	_I("Success to change state (%s) requested by pid(%d)", states[st].name, pid);

	return 0;
}

static int tv_proc_change_state(unsigned int cond, pid_t pid)
{
	enum state_t next;
	int ret;

	next = GET_COND_STATE(cond);
	if (pm_cur_state == next) {
		_I("current state (%d) == next state (%d)", pm_cur_state, next);
		return 0;
	}

	if (pid < INTERNAL_LOCK_BASE) { /* Request from other process*/
		if (next == S_SUSPEND || next == S_POWEROFF) {
			_E("Do not change to suspend or power off directly");
			return -EPERM;
		}
	}

	_I("Change State to %s (%d)", states[next].name, pid);

	ret = change_state(pid, PM_CONTROL_EVENT, next);
	if (ret != 0) {
		_E("Failed to change state (%d)", ret);
		return ret;
	}

	return 0;
}

unsigned long long get_uptime(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec;
}

static int lcdon_check(int curr, int next)
{
	/* do not changed to next state if who lock the normal mode */
	check_processes(pm_cur_state);
	if (check_lock_state(pm_cur_state)) {
		_I("S_LCDON Lock state");
		return 1;
	}

	return 0;
}

static int lcdon_pre(void *data)
{
	char *arr[1];

	/* TODO: cancel suspend */
	//cancel_suspend();

	/* That will unlock callback registration in case of getting back to normal
	 * from partial poweroff. If someone start poweroff with standby lock and then
	 * unlock, change state to lcdon, registration of poweroff callback would be blocked
	 * as unblocking is done when resuming from suspend.
	 */
	device_notify(DEVICE_NOTIFIER_POWER_RESUME, NULL);

	if (pm_cur_state == S_STANDBY){
		arr[0] = states[S_LCDON].name;
		_I("send pre state change NORMAL");
		broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
				SIGNAL_PRE_CHANGE_STATE, "s", arr);
		/*Give time to process callback */
		usleep(PRE_STATE_CHANGE_TIMEOUT);
	}

	return 0;
}

static int lcdon_post(void *data)
{
	char *arr[1];

	arr[0] = states[S_LCDON].name;
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			SIGNAL_CHANGE_STATE, "s", arr);

	/* TODO: set_power */
//	tv_system_set_power(POWER_TYPE_PICTURE_ON);

	return 0;
}

static int lcdon_action(int timeout)
{
	if (pm_cur_state != pm_old_state &&
		pm_cur_state != S_SLEEP)
		set_setting_pmstate(pm_cur_state);

	if (pm_old_state != S_LCDOFF ||
		pm_old_state != S_SLEEP) {
		_I("pm_old_state (%s). Skip lcd on", states[pm_old_state].name);
		return 0;
	}

	if (pre_suspend_flag) {
		/* TODO: post resume */
	//	system_post_resume();
		pre_suspend_flag = false;
	}

//	if (pm_initialized)
		backlight_ops.on(0);
//	else
		/* In order to remove delay at booting time,
		 * turn on FRC backlight in async mode */
//		backlight_on_async();

	return 0;
}

static int lcdon_trans(int evt)
{
	int ret;
	struct state *st;

	ret = lcdon_pre(NULL);
	if (ret < 0) {
		_E("S_LCDON pre-operation failed(%d)", ret);
		return ret;
	}

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = S_LCDON;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(0);

	ret = lcdon_post(NULL);
	if (ret < 0)
		_E("S_LCDON post-operation failed(%d)", ret);

	return 0;
}

static int lcdoff_check(int curr, int next)
{
	/* LCD OFF can change to LCD ON and STANDBY state */
	return 0;
}

static int lcdoff_pre(void *data)
{
	return 0;
}

static int lcdoff_post(void *data)
{
	char *arr[1];

	/* broadcast to other application */
	arr[0] = states[S_LCDOFF].name;
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			SIGNAL_CHANGE_STATE, "s", arr);

	return 0;
}

static int lcdoff_action(int timeout)
{
	if (pm_cur_state != pm_old_state &&
		pm_cur_state != S_SLEEP)
		set_setting_pmstate(pm_cur_state);

	if (pm_old_state == S_SUSPEND ||
		pm_old_state == S_LCDOFF)
		return 0;

	//backlight_off_async();
	backlight_ops.off(0);

	return 0;
}

static int lcdoff_trans(int evt)
{
	int ret;
	struct state *st;

	ret = lcdoff_pre(NULL);
	if (ret < 0) {
		_E("S_LCDOFF pre-operation failed (%d)", ret);
		return ret;
	}

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = S_LCDOFF;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(0);

	ret = lcdoff_post(NULL);
	if (ret < 0)
		_E("S_LCDOFF post-operation failed (%d)", ret);

	return 0;
}

static int standby_check(int curr, int next)
{
	/* STANDBY can change to LCD ON or POWER OFF state */
	if (next == S_LCDOFF)
		return -EPERM;

	/* do not change to next state if who lock the standby mode */
	check_processes(pm_cur_state);
	if (check_lock_state(pm_cur_state)) {
		_I("S_STANDBY Lock state");
		return 1;
	}

	/* TODO: Instant on timer */

	return 0;
}

static int standby_pre(void *data)
{
	return 0;
}

static int standby_post(void *data)
{
	char *arr[1];

	/* broadcast to other application */
	arr[0] = states[S_STANDBY].name;
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
			SIGNAL_CHANGE_STATE, "s", arr);

	backlight_ops.off(0);
//	tv_system_set_power(POWER_TYPE_PICTURE_OFF);

	return 0;
}

static int standby_action(int timeout)
{
	if (pm_cur_state != pm_old_state &&
		pm_cur_state != S_SLEEP)
		set_setting_pmstate(pm_cur_state);

	return 0;
}

static int standby_trans(int evt)
{
	int ret;
	struct state *st;

	ret = standby_pre(NULL);
	if (ret < 0) {
		_E("S_STANDBY pre-operation failed(%d)", ret);
		return ret;
	}

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = S_STANDBY;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(0);

	ret = standby_post(NULL);
	if (ret < 0)
		_E("S_STANDBY post-operation failed(%d)", ret);

	return 0;
}

static int suspend_check(int curr, int next)
{
	/* DO NOT USE THIS FUNCTION */
	return 0;
}

static int suspend_pre(void *data)
{
	return 0;
}

static int suspend_post(void *data)
{
	char *arr[1];
	int ret;
	unsigned int cond = 0;


	/* Save wakeup tickcount */
	resumetime = get_uptime();

	/* count InstantOn */
	wakeupcnt++;

	cond = S_LCDON;
	ret = tv_proc_change_state(cond, getpid());
	if (ret < 0)
		_E("Fail to change state to next_state(%s)", states[cond].name);

	/* Broadcast pre-wakeup signal */
	arr[0] = "0";
	broadcast_edbus_signal(DEVICED_PATH_POWER, DEVICED_INTERFACE_POWER,
			SIGNAL_PRE_WAKEUP, "i", arr);

	/* Notify resume state */
	device_notify(DEVICE_NOTIFIER_POWER_RESUME, NULL);

	return 0;
}

static int suspend_action(int timeout)
{
	struct state *st;

	if (pm_cur_state != pm_old_state &&
		pm_cur_state != S_SLEEP)
		set_setting_pmstate(pm_cur_state);

	/* TODO: set wakeup count */

	/* sleep state : set system mode to SUSPEND */
	power_ops.suspend();
//	system_suspend();

	_I("system wakeup!!");

	/* Resume !! */
	/* system waked up by devices */
	pm_old_state = pm_cur_state;
	pm_cur_state = S_LCDOFF;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(0);

	return 0;
}

static int suspend_trans(int evt)
{
	int ret;
	struct state *st;

	ret = suspend_pre(NULL);
	if (ret < 0) {
		_E("S_SUSPEND pre-operation failed(%d)", ret);
		return ret;
	}

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = S_LCDOFF;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(0);

	pm_old_state = pm_cur_state;
	pm_cur_state = S_SUSPEND;
	st = &states[pm_cur_state];

	if (st->action)
		st->action(0);

	ret = suspend_post(NULL);
	if (ret < 0)
		_E("S_SUSPEND post-operation failed(%d)", ret);

	return 0;
}

static int poweroff_check(int curr, int next)
{
	/* DO NOT USE THIS FUNCTION */
	return 0;
}

static int poweroff_action(int timeout)
{
	static const struct device_ops *ops;

	FIND_DEVICE_INT(ops, POWER_OPS_NAME);

	return ops->execute(POWER_POWEROFF);
}

static int poweroff_trans(int evt)
{
	struct state *st;
	st = &states[S_POWEROFF];
	if (st->action)
		st->action(0);
	return 0;
}

static void set_tv_operations(enum state_t st,
		char *name,
		int (*check) (int curr, int next),
		int (*trans) (int evt),
		int (*action) (int timeout))
{
	change_state_name(st, name);
	change_state_check(st, check);
	change_state_trans(st, trans);
	change_state_action(st, action);
}

struct _tv_states {
	enum state_t state;
	char *name;
	int (*check)(int curr, int next);
	int (*trans)(int evt);
	int (*action)(int timeout);
} tv_states [] = {
	{ S_LCDON,    "S_LCDON",    lcdon_check,    lcdon_trans,    lcdon_action    },
	{ S_LCDOFF,   "S_LCDOFF",   lcdoff_check,   lcdoff_trans,   lcdoff_action   },
	{ S_STANDBY,  "S_STANDBY",  standby_check,  standby_trans,  standby_action  },
	{ S_SUSPEND,  "S_SUSPEND",  suspend_check,  suspend_trans,  suspend_action  },
	{ S_POWEROFF, "S_POWEROFF", poweroff_check, poweroff_trans, poweroff_action },
};

static void __CONSTRUCTOR__ state_tv_init(void)
{
	_I("TV Profile !!");
	set_tv_operations(S_LCDON, "S_LCDON",
			lcdon_check, lcdon_trans, lcdon_action);
	set_tv_operations(S_LCDOFF, "S_LCDOFF",
			lcdoff_check, lcdoff_trans, lcdoff_action);
	set_tv_operations(S_STANDBY, "S_STANDBY",
			standby_check, standby_trans, standby_action);
	set_tv_operations(S_SUSPEND, "S_SUSPEND",
			suspend_check, suspend_trans, suspend_action);
	set_tv_operations(S_POWEROFF, "S_POWEROFF",
			poweroff_check, poweroff_trans, poweroff_action);

	change_proc_change_state(tv_proc_change_state);
}
