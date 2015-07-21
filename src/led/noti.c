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
#include <unistd.h>

#include "core/log.h"
#include "core/edbus-handler.h"

#define METHOD_TORCH_NOTI_ON		"TorchNotiOn"
#define METHOD_TORCH_NOTI_OFF		"TorchNotiOff"

static int noti_h;

int ongoing_show(void)
{
	int ret_val;

	if (noti_h > 0) {
		_D("already ongoing noti show : handle(%d)", noti_h);
		return 0;
	}

	ret_val = dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_LED,
			POPUP_INTERFACE_LED,
			METHOD_TORCH_NOTI_ON,
			NULL, NULL);

	noti_h = ret_val;
	_D("insert noti handle : %d", noti_h);
	return (ret_val < 0) ? ret_val : 0;
}

int ongoing_clear(void)
{
	char str_h[32];
	char *arr[1];
	int ret_val;

	if (noti_h <= 0) {
		_D("already ongoing noti clear");
		return 0;
	}

	snprintf(str_h, sizeof(str_h), "%d", noti_h);
	arr[0] = str_h;

	ret_val = dbus_method_sync(POPUP_BUS_NAME,
			POPUP_PATH_LED,
			POPUP_INTERFACE_LED,
			METHOD_TORCH_NOTI_OFF,
			"i", arr);

	_D("delete noti handle : %d", noti_h);
	noti_h = 0;
	return ret_val;
}
