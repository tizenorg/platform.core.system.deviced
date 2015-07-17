/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>
#include <assert.h>

#include "core/log.h"
#include "core/common.h"
#include "core/config-parser.h"
#include "battery.h"
#include "config.h"

#define BAT_CONF_FILE	"/etc/deviced/battery.conf"

static int load_config(struct parse_result *result, void *user_data)
{
	struct battery_config_info *info = user_data;
	char *name;
	char *value;

	_D("%s,%s,%s", result->section, result->name, result->value);

	if (!info)
		return -EINVAL;

	if (!MATCH(result->section, "LOWBAT"))
		return -EINVAL;

	name = result->name;
	value = result->value;
	if (MATCH(name, "Normal"))
		info->normal = atoi(value);
	else if (MATCH(name, "Warning"))
		info->warning = atoi(value);
	else if (MATCH(name, "Critical"))
		info->critical = atoi(value);
	else if (MATCH(name, "PowerOff"))
		info->poweroff = atoi(value);
	else if (MATCH(name, "RealOff"))
		info->realoff = atoi(value);
	else if (MATCH(name, "WarningMethod"))
		snprintf(info->warning_method,
				sizeof(info->warning_method), "%s", value);
	else if (MATCH(name, "CriticalMethod"))
		snprintf(info->critical_method,
				sizeof(info->critical_method), "%s", value);

	return 0;
}

void battery_config_load(struct battery_config_info *info)
{
	int ret;

	ret = config_parse(BAT_CONF_FILE, load_config, info);
	if (ret < 0)
		_E("Failed to load %s, %d Use default value!", BAT_CONF_FILE, ret);
}
