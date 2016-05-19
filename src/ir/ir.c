/*
 * deviced
 *
 * Copyright (c) 2014-2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stdbool.h>
#include <hw/ir.h>
#include "core/edbus-handler.h"
#include "core/devices.h"
#include "core/common.h"
#include "core/log.h"

static struct ir_device *ir_dev;

static DBusMessage *edbus_ir_is_available(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	bool val;

	if (!ir_dev)
		goto exit;

	ret = ir_dev->is_available(&val);
	if (ret >= 0)
		ret = val;

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_ir_transmit(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	int size;
	int *freq_pattern;

	if (!ir_dev) {
		ret = -ENODEV;
		goto exit;
	}

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &freq_pattern, &size,
				DBUS_TYPE_INVALID)) {
		ret = -EINVAL;
		goto exit;
	}

	_I("frequency : %d, pattern_size: %d", freq_pattern[0], size);

	ret = ir_dev->transmit(freq_pattern, size);

exit:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "IRIsAvailable", NULL, "i", edbus_ir_is_available},
	{ "TransmitIR", "ai", "i", edbus_ir_transmit},
};

static int ir_probe(void *data)
{
	struct hw_info *info;
	int ret;

	if (ir_dev)
		return 0;

	ret = hw_get_info(IR_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);

	if (ret < 0) {
		_E("Fail to load ir(%d)", ret);
		return -ENODEV;
	}

	if (!info->open) {
		_E("Failed to open ir device; open(NULL)");
		return -ENODEV;
	}

	ret = info->open(info, NULL, (struct hw_common**)&ir_dev);
	if (ret < 0) {
		_E("Failed to get ir device structure (%d)", ret);
		return ret;
	}

	_I("ir device structure load success");
	return 0;
}

static void ir_init(void *data)
{
	int ret;

	ret = register_edbus_interface_and_method(DEVICED_PATH_IR,
			DEVICED_INTERFACE_IR,
			edbus_methods, ARRAY_SIZE(edbus_methods), false);

	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static void ir_exit(void *data)
{
	struct hw_info *info;

	if (!ir_dev)
		return;

	info = ir_dev->common.info;
	if (!info)
		free(ir_dev);
	else
		info->close((struct hw_common *)ir_dev);

	ir_dev = NULL;
}

static const struct device_ops ir_device_ops = {
	.name     = "ir",
	.probe    = ir_probe,
	.init     = ir_init,
	.exit     = ir_exit,
};

DEVICE_OPS_REGISTER(&ir_device_ops)

