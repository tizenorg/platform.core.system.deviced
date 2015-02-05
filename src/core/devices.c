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

#include "log.h"
#include "list.h"
#include "common.h"
#include "devices.h"

static const struct device_ops default_ops = {
	.name = "default-ops",
};

static dd_list *dev_head;

void add_device(const struct device_ops *dev)
{
	if (dev->priority == DEVICE_PRIORITY_HIGH)
		DD_LIST_PREPEND(dev_head, dev);
	else
		DD_LIST_APPEND(dev_head, dev);
}

void remove_device(const struct device_ops *dev)
{
	DD_LIST_REMOVE(dev_head, dev);
}

const struct device_ops *find_device(const char *name)
{
	dd_list *elem;
	const struct device_ops *dev;

	DD_LIST_FOREACH(dev_head, elem, dev) {
		if (!strcmp(dev->name, name))
			return dev;
	}

	dev = &default_ops;
	return dev;
}

int check_default(const struct device_ops *dev)
{
	return (dev == &default_ops);
}

void devices_init(void *data)
{
	dd_list *elem;
	const struct device_ops *dev;

	DD_LIST_FOREACH(dev_head, elem, dev) {
		if (dev->probe && dev->probe(data) != 0) {
			_E("[%s] probe fail", dev->name);
			continue;
		}

		_D("[%s] initialize", dev->name);
		if (dev->init)
			dev->init(data);
	}
}

void devices_exit(void *data)
{
	dd_list *elem;
	const struct device_ops *dev;

	DD_LIST_FOREACH(dev_head, elem, dev) {
		_D("[%s] deinitialize", dev->name);
		if (dev->exit)
			dev->exit(data);
	}
}
