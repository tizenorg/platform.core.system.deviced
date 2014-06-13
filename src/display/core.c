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


/**
 * @file	core.c
 * @brief	Power manager main loop.
 *
 * This file includes Main loop, the FSM, signal processing.
 */
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <heynoti.h>
#include <aul.h>
#include <vconf-keys.h>
#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "battery.h"
#include "device-node.h"
#include "core/queue.h"
#include "core/data.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/udev.h"
#include "core/list.h"
#include "core/common.h"
#include <tzplatform_config.h>

#define USB_CON_PIDFILE			"/var/run/.system_server.pid"
#define PM_STATE_LOG_FILE		"/var/log/pm_state.log"
#define PM_WAKEUP_NOTI_NAME		"system_wakeup"
#define PM_EVENT_NOTI_NAME		"pm_event"
#define PM_EVENT_NOTI_PATH		tzplatform_mkpath(TZ_SYS_SHARE,"noti/"PM_EVENT_NOTI_NAME)

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

#define LOCKSTATUS_TIMEOUT		3
#define TELEPHONY_SIGNAL_TIMEOUT	10

#define SET_BRIGHTNESS_IN_BOOTLOADER	"/usr/bin/save_blenv SLP_LCD_BRIGHT"

/* default transition, action fuctions */
static int default_trans(int evt);
static int default_action(int timeout);
static int default_check(int next);

unsigned int pm_status_flag;

static void (*power_saving_func) (int onoff);
static void reset_timeout(int timeout);
static int get_lcd_timeout_from_settings();

static enum device_ops_status status = DEVICE_OPS_STATUS_UNINIT;

int pm_cur_state;
int pm_old_state;
Ecore_Timer *timeout_src_id;
static time_t last_t;
static int pre_suspend_flag = false;
int system_wakeup_flag = false;

struct state states[S_END] = {
	{S_START, default_trans, default_action, default_check,},
	{S_NORMAL, default_trans, default_action, default_check,},
	{S_LCDDIM, default_trans, default_action, default_check,},
	{S_LCDOFF, default_trans, default_action, default_check,},
	{S_SLEEP, default_trans, default_action, default_check,}
};

int (*pm_init_extention) (void *data);
void (*pm_exit_extention) (void);

static char state_string[5][10] =
	{ "S_START", "S_NORMAL", "S_LCDDIM", "S_LCDOFF", "S_SLEEP" };

static int trans_table[S_END][EVENT_END] = {
	/* Timeout , Input */
	{S_START, S_START},	/* S_START */
	{S_LCDDIM, S_NORMAL},	/* S_NORMAL */
	{S_LCDOFF, S_NORMAL},	/* S_LCDDIM */
#ifdef TIZEN_EMUL
	{S_LCDOFF, S_NORMAL},	/* S_LCDOFF */
#else
	{S_SLEEP, S_NORMAL},	/* S_LCDOFF */
#endif
	{S_LCDOFF, S_NORMAL},	/* S_SLEEP */
};

#define SHIFT_UNLOCK		4
#define MASK_RESET_TIMEOUT	0x8	/* 1000 */
#define MASK_MARGIN_TIMEOUT	(0x1 << 8)
#define SHIFT_CHANGE_STATE	7
#define CHANGE_STATE_BIT	0xF00	/* 1111 0000 0000 */
#define LOCK_SCREEN_TIMEOUT	5
#define SHIFT_LOCK_FLAG	16
#define HOLD_KEY_BLOCK_BIT	0x1

#define DEFAULT_NORMAL_TIMEOUT	30
#define DEFAULT_DIM_TIMEOUT		5
#define DEFAULT_OFF_TIMEOUT		1
#define GET_HOLDKEY_BLOCK_STATE(x) ((x >> SHIFT_LOCK_FLAG) & HOLD_KEY_BLOCK_BIT)
#define LOCK_SCREEN_WATING_TIME		50000	/* 50 ms */
#define LOCK_SCREEN_WATING_MAX_COUNT	14	/* 50 * 14 : 700 ms at worst */
#define MASK32				0xffffffff
#define LOCK_TIME_ERROR			600	/* 600 seconds */
#define LOCK_TIME_WARNING		60	/* 60 seconds */

#define ACTIVE_ACT "active"
#define INACTIVE_ACT "inactive"

static int received_sleep_cmd = 0;

typedef struct _pm_lock_node {
	pid_t pid;
	Ecore_Timer *timeout_id;
	time_t time;
	bool holdkey_block;
	struct _pm_lock_node *next;
} PmLockNode;

static PmLockNode *cond_head[S_END];

void set_process_active(bool flag, pid_t pid)
{
	char str[6];

	sprintf(str, "%d", (int)pid);
	action_entry_call_internal(
	    (flag ? ACTIVE_ACT : INACTIVE_ACT), 1, str);
}

static int refresh_app_cond()
{
	trans_condition = 0;

	if (cond_head[S_LCDDIM] != NULL)
		trans_condition = trans_condition | MASK_DIM;
	if (cond_head[S_LCDOFF] != NULL)
		trans_condition = trans_condition | MASK_OFF;
	if (cond_head[S_SLEEP] != NULL)
		trans_condition = trans_condition | MASK_SLP;

	return 0;
}

static PmLockNode *find_node(enum state_t s_index, pid_t pid)
{
	PmLockNode *t = cond_head[s_index];

	while (t != NULL) {
		if (t->pid == pid)
			break;
		t = t->next;
	}
	return t;
}

static PmLockNode *add_node(enum state_t s_index, pid_t pid, Ecore_Timer *timeout_id,
		bool holdkey_block)
{
	PmLockNode *n;
	time_t now;

	n = (PmLockNode *) malloc(sizeof(PmLockNode));
	if (n == NULL) {
		_E("Not enough memory, add cond. fail");
		return NULL;
	}

	time(&now);
	n->pid = pid;
	n->timeout_id = timeout_id;
	n->time = now;
	n->holdkey_block = holdkey_block;
	n->next = cond_head[s_index];
	cond_head[s_index] = n;

	refresh_app_cond();
	return n;
}

