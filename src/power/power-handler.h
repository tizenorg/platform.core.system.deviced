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

#ifndef __POWER_HANDLE_H__
#define __POWER_HANDLE_H__

#define POWER_OPS_NAME      "power"

#define POWER_POWEROFF      "poweroff"
#define POWER_POWEROFF_LEN  8
#define POWER_REBOOT        "reboot"
#define POWER_REBOOT_LEN    6
#define POWER_RECOVERY      "reboot-recovery"
#define POWER_RECOVERY_LEN  15
#define POWER_FOTA          "fota"
#define POWER_FOTA_LEN      4
#define PWROFF_POPUP        "pwroff-popup"
#define PWROFF_POPUP_LEN    12
#define INTERNAL_PWROFF     "internal"
#define INTERNAL_PWROFF_LEN 8

enum poweroff_type {
	POWER_OFF_NONE = 0,
	POWER_OFF_POPUP,
	POWER_OFF_DIRECT,
	POWER_OFF_RESTART,
	POWER_OFF_INTERNAL,
	POWER_OFF_RESTART_INTERNAL,
	POWER_OFF_RESTART_RECOVERY,
	POWER_OFF_RESTART_FOTA,
};

int poweroff_request_internal(enum poweroff_type val);

#ifndef SYSTEMD_SHUTDOWN
void restart_ap(int data);
void powerdown_ap(void *data);
#endif

#endif /* __POWER_HANDLE_H__ */
