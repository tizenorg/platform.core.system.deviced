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
#include <device-node.h>

#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/devices.h"

static DBusMessage *edbus_set_ir_command(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *str;
	int ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = device_set_property(DEVICE_TYPE_LED, PROP_LED_IR_COMMAND, (int)str);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "SetIrCommand",      "s",   "i", edbus_set_ir_command },
	/* Add methods here */
};

static void ir_init(void *data)
{
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_LED, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static const struct device_ops irled_device_ops = {
	.name     = "irled",
	.init     = ir_init,
};

DEVICE_OPS_REGISTER(&irled_device_ops)
