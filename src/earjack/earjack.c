/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <device-node.h>
#include <Ecore.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-handler.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "display/poll.h"

#define SIGNAL_EARJACK_STATE	"ChangedEarjack"

static int earjack_status;

static void earjack_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast earjack status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_EARJACK_STATE, "i", arr);
}

static void earjack_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARJACK_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}
	_I("jack - earjack changed %d", val);
	vconf_set_int(VCONFKEY_SYSMAN_EARJACK, val);
	earjack_send_broadcast(val);
	if (CONNECTED(val)) {
		extcon_set_count(EXTCON_EARJACK);
		internal_pm_change_state(LCD_NORMAL);
	}
}

static DBusMessage *dbus_get_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val;

	val = earjack_status;
	_D("get hall status %d", val);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &val);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "getstatus",       NULL,   "i", dbus_get_status },
	/* Add methods here */
};

static void earjack_init(void *data)
{
	int ret, val;

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARJACK_ONLINE, &val) == 0) {
		if (CONNECTED(val))
			extcon_set_count(EXTCON_EARJACK);
		vconf_set_int(VCONFKEY_SYSMAN_EARJACK, val);
	}

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static int earjack_get_status(void)
{
	return earjack_status;
}

static int earjack_execute(void *data)
{
	earjack_chgdet_cb(data);
	return 0;
}

static const struct device_ops earjack_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "earjack",
	.init     = earjack_init,
	.status   = earjack_get_status,
	.execute  = earjack_execute,
};

DEVICE_OPS_REGISTER(&earjack_device_ops)
