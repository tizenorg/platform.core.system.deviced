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


#include <stdio.h>
#include <errno.h>

#include "log.h"
#include "dbus.h"
#include "common.h"

#define METHOD_GET_PERCENT		"GetPercent"
#define METHOD_GET_PERCENT_RAW	"GetPercentRaw"
#define METHOD_IS_FULL			"IsFull"
#define METHOD_GET_HEALTH		"GetHealth"

API int battery_get_percent(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_BATTERY, DEVICED_INTERFACE_BATTERY,
			METHOD_GET_PERCENT, NULL, NULL);
}

API int battery_get_percent_raw(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_BATTERY, DEVICED_INTERFACE_BATTERY,
			METHOD_GET_PERCENT_RAW, NULL, NULL);
}

API int battery_is_full(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_BATTERY, DEVICED_INTERFACE_BATTERY,
			METHOD_IS_FULL, NULL, NULL);
}

API int battery_get_health(void)
{
	return dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_BATTERY, DEVICED_INTERFACE_BATTERY,
			METHOD_GET_HEALTH, NULL, NULL);
}
