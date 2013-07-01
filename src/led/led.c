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
#include <assert.h>
#include <device-node.h>

#include "deviced/dd-led.h"
#include "core/log.h"
#include "core/devices.h"

#define PREDEF_LED			"led"

enum {
	SET_BRT = 0,
};

static int set_brightness(int val)
{
	int r;

	r = device_set_property(DEVICE_TYPE_LED, PROP_LED_BRIGHTNESS, val);
	if (r < 0)
		return r;

	return 0;
}

static int predefine_action(int argc, char **argv)
{
	int i;
	int pid;
	int prop;

	PRT_TRACE("argc : %d", argc);
	for (i = 0; i < argc; ++i)
		PRT_TRACE("[%2d] %s", i, argv[i]);

	if (argc > 4) {
		PRT_TRACE_ERR("Invalid argument");
		errno = EINVAL;
		return -1;
	}

	pid = atoi(argv[0]);
	prop = atoi(argv[1]);
	PRT_TRACE("pid : %d, prop : %d", pid, prop);

	switch(prop) {
	case SET_BRT:
		return set_brightness(atoi(argv[2]));
	default:
		break;
	}

	return -1;
}

static void led_init(void *data)
{
	ss_action_entry_add_internal(PREDEF_LED, predefine_action, NULL, NULL);
}

const struct device_ops led_device_ops = {
	.init = led_init,
};