static int del_node(enum state_t s_index, PmLockNode *n)
{
	PmLockNode *t;
	PmLockNode *prev;

	if (n == NULL)
		return 0;

	t = cond_head[s_index];
	prev = NULL;
	while (t != NULL) {
		if (t == n) {
			if (prev != NULL)
				prev->next = t->next;
			else
				cond_head[s_index] = cond_head[s_index]->next;
			free(t);
			break;
		}
		prev = t;
		t = t->next;
	}
	refresh_app_cond();
	return 0;
}

static void print_node(int next)
{
	PmLockNode *n;
	char buf[30];
	time_t now;
	double diff;

	if (next <= S_START || next >= S_END)
		return;

	time(&now);
	n = cond_head[next];
	while (n != NULL) {
		diff = difftime(now, n->time);
		ctime_r(&n->time, buf);

		if (diff > LOCK_TIME_ERROR)
			_E("over %.0f s, pid: %5d, lock time: %s", diff, n->pid, buf);
		else if (diff > LOCK_TIME_WARNING)
			_I("over %.0f s, pid: %5d, lock time: %s", diff, n->pid, buf);
		else
			_I("pid: %5d, lock time: %s", n->pid, buf);

		n = n->next;
	}
}

static Eina_Bool del_dim_cond(void *data)
{
	PmLockNode *tmp = NULL;
	_I("delete prohibit dim condition by timeout");

	tmp = find_node(S_LCDDIM, (pid_t) data);
	del_node(S_LCDDIM, tmp);

	if (!timeout_src_id)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	return EINA_FALSE;
}

static Eina_Bool del_off_cond(void *data)
{
	PmLockNode *tmp = NULL;
	_I("delete prohibit off condition by timeout");

	tmp = find_node(S_LCDOFF, (pid_t) data);
	del_node(S_LCDOFF, tmp);

	if (!timeout_src_id)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	return EINA_FALSE;
}

static Eina_Bool del_sleep_cond(void *data)
{
	PmLockNode *tmp = NULL;
	_I("delete prohibit sleep condition by timeout");

	tmp = find_node(S_SLEEP, (pid_t) data);
	del_node(S_SLEEP, tmp);

	if (!timeout_src_id)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	set_process_active(EINA_FALSE, (pid_t)data);
	return EINA_FALSE;
}

/* update transition condition for application requrements */
static int proc_condition(PMMsg *data)
{
	PmLockNode *tmp = NULL;
	unsigned int val = data->cond;
	pid_t pid = data->pid;
	Ecore_Timer *cond_timeout_id = NULL;
	bool holdkey_block = 0;
	int val_timeout;

	if (val == 0)
		return 0;
	/* for debug */
	char pname[PATH_MAX];
	char buf[PATH_MAX];
	int fd_cmdline;
	time_t now;

	snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);
	fd_cmdline = open(buf, O_RDONLY);
	if (fd_cmdline < 0) {
		snprintf(pname, PATH_MAX,
			"does not exist now(may be dead without unlock)");
	} else {
		int r;
		r = read(fd_cmdline, pname, PATH_MAX);
		if ((r >= 0) && (r < PATH_MAX))
			pname[r] = '\0';
		else
			pname[0] = '\0';
		close(fd_cmdline);
	}

	if (val & MASK_DIM) {
		if (data->timeout > 0) {
			cond_timeout_id =
			    ecore_timer_add(data->timeout / 1000,
				    (Ecore_Task_Cb)del_dim_cond, (void*)pid);
		}
		holdkey_block = GET_HOLDKEY_BLOCK_STATE(val);
		tmp = find_node(S_LCDDIM, pid);
		if (tmp == NULL)
			add_node(S_LCDDIM, pid, cond_timeout_id, holdkey_block);
		else if (tmp->timeout_id > 0) {
			time(&now);
			ecore_timer_del(tmp->timeout_id);
			tmp->timeout_id = cond_timeout_id;
			tmp->holdkey_block = holdkey_block;
			tmp->time = now;
		}
		/* for debug */
		_I("[%s] locked by pid %d - process %s", "S_NORMAL", pid,
			pname);
	}
	if (val & MASK_OFF) {
		if (data->timeout > 0) {
			cond_timeout_id =
			    ecore_timer_add(data->timeout / 1000,
				    (Ecore_Task_Cb)del_off_cond, (void*)pid);
		}
		holdkey_block = GET_HOLDKEY_BLOCK_STATE(val);
		tmp = find_node(S_LCDOFF, pid);
		if (tmp == NULL)
			add_node(S_LCDOFF, pid, cond_timeout_id, holdkey_block);
		else if (tmp->timeout_id > 0) {
			time(&now);
			ecore_timer_del(tmp->timeout_id);
			tmp->timeout_id = cond_timeout_id;
			tmp->holdkey_block = holdkey_block;
			tmp->time = now;
		}
		/* for debug */
		_I("[%s] locked by pid %d - process %s", "S_LCDDIM", pid,
			pname);
	}
	if (val & MASK_SLP) {
		if (data->timeout > 0) {
			cond_timeout_id =
			    ecore_timer_add(data->timeout / 1000,
				    (Ecore_Task_Cb)del_sleep_cond, (void*)pid);
		}
		tmp = find_node(S_SLEEP, pid);
		if (tmp == NULL)
			add_node(S_SLEEP, pid, cond_timeout_id, 0);
		else if (tmp->timeout_id > 0) {
			time(&now);
			ecore_timer_del(tmp->timeout_id);
			tmp->timeout_id = cond_timeout_id;
			tmp->holdkey_block = 0;
			tmp->time = now;
		}
		set_process_active(EINA_TRUE, pid);

		/* for debug */
		_I("[%s] locked by pid %d - process %s", "S_LCDOFF", pid,
			pname);
	}

	/* UNLOCK(GRANT) condition processing */
	val = val >> SHIFT_UNLOCK;
	if (val & MASK_DIM) {
		tmp = find_node(S_LCDDIM, pid);
		del_node(S_LCDDIM, tmp);
		_I("[%s] unlocked by pid %d - process %s", "S_NORMAL",
			pid, pname);
	}
	if (val & MASK_OFF) {
		tmp = find_node(S_LCDOFF, pid);
		del_node(S_LCDOFF, tmp);
		_I("[%s] unlocked by pid %d - process %s", "S_LCDDIM",
			pid, pname);
	}
	if (val & MASK_SLP) {
		tmp = find_node(S_SLEEP, pid);
		del_node(S_SLEEP, tmp);
		set_process_active(EINA_FALSE, pid);

		_I("[%s] unlocked by pid %d - process %s", "S_LCDOFF",
			pid, pname);
	}
	val = val >> 8;
	if (val != 0) {
		if ((val & 0x1)) {
			reset_timeout(states[pm_cur_state].timeout);
			_I("reset timeout (%d seconds)",
				states[pm_cur_state].timeout);
		}
	} else {
		/* guard time for suspend */
		if (pm_cur_state == S_LCDOFF) {
			reset_timeout(states[S_LCDOFF].timeout);
			_I("margin timeout (%d seconds)",
				states[S_LCDOFF].timeout);
		}
	}

	if (timeout_src_id == 0)
		states[pm_cur_state].trans(EVENT_TIMEOUT);

	return 0;
}

