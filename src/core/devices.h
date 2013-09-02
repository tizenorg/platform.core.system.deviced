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


#ifndef __DEVICES_H__
#define __DEVICES_H__

#include <errno.h>
#include "common.h"

enum device_priority {
	DEVICE_PRIORITY_NORMAL = 0,
	DEVICE_PRIORITY_HIGH,
};

struct device_ops {
	enum device_priority priority;
	char *name;
	void (*init) (void *data);
	void (*exit) (void *data);
	int (*start) (void);
	int (*stop) (void);
	int (*status) (void);
};

enum device_ops_status {
	DEVICE_OPS_STATUS_UNINIT,
	DEVICE_OPS_STATUS_START,
	DEVICE_OPS_STATUS_STOP,
	DEVICE_OPS_STATUS_MAX,
};

void devices_init(void *data);
void devices_exit(void *data);

static inline int device_start(const struct device_ops *dev)
{
	if (dev && dev->start)
		return dev->start();

	return -EINVAL;
}

static inline int device_stop(const struct device_ops *dev)
{
	if (dev && dev->stop)
		return dev->stop();

	return -EINVAL;
}

static inline int device_get_status(const struct device_ops *dev)
{
	if (dev && dev->status)
		return dev->status();

	return -EINVAL;
}

#define DEVICE_OPS_REGISTER(dev)       \
static void __CONSTRUCTOR__ module_init(void)  \
{      \
	add_device(dev);        \
}      \
static void __DESTRUCTOR__ module_exit(void)   \
{      \
	remove_device(dev);     \
}

void add_device(const struct device_ops *dev);
void remove_device(const struct device_ops *dev);

#endif
