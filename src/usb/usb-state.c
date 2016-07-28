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
#include <stdbool.h>
#include <vconf.h>
#include <bundle.h>
#include <eventsystem.h>

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "display/poll.h"
#include "extcon/extcon.h"
#include "apps/apps.h"
#include "usb.h"
#include "usb-tethering.h"

static usb_connection_state_e usb_connection = USB_DISCONNECTED;
static unsigned int usb_mode = USB_GADGET_NONE;
static unsigned int usb_selected_mode = USB_GADGET_SDB; /* for debugging */

static void usb_state_send_system_event(int status)
{
	bundle *b;
	const char *str;

	switch (status) {
	case VCONFKEY_SYSMAN_USB_DISCONNECTED:
		str = EVT_VAL_USB_DISCONNECTED;
		break;
	case VCONFKEY_SYSMAN_USB_CONNECTED:
		str = EVT_VAL_USB_CONNECTED;
		break;
	case VCONFKEY_SYSMAN_USB_AVAILABLE:
		str = EVT_VAL_USB_AVAILABLE;
		break;
	default:
		return;
	}

	_I("system_event (%s)", str);

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_USB_STATUS, str);
	eventsystem_send_system_event(SYS_EVENT_USB_STATUS, b);
	bundle_free(b);
}

static void usb_state_set_connection(usb_connection_state_e conn)
{
	if (usb_connection != conn)
		broadcast_usb_state_changed();
	usb_connection = conn;
}

usb_connection_state_e usb_state_get_connection(void)
{
	return usb_connection;
}

void usb_state_retrieve_selected_mode(void)
{
	int ret, mode;
	ret = vconf_get_int(VCONFKEY_USB_SEL_MODE, &mode);
	if (ret != 0) {
		_E("Failed to retrieve selected mode");
		return;
	}

#ifdef ENGINEER_MODE
	if (!(mode & USB_GADGET_SDB)) {
		mode = USB_GADGET_MTP | USB_GADGET_ACM | USB_GADGET_SDB;
		usb_state_set_selected_mode(mode);
	}
#endif

	usb_selected_mode = (unsigned int)mode;
}

void usb_state_set_selected_mode(unsigned int mode)
{
	usb_selected_mode = mode;
	vconf_set_int(VCONFKEY_USB_SEL_MODE, mode);
}

unsigned int usb_state_get_current_mode(void)
{
	return usb_mode;
}

unsigned int usb_state_get_selected_mode(void)
{
	return usb_selected_mode;
}

char *usb_state_get_mode_str(unsigned int mode, char *str, size_t len)
{
	char buf[256] = {0,}, tmp[256];
	int i;
	bool found;

	if (mode == USB_GADGET_NONE) {
		for (i = 0 ; i < ARRAY_SIZE(usb_modes) ; i++) {
			if (mode == usb_modes[i].mode) {
				snprintf(str, len, "%s", usb_modes[i].mode_str);
				return str;
			}
		}
		return NULL;
	}

	found = false;
	for (i = 1 ; i < ARRAY_SIZE(usb_modes) ; i++) {
		if (mode & usb_modes[i].mode) {
			if (found) {
				snprintf(tmp, sizeof(tmp), "%s", buf);
				snprintf(buf, sizeof(buf), "%s,%s", tmp, usb_modes[i].mode_str);
			} else {
				found = true;
				snprintf(buf, sizeof(buf), "%s", usb_modes[i].mode_str);
			}
		}
	}

	snprintf(str, len, "%s", buf);
	return str;
}

void usb_state_update_state(usb_connection_state_e state, unsigned int mode)
{
	static int old_mode = -1; /* VCONFKEY_USB_CUR_MODE */
	static int old_status = -1; /* VCONFKEY_SYSMAN_USB_STATUS */
	static int noti_id = -1;
	int status;

	if (state == USB_DISCONNECTED && mode != USB_GADGET_NONE)
		mode = USB_GADGET_NONE;

	if (mode == USB_GADGET_NONE) {
		if (noti_id >= 0) {
			remove_notification("MediaDeviceNotiOff", noti_id);
			noti_id = -1;
		}
	} else if (mode | USB_GADGET_MTP) {
		if (noti_id < 0)
			noti_id = add_notification("MediaDeviceNotiOn");
		if (noti_id < 0)
			_E("Failed to show notification for usb connection");
	}

	usb_state_set_connection(state);
	if (state == USB_CONNECTED) {
		if (mode == USB_GADGET_NONE)
			status = VCONFKEY_SYSMAN_USB_CONNECTED;
		else
			status = VCONFKEY_SYSMAN_USB_AVAILABLE;
	} else
		status = VCONFKEY_SYSMAN_USB_DISCONNECTED;

	if (old_status != status) {
		usb_state_send_system_event(status);
		vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS, status);
		old_status = status;
	}

	if (old_mode != mode) {
		vconf_set_int(VCONFKEY_USB_CUR_MODE, mode);
		broadcast_usb_mode_changed();
		if (mode == USB_GADGET_NONE)
			broadcast_usb_config_enabled(DISABLED);
		else
			broadcast_usb_config_enabled(ENABLED);
		usb_mode = mode;
		old_mode = mode;
	}
}
