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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "conf.h"

enum {
	IDX_NAME = 0,
	IDX_DEFAULT,
	IDX_END
};

static const char *def_values[][IDX_END] = {
	{"PM_INPUT", "/dev/event0:/dev/event1"},
	{"PM_TO_START", "0"},
	{"PM_TO_NORMAL", "600"},
	{"PM_TO_LCDDIM", "5"},
	{"PM_TO_LCDOFF", "1"},
	{"PM_TO_SLEEP", "0"},
	{"PM_SYS_POWER", "/sys/power/state"},
	{"PM_SYS_BRIGHT", "/sys/class/backlight/mobile-bl/brightness"},
	{"PM_SYS_BRIGHT", "/sys/class/backlight/mobile-bl/max_brightness"},
	{"PM_SYS_BLPWR", "/sys/class/backlight/mobile-bl/bl_power"},
	{"PM_SYS_DIMBRT", "0"},
	{"PM_SYS_BLON", "0"},
	{"PM_SYS_BLOFF", "4"},
	{"PM_SYS_FB_NORMAL", "1"},
	{"PM_SYS_STATE", "mem"},
	{"PM_EXEC_PRG", NULL},
	{"PM_END", ""},
};

static char *_find_default(char *name)
{
	char *ret = NULL;
	int i = 0;

	while (strcmp("PM_END", def_values[i][IDX_NAME])) {
		if (!strcmp(name, def_values[i][IDX_NAME])) {
			ret = def_values[i][IDX_DEFAULT];
			break;
		}
		i++;
	}
	return ret;
}

int get_env(char *name, char *buf, int size)
{
	char *ret;

	ret = getenv(name);
	if ((ret == NULL) || (strlen(ret) > 1024)) {
		ret = _find_default(name);
		if (ret)
			snprintf(buf, size, "%s", ret);
		else
			snprintf(buf, size, "");
	} else {
		snprintf(buf, size, "%s", ret);
	}

	return 0;
}
