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

#include "core/log.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "extcon/extcon.h"

enum usb_state {
	USB_DISCONNECTED,
	USB_CONNECTED,
};

struct extcon_ops extcon_usb_ops;

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

static void usb_init(void *data)
{
	int ret, status;

	register_notifier(DEVICE_NOTIFIER_USB, usb_state_changed);

	ret = usb_state_changed(&(extcon_usb_ops.status));
	if (ret < 0)
		_E("Failed to update usb status(%d)", ret);
}

static void usb_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_USB, usb_state_changed);
}

struct extcon_ops extcon_usb_ops = {
	.name	= "USB",
	.noti	= DEVICE_NOTIFIER_USB,
	.init	= usb_init,
	.exit	= usb_exit,
};

EXTCON_OPS_REGISTER(&extcon_usb_ops)
