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


#include <error.h>
#include <stdbool.h>
#include <device-node.h>
#include <vconf.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "shared/dbus.h"
#include "usb.h"

/* Legacy signals */
#define SIGNAL_STATE_CHANGED  "StateChanged"
#define SIGNAL_MODE_CHANGED   "ModeChanged"
#define SIGNAL_CONFIG_ENABLED "ConfigEnabled"
#define CHANGE_USB_MODE       "ChangeUsbMode"

unsigned int get_usb_state(void)
{
	usb_connection_state_e conn;
	unsigned int mode;
	unsigned int state;
	enum {
		US_DISCONNECTED,
		US_CONNECTED,
		US_AVAILABLE,
	};

	conn = usb_state_get_connection();
	if (conn == USB_DISCONNECTED)
		state = US_DISCONNECTED;
	else {
		state = US_CONNECTED;
		mode = usb_state_get_current_mode();
		if (mode != USB_GADGET_NONE)
			state |= US_AVAILABLE;
	}

	return state;
}

/* dbus signals */
void broadcast_usb_config_enabled(int state)
{
	int ret;
	char *param[1];
	char buf[2];

	snprintf(buf, sizeof(buf), "%d", state);
	param[0] = buf;

	_I("USB config enabled (%d)", state);

	ret = broadcast_edbus_signal(DEVICED_PATH_USB,
			DEVICED_INTERFACE_USB, SIGNAL_CONFIG_ENABLED,
			"i", param);
	if (ret < 0)
		_E("Failed to send dbus signal");
}

void broadcast_usb_state_changed(void)
{
	int ret;
	char *param[1];
	char text[16];
	unsigned int state;
	static unsigned int prev_state = UINT_MAX;

	state = get_usb_state();
	if (state == prev_state)
		return;
	prev_state = state;

	_I("USB state changed (%u)", state);

	snprintf(text, sizeof(text), "%u", state);
	param[0] = text;

	ret = broadcast_edbus_signal(DEVICED_PATH_USB,
			DEVICED_INTERFACE_USB, SIGNAL_STATE_CHANGED,
			"u", param);
	if (ret < 0)
		_E("Failed to send dbus signal");
}

void broadcast_usb_mode_changed(void)
{
	int ret;
	char *param[1];
	char text[16];
	unsigned int mode;
	static unsigned int prev_mode = UINT_MAX;

	mode = usb_state_get_current_mode();
	if (mode == prev_mode)
		return;
	prev_mode = mode;

	snprintf(text, sizeof(text), "%u", mode);
	param[0] = text;

	_I("USB mode changed (%u)", mode);

	ret = broadcast_edbus_signal(DEVICED_PATH_USB,
			DEVICED_INTERFACE_USB, SIGNAL_MODE_CHANGED,
			"u", param);
	if (ret < 0)
		_E("Failed to send dbus signal");
}

static void change_usb_client_mode(void *data, DBusMessage *msg)
{
	DBusError err;
	int req, debug;
	int ret;
	unsigned int mode;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_USB, CHANGE_USB_MODE) == 0)
		return;

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &req, DBUS_TYPE_INVALID) == 0) {
		_E("FAIL: dbus_message_get_args");
		goto out;
	}

	debug = 0;
	switch (req) {
	case SET_USB_DEFAULT:
		mode = USB_GADGET_MTP | USB_GADGET_ACM;
		break;
	case SET_USB_SDB:
		mode = USB_GADGET_MTP | USB_GADGET_ACM | USB_GADGET_SDB;
		debug = 1;
		break;
	case SET_USB_SDB_DIAG:
		mode = USB_GADGET_MTP | USB_GADGET_ACM | USB_GADGET_SDB | USB_GADGET_DIAG;
		debug = 1;
		break;
	case SET_USB_RNDIS:
		mode = USB_GADGET_RNDIS;
		break;
	case SET_USB_RNDIS_DIAG:
		mode = USB_GADGET_RNDIS | USB_GADGET_DIAG;
		break;
	case SET_USB_RNDIS_SDB:
		mode = USB_GADGET_RNDIS | USB_GADGET_SDB;
		debug = 1;
		break;
	case 11: /* SET_USB_DIAG_RMNET */
		mode = USB_GADGET_DIAG | USB_GADGET_ACM | USB_GADGET_RMNET;
		break;
	case 12: /* SET_USB_ACM_SDB_DM */
		mode = USB_GADGET_ACM | USB_GADGET_SDB | USB_GADGET_DM;
		debug = 1;
		break;
	default:
		_E("(%d) is unknown usb mode", req);
		goto out;
	}

	if (vconf_set_bool(VCONFKEY_SETAPPL_USB_DEBUG_MODE_BOOL, debug) != 0)
		_E("Failed to set usb debug toggle (%d)", debug);

	ret = usb_change_mode(mode);
	if (ret < 0)
		_E("Failed to change usb mode (%d)", ret);

out:
	dbus_error_free(&err);
}

/* dbus methods */
static DBusMessage *get_usb_client_state(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int state;

	state = get_usb_state();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &state);
	return reply;
}

static DBusMessage *get_usb_client_mode(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	unsigned int mode;

	mode = usb_state_get_current_mode();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &mode);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "GetState", NULL, "u", get_usb_client_state },  /* from Tizen 2.3 */
	{ "GetMode", NULL, "u", get_usb_client_mode  },   /* from Tizen 2.3 */
	/* Add methods here */
};

int usb_dbus_init(void)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_USB,
		    edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0) {
		_E("Failed to register dbus method (%d)", ret);
		return ret;
	}

	ret = register_edbus_signal_handler(DEVICED_PATH_USB,
			DEVICED_INTERFACE_USB, "ChangeUsbMode",
			change_usb_client_mode);
	if (ret < 0) {
		_E("Failed to registser dbus signal (%d)", ret);
		return ret;
	}

	return 0;
}
