/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
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
#include <errno.h>
#include <assert.h>
#include <vconf.h>
#include <device-node.h>
#include <hw/led.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "torch.h"

#define LED_MAX_BRIGHTNESS      100
#define GET_BRIGHTNESS(x)       (((x) >> 24) & 0xFF)

#define SIGNAL_FLASH_STATE "ChangeFlashState"

static struct led_device *led_dev;
static struct led_state led_state = {
	.type = LED_TYPE_MANUAL,
	.color = 0x0,
	.duty_on = 0,
	.duty_off = 0,
};

static void flash_state_broadcast(int val)
{
	char *arr[1];
	char str_state[32];

	snprintf(str_state, sizeof(str_state), "%d", val);
	arr[0] = str_state;

	broadcast_edbus_signal(DEVICED_PATH_LED, DEVICED_INTERFACE_LED,
			SIGNAL_FLASH_STATE, "i", arr, false);
}

static DBusMessage *edbus_get_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret, alpha;

	if (!led_dev) {
		_E("there is no led device");
		ret = -ENOENT;
		goto error;
	}

	alpha = GET_BRIGHTNESS(led_state.color);
	ret = alpha * 100.f / 255;
	if (alpha != 0 && alpha != 0xFF)
		ret += 1;
	_D("color : val(%d), color(%x)", ret, led_state.color);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_get_max_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = LED_MAX_BRIGHTNESS;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_set_brightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, enable, ret;
	struct led_state tmp = {0,};

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &val,
			DBUS_TYPE_INT32, &enable, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (!led_dev) {
		_E("there is no led device");
		ret = -ENOENT;
		goto error;
	}

	tmp.color = (((int)(val * 255.f) / LED_MAX_BRIGHTNESS) & 0xFF) << 24;
	_D("color : val(%d), color(%x)", val, tmp.color);

	ret = led_dev->set_state(&tmp);
	if (ret < 0)
		goto error;

	memcpy(&led_state, &tmp, sizeof(led_state));

	/* flash status broadcast */
	flash_state_broadcast(val);

	/* if enable is ON, noti will be show or hide */
	if (enable) {
		if (val)
			ongoing_show();
		else
			ongoing_clear();
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	/**
	 * GetBrightnessForCamera is for camera library.
	 * Currently they do not have a daemon for camera,
	 * but they need to get camera brightness value without led priv.
	 * It's a temporary solution on Tizen 2.4 and will be removed asap.
	 */
	{ "GetBrightnessForCamera", NULL,   "i", edbus_get_brightness },
	{ "GetBrightness",    NULL,   "i", edbus_get_brightness },
	{ "GetMaxBrightness", NULL,   "i", edbus_get_max_brightness },
	{ "SetBrightness",    "ii",   "i", edbus_set_brightness },
	/* Add methods here */
};

static int led_service_load(void)
{
	struct hw_info *info;
	int r;

	r = hw_get_info(LED_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);
	if (r < 0) {
		_E("fail to load led shared library : %d", r);
		return -ENOENT;
	}

	if (!info->open) {
		_E("fail to open camera led device : open(NULL)");
		return -EPERM;
	}

	r = info->open(info, LED_ID_CAMERA_BACK,
			(struct hw_common **)&led_dev);
	if (r < 0) {
		_E("fail to get camera led device : %d", r);
		return -EPERM;
	}

	_D("camera led device structure load success");
	return 0;
}

static int led_service_free(void)
{
	struct hw_info *info;

	if (!led_dev)
		return -ENOENT;

	info = led_dev->common.info;

	assert(info);

	info->close((struct hw_common *)led_dev);

	return 0;
}

static int torch_probe(void *data)
{
	/* load led device */
	return led_service_load();
}

static void torch_init(void *data)
{
	int ret;

	/* init dbus interface */
	ret = register_edbus_interface_and_method(DEVICED_PATH_LED,
			DEVICED_INTERFACE_LED,
			edbus_methods, ARRAY_SIZE(edbus_methods), false);
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static void torch_exit(void *data)
{
	/* free led device */
	led_service_free();
}

static const struct device_ops torchled_device_ops = {
	.name     = "torchled",
	.probe    = torch_probe,
	.init     = torch_init,
	.exit     = torch_exit,
};

DEVICE_OPS_REGISTER(&torchled_device_ops)
