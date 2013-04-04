/*
 *  deviced
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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
 *
*/


/**
 * @file	display-dbus.c
 * @brief	dbus interface
 *
 */

#include <Ecore.h>

#include "util.h"
#include "core.h"
#include "device-node.h"
#include "core/common.h"

#define DISP_INDEX_BIT 4
#define COMBINE_DISP_CMD(cmd, prop, index) (cmd = (prop | (index << DISP_INDEX_BIT)))

static DBusMessage* e_dbus_start_cb(E_DBus_Object *obj, DBusMessage* msg)
{
	start_pm_main();
	return dbus_message_new_method_return(msg);
}

static DBusMessage* e_dbus_stop_cb(E_DBus_Object *obj, DBusMessage* msg)
{
	end_pm_main();
	return dbus_message_new_method_return(msg);
}

static DBusMessage* e_dbus_getbrightness_cb(E_DBus_Object *obj, DBusMessage* msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	int cmd;
	int brightness = -1;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brightness);

	LOGINFO("get brightness %d, %d", brightness, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &brightness);

	return reply;
}

static DBusMessage* e_dbus_setbrightness_cb(E_DBus_Object *obj, DBusMessage* msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	int cmd;
	int brightness;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brightness);

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brightness);

	LOGINFO("set brightness %d, %d", brightness, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static struct edbus_method {
	const char *member;
	const char *signature;
	const char *reply_signature;
	E_DBus_Method_Cb func;
} edbus_methods[] = {
        { "start",           NULL,  NULL, e_dbus_start_cb },
        { "stop",            NULL,  NULL, e_dbus_stop_cb },
        { "getbrightness",   NULL,   "i", e_dbus_getbrightness_cb },
        { "setbrightness",    "i",   "i", e_dbus_setbrightness_cb },
        /* Add methods here */
};

int init_pm_dbus(void)
{
	E_DBus_Interface *iface;
	int ret;
	int i;

	iface = get_edbus_interface(DEVICED_PATH_DISPLAY);

	LOGINFO("%s, %x", DEVICED_PATH_DISPLAY, iface);

	if (!iface) {
		LOGERR("fail to get edbus interface!");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(edbus_methods); i++) {
		ret = e_dbus_interface_method_add(iface,
				    edbus_methods[i].member,
				    edbus_methods[i].signature,
				    edbus_methods[i].reply_signature,
				    edbus_methods[i].func);
		if (!ret) {
			LOGERR("fail to add method %s!", edbus_methods[i].member);
			return -1;
		}
	}

	return 0;
}

