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
#include "core/list.h"
#include "core/common.h"
#include "core/device-notifier.h"
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

	/* TODO:
	 * parameter "DEFAULT" can be changed */
	return config_plugin->init("DEFAULT");
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

	/* TODO:
	 * parameter "DEFAULT" can be changed */
	config_plugin->deinit("DEFAULT");
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

	/* TODO:
	 * parameter "DEFAULT" can be changed */
	return config_plugin->enable("DEFAULT");
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

	/* TODO:
	 * parameter "DEFAULT" can be changed */
	return config_plugin->disable("DEFAULT");
}

static int usb_state_changed(void *data)
{
	static int state = USB_DISCONNECTED;
	int input, ret;

	if (!data)
		return -EINVAL;

	input = *(int *)data;

	_I("USB state is changed from (%d) to (%d)", state, input);

	if (state == input)
		return 0;

	switch (input) {
	case USB_CONNECTED:
		_I("USB cable is connected");
		ret = usb_config_enable();
		break;
	case USB_DISCONNECTED:
		_I("USB cable is disconnected");
		ret = usb_config_disable();
		break;
	default:
		_E("Invalid USB state(%d)", state);
		return -EINVAL;
	}
	if (ret < 0)
		_E("Failed to operate usb connection(%d)", ret);
	else
		state = input;

	return ret;
}

static void usb_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_USB, usb_state_changed);

	ret = usb_config_module_load();
	if (ret < 0) {
		_E("Failed to get config module (%d)", ret);
		return;
	}

	ret = usb_config_init();
	if (ret < 0)
		_E("Failed to initialize usb configuation");

	ret = usb_state_changed(&(extcon_usb_ops.status));
	if (ret < 0)
		_E("Failed to update usb status(%d)", ret);
}

static void usb_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_USB, usb_state_changed);
	usb_config_deinit();
	usb_config_module_unload();
}

struct extcon_ops extcon_usb_ops = {
	.name	= EXTCON_CABLE_USB,
	.noti	= DEVICE_NOTIFIER_USB,
	.init	= usb_init,
	.exit	= usb_exit,
};

EXTCON_OPS_REGISTER(&extcon_usb_ops)
