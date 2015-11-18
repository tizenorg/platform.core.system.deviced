/*
 * deviced
 *
 * Copyright (c) 2012 - 2015 Samsung Electronics Co., Ltd.
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

#include <stdarg.h>
#include "core/log.h"
#include "core/common.h"
#include "apps.h"
#include "core/edbus-handler.h"

#define POPUP_METHOD "PopupLaunch"

static const struct app_dbus_match {
	const char *type;
	const char *bus;
	const char *path;
	const char *iface;
	const char *method;
} app_match[] = {
	{ APP_DEFAULT , POPUP_BUS_NAME, POPUP_PATH_SYSTEM  , POPUP_INTERFACE_SYSTEM  , POPUP_METHOD },
	{ APP_POWEROFF, POPUP_BUS_NAME, POPUP_PATH_POWEROFF, POPUP_INTERFACE_POWEROFF, POPUP_METHOD },
};

int launch_system_app(char *type, int num, ...)
{
	char *app_type;
	va_list args;
	int i, match, ret;

	if (type)
		app_type = type;
	else
		app_type = APP_DEFAULT;

	match = -1;
	for (i = 0 ; i < ARRAY_SIZE(app_match) ; i++) {
		if (strncmp(app_type, app_match[i].type, strlen(app_type)))
			continue;
		match = i;
		break;
	}
	if (match < 0) {
		_E("Failed to find app matched (%s)", app_type);
		return -EINVAL;
	}

	va_start(args, num);

	ret = dbus_method_sync_pairs(app_match[match].bus,
			app_match[match].path,
			app_match[match].iface,
			app_match[match].method,
			num, args);

	va_end(args);

	return ret;
}

int launch_message_post(char *type)
{
	char *param[1];

	if (!type)
		return -EINVAL;

	param[0] = type;

	return dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_NOTI,
			POPUP_INTERFACE_NOTI,
			"MessagePostOn",
			"s", param);
}
