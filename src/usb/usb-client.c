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
#include <vconf.h>
#include <string.h>

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "extcon/extcon.h"

#define BUF_MAX 256

enum usb_state {
	USB_DISCONNECTED,
	USB_CONNECTED,
};

static int get_current_usb_status(void)
{
	return get_extcon_status("USB");
}

static int usb_state_changed(void *data)
{
	static int state = USB_DISCONNECTED;
	int input;

	if (!data)
		return -EINVAL;

	input = *(int *)data;

	_I("USB state is changed from (%d) to (%d)", state, input);

	if (state == input)
		return 0;

	switch (input) {
	case USB_CONNECTED:
		_I("USB cable is connected");
		break;
	case USB_DISCONNECTED:
		_I("USB cable is disconnected");
		break;
	default:
		_E("Invalid USB state(%d)", state);
		return -EINVAL;
	}

	state = input;

	return 0;
}

static int usb_extcon_ready(void *data)
{
	int ret, status;

	_I("USB extcon ready");

	register_notifier(DEVICE_NOTIFIER_USB, usb_state_changed);
	unregister_notifier(DEVICE_NOTIFIER_EXTCON_READY, usb_extcon_ready);

	status = get_current_usb_status();
	if (status < 0) {
		_E("Failed to get usb status");
		return status;
	}

	ret = usb_state_changed(&status);
	if (ret < 0)
		_E("Failed to update usb status(%d)", ret);

	return 0;
}

static void usb_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_EXTCON_READY, usb_extcon_ready);
}

static void usb_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_USB, usb_state_changed);
}

const struct device_ops usb_device_ops = {
	.name     = "usb",
	.init     = usb_init,
	.exit     = usb_exit,
};

DEVICE_OPS_REGISTER(&usb_device_ops)