static int proc_change_state(unsigned int cond)
{
	int next_state = 0;
	struct state *st;
	int i;

	for (i = S_NORMAL; i < S_END; i++) {
		if ((cond >> (SHIFT_CHANGE_STATE + i)) & 0x1) {
			next_state = i;
			break;
		}
	}
	_I("Change State to %s", state_string[next_state]);

	switch (next_state) {
	case S_NORMAL:
	case S_LCDDIM:
	case S_LCDOFF:
		/* state transition */
		pm_old_state = pm_cur_state;
		pm_cur_state = next_state;
		st = &states[pm_cur_state];

		/* enter action */
		if (st->action) {
			st->action(st->timeout);
		}
		break;
	case S_SLEEP:
		_I("Dangerous requests.");
		/* at first LCD_OFF and then goto sleep */
		/* state transition */
		pm_old_state = pm_cur_state;
		pm_cur_state = S_LCDOFF;
		st = &states[pm_cur_state];
		if (st->action) {
			st->action(0);
		}
		pm_old_state = pm_cur_state;
		pm_cur_state = S_SLEEP;
		st = &states[pm_cur_state];
		if (st->action) {
			st->action(0);
		}
		break;

	default:
		return -1;
	}

	return 0;
}

/* If some changed, return 1 */
int check_processes(enum state_t prohibit_state)
{
	PmLockNode *t = cond_head[prohibit_state];
	PmLockNode *tmp = NULL;
	int ret = 0;

	while (t != NULL) {
		if (kill(t->pid, 0) == -1) {
			_E("%d process does not exist, delete the REQ"
				" - prohibit state %d ",
				t->pid, prohibit_state);
			tmp = t;
			ret = 1;
		}
		t = t->next;

		if (tmp != NULL) {
			del_node(prohibit_state, tmp);
			tmp = NULL;
		}
	}

	return ret;
}

int check_holdkey_block(enum state_t state)
{
	PmLockNode *t = cond_head[state];
	int ret = 0;

	_I("check holdkey block : state of %s", state_string[state]);

	while (t != NULL) {
		if (t->holdkey_block == true) {
			ret = 1;
			_I("Hold key blocked by pid(%d)!", t->pid);
			break;
		}
		t = t->next;
	}

	return ret;
}

int delete_condition(enum state_t state)
{
	PmLockNode *t = cond_head[state];
	int ret = 0;
	PmLockNode *tmp = NULL;

	_I("delete condition : state of %s", state_string[state]);

	while (t != NULL) {
		if (t->timeout_id > 0) {
			ecore_timer_del(t->timeout_id);
			t->timeout_id = NULL;
		}
		tmp = t;
		t = t->next;
		_I("delete node of pid(%d)", tmp->pid);
		del_node(state, tmp);
	}

	return 0;
}

#ifdef ENABLE_PM_LOG

typedef struct _pm_history {
        time_t time;
        enum pm_log_type log_type;
        int keycode;
} pm_history;

static int max_history_count = MAX_LOG_COUNT;
static pm_history pm_history_log[MAX_LOG_COUNT] = {0,};
static int history_count = 0;

static char history_string[PM_LOG_MAX][15] =
	{"PRESS", "LONG PRESS", "RELEASE", "LCD ON", "LCD ON FAIL",
	"LCD DIM", "LCD DIM FAIL", "LCD OFF", "LCD OFF FAIL", "SLEEP"};

void pm_history_init()
{
	memset(pm_history_log, 0x0, sizeof(pm_history_log));
	history_count = 0;
	max_history_count = MAX_LOG_COUNT;
}

void pm_history_save(enum pm_log_type log_type, int code)
{
        time_t now;

        time(&now);
        pm_history_log[history_count].time = now;
        pm_history_log[history_count].log_type = log_type;
        pm_history_log[history_count].keycode = code;
        history_count++;

        if (history_count >= max_history_count)
                history_count = 0;
}

void pm_history_print(int fd, int count)
{
	int start_index, index, i;
	char buf[255];
	char time_buf[30];

	if (count <= 0 || count > max_history_count)
		return;

	start_index = (history_count - count + max_history_count)
		    % max_history_count;

	for (i = 0; i < count; i++) {
		index = (start_index + i) % max_history_count;

		if (pm_history_log[index].time == 0)
			continue;

		if (pm_history_log[index].log_type < PM_LOG_MIN ||
		    pm_history_log[index].log_type >= PM_LOG_MAX)
			continue;
		ctime_r(&pm_history_log[index].time, time_buf);
		snprintf(buf, sizeof(buf), "[%3d] %15s %3d %s",
			index,
			history_string[pm_history_log[index].log_type],
			pm_history_log[index].keycode,
			time_buf);
		write(fd, buf, strlen(buf));
	}
}
#endif

/* logging indev_list for debug */
void print_dev_list(int fd)
{
	int i;
	unsigned int total = 0;
	indev *tmp;

	total = eina_list_count(indev_list);
	_I("***** total list : %d *****", total);
	for (i = 0; i < total; i++) {
		tmp = (indev*)eina_list_nth(indev_list, i);
		_I("* %d | path:%s, fd:%d, dev_fd:%d",
			i, tmp->dev_path, tmp->fd, tmp->dev_fd);
		if (fd >= 0) {
			char buf[255];
			snprintf(buf, sizeof(buf), " %2d| path:%s, fd:%d, dev_fd:%d\n",
				i, tmp->dev_path, tmp->fd, tmp->dev_fd);
			write(fd, buf, strlen(buf));
		}
	}
	_I("***************************\n");
}

