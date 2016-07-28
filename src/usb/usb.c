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

static struct usb_client_device *usb_dev;
struct extcon_ops extcon_usb_ops;

static int usb_probe(void)
{
	struct hw_info *info;
	int ret;

	if (usb_dev)
		return 0;

	ret = hw_get_info(USB_CLIENT_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);
	if (ret == 0) {
		if (!info->open) {
			_E("Failed to open extcon device; open(NULL)");
			return -ENODEV;
		}

		ret = info->open(info, NULL, (struct hw_common **)&usb_dev);
		if (ret < 0) {
			_E("Failed to get usb client device structure (%d)", ret);
			return ret;
		}

		_I("usb client device structure load success");
		return 0;
	}

	/* TODO default operations in case that usb client HAL does not exist */

	return -ENOENT;
}

static int usb_config_init(void)
{
	int ret;
	unsigned int mode;

	if (usb_dev) {
		if (!usb_dev->init_gadgets) {
			_I("There in no operation for init usb gadgets");
			return 0;
		}

		mode = usb_state_get_selected_mode();
		ret = usb_dev->init_gadgets(mode);
		if (ret < 0) {
			_E("Failed to init usb gadgets (%d)", ret);
			return ret;
		}

		return 0;
	}

	/* TODO add operations in case that HAL does not exist */
	return -ENODEV;
}

static void usb_config_deinit(void)
{
	int ret;

	if (usb_dev) {
		if (!usb_dev->deinit_gadgets) {
			_I("There in no operation for deinit usb gadgets");
			return;
		}

		ret = usb_dev->deinit_gadgets();
		if (ret < 0)
			_E("Failed to deinit usb gadgets");

		return;
	}

	/* TODO add operations in case that HAL does not exist */
}

static int usb_config_enable(unsigned int mode)
{
	int ret;
	char mode_str[128];

	if (usb_dev) {
		usb_state_get_mode_str(mode, mode_str, sizeof(mode_str));

		if (usb_dev->gadgets_supported &&
			!usb_dev->gadgets_supported(mode)) {
			_E("Not supported gadget composite(%s)", mode_str);
			return -ENOTSUP;
		}

		if (!usb_dev->enable_gadgets) {
			_E("usb gadgets enable function does not exist");
			return -ENOENT;
		}

		ret = usb_dev->enable_gadgets(mode);
		if (ret < 0) {
			_E("Failed to enabld usb gadgets(%s, ret:%d)", mode_str, ret);
			return ret;
		}

		_I("Success to enable usb gadgets (%s)", mode_str);
		return 0;
	}

	/* TODO add operations in case that HAL does not exist */
	return -ENODEV;
}

static int usb_config_disable(void)
{
	int ret;

	if (usb_dev) {
		if (!usb_dev->disable_gadgets) {
			_E("usb gadgets disable function does not exist");
			return -ENOENT;
		}

		ret = usb_dev->disable_gadgets();
		if (ret < 0) {
			_E("Failed to disable usb gadgets(ret:%d)", ret);
			return ret;
		}

		_I("Success to disable usb gadgets");
		return 0;
	}

	/* TODO add operations in case that HAL does not exist */
	return -ENODEV;
}

int usb_change_mode(unsigned int mode)
{
	int ret;
	char mode_str[128];
	usb_state_get_mode_str(mode, mode_str, sizeof(mode_str));

	_I("USB mode change to (%s) is requested", mode_str);

	if (usb_dev) {
		if (usb_dev->gadgets_supported &&
			!usb_dev->gadgets_supported(mode)) {
			_E("Not supported usb gadgets (%s)", mode_str);
			return -ENOTSUP;
		}

		usb_operation_stop(usb_state_get_current_mode());
		usb_state_update_state(USB_CONNECTED, USB_GADGET_NONE);

		ret = usb_config_disable();
		if (ret < 0) {
			_E("Failed to disable usb gadgets (%d)", ret);
			return ret;
		}

		ret = usb_config_enable(mode);
		if (ret < 0) {
			_E("Failed to enable usb gadgets (%d)", ret);
			return ret;
		}

		usb_state_update_state(USB_CONNECTED, mode);
		usb_operation_start(mode);

		usb_state_set_selected_mode(mode);
		return 0;
	}

	/* TODO add operations in case that HAL does not exist */
	return -ENODEV;
}

static void gadgets_enabled(unsigned int mode, void *data)
{
	if (usb_dev && usb_dev->unregister_gadgets_enabled_event)
		usb_dev->unregister_gadgets_enabled_event(gadgets_enabled);
	if (mode == USB_GADGET_NONE)
		mode = (unsigned int)data;

	_I("Real USB configured");
	usb_state_update_state(USB_CONNECTED, USB_GADGET_NONE);
	usb_operation_start(mode);
	usb_state_update_state(USB_CONNECTED, mode);
	pm_lock_internal(INTERNAL_LOCK_USB,
			LCD_OFF, STAY_CUR_STATE, 0);
}

static int usb_state_changed(int status)
{
	static int old = -1;	/* to update at the first time */
	int ret, cb = -1;
	unsigned int mode;

	_I("USB state is changed from (%d) to (%d)", old, status);

	if (old == status)
		return 0;

	switch (status) {
	case USB_CONNECTED:
		_I("USB cable is connected");
		mode = usb_state_get_selected_mode();
		if (usb_dev && usb_dev->register_gadgets_enabled_event)
			cb = usb_dev->register_gadgets_enabled_event(gadgets_enabled, (void *)mode);
		else
			usb_state_update_state(USB_CONNECTED, USB_GADGET_NONE);
		ret = usb_config_enable(mode);
		if (ret < 0) {
			_E("Failed to enable usb config (%d)", ret);
			break;
		}
		if (cb == 0)
			break;
		usb_operation_start(mode);
		usb_state_update_state(USB_CONNECTED, mode);
		pm_lock_internal(INTERNAL_LOCK_USB,
				LCD_OFF, STAY_CUR_STATE, 0);
		break;
	case USB_DISCONNECTED:
		_I("USB cable is disconnected");
		if (usb_dev && usb_dev->unregister_gadgets_enabled_event)
			usb_dev->unregister_gadgets_enabled_event(gadgets_enabled);
		usb_operation_stop(usb_state_get_current_mode());
		usb_state_update_state(USB_DISCONNECTED, USB_GADGET_NONE);
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

	ret = usb_probe();
	if (ret < 0) {
		_I("USB client cannot be used (%d)", ret);
		return;
	}

	ret = usb_config_init();
	if (ret < 0)
		_E("Failed to initialize usb configuation");

	usb_state_retrieve_selected_mode();

	add_usb_tethering_handler();

	ret = usb_state_changed(extcon_usb_ops.status);
	if (ret < 0)
		_E("Failed to update usb status(%d)", ret);
}

static void usb_exit(void *data)
{
	remove_usb_tethering_handler();
	usb_state_update_state(USB_DISCONNECTED, USB_GADGET_NONE);
	usb_config_deinit();
}

struct extcon_ops extcon_usb_ops = {
	.name	= EXTCON_CABLE_USB,
	.init	= usb_init,
	.exit	= usb_exit,
	.update = usb_state_changed,
};

EXTCON_OPS_REGISTER(extcon_usb_ops)
