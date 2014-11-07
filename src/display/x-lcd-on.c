/*
 *  deviced
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
 *
*/


#ifndef __PM_X_LCD_ONOFF_C__
#define __PM_X_LCD_ONOFF_C__

#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "core/log.h"
#include "core/common.h"

#define CMD_ON		"on"
#define CMD_OFF		"off"
#define CMD_STANDBY	"standby"

static const char *xset_arg[] = {
	"/usr/bin/xset",
	"dpms", "force", NULL, NULL,
};

static int pm_x_set_lcd_backlight(struct _PMSys *p, int on)
{
	pid_t pid;
	char cmd_line[8];
	int argc;

	_D("Backlight on=%d", on);

	switch (on) {
	case STATUS_ON:
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_ON);
		break;
	case STATUS_OFF:
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_OFF);
		break;
	case STATUS_STANDBY:
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_STANDBY);
		break;
	}

	argc = ARRAY_SIZE(xset_arg);
	xset_arg[argc - 2] = cmd_line;
	return run_child(argc, xset_arg);
}

#endif				/*__PM_X_LCD_ONOFF_C__ */
