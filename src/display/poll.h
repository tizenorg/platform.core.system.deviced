/*
 * deviced
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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


/**
 * @file	poll.h
 * @brief	Power Manager input device poll implementation
 *
 */

#ifndef __PM_POLL_H__
#define __PM_POLL_H__

#include <Ecore.h>
#include "core/edbus-handler.h"
/**
 * @addtogroup POWER_MANAGER
 * @{
 */

enum {
	INPUT_POLL_EVENT = -9,
	SIDEKEY_POLL_EVENT,
	PWRKEY_POLL_EVENT,
	PM_CONTROL_EVENT,
};

enum {
	INTERNAL_LOCK_BASE = 100000,
	INTERNAL_LOCK_BATTERY,
	INTERNAL_LOCK_BATTERY_FULL,
	INTERNAL_LOCK_BOOTING,
	INTERNAL_LOCK_DUMPMODE,
	INTERNAL_LOCK_HDMI,
	INTERNAL_LOCK_ODE,
	INTERNAL_LOCK_POPUP,
	INTERNAL_LOCK_SOUNDDOCK,
	INTERNAL_LOCK_TIME,
	INTERNAL_LOCK_USB,
	INTERNAL_LOCK_POWEROFF,
	INTERNAL_LOCK_COOL_DOWN,
	INTERNAL_LOCK_LOWBAT,
};

#define SIGNAL_NAME_LCD_CONTROL		"lcdcontol"

#define LCD_NORMAL  0x01	/**< NORMAL state */
#define LCD_DIM     0x02	/**< LCD dimming state */
#define LCD_OFF     0x04	/**< LCD off state */
#define SUSPEND     0x08	/**< Suspend state */
#define POWER_OFF   0x10	/**< Sleep state */
#define STANDBY     0x20	/**< Standby state */


#define STAY_CUR_STATE	0x1
#define GOTO_STATE_NOW	0x2
#define HOLD_KEY_BLOCK  0x4

#define PM_SLEEP_MARGIN	0x0	/**< keep guard time for unlock */
#define PM_RESET_TIMER	0x1	/**< reset timer for unlock */
#define PM_KEEP_TIMER		0x2	/**< keep timer for unlock */

/**
 * display lock condition (unsigned integer)
 *
 *     xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
 *     reserved  flags   request   state
 *
 */

enum cond_request_e {
	PM_REQUEST_LOCK     = 1 << 0,
	PM_REQUEST_UNLOCK   = 1 << 1,
	PM_REQUEST_CHANGE   = 1 << 2,
};

enum cond_flags_e {
	PM_FLAG_BLOCK_HOLDKEY  = 1 << 0,
	PM_FLAG_RESET_TIMER    = 1 << 1,
	PM_FLAG_KEEP_TIMER     = 1 << 2,
};

#define SHIFT_STATE            0
#define SHIFT_REQUEST          8
#define SHIFT_FLAGS            16
#define COND_MASK              0xff /* 11111111 */
#define SET_COND_REQUEST(cond, req)      ((cond) | ((req) << SHIFT_REQUEST))
#define SET_COND_FLAG(cond, flags)       ((cond) | ((flags) << SHIFT_FLAGS))
#define IS_COND_REQUEST_LOCK(cond)       (((cond) >> SHIFT_REQUEST) & COND_MASK & PM_REQUEST_LOCK)
#define IS_COND_REQUEST_UNLOCK(cond)     (((cond) >> SHIFT_REQUEST) & COND_MASK & PM_REQUEST_UNLOCK)
#define IS_COND_REQUEST_CHANGE(cond)     (((cond) >> SHIFT_REQUEST) & COND_MASK & PM_REQUEST_CHANGE)
#define GET_COND_STATE(cond)             ((cond) & COND_MASK)
#define GET_COND_FLAG(cond)              (((cond) >> SHIFT_FLAGS) & COND_MASK)


#define PM_LOCK_STR	"lock"
#define PM_UNLOCK_STR	"unlock"
#define PM_CHANGE_STR	"change"

#define PM_LCDOFF_STR	"lcdoff"
#define PM_LCDDIM_STR	"lcddim"
#define PM_LCDON_STR	"lcdon"
#define PM_STANDBY_STR	"standby"
#define PM_SUSPEND_STR	"suspend"

#define STAYCURSTATE_STR "staycurstate"
#define GOTOSTATENOW_STR "gotostatenow"

#define HOLDKEYBLOCK_STR "holdkeyblock"
#define STANDBYMODE_STR  "standbymode"

#define SLEEP_MARGIN_STR "sleepmargin"
#define RESET_TIMER_STR  "resettimer"
#define KEEP_TIMER_STR   "keeptimer"

typedef struct {
	pid_t pid;
	unsigned int cond;
	unsigned int timeout;
	unsigned int timeout2;
} PMMsg;

PMMsg recv_data;
int (*pm_callback) (int, PMMsg *);

int init_input(void);
int exit_input(void);

extern int pm_lock_internal(pid_t pid, int s_bits, int flag, int timeout);
extern int pm_unlock_internal(pid_t pid, int s_bits, int flag);
extern int pm_change_internal(pid_t pid, int s_bits);

/**
 * @}
 */

#endif				/*__PM_POLL_H__ */