void print_info(int fd)
{
	int s_index = 0;
	char buf[255];
	int i = 1, ret;
	char pname[PATH_MAX];
	int fd_cmdline;

	if (fd < 0)
		return;

	snprintf(buf, sizeof(buf),
		"\n==========================================="
		"===========================\n");
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf),"Timeout Info: Run[%d] Dim[%d] Off[%d]\n",
		 states[S_NORMAL].timeout,
		 states[S_LCDDIM].timeout, states[S_LCDOFF].timeout);
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "Tran. Locked : %s %s %s\n",
		 (trans_condition & MASK_DIM) ? state_string[S_NORMAL] : "-",
		 (trans_condition & MASK_OFF) ? state_string[S_LCDDIM] : "-",
		 (trans_condition & MASK_SLP) ? state_string[S_LCDOFF] : "-");
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "Current State: %s\n",
		state_string[pm_cur_state]);
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "Current Lock Conditions: \n");
	write(fd, buf, strlen(buf));

	for (s_index = S_NORMAL; s_index < S_END; s_index++) {
		PmLockNode *t;
		char time_buf[30];
		t = cond_head[s_index];

		while (t != NULL) {
			snprintf(buf, sizeof(buf), "/proc/%d/cmdline", t->pid);
			fd_cmdline = open(buf, O_RDONLY);
			if (fd_cmdline < 0) {
				snprintf(pname, PATH_MAX,
					"does not exist now"
					"(may be dead without unlock)");
			} else {
				int r;
				r = read(fd_cmdline, pname, PATH_MAX);
				if ((r >= 0) && (r < PATH_MAX))
					pname[r] = '\0';
				else
					pname[0] = '\0';
				close(fd_cmdline);
			}
			ctime_r(&t->time, time_buf);
			snprintf(buf, sizeof(buf),
				 " %d: [%s] locked by pid %d %s %s",
				 i++, state_string[s_index - 1], t->pid, pname, time_buf);
			write(fd, buf, strlen(buf));
			t = t->next;
		}
	}

	print_dev_list(fd);

#ifdef ENABLE_PM_LOG
	pm_history_print(fd, 250);
#endif
}

/* SIGHUP signal handler
 * For debug... print info to syslog
 */
static void sig_hup(int signo)
{
	int fd;
	char buf[255];
	time_t now_time;
	char time_buf[30];

	time(&now_time);
	ctime_r(&now_time, time_buf);

	fd = open(PM_STATE_LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd != -1) {
		snprintf(buf, sizeof(buf),
			"\npm_state_log now-time : %d(s) %s\n\n",
			(int)now_time, time_buf);
		write(fd, buf, strlen(buf));

		snprintf(buf, sizeof(buf), "pm_status_flag: %x\n", pm_status_flag);
		write(fd, buf, strlen(buf));

		snprintf(buf, sizeof(buf), "received sleep cmd count : %d\n",
			 received_sleep_cmd);
		write(fd, buf, strlen(buf));

		print_info(fd);
		close(fd);
	}

	fd = open("/dev/console", O_WRONLY);
	if (fd != -1) {
		print_info(fd);
		close(fd);
	}
}

/* timeout handler  */
Eina_Bool timeout_handler(void *data)
{
	_I("Time out state %s", state_string[pm_cur_state]);

	if (timeout_src_id) {
		ecore_timer_del(timeout_src_id);
		timeout_src_id = NULL;
	}

	if ((pm_status_flag & VCALL_FLAG)
	    && (pm_cur_state == S_LCDOFF || pm_cur_state == S_SLEEP)) {
		pm_status_flag &= ~(VCALL_FLAG);
		reset_timeout(TELEPHONY_SIGNAL_TIMEOUT);
		return EINA_FALSE;
	}
	states[pm_cur_state].trans(EVENT_TIMEOUT);
	return EINA_FALSE;
}

static void reset_timeout(int timeout)
{
	if (timeout_src_id != 0) {
		ecore_timer_del(timeout_src_id);
		timeout_src_id = NULL;
	}
	if (timeout > 0)
		timeout_src_id = ecore_timer_add(timeout,
		    (Ecore_Task_Cb)timeout_handler, NULL);
}

static void sig_usr(int signo)
{
	pm_status_flag |= VCALL_FLAG;
}

/*
 * default transition function
 *   1. call check
 *   2. transition
 *   3. call enter action function
 */
static int default_trans(int evt)
{
	struct state *st = &states[pm_cur_state];
	int next_state;

	if (pm_cur_state == S_NORMAL && st->timeout == 0) {
		_I("LCD always on enabled!");
		return 0;
	}

	next_state = (enum state_t)trans_table[pm_cur_state][evt];

	/* check conditions */
	while (st->check && !st->check(next_state)) {
		/* There is a condition. */
		_I("%s -> %s : check fail", state_string[pm_cur_state],
		       state_string[next_state]);
		if (!check_processes(next_state)) {
			/* this is valid condition - the application that sent the condition is running now. */
			return -1;
		}
	}

	/* state transition */
	pm_old_state = pm_cur_state;
	pm_cur_state = next_state;
	st = &states[pm_cur_state];

	/* enter action */
	if (st->action) {
		if (pm_cur_state == S_LCDDIM && st->timeout == 0) {
			/* dim timeout 0, enter next state directly */
			_I("dim timeout 0, goto LCDOFF state");
			states[pm_cur_state].trans(EVENT_TIMEOUT);
		}
		else {
			st->action(st->timeout);
		}
	}

	return 0;
}

