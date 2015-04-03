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


#ifndef __UDEV_H__
#define __UDEV_H__

#include <libudev.h>

#define UDEV			"kernel"

#define UDEV_CHANGE		"change"
#define UDEV_ADD		"add"
#define UDEV_REMOVE		"remove"

#define UDEV_DEVPATH		"DEVPATH"
#define UDEV_DEVTYPE		"DEVTYPE"

#define UDEV_MONITOR_SIZE	(10*1024)
#define UDEV_MONITOR_SIZE_LARGE (128*1024*1024)

/* battery device */
#define POWER_SUBSYSTEM		"power_supply"
#define POWER_PATH			"/sys/class/power_supply/battery"
#define POWER_SUPPLY_UEVENT POWER_PATH"/uevent"
#define CAPACITY			"POWER_SUPPLY_CAPACITY"
#define CHARGE_FULL			"POWER_SUPPLY_CHARGE_FULL"
#define CHARGE_NOW			"POWER_SUPPLY_CHARGE_NOW"
#define CHARGE_HEALTH		"POWER_SUPPLY_HEALTH"
#define CHARGE_PRESENT		"POWER_SUPPLY_PRESENT"
#define CHARGE_NAME			"POWER_SUPPLY_NAME"
#define CHARGE_STATUS		"POWER_SUPPLY_STATUS"
#define CHARGE_ONLINE		"POWER_SUPPLY_ONLINE"

/* extcon */
#define EXTCON_SUBSYSTEM	"extcon"

/* usb */
#define USB_SUBSYSTEM           "usb"
#define USB_INTERFACE_DEVTYPE   "usb_interface"

/* power supply status */
enum {
	POWER_SUPPLY_STATUS_UNKNOWN = 0,
	POWER_SUPPLY_STATUS_CHARGING,
	POWER_SUPPLY_STATUS_DISCHARGING,
	POWER_SUPPLY_STATUS_NOT_CHARGING,
	POWER_SUPPLY_STATUS_FULL,
};

enum {
	 POWER_SUPPLY_TYPE_UNKNOWN = 0,
	 POWER_SUPPLY_TYPE_BATTERY,
	 POWER_SUPPLY_TYPE_UPS,
	 POWER_SUPPLY_TYPE_MAINS,
	 POWER_SUPPLY_TYPE_USB,
};

enum dock_type {
	DOCK_NONE   = 0,
	DOCK_SOUND  = 7,
};

struct uevent_handler {
	char *subsystem;
	void (*uevent_func)(struct udev_device *dev);
	void *data;
};

int register_kernel_uevent_control(const struct uevent_handler *uh);
int unregister_kernel_uevent_control(const struct uevent_handler *uh);

#endif /* __UDEV_H__ */
