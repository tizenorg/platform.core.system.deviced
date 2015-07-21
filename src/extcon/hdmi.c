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


#include <vconf.h>
#include "core/log.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "display/core.h"
#include "extcon.h"

#define METHOD_GET_HDMI		"GetHDMI"
#define SIGNAL_HDMI_STATE	"ChangedHDMI"

static struct extcon_ops hdmi_extcon_ops;

static void hdmi_send_broadcast(int status)
{
	static int old;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast hdmi status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_STATE, "i", arr);
}

static int hdmi_update(int status)
{
	_I("jack - hdmi changed %d", status);
	pm_change_internal(getpid(), LCD_NORMAL);
	vconf_set_int(VCONFKEY_SYSMAN_HDMI, status);
	hdmi_send_broadcast(status);

	if (status == 1)
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, STAY_CUR_STATE, 0);
	else
		pm_unlock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, PM_SLEEP_MARGIN);

	return 0;
}

static int display_changed(void *data)
{
	enum state_t state;
	int hdmi;

	if (!data)
		return 0;

	state = *(int *)data;
	if (state != S_NORMAL)
		return 0;

	hdmi = hdmi_extcon_ops.status;
	if (hdmi == 0) {
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, STAY_CUR_STATE, 0);
		_I("hdmi is connected! dim lock is on.");
	}
	return 0;
}

static DBusMessage *dbus_hdmi_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	return make_reply_message(msg, hdmi_extcon_ops.status);
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_GET_HDMI,       NULL, "i", dbus_hdmi_handler },
};

static void hdmi_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static void hdmi_exit(void *data)
{
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);
}

static struct extcon_ops hdmi_extcon_ops = {
	.name   = EXTCON_CABLE_HDMI,
	.init   = hdmi_init,
	.exit   = hdmi_exit,
	.update = hdmi_update,
};

EXTCON_OPS_REGISTER(hdmi_extcon_ops)
