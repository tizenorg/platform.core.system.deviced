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
#include <sys/types.h>
#include <dd-control.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"

static const struct control_device {
	const int id;
	const struct device_ops *ops;
} devices[] = {
	/*  code example
	 * { DEVICE_CONTROL_DISPLAY,       &display_device_ops },
	 */
	/* Add id & ops to provide start/stop control */
	{ DEVICE_CONTROL_MMC,	&mmc_device_ops },
};

static int control_handler(int argc, char **argv)
{
	int i;
	int pid;
	int device;
	bool enable;
	int ret;

	PRT_TRACE("argc : %d", argc);
	for (i = 0; i < argc; ++i)
		PRT_TRACE("[%2d] %s", i, argv[i]);

	if (argc > 5) {
		PRT_TRACE_ERR("Invalid argument");
		errno = EINVAL;
		return -1;
	}

	pid = atoi(argv[0]);
	device = atoi(argv[1]);
	enable = atoi(argv[2]);
	PRT_TRACE("pid : %d, device : %d, enable :%d", pid, device, enable);

	for (i = 0; i < ARRAY_SIZE(devices); i++)
		if (devices[i].id == device)
			break;

	if (i >= ARRAY_SIZE(devices))
		return -EINVAL;

	if (enable)
		ret = device_start(devices[i].ops);
	else
		ret = device_stop(devices[i].ops);

	return ret;
}

static void control_init(void *data)
{
	ss_action_entry_add_internal(CONTROL_HANDLER_NAME, control_handler, NULL, NULL);
}

const struct device_ops control_device_ops = {
	.init = control_init,
};

