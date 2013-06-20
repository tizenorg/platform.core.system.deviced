/*
 *  libslp-pm
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: DongGi Jang <dg0402.jang@samsung.com>
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
 *
*/ 


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <linux/limits.h>

#include "pmapi.h"
#include "pm.h"

#define SOCK_PATH			"/tmp/pm_sock"
#define SHIFT_UNLOCK			4
#define SHIFT_UNLOCK_PARAMETER		12
#define SHIFT_CHANGE_STATE		8
#define SHIFT_HOLD_KEY_BLOCK		16
#define TIMEOUT_RESET_BIT		0x80

struct pwr_msg {
	pid_t pid;
	unsigned int cond;
	unsigned int timeout;
};

static int send_msg(unsigned int s_bits, unsigned int timeout)
{
	int rc = 0;
	int sock;
	struct pwr_msg p;
	struct sockaddr_un remote;

	p.pid = getpid();
	p.cond = s_bits;
	p.timeout = timeout;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock == -1) {
		ERR("pm socket() failed");
		return -1;
	}

	remote.sun_family = AF_UNIX;
	if(strlen(SOCK_PATH) >= sizeof(remote.sun_path)) {
		ERR("socket path is vey long");
		return -1;
	}
	strncpy(remote.sun_path, SOCK_PATH, sizeof(remote.sun_path));

	rc = sendto(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote,
		    sizeof(struct sockaddr_un));
	if (rc == -1) {
		ERR("pm socket sendto() failed");
	} else
		rc = 0;

	close(sock);
	return rc;
}

API int pm_change_state(unsigned int s_bits)
{
	/* s_bits is LCD_NORMAL 0x1, LCD_DIM 0x2, LCD_OFF 0x4, SUSPEND 0x8
	 * Stage change to NORMAL       0x100
	 * Stage change to LCDDIM       0x200
	 * Stage change to LCDOFF       0x400
	 * Stage change to SLEEP        0x800
	 * */
	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
	case SUSPEND:
	case POWER_OFF:
		break;
	default:
		return -1;
	}
	return send_msg(s_bits << SHIFT_CHANGE_STATE, 0);
}

API int pm_lock_state(unsigned int s_bits, unsigned int flag,
		      unsigned int timeout)
{
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
		s_bits = s_bits | (1 << SHIFT_HOLD_KEY_BLOCK);

	return send_msg(s_bits, timeout);
}

API int pm_unlock_state(unsigned int s_bits, unsigned int flag)
{
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
	return send_msg(s_bits, 0);
}

