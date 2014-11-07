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


#include <ITapiModem.h>
#include <TelPower.h>
#include <tapi_event.h>
#include <tapi_common.h>

#include <unistd.h>
#include <assert.h>
#include <vconf.h>

#include <device-node.h>
#include "dd-deviced.h"
#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "display/core.h"
#include "power/power-handler.h"

#define PREDEF_FLIGHT_MODE	"flightmode"
#define PREDEF_ENTERSLEEP	"entersleep"
#define PREDEF_LEAVESLEEP	"leavesleep"

#define POWER_RESTART		5

static TapiHandle *tapi_handle = NULL;
static Ecore_Timer *poweroff_timer_id = NULL;
static int reboot_opt;

static Eina_Bool telephony_powerdown_ap_internal(void *data)
{
	powerdown_ap(data);
	return EINA_FALSE;
}
static void telephony_powerdown_ap(TapiHandle *handle, const char *noti_id, void *data, void *user_data)
{
	telephony_powerdown_ap_internal(data);
}

static void telephony_restart_ap(TapiHandle *handle, const char *noti_id, void *data, void *user_data)
{
	restart_ap((void *)reboot_opt);
}

static Eina_Bool telephony_restart_ap_by_force(void *data)
{
	if (poweroff_timer_id) {
		ecore_timer_del(poweroff_timer_id);
		poweroff_timer_id = NULL;
	}
	restart_ap(data);
	return EINA_TRUE;
}

static void powerdown_res_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	_D("poweroff command request : %d",result);
}

static Eina_Bool telephony_powerdown_ap_by_force(void *data)
{
	if (poweroff_timer_id) {
		ecore_timer_del(poweroff_timer_id);
		poweroff_timer_id = NULL;
	}
	powerdown_ap(data);
	return EINA_TRUE;
}

static int telephony_start(enum device_flags flags)
{
	int ready = 0;

	if (tapi_handle) {
		_I("already initialized");
		return 0;
	}
	if (vconf_get_bool(VCONFKEY_TELEPHONY_READY,&ready) != 0 || ready != 1) {
		_E("fail to get %s(%d)", VCONFKEY_TELEPHONY_READY, ready);
		return -EINVAL;
	}
	tapi_handle = tel_init(NULL);
	if (tapi_handle == NULL) {
		_E("tapi init error");
		return -EINVAL;
	}
	return 0;
}

static int telephony_stop(enum device_flags flags)
{
	int ret;

	ret = tel_deregister_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER);
	if (ret != TAPI_API_SUCCESS)
		_E("tel_deregister_noti_event is not subscribed. error %d", ret);

	ret = tel_deinit(tapi_handle);
	if (ret != 0) {
		_E("fail to deinit");
		return -EINVAL;
	}
	tapi_handle = NULL;
	return 0;
}

static void telephony_exit(void *data)
{
	int ret;

	if (!data) {
		_E("Option Failed");
		return;
	}

	if (!strncmp(data, POWER_POWEROFF, POWER_POWEROFF_LEN)) {
		_I("Terminate");
		ret = tel_register_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER,
				telephony_powerdown_ap, NULL);
		if (ret != TAPI_API_SUCCESS) {
			_E("tel_register_event is not subscribed. error %d", ret);
			telephony_powerdown_ap_by_force(NULL);
			return;
		}
		ret = tel_process_power_command(tapi_handle, TAPI_PHONE_POWER_OFF,
				powerdown_res_cb, NULL);
		if (ret != TAPI_API_SUCCESS) {
			_E("tel_process_power_command() error %d\n", ret);
			telephony_powerdown_ap_by_force(NULL);
			return;
		}
		poweroff_timer_id = ecore_timer_add(15,
		    telephony_powerdown_ap_internal, NULL);
		return;
	}

	if (strncmp(data, POWER_REBOOT, POWER_REBOOT_LEN) &&
	    strncmp(data, POWER_RECOVERY, POWER_RECOVERY_LEN) &&
	    strncmp(data, POWER_FOTA, POWER_FOTA_LEN)) {
		_E("Fail %s", data);
		return;
	}

	_I("Option: %s", data);
	 if (!strncmp(data, POWER_RECOVERY, POWER_RECOVERY_LEN))
		reboot_opt = SYSTEMD_STOP_POWER_RESTART_RECOVERY;
	else if (!strncmp(data, POWER_REBOOT, POWER_REBOOT_LEN))
		reboot_opt = SYSTEMD_STOP_POWER_RESTART;
	else if (!strncmp(data, POWER_FOTA, POWER_FOTA_LEN))
		reboot_opt = SYSTEMD_STOP_POWER_RESTART_FOTA;

	ret = tel_register_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER,
			telephony_restart_ap, NULL);
	if (ret != TAPI_API_SUCCESS) {
		_E("tel_register_event is not subscribed. error %d", ret);
		telephony_restart_ap_by_force((void *)POWER_RESTART);
		return;
	}
	ret = tel_process_power_command(tapi_handle, TAPI_PHONE_POWER_OFF,
			powerdown_res_cb, NULL);
	if (ret != TAPI_API_SUCCESS) {
		_E("tel_process_power_command() error %d", ret);
		telephony_restart_ap_by_force((void *)reboot_opt);
		return;
	}
	poweroff_timer_id = ecore_timer_add(15,telephony_restart_ap_by_force,
							(void *)reboot_opt);
}

