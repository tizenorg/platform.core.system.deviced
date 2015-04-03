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
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "core/device-handler.h"
#include "display/core.h"
#include "extcon/extcon.h"

#define METHOD_GET_CRADLE	"GetCradle"
#define SIGNAL_CRADLE_STATE	"ChangedCradle"

static struct extcon_ops cradle_extcon_ops;

static void cradle_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast cradle status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_CRADLE_STATE, "i", arr);
}

static int cradle_update(int status)
{
	_I("jack - cradle changed %d", status);
	pm_change_internal(getpid(), LCD_NORMAL);
	cradle_send_broadcast(status);
	if (vconf_set_int(VCONFKEY_SYSMAN_CRADLE_STATUS, status) != 0) {
		_E("failed to set vconf status");
		return -EIO;
	}

	if (status == DOCK_SOUND)
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
	else if (status == DOCK_NONE)
		pm_unlock_internal(getpid(), LCD_DIM, PM_SLEEP_MARGIN);

	return 0;
}

static int display_changed(void *data)
{
	enum state_t state;
	int cradle;

	if (!data)
		return 0;

	state = *(int *)data;
	if (state != S_NORMAL)
		return 0;

	cradle = cradle_extcon_ops.status;
	if (cradle == DOCK_SOUND) {
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
		_I("sound dock is connected! dim lock is on.");
	}

	return 0;
}

static DBusMessage *dbus_cradle_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, cradle_extcon_ops.status);
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_GET_CRADLE,     NULL, "i", dbus_cradle_handler },
};

static void cradle_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static void cradle_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);
}

static struct extcon_ops cradle_extcon_ops = {
	.name   = EXTCON_CABLE_DOCK,
	.init   = cradle_init,
	.update = cradle_update,
};

EXTCON_OPS_REGISTER(cradle_extcon_ops)
