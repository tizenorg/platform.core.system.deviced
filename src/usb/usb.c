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

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "display/poll.h"
#include "extcon/extcon.h"
#include "usb.h"

enum usb_state {
	USB_DISCONNECTED,
	USB_CONNECTED,
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
		return -ENOENT;
	}

	if (config_plugin->init == NULL) {
		_E("There is no usb config init function");
		return -ENOENT;
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

static int usb_config_enable(void)
{
	if (!config_plugin) {
		_E("There is no usb config plugin");
		return -ENOENT;
	}

	if (config_plugin->enable == NULL) {
		_E("There is no usb config enable function");
		return -ENOENT;
	}

	return config_plugin->enable(NULL);
}

static int usb_config_disable(void)
{
	if (!config_plugin) {
		_E("There is no usb config plugin");
		return -ENOENT;
	}

	if (config_plugin->disable == NULL) {
		_E("There is no usb config disable function");
		return -ENOENT;
	}

	return config_plugin->disable(NULL);
}

static void usb_change_state(int val)
{
	static int old_status = -1;
	static int old_mode = -1;
	int mode, legacy_mode;

	switch (val) {
	case VCONFKEY_SYSMAN_USB_DISCONNECTED:
	case VCONFKEY_SYSMAN_USB_CONNECTED:
		mode = SET_USB_NONE;
		legacy_mode = SETTING_USB_NONE_MODE;
		break;
	case VCONFKEY_SYSMAN_USB_AVAILABLE:
		mode = SET_USB_DEFAULT;
		legacy_mode = SETTING_USB_DEFAULT_MODE;
		break;
	default:
		return;
	}

	if (old_status != val) {
		vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS, val);
		old_status = val;
	}

	if (old_mode != mode) {
		vconf_set_int(VCONFKEY_USB_CUR_MODE, mode);
		vconf_set_int(VCONFKEY_SETAPPL_USB_MODE_INT, legacy_mode);
		old_mode = mode;
	}
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
		usb_change_state(VCONFKEY_SYSMAN_USB_CONNECTED);
		ret = usb_config_enable();
		if (ret != 0) {
			_E("Failed to enable usb config (%d)", ret);
			break;
		}
		usb_change_state(VCONFKEY_SYSMAN_USB_AVAILABLE);
		pm_lock_internal(INTERNAL_LOCK_USB,
				LCD_OFF, STAY_CUR_STATE, 0);
		break;
	case USB_DISCONNECTED:
		_I("USB cable is disconnected");
		usb_change_state(VCONFKEY_SYSMAN_USB_DISCONNECTED);
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

	ret = usb_state_changed(extcon_usb_ops.status);
	if (ret < 0)
		_E("Failed to update usb status(%d)", ret);
}

static void usb_exit(void *data)
{
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
