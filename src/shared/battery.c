/*
 * deviced
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
#include <vconf.h>
#include <errno.h>
#include <device-node.h>

#include "log.h"
#include "dd-battery.h"

API int battery_get_percent(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY, &val);
	if (r < 0)
		return r;

	if (val < 0 || val > 100) {
		PRT_ERR("capacity value is wrong");
		errno = EPERM;
		return -1;
	}

	return val;
}

API int battery_get_percent_raw(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY_RAW, &val);
	if (r < 0)
		return r;

	if (val > 10000)
		return 10000;

	return val;
}

API int battery_is_full(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_FULL, &val);
	if (r < 0)
		return r;

	if (val != 0 && val != 1) {
		PRT_ERR("charge_full value is wrong");
		errno = EPERM;
		return -1;
	}

	return val;
}

API int battery_get_health(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_HEALTH, &val);
	if (r < 0)
		return r;

	if (val < BAT_UNKNOWN || val > BAT_COLD) {
		PRT_ERR("battery health value is wrong");
		errno = EPERM;
		return -1;
	}

	return val;
}
