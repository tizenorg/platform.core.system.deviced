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
#define __STANDBY_MODE_BIT               0x2
#define HOLDKEY_BLOCK_BIT               (__HOLDKEY_BLOCK_BIT << LOCK_FLAG_SHIFT)
#define STANDBY_MODE_BIT                (__STANDBY_MODE_BIT << LOCK_FLAG_SHIFT)


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

int pm_lock_internal(pid_t pid, int s_bits, int flag, int timeout)
{
	if (!pm_callback)
		return -1;

	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}
	if (flag & GOTO_STATE_NOW)
		/* if the flag is true, go to the locking state directly */
		s_bits = s_bits | (s_bits << SHIFT_CHANGE_STATE);

	if (flag & HOLD_KEY_BLOCK)
		s_bits = s_bits | HOLDKEY_BLOCK_BIT;

	if (flag & STANDBY_MODE)
		s_bits = s_bits | STANDBY_MODE_BIT;

	recv_data.pid = pid;
	recv_data.cond = s_bits;
	recv_data.timeout = timeout;

	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

int pm_unlock_internal(pid_t pid, int s_bits, int flag)
{
	if (!pm_callback)
		return -1;

	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}

	s_bits = (s_bits << SHIFT_UNLOCK);
	s_bits = (s_bits | (flag << SHIFT_UNLOCK_PARAMETER));

	recv_data.pid = pid;
	recv_data.cond = s_bits;

	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

int pm_change_internal(pid_t pid, int s_bits)
{
	if (!pm_callback)
		return -1;

	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
	case SUSPEND:
		break;
	default:
		return -1;
	}

	recv_data.pid = pid;
	recv_data.cond = s_bits << SHIFT_CHANGE_STATE;

	(*pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

