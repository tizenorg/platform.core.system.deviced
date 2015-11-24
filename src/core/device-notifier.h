/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __DEVICE_NOTIFIER_H__
#define __DEVICE_NOTIFIER_H__

enum device_notifier_type {
	DEVICE_NOTIFIER_BOOTING_DONE,
	DEVICE_NOTIFIER_LCD,
	DEVICE_NOTIFIER_TA,
	DEVICE_NOTIFIER_LOWBAT,
	DEVICE_NOTIFIER_FULLBAT,
	DEVICE_NOTIFIER_TOUCH_HARDKEY,
	DEVICE_NOTIFIER_PROCESS_TERMINATED,
	DEVICE_NOTIFIER_POWER_SUPPLY,
	DEVICE_NOTIFIER_BATTERY_HEALTH,
	DEVICE_NOTIFIER_BATTERY_PRESENT,
	DEVICE_NOTIFIER_BATTERY_OVP,
	DEVICE_NOTIFIER_BATTERY_CHARGING,
	DEVICE_NOTIFIER_POWEROFF,
	DEVICE_NOTIFIER_POWEROFF_HAPTIC,
	DEVICE_NOTIFIER_PMQOS_OOM,
	DEVICE_NOTIFIER_PROCESS_BACKGROUND,
	DEVICE_NOTIFIER_MAX,
};

/*
 * This is for internal callback method.
 */
int register_notifier(enum device_notifier_type status, int (*func)(void *data));
int unregister_notifier(enum device_notifier_type status, int (*func)(void *data));
void device_notify(enum device_notifier_type status, void *value);

#endif /* __DEVICE_NOTIFIER_H__ */
