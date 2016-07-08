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

enum usb_state {
	USB_DISCONNECTED,
	USB_CONNECTED,
};

typedef enum {
	USB_MODE_NONE,
	USB_MODE_DEFAULT,
	USB_MODE_TETHERING,
} usb_mode_e;

static const struct _usb_modes {
	usb_mode_e mode;
	const char *mode_str;
} usb_modes[] = {
	{ USB_MODE_NONE,        USB_MODE_STR_NONE       },
	{ USB_MODE_DEFAULT,     USB_MODE_STR_DEFAULT    },
	{ USB_MODE_TETHERING,   USB_MODE_STR_TETHERING  },
};

static dd_list *config_list;
struct extcon_ops extcon_usb_ops;
static const struct usb_config_plugin_ops *config_plugin;

void add_usb_config(const struct usb_config_ops *ops)
{
	DD_LIST_APPEND(config_list, ops);
}

void remove_usb_config(const struct usb_config_ops *ops)
{
	DD_LIST_REMOVE(config_list, ops);
}

static int usb_config_module_load(void)
{
	dd_list *l;
	struct usb_config_ops *ops;

	DD_LIST_FOREACH(config_list, l, ops) {
		if (ops->is_valid && ops->is_valid()) {
			if (ops->load)
				config_plugin = ops->load();
			return 0;
		}
	}
	return -ENOENT;
}

static void usb_config_module_unload(void)
{
	dd_list *l;
	struct usb_config_ops *ops;

	config_plugin = NULL;

	DD_LIST_FOREACH(config_list, l, ops) {
		if (ops->is_valid && ops->is_valid()) {
			if (ops->release)
				ops->release();
		}
	}
}

static int usb_config_init(void)
{
	if (!config_plugin) {
		_E("There is no usb config plugin");
		return 0;
	}

	if (config_plugin->init == NULL) {
		_E("There is no usb config init function");
		return 0;
	}

	return config_plugin->init(NULL);
}

static void usb_config_deinit(void)
{
	if (!config_plugin) {
		_E("There is no usb config plugin");
		return;
	}

	if (config_plugin->deinit == NULL) {
		_E("There is no usb config deinit function");
		return;
	}

	config_plugin->deinit(NULL);
}

static usb_mode_e usb_get_selected_mode(void)
{
	if (usb_tethering_state())
		return USB_MODE_TETHERING;

	return USB_MODE_DEFAULT;
}

static const char *usb_get_mode_str(usb_mode_e mode)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(usb_modes) ; i++)
		if (mode == usb_modes[i].mode)
			return usb_modes[i].mode_str;

	return NULL;
}

static usb_mode_e usb_get_mode_int(const char *mode)
{
	int i;
	size_t len;

	if (!mode)
		return USB_MODE_NONE;

	len = strlen(mode) + 1;
	for (i = 0 ; i < ARRAY_SIZE(usb_modes) ; i++)
		if (!strncmp(mode, usb_modes[i].mode_str, len))
			return usb_modes[i].mode;

	return USB_MODE_NONE;
}

static int usb_config_enable(void)
{
	char *mode;
	int ret;
	int mode_e;

	if (!config_plugin) {
		_E("There is no usb config plugin");
		return 0;
	}

	if (config_plugin->enable == NULL) {
		_E("There is no usb config enable function");
		return 0;
	}

	mode_e = usb_get_selected_mode();
	mode = (char *)usb_get_mode_str(mode_e);
	if (!mode) {
		_E("Failed to get selected usb mode");
		return -ENOENT;
	}

	ret = config_plugin->enable(mode);
	if (ret < 0)
		return ret;

	return mode_e;
}

static int usb_config_disable(void)
{
	if (!config_plugin) {
		_E("There is no usb config plugin");
		return 0;
	}

	if (config_plugin->disable == NULL) {
		_E("There is no usb config disable function");
		return 0;
	}

	return config_plugin->disable(NULL);
}

static void usb_state_send_system_event(int state)
{
	bundle *b;
	const char *str;

	if (state == VCONFKEY_SYSMAN_USB_DISCONNECTED)
		str = EVT_VAL_USB_DISCONNECTED;
	else if (state == VCONFKEY_SYSMAN_USB_CONNECTED)
		str = EVT_VAL_USB_CONNECTED;
	else if (state == VCONFKEY_SYSMAN_USB_AVAILABLE)
		str = EVT_VAL_USB_AVAILABLE;
	else
		return;

	_I("system_event (%s)", str);

	b = bundle_create();
	bundle_add_str(b, EVT_KEY_USB_STATUS, str);
	eventsystem_send_system_event(SYS_EVENT_USB_STATUS, b);
	bundle_free(b);
}