/* default enter action function */
static int default_action(int timeout)
{
	int ret;
	int wakeup_count = -1;
	char buf[NAME_MAX];
	char *pkgname = NULL;
	int i = 0;
	int lock_state = -1;
	struct itimerval val;

	if (status != DEVICE_OPS_STATUS_START) {
		_E("display is not started!");
		return -EINVAL;
	}

	if (pm_cur_state != pm_old_state && pm_cur_state != S_SLEEP) {
		set_setting_pmstate(pm_cur_state);
		device_notify(DEVICE_NOTIFIER_LCD, (void *)pm_cur_state);
	}
	switch (pm_cur_state) {
	case S_NORMAL:
		/*
		 * normal state : backlight on and restore
		 * the previous brightness
		 */
		if (pm_old_state == S_LCDOFF || pm_old_state == S_SLEEP) {
			if (pre_suspend_flag == true) {
				power_ops.post_resume();
				pre_suspend_flag = false;
			}
			for (i = 0; i < LOCK_SCREEN_WATING_MAX_COUNT;
				i++) {
				vconf_get_int(VCONFKEY_IDLE_LOCK_STATE,
					&lock_state);
				_E("Idle lock check: %d, vonf: %d",
					i, lock_state);
				if (lock_state)
					break;
				usleep(LOCK_SCREEN_WATING_TIME);
			}

#ifndef X86 /* Different with ARM FB driver, IA gfx driver is sensitive to the order. */
			backlight_ops.update();
			backlight_ops.on();
#else
			backlight_ops.on();
			backlight_ops.update();
#endif
			key_backlight_enable(true);
		} else if (pm_old_state == S_LCDDIM)
			backlight_ops.update();

		if (backlight_ops.get_lcd_power() != PM_LCD_POWER_ON)
			backlight_ops.on();
		break;

	case S_LCDDIM:
		/* lcd dim state : dim the brightness */
		backlight_ops.dim();
		if (pm_old_state == S_LCDOFF || pm_old_state == S_SLEEP) {
			backlight_ops.on();
		}

		if (backlight_ops.get_lcd_power() != PM_LCD_POWER_ON)
			backlight_ops.on();
		break;

	case S_LCDOFF:
		if (pm_old_state != S_SLEEP && pm_old_state != S_LCDOFF) {
			/* lcd off state : turn off the backlight */
			backlight_ops.off();
			key_backlight_enable(false);

			if (pre_suspend_flag == false) {
				pre_suspend_flag = true;
				power_ops.pre_suspend();
			}
		}

		if (backlight_ops.get_lcd_power() != PM_LCD_POWER_OFF)
			backlight_ops.off();
		break;

	case S_SLEEP:
		/*
		 * We can not always be blocked here because of that
		 * in-progress checking in kernel for wakeup count,
		 * if we cannot get the result within some time, then
		 * just give it up and then retry it in the next time,
		 * this method would not break that original expected
		 * 'aggresive suspend' idea, but at the same time make
		 * the system is 'always' responsible to some keypress
		 * actions like power button press.
		 */
		val.it_value.tv_sec = TOLERANCE_SLOT;
		val.it_value.tv_usec = 0;
		val.it_interval.tv_sec = val.it_interval.tv_usec = 0;
		setitimer(ITIMER_REAL, &val, NULL);

		/* sleep state : set system mode to SUSPEND */
		if (device_get_property(DEVICE_TYPE_POWER,
		    PROP_POWER_WAKEUP_COUNT, &wakeup_count) < 0)
			_E("wakeup count read error");

		if (wakeup_count < 0) {
			_I("Wakup Event! Can not enter suspend mode.");
			goto go_lcd_off;
		}

		if (device_set_property(DEVICE_TYPE_POWER,
		    PROP_POWER_WAKEUP_COUNT, wakeup_count) < 0) {
			_E("wakeup count write error");
			goto go_lcd_off;
		}
		goto go_suspend;
	}

	/* set timer with current state timeout */
	reset_timeout(timeout);
	_I("timout set: %s state %d sec",
		state_string[pm_cur_state], timeout);

	return 0;

go_suspend:
#ifdef ENABLE_PM_LOG
	pm_history_save(PM_LOG_SLEEP, pm_cur_state);
#endif
	power_ops.suspend();
	_I("system wakeup!!");
	system_wakeup_flag = true;
	heynoti_publish(PM_WAKEUP_NOTI_NAME);
	/* Resume !! */
	if (power_ops.check_wakeup_src() == EVENT_DEVICE)
		/* system waked up by devices */
		states[pm_cur_state].trans(EVENT_DEVICE);
	else
		/* system waked up by user input */
		states[pm_cur_state].trans(EVENT_INPUT);
	return 0;

go_lcd_off:
	heynoti_publish(PM_WAKEUP_NOTI_NAME);
	/* Resume !! */
	states[pm_cur_state].trans(EVENT_DEVICE);
	return 0;
}

/*
 * default check function
 *   return
 *    0 : can't transit, others : transitable
 */
static int default_check(int next)
{
	int trans_cond = trans_condition & MASK_BIT;
	int lock_state = -1;

	vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
	if (lock_state==VCONFKEY_IDLE_LOCK && next != S_SLEEP) {
		_I("default_check:LOCK STATE, it's transitable");
		return 1;
	}

	switch (next) {
	case S_LCDDIM:
		trans_cond = trans_cond & MASK_DIM;
		break;
	case S_LCDOFF:
		trans_cond = trans_cond & MASK_OFF;
		break;
	case S_SLEEP:
		trans_cond = trans_cond & MASK_SLP;
		break;
	default:		/* S_NORMAL is exceptional */
		trans_cond = 0;
		break;
	}

	if (trans_cond != 0) {
		print_node(next);
		return 0;
	}

	return 1;		/* transitable */
}

/* get configurations from setting */
static int get_lcd_timeout_from_settings(void)
{
	int i;
	int val = 0;
	int ret = -1;
	char buf[255];

	for (i = 0; i < S_END; i++) {
		switch (states[i].state) {
		case S_NORMAL:
			ret = get_run_timeout(&val);
			if (ret != 0) {
				ret = get_env("PM_TO_NORMAL", buf, 255);
				val = atoi(buf);
			}
			break;
		case S_LCDDIM:
			ret = get_dim_timeout(&val);
			break;
		case S_LCDOFF:
			ret = get_off_timeout(&val);
			break;
		default:
			/* This state doesn't need to set time out. */
			ret = -1;
			break;
		}
		if (ret == 0 || val < 0) {
			states[i].timeout = val;
		} else {
			switch (states[i].state) {
			case S_NORMAL:
				states[i].timeout = DEFAULT_NORMAL_TIMEOUT;
				break;
			case S_LCDDIM:
				states[i].timeout = DEFAULT_DIM_TIMEOUT;
				break;
			case S_LCDOFF:
				states[i].timeout = DEFAULT_OFF_TIMEOUT;
				break;
			default:
				states[i].timeout = 0;
				break;
			}
		}
		_I("%s state : %d timeout", state_string[i],
			states[i].timeout);
	}

	return 0;
}

