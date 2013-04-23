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

struct device_ops {
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

static inline int device_start(struct device_ops *dev)
{
	if (dev && dev->start)
		return dev->start();

	return -EINVAL;
}

static inline int device_stop(struct device_ops *dev)
{
	if (dev && dev->stop)
		return dev->stop();

	return -EINVAL;
}

static inline int device_get_status(struct device_ops *dev)
{
	if (dev && dev->status)
		return dev->status();

	return -EINVAL;
}

extern const struct device_ops edbus_device_ops;
extern const struct device_ops display_device_ops;
extern const struct device_ops sysnoti_device_ops;
extern const struct device_ops noti_device_ops;
extern const struct device_ops control_device_ops;
extern const struct device_ops core_device_ops;
extern const struct device_ops signal_device_ops;
extern const struct device_ops predefine_device_ops;
extern const struct device_ops lowmem_device_ops;
extern const struct device_ops lowbat_device_ops;
extern const struct device_ops change_device_ops;
extern const struct device_ops power_device_ops;
extern const struct device_ops bs_device_ops;
extern const struct device_ops process_device_ops;
extern const struct device_ops time_device_ops;
extern const struct device_ops cpu_device_ops;
extern const struct device_ops usb_device_ops;
extern const struct device_ops ta_device_ops;
extern const struct device_ops pmon_device_ops;
extern const struct device_ops mmc_device_ops;
extern const struct device_ops led_device_ops;
extern const struct device_ops vibrator_device_ops;
extern const struct device_ops notifier_device_ops;

#endif
