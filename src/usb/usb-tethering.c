/*
 * deviced
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
#include <vconf.h>

#include "core/log.h"
#include "core/common.h"
#include "core/config-parser.h"
#include "core/launch.h"
#include "core/device-notifier.h"
#include "usb.h"

static bool tethering_state;

bool usb_tethering_state(void)
{
	return tethering_state;
}

static void usb_tethering_changed(keynode_t *key, void *data)
{
	int mode;
	bool curr;

	if (!key)
		return;

	mode = vconf_keynode_get_int(key);
	curr = mode & VCONFKEY_MOBILE_HOTSPOT_MODE_USB;

	if (curr == tethering_state)
		return;

	tethering_state = curr;

	device_notify(DEVICE_NOTIFIER_USB_TETHERING_MODE, (void *)curr);
}

static int usb_tethering_mode_changed(void *data)
{
	bool on;
	char *mode;
	int ret;

	on = (bool)data;

	if (on)
		mode = USB_MODE_STR_TETHERING;
	else
		mode = USB_MODE_STR_DEFAULT;

	ret = usb_change_mode(mode);
	if (ret < 0)
		_E("Failed to change usb mode to (%s)", mode);

	return ret;
}

void add_usb_tethering_handler(void)
{
	if (vconf_notify_key_changed(VCONFKEY_MOBILE_HOTSPOT_MODE,
				usb_tethering_changed, NULL) != 0)
		_E("Failed to add usb tethering handler");

	register_notifier(DEVICE_NOTIFIER_USB_TETHERING_MODE,
			usb_tethering_mode_changed);
}

void remove_usb_tethering_handler(void)
{
	vconf_ignore_key_changed(VCONFKEY_MOBILE_HOTSPOT_MODE,
			usb_tethering_changed);

	unregister_notifier(DEVICE_NOTIFIER_USB_TETHERING_MODE,
			usb_tethering_mode_changed);
}