static void default_saving_mode(int onoff)
{
	if (onoff) {
		pm_status_flag |= PWRSV_FLAG;
	} else {
		pm_status_flag &= ~PWRSV_FLAG;
	}
	if (pm_cur_state == S_NORMAL)
		backlight_ops.update();
}

static int poll_callback(int condition, PMMsg *data)
{
	time_t now;

	if (condition == INPUT_POLL_EVENT) {
		if (pm_cur_state == S_LCDOFF || pm_cur_state == S_SLEEP)
			_I("Power key input");
		time(&now);
		if (last_t != now) {
			states[pm_cur_state].trans(EVENT_INPUT);
			last_t = now;
		}
	} else if (condition == PM_CONTROL_EVENT) {
		_I("process pid(%d) pm_control condition : %x ",
			data->pid, data->cond);

		if (data->cond & MASK_BIT
			|| (data->cond >> SHIFT_UNLOCK) & MASK_BIT)
			proc_condition(data);

		if (data->cond & CHANGE_STATE_BIT) {
			_I("Change state by pid(%d) request.", data->pid);
			proc_change_state(data->cond);
		}
	}

	return 0;
}

static int update_setting(int key_idx, int val)
{
	char buf[PATH_MAX];
	int ret = -1;
	int dim_timeout = -1;
	int run_timeout = -1;
	int power_saving_stat = -1;
	int power_saving_display_stat = -1;

	switch (key_idx) {
	case SETTING_TO_NORMAL:
		ret = get_dim_timeout(&dim_timeout);
		if (ret < 0 || dim_timeout < 0) {
			_E("Can not get dim timeout.set default %d seconds",
			    DEFAULT_DIM_TIMEOUT);
			dim_timeout = DEFAULT_DIM_TIMEOUT;
		}
		if (val <= 0) {
			_E("LCD timeout is wrong(%d), set default %d seconds",
			    val, DEFAULT_NORMAL_TIMEOUT);
			val = DEFAULT_NORMAL_TIMEOUT;
		}

		if (val > dim_timeout) {
			states[S_NORMAL].timeout = val - dim_timeout;
		} else {
			states[S_NORMAL].timeout = 1;
		}
		states[pm_cur_state].trans(EVENT_INPUT);
		break;
	case SETTING_LOW_BATT:
		if (val <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW &&
			val >= VCONFKEY_SYSMAN_BAT_POWER_OFF ) {
			if (!(pm_status_flag & CHRGR_FLAG))
				power_saving_func(true);
			pm_status_flag |= LOWBT_FLAG;
		} else {
			if (pm_status_flag & PWRSV_FLAG)
				power_saving_func(false);
			pm_status_flag &= ~LOWBT_FLAG;
			pm_status_flag &= ~BRTCH_FLAG;
			vconf_set_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM,
			    false);
		}
		break;
	case SETTING_CHARGING:
		if (val) {
			if (pm_status_flag & LOWBT_FLAG) {
				power_saving_func(false);
				pm_status_flag &= ~LOWBT_FLAG;
			}
			pm_status_flag |= CHRGR_FLAG;
		} else {
			int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
			vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW,
				&bat_state);
			if (bat_state <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW &&
				bat_state >= VCONFKEY_SYSMAN_BAT_POWER_OFF ) {
				power_saving_func(true);
				pm_status_flag |= LOWBT_FLAG;
			}
			pm_status_flag &= ~CHRGR_FLAG;
		}
		break;
	case SETTING_BRT_LEVEL:
		if (pm_status_flag & PWRSV_FLAG) {
			pm_status_flag |= BRTCH_FLAG;
			vconf_set_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM,
			    true);
			_I("brightness changed in low battery,"
				"escape dim state");
		}
		backlight_ops.set_default_brt(val);
		snprintf(buf, sizeof(buf), "%s %d",
			SET_BRIGHTNESS_IN_BOOTLOADER, val);
		_I("Brightness set in bl : %d",val);
		system(buf);
		break;
	case SETTING_LOCK_SCREEN:
		if (val == VCONFKEY_IDLE_LOCK) {
			set_lock_screen_state(val);
			states[S_NORMAL].timeout = LOCK_SCREEN_TIMEOUT;
			states[S_LCDDIM].timeout = 0;
			_E("LOCKED: NORMAL timeout is set by %d seconds",
				LOCK_SCREEN_TIMEOUT);
			key_backlight_enable(false);
		} else {
			set_lock_screen_state(val);
			get_run_timeout(&run_timeout);
			if (run_timeout < 0) {
				_E("Can not get run timeout."
				    "set default %d seconds",
				    DEFAULT_NORMAL_TIMEOUT);
				run_timeout = DEFAULT_NORMAL_TIMEOUT -
				    DEFAULT_DIM_TIMEOUT;
			}
			states[S_NORMAL].timeout = run_timeout;
			states[S_LCDDIM].timeout = DEFAULT_DIM_TIMEOUT;
			_I("UNLOCKED: NORMAL timeout is set by"
				" %d seconds", run_timeout);
		}
		if (pm_cur_state == S_NORMAL) {
			states[pm_cur_state].trans(EVENT_INPUT);
		}
		break;
	case SETTING_LOCK_SCREEN_BG:
		set_lock_screen_bg_state(val);
		if (pm_cur_state == S_NORMAL) {
			states[pm_cur_state].trans(EVENT_INPUT);
		}
		break;
	case SETTING_POWER_SAVING:
		if (val == 1)
			vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_DISPLAY,
				 &power_saving_display_stat);
		if (power_saving_display_stat != 1)
			power_saving_display_stat = 0;
		set_power_saving_display_stat(power_saving_display_stat);
		if (device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_FRAME_RATE,
		    power_saving_display_stat) < 0) {
			_E("Fail to set display frame rate!");
		}
		backlight_ops.update();
		break;
	case SETTING_POWER_SAVING_DISPLAY:
		vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_SYSMODE_STATUS,
			&power_saving_stat);
		if (power_saving_stat == 1) {
			if (val == 1)
				power_saving_display_stat = 1;
			else
				power_saving_display_stat = 0;
			set_power_saving_display_stat(power_saving_display_stat);
			if (device_set_property(DEVICE_TYPE_DISPLAY,
			    PROP_DISPLAY_FRAME_RATE, power_saving_display_stat) < 0) {
				_E("Fail to set display frame rate!");
			}
			backlight_ops.update();
		}
		break;
	case SETTING_POWEROFF:
		switch (val) {
		case VCONFKEY_SYSMAN_POWER_OFF_NONE:
		case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
			pm_status_flag &= ~PWROFF_FLAG;
			break;
		case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
		case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
			pm_status_flag |= PWROFF_FLAG;
			break;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static void check_seed_status(void)
{
	int ret = -1;
	int tmp = 0;
	int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
	int max_brt = 0;
	int brt = 0;
	int lock_state = -1;
	int power_saving_stat = -1;
	int power_saving_display_stat = -1;
	Ecore_Timer *timeout = NULL;

	/* Charging check */
	if ((get_charging_status(&tmp) == 0) && (tmp > 0)) {
		pm_status_flag |= CHRGR_FLAG;
	}

	ret = get_setting_brightness(&tmp);
	if (ret != 0 || (tmp < PM_MIN_BRIGHTNESS || tmp > PM_MAX_BRIGHTNESS)) {
		_I("fail to read vconf value for brightness");
		brt = PM_DEFAULT_BRIGHTNESS;
		if (tmp < PM_MIN_BRIGHTNESS || tmp > PM_MAX_BRIGHTNESS)
			vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt);
		tmp = brt;
	}
	_I("Set brightness from Setting App. %d", tmp);
	backlight_ops.set_default_brt(tmp);

	vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state);
	if (bat_state <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW &&
		bat_state >= VCONFKEY_SYSMAN_BAT_POWER_OFF) {
		if (!(pm_status_flag & CHRGR_FLAG)) {
			power_saving_func(true);
			pm_status_flag |= LOWBT_FLAG;
		}
	}
	backlight_ops.update();

	/* USB connection check
	 * If connected, add sleep prohibit condition */
	if ((get_usb_status(&tmp) == 0) && (tmp > 0)) {
		tmp = readpid(USB_CON_PIDFILE);
		if (tmp != -1) {
			add_node(S_SLEEP, tmp, timeout, 0);
		}
	}

	/* lock screen check */
	ret = vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
	set_lock_screen_state(lock_state);
	if (lock_state == VCONFKEY_IDLE_LOCK) {
		states[S_NORMAL].timeout = LOCK_SCREEN_TIMEOUT;
		states[S_LCDDIM].timeout = 0;
		_E("LCD NORMAL timeout is set by %d seconds"
			" for lock screen", LOCK_SCREEN_TIMEOUT);
	}

	/* power saving display stat */
	vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_SYSMODE_STATUS,
	    &power_saving_stat);
	if (power_saving_stat <= 0) {
		power_saving_display_stat = 0;
	} else {
		vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_DISPLAY,
		    &power_saving_display_stat);
	}
	if (power_saving_display_stat < 0) {
		_E("failed to read power saving display stat!");
	} else {
		_I("power saving display stat : %d",
		    power_saving_display_stat);
		set_power_saving_display_stat(power_saving_display_stat);
	}

	return;
}