static void usb_change_state(int val, usb_mode_e mode_e)
{
	static int old_status = -1;
	static int old_mode = -1;
	int mode, legacy_mode;
	static int noti_id = -1;

	switch (val) {
	case VCONFKEY_SYSMAN_USB_DISCONNECTED:
	case VCONFKEY_SYSMAN_USB_CONNECTED:
		mode = SET_USB_NONE;
		legacy_mode = SETTING_USB_NONE_MODE;
		if (noti_id >= 0) {
			remove_notification("MediaDeviceNotiOff", noti_id);
			noti_id = -1;
		}
		break;
	case VCONFKEY_SYSMAN_USB_AVAILABLE:
		switch (mode_e) {
		case USB_MODE_TETHERING:
			mode = SET_USB_RNDIS;
			legacy_mode = SETTING_USB_TETHERING_MODE;
			break;
		case USB_MODE_DEFAULT:
		default:
			mode = SET_USB_DEFAULT;
			legacy_mode = SETTING_USB_DEFAULT_MODE;
			if (noti_id < 0)
				noti_id = add_notification("MediaDeviceNotiOn");
			if (noti_id < 0)
				_E("Failed to show notification for usb connection");
			break;
		}
		break;
	default:
		return;
	}

	if (old_status != val) {
		usb_state_send_system_event(val);
		vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS, val);
		old_status = val;
	}

	if (old_mode != mode) {
		vconf_set_int(VCONFKEY_USB_CUR_MODE, mode);
		vconf_set_int(VCONFKEY_SETAPPL_USB_MODE_INT, legacy_mode);
		old_mode = mode;
	}
}

int usb_change_mode(char *name)
{
	int ret;
	usb_mode_e mode_e;

	if (!name)
		return -EINVAL;

	if (!config_plugin) {
		_E("There is no usb config plugin");
		return -ENOENT;
	}

	if (config_plugin->change == NULL) {
		_E("There is no usb config change function");
		return -ENOENT;
	}

	_I("USB mode change to (%s) is requested", name);
	usb_change_state(VCONFKEY_SYSMAN_USB_CONNECTED, USB_MODE_NONE);

	ret = config_plugin->change(name);
	if (ret < 0) {
		_E("Failed to change usb mode (%d)", ret);
		return ret;
	}

	mode_e = usb_get_mode_int(name);
	usb_change_state(VCONFKEY_SYSMAN_USB_AVAILABLE, mode_e);

	return 0;
}

static int usb_state_changed(int status)
{
	static int old = -1;	/* to update at the first time */
	int ret;

	_I("USB state is changed from (%d) to (%d)", old, status);

	if (old == status)
		return 0;

	switch (status) {
	case USB_CONNECTED:
		_I("USB cable is connected");
		usb_change_state(VCONFKEY_SYSMAN_USB_CONNECTED, USB_MODE_NONE);
		ret = usb_config_enable();
		if (ret < 0) {
			_E("Failed to enable usb config (%d)", ret);
			break;
		}
		usb_change_state(VCONFKEY_SYSMAN_USB_AVAILABLE, ret);
		pm_lock_internal(INTERNAL_LOCK_USB,
				LCD_OFF, STAY_CUR_STATE, 0);
		break;
	case USB_DISCONNECTED:
		_I("USB cable is disconnected");
		usb_change_state(VCONFKEY_SYSMAN_USB_DISCONNECTED, USB_MODE_NONE);
		ret = usb_config_disable();
		if (ret != 0)
			_E("Failed to disable usb config (%d)", ret);
		pm_unlock_internal(INTERNAL_LOCK_USB,
				LCD_OFF, STAY_CUR_STATE);
		break;
	default:
		_E("Invalid USB state(%d)", status);
		return -EINVAL;
	}
	if (ret < 0)
		_E("Failed to operate usb connection(%d)", ret);
	else
		old = status;

	return ret;
}

static void usb_init(void *data)
{
	int ret;

	ret = usb_config_module_load();
	if (ret < 0) {
		_E("Failed to get config module (%d)", ret);
		return;
	}

	ret = usb_config_init();
	if (ret < 0)
		_E("Failed to initialize usb configuation");

	add_usb_tethering_handler();

	ret = usb_state_changed(extcon_usb_ops.status);
	if (ret < 0)
		_E("Failed to update usb status(%d)", ret);
}

static void usb_exit(void *data)
{
	remove_usb_tethering_handler();
	usb_change_state(VCONFKEY_SYSMAN_USB_DISCONNECTED, USB_MODE_NONE);
	usb_config_deinit();
	usb_config_module_unload();
}

struct extcon_ops extcon_usb_ops = {
	.name	= EXTCON_CABLE_USB,
	.init	= usb_init,
	.exit	= usb_exit,
	.update = usb_state_changed,
};

EXTCON_OPS_REGISTER(extcon_usb_ops)
