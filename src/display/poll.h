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
 * This file includes the input device poll implementation.
 * Default input devices are /dev/event0 and /dev/event1
 * User can use "PM_INPUT_DEV" for setting another input device poll in an environment file (/etc/profile).
 * (ex: PM_INPUT_DEV=/dev/event0:/dev/event1:/dev/event5 )
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
	INTERNAL_LOCK_BOOTING,
	INTERNAL_LOCK_DUMPMODE,
	INTERNAL_LOCK_HDMI,
	INTERNAL_LOCK_ODE,
	INTERNAL_LOCK_POPUP,
	INTERNAL_LOCK_SOUNDDOCK,
	INTERNAL_LOCK_TA,
	INTERNAL_LOCK_TIME,
	INTERNAL_LOCK_USB,
	INTERNAL_LOCK_POWEROFF,
	INTERNAL_LOCK_COOL_DOWN,
};

#define SIGNAL_NAME_LCD_CONTROL		"lcdcontol"

#define LCD_NORMAL	0x1		/**< NORMAL state */
#define LCD_DIM		0x2		/**< LCD dimming state */
#define LCD_OFF		0x4		/**< LCD off state */
#define SUSPEND		0x8		/**< Suspend state */
#define POWER_OFF	0x16	/**< Sleep state */

#define STAY_CUR_STATE	0x1
#define GOTO_STATE_NOW	0x2
#define HOLD_KEY_BLOCK  0x4
#define STANDBY_MODE    0x8

#define PM_SLEEP_MARGIN	0x0	/**< keep guard time for unlock */
#define PM_RESET_TIMER	0x1	/**< reset timer for unlock */
#define PM_KEEP_TIMER		0x2	/**< keep timer for unlock */

#define PM_LOCK_STR	"lock"
#define PM_UNLOCK_STR	"unlock"
#define PM_CHANGE_STR	"change"

#define PM_LCDOFF_STR	"lcdoff"
#define PM_LCDDIM_STR	"lcddim"
#define PM_LCDON_STR	"lcdon"
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

typedef struct {
	char *dev_path;
	int fd;
	Ecore_Fd_Handler *dev_fd;
	int pre_install;
} indev;

Eina_List *indev_list;

PMMsg recv_data;
int (*g_pm_callback) (int, PMMsg *);

extern int init_pm_poll(int (*pm_callback) (int, PMMsg *));
extern int exit_pm_poll();
extern int init_pm_poll_input(int (*pm_callback)(int , PMMsg * ), const char *path);

extern int pm_lock_internal(pid_t pid, int s_bits, int flag, int timeout);
extern int pm_unlock_internal(pid_t pid, int s_bits, int flag);
extern int pm_change_internal(pid_t pid, int s_bits);

/**
 * @}
 */

#endif				/*__PM_POLL_H__ */
