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
#include <vconf.h>
#include <errno.h>
#include <device-node.h>

#include "log.h"
#include "dd-deviced.h"

#define PREDEF_LED			"led"

enum {
	SET_BRT = 0,
};

API int led_get_brightness(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, &val);
	if (r < 0)
		return r;

	return val;
}

API int led_get_max_brightness(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_LED, PROP_LED_MAX_BRIGHTNESS, &val);
	if (r < 0)
		return r;

	return val;
}

API int led_set_brightness(int val)
{
	char buf_pid[32];
	char buf_prop[32];
	char buf_val[32];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_prop, sizeof(buf_prop), "%d", SET_BRT);
	snprintf(buf_val, sizeof(buf_val), "%d", val);
	return deviced_call_predef_action(PREDEF_LED, 3, buf_pid, buf_prop, buf_val);
}
