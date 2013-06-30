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

#include "log.h"
#include "devices.h"
#include "common.h"

static const struct device_ops *devices[] = {
	/* The below devices have init dependency with other module */
	&edbus_device_ops,
	&display_device_ops,
	/* The below devices don't have any init dependency */
	&sysnoti_device_ops,
	&noti_device_ops,
	&core_device_ops,
	&signal_device_ops,
	&predefine_device_ops,
	&lowmem_device_ops,
	&lowbat_device_ops,
	&change_device_ops,
	&power_device_ops,
	&bs_device_ops,
	&process_device_ops,
	&time_device_ops,
	&cpu_device_ops,
	&usb_device_ops,
	&ta_device_ops,
	&pmon_device_ops,
	&mmc_device_ops,
	&led_device_ops,
	&vibrator_device_ops,
};

void devices_init(void *data)
{
	int i;
	int size;
	struct device_ops *dev;

	size = ARRAY_SIZE(devices);
	for(i = 0; i < size; ++i) {
		dev = devices[i];
		if (dev->init)
			dev->init(data);
	}
}

void devices_exit(void *data)
{
	int i;
	int size;
	struct device_ops *dev;

	size = ARRAY_SIZE(devices);
	for(i = 0; i < size; ++i) {
		dev = devices[i];
		if (dev->exit)
			dev->exit(data);
	}
}