static void telephony_flight_mode_on(TapiHandle *handle, int result, void *data, void *user_data)
{
	int ret;
	int bCurFlightMode = 0;

	if (result != TAPI_POWER_FLIGHT_MODE_ENTER) {
		_E("flight mode enter failed %d", result);
		return;
	}
	_D("enter flight mode result : %d", result);
	ret = vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &bCurFlightMode);
	if (ret == 0)
		_D("Flight Mode is %d", bCurFlightMode);
	else
		_E("failed to get vconf key");
}

static void telephony_flight_mode_off(TapiHandle *handle, int result, void *data, void *user_data)
{
	int ret;
	int bCurFlightMode = 0;

	if (result != TAPI_POWER_FLIGHT_MODE_LEAVE) {
		_E("flight mode leave failed %d", result);
		return;
	}
	_D("leave flight mode result : %d", result);
	ret = vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &bCurFlightMode);
	if (ret == 0)
		_D("Flight Mode is %d", bCurFlightMode);
	else
		_E("failed to get vconf key");
}

static int telephony_execute(void *data)
{
	int ret;
	int mode = *(int *)(data);
	int err = TAPI_API_SUCCESS;

	if (tapi_handle == NULL) {
		ret = telephony_start(NORMAL_MODE);
		if (ret != 0) {
			_E("fail to get tapi handle");
			return -1;
		}
	}

	if (mode == 1) {
		err = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_LEAVE,
				telephony_flight_mode_off, NULL);
	} else if (mode == 0) {
		err = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_ENTER,
				telephony_flight_mode_on, NULL);
	}
	if (err != TAPI_API_SUCCESS)
		_E("FlightMode tel api action failed %d",err);

	return 0;
}

static int telephony_flight_mode(int argc, char **argv)
{
	int mode;

	if (argc != 1 || argv[0] == NULL) {
		_E("FlightMode Set predefine action failed");
		return -1;
	}
	mode = atoi(argv[0]);
	telephony_execute(&mode);
	return 0;
}

static int telephony_enter_sleep(int argc, char **argv)
{
	int ret;

	pm_change_internal(getpid(), LCD_NORMAL);
	sync();

	/* flight mode
	 * TODO - add check, cb, etc...
	 * should be checked wirh telephony part */
	ret = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_ENTER,
			telephony_flight_mode_on, NULL);
	_I("request for changing into flight mode : %d", ret);

	launch_evenif_exist("/etc/rc.d/rc.entersleep", "");
	pm_change_internal(getpid(), POWER_OFF);

	return 0;
}

static int telephony_leave_sleep(int argc, char **argv)
{
	int ret;

	pm_change_internal(getpid(), LCD_NORMAL);
	sync();

	/* flight mode
	 * TODO - add check, cb, etc...
	 * should be checked wirh telephony part */
	ret = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_LEAVE,
			telephony_flight_mode_off, NULL);
	_I("request for changing into flight mode : %d", ret);

	return 0;
}

static DBusMessage *flight_mode_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	telephony_flight_mode(argc, (char **)&argv);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *telephony_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (tapi_handle == NULL) {
		if (telephony_start(NORMAL_MODE) != 0)
			_E("fail to get tapi handle");
	}

	if (!strncmp(type_str, PREDEF_ENTERSLEEP, strlen(PREDEF_ENTERSLEEP)))
		ret = telephony_enter_sleep(0, NULL);
	else if (!strncmp(type_str, PREDEF_LEAVESLEEP, strlen(PREDEF_LEAVESLEEP)))
		ret = telephony_leave_sleep(0, NULL);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_FLIGHT_MODE, "sis", "i", flight_mode_handler },
	{ PREDEF_ENTERSLEEP, "si", "i", telephony_handler },
	{ PREDEF_LEAVESLEEP, "si", "i", telephony_handler },
	/* Add methods here */
};

static void telephony_init(void *data)
{
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_POWER, edbus_methods,
			ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static const struct device_ops tel_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "telephony",
	.init     = telephony_init,
	.start    = telephony_start,
	.stop     = telephony_stop,
	.exit     = telephony_exit,
	.execute = telephony_execute,
};

DEVICE_OPS_REGISTER(&tel_device_ops)