enum {
	INIT_SETTING = 0,
	INIT_INTERFACE,
	INIT_POLL,
	INIT_FIFO,
	INIT_DBUS,
	INIT_END
};

static char *errMSG[INIT_END] = {
	[INIT_SETTING] = "setting init error",
	[INIT_INTERFACE] = "lowlevel interface(sysfs or others) init error",
	[INIT_POLL] = "input devices poll init error",
	[INIT_FIFO] = "FIFO poll init error",
	[INIT_DBUS] = "d-bus init error",
};

static int input_action(char* input_act, char* input_path)
{
	int ret = -1;
	Eina_List *list_node = NULL;
	Eina_List *l = NULL;
	Eina_List *l_next = NULL;
	Ecore_Timer *timeout = NULL;
	indev *data = NULL;
	PmLockNode *tmp = NULL;

	if (!strcmp("add", input_act)) {
		_I("add input path : %s", input_path);
		ret = init_pm_poll_input(poll_callback, input_path);
	} else if (!strcmp("remove", input_act)) {
		EINA_LIST_FOREACH_SAFE(indev_list, l, l_next, data)
			if(!strcmp(input_path, data->dev_path)) {
				_I("remove %s", input_path);
				ecore_main_fd_handler_del(data->dev_fd);
				close(data->fd);
				free(data->dev_path);
				free(data);
				indev_list = eina_list_remove_list(indev_list, l);
			}
		ret = 0;
	} else if (!strcmp("change", input_act)) {
		if (!strcmp("ESD", input_path)) {
			_I("ESD on");
			if (pm_cur_state == S_NORMAL) {
				backlight_ops.off();
				backlight_ops.on();
			} else if (pm_cur_state == S_LCDDIM) {
				backlight_ops.off();
				backlight_ops.dim();
			}
			ret = 0;
		}
		ret = 0;
	} else if (!strcmp("lock", input_act)) {
		if (!strcmp("lcdoff", input_path)) {
			tmp = find_node(S_SLEEP, 1);
			if (!tmp && add_node(S_SLEEP, 1, timeout, 0) != NULL)
				_I("lock LCD OFF from pm_event");
		}
		ret = 0;
	} else if (!strcmp("unlock", input_act)) {
		if (!strcmp("lcdoff", input_path)) {
			tmp = find_node(S_SLEEP, 1);
			if (tmp != NULL) {
				del_node(S_SLEEP, tmp);
				_I("unlock LCD OFF from pm_event");
			}
		}
		ret = 0;
	}
	return ret;
}

