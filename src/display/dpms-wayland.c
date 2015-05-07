/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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

#include <stdio.h>

#include "core/edbus-handler.h"
#include "device-interface.h"

#define ENLIGHTENMENT_BUS_NAME          "org.enlightenment.wm"
#define ENLIGHTENMENT_OBJECT_PATH       "/org/enlightenment/wm"
#define ENLIGHTENMENT_INTERFACE_NAME    ENLIGHTENMENT_BUS_NAME".dpms"

static int dpms = DPMS_OFF;

int dpms_set_power(enum dpms_state state)
{
	char *arr[1];
	char *str[32];
	int ret;

	snprintf(str, sizeof(str), "%d", state);
	arr[0] = str;
	ret = dbus_method_sync(ENLIGHTENMENT_BUS_NAME,
			ENLIGHTENMENT_OBJECT_PATH,
			ENLIGHTENMENT_INTERFACE_NAME,
			"set", "u", arr);

	if (ret < 0)
		return ret;

	dpms = state;
	return 0;
}

int dpms_get_power(enum dpms_state *state)
{
	if (!state)
		return -EINVAL;

	*state = dpms;
	return 0;
}
