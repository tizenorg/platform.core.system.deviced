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


#include <vconf.h>
#include <device-node.h>

#include "core/log.h"
#include "core/devices.h"
#include "display/poll.h"
#include "core/common.h"

#define RETRY	3

enum ta_connect_status{
	TA_OFFLINE = 0,
	TA_ONLINE = 1,
};

static int __check_insuspend_charging(void)
{
	int val, ret;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_INSUSPEND_CHARGING_SUPPORT, &val);
	if (ret != 0)
		val = 0;
	if (val == 0)
		ret = pm_lock_internal(INTERNAL_LOCK_TA, LCD_OFF, STAY_CUR_STATE, 0);
	else
		ret = 0;
	return ret;
}

static void ta_init(void *data)
{
	int val, i = 0;
	int ret;

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_TA_ONLINE, &val) != 0)
		val = -EINVAL;

	if (val == TA_ONLINE) {
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS,
				VCONFKEY_SYSMAN_CHARGER_CONNECTED);
		while (i < RETRY
			   && __check_insuspend_charging() == -1) {
			i++;
			sleep(1);
		}
	} else if (val == TA_OFFLINE) {
		vconf_set_int(VCONFKEY_SYSMAN_CHARGER_STATUS,
				VCONFKEY_SYSMAN_CHARGER_DISCONNECTED);
	}
}

static const struct device_ops ta_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "ta",
	.init     = ta_init,
};

DEVICE_OPS_REGISTER(&ta_device_ops)