static void input_cb(void* data)
{
	FILE *fp;
	char input_act[NAME_MAX], input_path[MAX_INPUT];
	char args[NAME_MAX + MAX_INPUT];
	int i, ret = -1;

	fp = fopen((char *) data, "r");
	if (fp == NULL) {
		_E("input file open fail");
		return ;
	}

	while (fgets(args, NAME_MAX + MAX_INPUT, fp) != NULL) {
		if (args[strlen(args)-1] != '\n') {
			_E("input file must be terminated with"
				" new line character\n");
			break;
		}
		args[strlen(args) - 1] = '\0';
		for (i = 0; i< NAME_MAX + MAX_INPUT; i++) {
			if (args[i] == ' ') {
				if (i >= NAME_MAX) {
					_E("bsfile name is over"
						" the name_max(255)\n");
					break;
				}
				strncpy(input_act, args,
					i < NAME_MAX ? i : NAME_MAX);
				input_act[i]='\0';
				strncpy(input_path, args + i + 1, MAX_INPUT);
				input_path[MAX_INPUT - 1] = '\0';
				ret = input_action(input_act, input_path);

				/* debug */
				print_dev_list(-1);
				break;
			}
		}
		if (ret < 0)
			break;
	}
	fclose(fp);

	if (ret != -1) {
		fp = fopen((char *) data, "w");
		if (fp == NULL) {
			return ;
		}
		fclose(fp);
	}
	return ;
}

static int set_noti(int *noti_fd)
{
	int fd;
	char buf[PATH_MAX];

	fd = heynoti_init();
	if (fd < 0) {
		_E("heynoti_init error");
		return -1;
	}

	if (heynoti_subscribe(fd, PM_EVENT_NOTI_NAME,
	    input_cb, PM_EVENT_NOTI_PATH) < 0) {
		_E("input file change noti add failed(%s). %s",
		    buf, strerror(errno));
		return -1;
	} else {
		_E("input file change noti add ok");
	}

	if (heynoti_attach_handler(fd) < 0) {
		_E("heynoti_attach_handler error");
		return -1;
	}

	*noti_fd = fd;
	return 0;
}

static int unset_noti(int noti_fd)
{
	if (noti_fd < 0) {
		_I("set noti already failed. nothing to do in unset");
		return 0;
	}

	if (heynoti_unsubscribe(noti_fd, PM_EVENT_NOTI_NAME, input_cb) < 0
		|| heynoti_detach_handler(noti_fd) < 0) {
		_E("heynoti unsubsribe or detach error");
		return -1;
	}
	return 0;
}

static int noti_fd = -1;

static int input_device_add(void *data)
{
	char *path = (char *)data;

	if (!path)
		return -EINVAL;

	input_action(ADD, path);

	return 0;
}

static int input_device_remove(void *data)
{
	char *path = (char *)data;

	if (!path)
		return -EINVAL;

	input_action(REMOVE, path);

	return 0;
}

static int process_terminated(void *data)
{
	const char *name = data;

	_I("%s is terminated!", name);
	reset_autobrightness_min(name);

	return 0;
}

/**
 * Power manager Main
 *
 */
static void display_init(void *data)
{
	int ret, i;
	unsigned int flags = (WITHOUT_STARTNOTI | FLAG_X_DPMS);
	int timeout = 0;

	_I("Start power manager");

	signal(SIGHUP, sig_hup);

	power_saving_func = default_saving_mode;
	/* noti init for new input device like bt mouse */
	indev_list = NULL;
	set_noti(&noti_fd);

	register_notifier(DEVICE_NOTIFIER_INPUT_ADD, input_device_add);
	register_notifier(DEVICE_NOTIFIER_INPUT_REMOVE, input_device_remove);
	register_notifier(DEVICE_NOTIFIER_PROCESS_TERMINATED, process_terminated);

	for (i = INIT_SETTING; i < INIT_END; i++) {
		switch (i) {
		case INIT_SETTING:
			ret = init_setting(update_setting);
			break;
		case INIT_INTERFACE:
			get_lcd_timeout_from_settings();
			ret = init_sysfs(flags);
			break;
		case INIT_POLL:
			_I("poll init");
			ret = init_pm_poll(poll_callback);
			break;
		case INIT_DBUS:
			_I("dbus init");
			ret = init_pm_dbus();
			break;
		}
		if (ret != 0) {
			_E("%s", errMSG[i]);
			break;
		}
	}

	if (i == INIT_END) {
#ifdef ENABLE_PM_LOG
		pm_history_init();
#endif
		check_seed_status();

		if (pm_init_extention != NULL)
			pm_init_extention(NULL);

		if (flags & WITHOUT_STARTNOTI) {	/* start without noti */
			_I("Start Power managing without noti");
			pm_cur_state = S_NORMAL;
			set_setting_pmstate(pm_cur_state);

			timeout = states[S_NORMAL].timeout;
			/* check minimun lcd on time */
			if (timeout < DEFAULT_NORMAL_TIMEOUT)
				timeout = DEFAULT_NORMAL_TIMEOUT;

			reset_timeout(timeout);
		}
		start_battinfo_gathering(30);
	}
	status = DEVICE_OPS_STATUS_START;
}

static void display_exit(void *data)
{
	int i = INIT_END;

	status = DEVICE_OPS_STATUS_STOP;
	end_battinfo_gathering();

	for (i = i - 1; i >= INIT_SETTING; i--) {
		switch (i) {
		case INIT_SETTING:
			exit_setting();
			break;
		case INIT_INTERFACE:
			exit_sysfs();
			break;
		case INIT_POLL:
			unregister_notifier(DEVICE_NOTIFIER_INPUT_ADD,
			    input_device_add);
		        unregister_notifier(DEVICE_NOTIFIER_INPUT_REMOVE,
			    input_device_remove);
			unregister_notifier(DEVICE_NOTIFIER_PROCESS_TERMINATED,
			    process_terminated);
			unset_noti(noti_fd);
			exit_pm_poll();
			break;
		}
	}

	if (pm_exit_extention != NULL)
		pm_exit_extention();

	_I("Stop power manager");
}

static int display_status(void)
{
	return status;
}

const struct device_ops display_device_ops = {
	.priority = DEVICE_PRIORITY_HIGH,
	.name     = "display",
	.init = display_init,
	.exit = display_exit,
	.status = display_status,
};

DEVICE_OPS_REGISTER(&display_device_ops)

/**
 * @}
 */
