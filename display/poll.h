/*
 * power-manager
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#define SOCK_PATH "/tmp/pm_sock"

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
} indev;

Eina_List *indev_list;

PMMsg recv_data;
int (*g_pm_callback) (int, PMMsg *);

extern int init_pm_poll(int (*pm_callback) (int, PMMsg *));
extern int exit_pm_poll();
extern int init_pm_poll_input(int (*pm_callback)(int , PMMsg * ), const char *path);

/**
 * @}
 */

#endif				/*__PM_POLL_H__ */
