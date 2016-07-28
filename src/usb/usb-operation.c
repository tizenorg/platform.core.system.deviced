/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <string.h>

#include "core/log.h"
#include "core/common.h"
#include "core/config-parser.h"
#include "core/launch.h"
#include "shared/deviced-systemd.h"
#include "usb.h"

#define USB_OPERATION   "/etc/deviced/usb-operation.conf"

#define KEY_START_STR   "Start"
#define KEY_STOP_STR    "Stop"

#define BUF_MAX 128

typedef enum {
	OPERATION_STOP,
	OPERATION_START,
} operation_e;

struct oper_data {
	char mode_str[BUF_MAX];
	operation_e type;
};

static int load_operation_config(struct parse_result *result, void *user_data)
{
	struct oper_data *data = user_data;
	int ret;
	operation_e type;

	if (!data || !result)
		return -EINVAL;

	if (!strstr(data->mode_str, result->section))
		return 0;

	if (!strncmp(result->name, KEY_START_STR, strlen(KEY_START_STR)))
		type = OPERATION_START;
	else if (!strncmp(result->name, KEY_STOP_STR, strlen(KEY_STOP_STR)))
		type = OPERATION_STOP;
	else {
		_E("Invalid name (%s)", result->name);
		return -EINVAL;
	}

	if (type != data->type)
		return 0;

	if (strstr(result->name, "Service")) {
		if (type == OPERATION_START)
			ret = deviced_systemd_start_unit(result->value);
		if (type == OPERATION_STOP)
			ret = deviced_systemd_stop_unit(result->value);
	} else
		ret = launch_app_cmd(result->value);

	_I("Execute(%s %s: %d)", result->name, result->value, ret);

	return 0;
}

static int usb_execute_operation(unsigned int mode, operation_e type)
{
	int ret;
	struct oper_data data;

	if (mode == USB_GADGET_NONE)
		return -EINVAL;

	usb_state_get_mode_str(mode, data.mode_str, sizeof(data.mode_str));

	data.type = type;

	ret = config_parse(USB_OPERATION,
			load_operation_config, &data);
	if (ret < 0)
		_E("Failed to load usb operation (%d)", ret);

	return ret;
}

int usb_operation_start(unsigned int mode)
{
	return usb_execute_operation(mode, OPERATION_START);
}

int usb_operation_stop(unsigned int mode)
{
	return usb_execute_operation(mode, OPERATION_STOP);
}
