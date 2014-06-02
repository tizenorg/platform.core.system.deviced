/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#include <vconf.h>
#include "core/log.h"
#include "apps.h"
#include "core/devices.h"
#include "core/list.h"

static dd_list *apps_head = NULL;

void add_apps(const struct apps_ops *dev)
{
	_I("add %s", dev->name);
	DD_LIST_APPEND(apps_head, (void*)dev);
}

void remove_apps(const struct apps_ops *dev)
{
	DD_LIST_REMOVE(apps_head, (void*)dev);
}

static void apps_init(void *data)
{
	const struct apps_ops*dev;
	dd_list *elem;
	struct apps_data *input_data;
	static int initialized =0;

	if (!initialized) {
		initialized = 1;
		return;
	}

	input_data = (struct apps_data *)data;
	if (input_data == NULL || input_data->name == NULL)
		return;

	DD_LIST_FOREACH(apps_head, elem, dev) {
		if (!strncmp(dev->name, (char *)input_data->name, strlen(dev->name))) {
			if (dev->launch)
				dev->launch(data);
			break;
		}
	}
}

static void apps_exit(void *data)
{
	const struct apps_ops*dev;
	dd_list *elem;
	struct apps_data *input_data;

	input_data = (struct apps_data *)data;
	if (input_data == NULL || input_data->name == NULL)
		return;

	DD_LIST_FOREACH(apps_head, elem, dev) {
		if (!strncmp(dev->name, (char *)input_data->name, strlen(dev->name))) {
			if (dev->terminate)
				dev->terminate(data);
			break;
		}
	}
}

static const struct device_ops apps_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "apps",
	.init     = apps_init,
	.exit     = apps_exit,
};

DEVICE_OPS_REGISTER(&apps_device_ops)
