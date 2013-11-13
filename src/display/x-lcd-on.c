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


#ifndef __PM_X_LCD_ONOFF_C__
#define __PM_X_LCD_ONOFF_C__

#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#define CMD_ON		"on"
#define CMD_OFF		"off"

static int pm_x_set_lcd_backlight(struct _PMSys *p, int on)
{
	pid_t pid;
	char cmd_line[8];
	int ret;

	_I("Backlight on=%d", on);

	switch (on) {
	case STATUS_ON:
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_ON);
		break;
	case STATUS_OFF:
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_OFF);
		break;
	}

	signal(SIGCHLD, SIG_DFL);
	pid = vfork();

	if (pid < 0) {
		_E("[1] Failed to fork child process for LCD On/Off");
		return -1;
	}

	if (pid == 0) {
		_I("[1] Child proccess for LCD %d was created (%s)",
		    on, cmd_line);
		execl("/usr/bin/xset", "/usr/bin/xset", "dpms", "force",
		    cmd_line, NULL);
		_exit(0);
	} else if (pid != (ret = waitpid(pid, NULL, 0))) {
		_E
		    ("[1] Waiting failed for the child process pid: %d, ret: %d, errno: %d",
		     pid, ret, errno);
	}

	signal(SIGCHLD, SIG_IGN);
	return 0;
}

#endif				/*__PM_X_LCD_ONOFF_C__ */
