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
 * @file	poll.c
 * @brief	 Power Manager poll implementation
 *
 */

#include <stdio.h>
#include "util.h"
#include "core.h"
#include "poll.h"

#define SHIFT_UNLOCK                    4
#define SHIFT_UNLOCK_PARAMETER          12
#define SHIFT_CHANGE_STATE              8
#define SHIFT_CHANGE_TIMEOUT            20
#define LOCK_FLAG_SHIFT                 16
#define __HOLDKEY_BLOCK_BIT              0x1
#define HOLDKEY_BLOCK_BIT               (__HOLDKEY_BLOCK_BIT << LOCK_FLAG_SHIFT)

PMMsg recv_data;

int check_dimstay(int next_state, int flag)
{
	if (next_state != LCD_OFF)
		return false;

	if (!(flag & GOTO_STATE_NOW))
		return false;

	if (!(pm_status_flag & DIMSTAY_FLAG))
		return false;

	return true;
}

static enum state_t get_state(int s_bits)
{
	switch (s_bits) {
	case LCD_NORMAL:
		return S_NORMAL;
	case LCD_DIM:
		return S_LCDDIM;
	case LCD_OFF:
		return S_LCDOFF;
	case STANDBY:
		return S_STANDBY;
	case SUSPEND:
		return S_SLEEP;
	default:
		return -EINVAL;
	}
}

int pm_lock_internal(pid_t pid, int s_bits, int flag, int timeout)
{
	int cond;

	if (!pm_callback)
		return -1;

	cond = get_state(s_bits);
	if (cond < 0)
		return cond;

	cond = SET_COND_REQUEST(cond, PM_REQUEST_LOCK);

	if (flag & GOTO_STATE_NOW)
		/* if the flag is true, go to the locking state directly */
		cond = SET_COND_FLAG(cond, PM_REQUEST_CHANGE);

	if (flag & HOLD_KEY_BLOCK)
		cond = SET_COND_FLAG(cond, PM_FLAG_BLOCK_HOLDKEY);

	recv_data.pid = pid;
	recv_data.cond = cond;
	recv_data.timeout = timeout;

	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

int pm_unlock_internal(pid_t pid, int s_bits, int flag)
{
	int cond;

	if (!pm_callback)
		return -1;

	cond = get_state(s_bits);
	if (cond < 0)
		return cond;

	cond = SET_COND_REQUEST(cond, PM_REQUEST_UNLOCK);

	if (flag & PM_KEEP_TIMER)
		cond = SET_COND_FLAG(cond, PM_FLAG_KEEP_TIMER);

	if (flag & PM_RESET_TIMER)
		cond = SET_COND_FLAG(cond, PM_FLAG_RESET_TIMER);

	recv_data.pid = pid;
	recv_data.cond = cond;
	recv_data.timeout = 0;

	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

int pm_change_internal(pid_t pid, int s_bits)
{
	int cond;

	if (!pm_callback)
		return -1;

	cond = get_state(s_bits);
	if (cond < 0)
		return cond;

	cond = SET_COND_REQUEST(cond, PM_REQUEST_CHANGE);

	recv_data.pid = pid;
	recv_data.cond = cond;
	recv_data.timeout = 0;

	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}
