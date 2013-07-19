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


#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <vconf.h>
#include <assert.h>
#include <limits.h>
#include <heynoti.h>
#include <vconf.h>
#include <fcntl.h>
#include <ITapiModem.h>
#include <TelPower.h>
#include <tapi_event.h>
#include <tapi_common.h>
#include <syspopup_caller.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include "dd-deviced.h"
#include "core/log.h"
#include "core/launch.h"
#include "core/queue.h"
#include "core/device-handler.h"
#include "device-node.h"
#include "core/predefine.h"
#include "core/data.h"
#include "core/common.h"
#include "core/devices.h"
#include "proc/proc-handler.h"
#include "display/poll.h"
#include "display/setting.h"
#include "display/core.h"
#include "core/edbus-handler.h"

#define SIGNAL_NAME_POWEROFF_POPUP	"poweroffpopup"

#define PREDEF_ENTERSLEEP		"entersleep"
#define PREDEF_LEAVESLEEP		"leavesleep"
#define PREDEF_POWEROFF			"poweroff"
#define PREDEF_REBOOT			"reboot"
#define PREDEF_PWROFF_POPUP		"pwroff-popup"
#define PREDEF_INTERNAL_POWEROFF	"internal_poweroff"
#define PREDEF_FLIGHT_MODE		"flightmode"

#define POWEROFF_NOTI_NAME		"power_off_start"
#define POWEROFF_DURATION		2
#define MAX_RETRY			2

#define SIGNAL_POWEROFF_STATE	"ChangeState"

static struct timeval tv_start_poweroff;

static Ecore_Timer *poweroff_timer_id = NULL;
static TapiHandle *tapi_handle = NULL;
static int power_off = 0;

static void poweroff_popup_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *str;
	int val = 0;

	if (dbus_message_is_signal(msg, INTERFACE_NAME, SIGNAL_NAME_POWEROFF_POPUP) == 0) {
		_E("there is no power off popup signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == 0) {
		_E("there is no message");
		return;
	}

	if (strncmp(str, PREDEF_PWROFF_POPUP, strlen(PREDEF_PWROFF_POPUP)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_POPUP;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_POWEROFF)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_DIRECT;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_REBOOT)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_RESTART;
	if (val == 0) {
		_E("not supported message : %s", str);
		return;
	}
	vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);
}

static void poweroff_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast poweroff %d", status);

	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_POWEROFF, DEVICED_INTERFACE_POWEROFF,
			SIGNAL_POWEROFF_STATE, 'i', arr);
}

static void poweroff_control_cb(keynode_t *in_key, struct ss_main_data *ad)
{
	int val;
	if (vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &val) != 0)
		return;

	if (val == VCONFKEY_SYSMAN_POWER_OFF_DIRECT || val == VCONFKEY_SYSMAN_POWER_OFF_RESTART)
		poweroff_send_broadcast(val);

	switch (val) {
	case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
		action_entry_call_internal(PREDEF_POWEROFF, 0);
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
		action_entry_call_internal(PREDEF_PWROFF_POPUP, 0);
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
		action_entry_call_internal(PREDEF_REBOOT, 0);
		break;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_POWEROFF, val);
}

/* umount usr data partition */
static void unmount_rw_partition()
{
	char buf[256];
	int ret;
	ret =  umount2("/opt/usr", MNT_DETACH);
	_D("/opt/usr unmount : %d", ret);
	sleep(1);
}

static void enter_flight_mode_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	int bCurFlightMode = 0;
	if (result != TAPI_POWER_FLIGHT_MODE_ENTER) {
		_E("flight mode enter failed %d",result);
	} else {
		_D("enter flight mode result : %d",result);
		if (vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE,&bCurFlightMode) == 0) {
			_D("Flight Mode is %d", bCurFlightMode);
		} else {
			_E("failed to get vconf key");
		}
	}
}

static void leave_flight_mode_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	int bCurFlightMode = 0;
	if (result != TAPI_POWER_FLIGHT_MODE_LEAVE) {
		_E("flight mode leave failed %d",result);
	} else {
		_D("leave flight mode result : %d",result);
		if (vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE,&bCurFlightMode) == 0) {
			_D("Flight Mode is %d", bCurFlightMode);
		} else {
			_E("failed to get vconf key");
		}
	}
}

static void tapi_handle_init(void)
{
	int ready = 0;
	if (vconf_get_bool(VCONFKEY_TELEPHONY_READY,&ready) != 0 || ready != 1) {
		_E("fail to get %s(%d)", VCONFKEY_TELEPHONY_READY, ready);
		return;
	}
	tapi_handle = tel_init(NULL);
	if (tapi_handle == NULL)
		_E("tapi init error");
}

int flight_mode_def_predefine_action(int argc, char **argv)
{
	int mode;
	int err = TAPI_API_SUCCESS;
	if (argc != 1 || argv[0] == NULL) {
		_E("FlightMode Set predefine action failed");
		return -1;
	}
	mode = atoi(argv[0]);

	if (tapi_handle == NULL)
		tapi_handle_init();

	if (mode == 1) {
		err = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_LEAVE, leave_flight_mode_cb, NULL);
	} else if (mode == 0) {
		err = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_ENTER, enter_flight_mode_cb, NULL);
	}
	if (err != TAPI_API_SUCCESS)
		_E("FlightMode tel api action failed %d",err);
	return 0;

}

static void __tel_init_cb(keynode_t *key_nodes,void *data)
{
	int bTelReady = 0;
	bTelReady = vconf_keynode_get_bool(key_nodes);
	if (bTelReady == 1) {
		vconf_ignore_key_changed(VCONFKEY_TELEPHONY_READY, (void*)__tel_init_cb);
		tapi_handle = tel_init(NULL);
		if (tapi_handle == NULL) {
			_E("tapi init error");
		}
	} else {
		_E("tapi is not ready yet");
	}
}

Eina_Bool powerdown_ap_by_force(void *data)
{
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	char *buf;

	if(tapi_handle != NULL)
	{
		tel_deinit(tapi_handle);
		tapi_handle = NULL;
	}
	/* Getting poweroff duration */
	buf = getenv("PWROFF_DUR");
	if (buf != NULL && strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60)
		poweroff_duration = POWEROFF_DURATION;

	gettimeofday(&now, NULL);
	/* Waiting until power off duration and displaying animation */
	while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
		usleep(100000);
		gettimeofday(&now, NULL);
	}

	_I("Power off by force");
	/* give a chance to be terminated for each process */
	power_off = 1;
	sync();
	unmount_rw_partition();
	reboot(RB_POWER_OFF);
	return EINA_TRUE;
}

static void powerdown_ap(TapiHandle *handle, const char *noti_id, void *data, void *user_data)
{
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	char *buf;

	if (poweroff_timer_id) {
		ecore_timer_del(poweroff_timer_id);
		poweroff_timer_id = NULL;
	}
	if (tapi_handle) {
		tel_deregister_noti_event(tapi_handle,TAPI_NOTI_MODEM_POWER);
		tel_deinit(tapi_handle);
		tapi_handle = NULL;
	}

	_I("Power off");

	/* Getting poweroff duration */
	buf = getenv("PWROFF_DUR");
	if (buf != NULL && strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60)
		poweroff_duration = POWEROFF_DURATION;

	gettimeofday(&now, NULL);
	/* Waiting until power off duration and displaying animation */
	while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
		usleep(100000);
		gettimeofday(&now, NULL);
	}

	/* give a chance to be terminated for each process */
	power_off = 1;
	sync();
	unmount_rw_partition();
	reboot(RB_POWER_OFF);
}
static void powerdown_res_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	_D("poweroff command request : %d",result);
}


int internal_poweroff_def_predefine_action(int argc, char **argv)
{
	int ret;

	display_device_ops.exit(NULL);
	system("/etc/rc.d/rc.shutdown &");
	sync();

	gettimeofday(&tv_start_poweroff, NULL);
	if (tapi_handle) {
		ret = tel_register_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER, powerdown_ap, NULL);

		if (ret != TAPI_API_SUCCESS) {
			_E("tel_register_event is not subscribed. error %d", ret);
			powerdown_ap_by_force(NULL);
			return 0;
		}

		ret = tel_process_power_command(tapi_handle, TAPI_PHONE_POWER_OFF, powerdown_res_cb, NULL);
		if (ret != TAPI_API_SUCCESS) {
			_E("tel_process_power_command() error %d\n", ret);
			powerdown_ap_by_force(NULL);
			return 0;
		}
		poweroff_timer_id = ecore_timer_add(15, powerdown_ap_by_force, NULL);
	} else {
		powerdown_ap_by_force(NULL);
	}
	return 0;
}

static void restart_ap(TapiHandle *handle, const char *noti_id, void *data, void *user_data);

Eina_Bool restart_ap_ecore(void *data)
{
	restart_ap(tapi_handle,NULL,(void *)-1,NULL);
	return EINA_TRUE;
}

static void restart_ap(TapiHandle *handle, const char *noti_id, void *data, void *user_data)
{
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	char *buf;

	if (poweroff_timer_id) {
		ecore_timer_del(poweroff_timer_id);
		poweroff_timer_id = NULL;
	}


	if(tapi_handle != NULL)
	{
		tel_deregister_noti_event(tapi_handle,TAPI_NOTI_MODEM_POWER);
		tel_deinit(tapi_handle);
		tapi_handle = NULL;
	}

	_I("Restart");
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void*)poweroff_control_cb);
	power_off = 1;
	sync();

	buf = getenv("PWROFF_DUR");
	if (buf != NULL && strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60)
		poweroff_duration = POWEROFF_DURATION;
	gettimeofday(&now, NULL);
	while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
		usleep(100000);
		gettimeofday(&now, NULL);
	}
	unmount_rw_partition();

	reboot(RB_AUTOBOOT);
}

static void restart_ap_by_force(void *data)
{
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	char *buf;

	if (poweroff_timer_id) {
		ecore_timer_del(poweroff_timer_id);
		poweroff_timer_id = NULL;
	}

	if(tapi_handle != NULL) {
		tel_deinit(tapi_handle);
		tapi_handle = NULL;
	}

	_I("Restart");
	power_off = 1;
	sync();

	buf = getenv("PWROFF_DUR");
	if (buf != NULL && strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60)
		poweroff_duration = POWEROFF_DURATION;
	gettimeofday(&now, NULL);
	while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
		usleep(100000);
		gettimeofday(&now, NULL);
	}
	unmount_rw_partition();

	reboot(RB_AUTOBOOT);
}

int entersleep_def_predefine_action(int argc, char **argv)
{
	int ret;

	pm_change_internal(getpid(), LCD_NORMAL);
	sync();

	ret = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_ENTER, enter_flight_mode_cb, NULL);
	_I("request for changing into flight mode : %d", ret);

	system("/etc/rc.d/rc.entersleep");
	pm_change_internal(getpid(), POWER_OFF);

	return 0;
}

int poweroff_def_predefine_action(int argc, char **argv)
{
	int retry_count = 0;

	heynoti_publish(POWEROFF_NOTI_NAME);

	while (retry_count < MAX_RETRY) {
		if (action_entry_call_internal(PREDEF_INTERNAL_POWEROFF, 0) < 0) {
			_E("failed to request poweroff to system_server");
			retry_count++;
			continue;
		}
		vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void*)poweroff_control_cb);
		return 0;
	}
	return -1;
}

int launching_predefine_action(int argc, char **argv)
{
	int ret;

	if (argc < 0)
		return -1;

	/* current just launching poweroff-popup */
	if (predefine_control_launch("poweroff-syspopup", NULL, 0) < 0) {
		_E("poweroff-syspopup launch failed");
		return -1;
	}
	return 0;
}

int leavesleep_def_predefine_action(int argc, char **argv)
{
	int ret;

	pm_change_internal(getpid(), LCD_NORMAL);
	sync();

	ret = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_LEAVE, leave_flight_mode_cb, NULL);
	_I("request for changing into flight mode : %d", ret);

	return 0;
}

int restart_def_predefine_action(int argc, char **argv)
{
	int ret;

	heynoti_publish(POWEROFF_NOTI_NAME);
	pm_change_internal(getpid(), LCD_NORMAL);
	display_device_ops.exit(NULL);
	system("/etc/rc.d/rc.shutdown &");
	sync();

	gettimeofday(&tv_start_poweroff, NULL);

	ret =
	    tel_register_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER, restart_ap, NULL);
	if (ret != TAPI_API_SUCCESS) {
		_E("tel_register_event is not subscribed. error %d", ret);
		restart_ap_by_force((void *)-1);
		return 0;
	}


	ret = tel_process_power_command(tapi_handle, TAPI_PHONE_POWER_OFF, powerdown_res_cb, NULL);
	if (ret != TAPI_API_SUCCESS) {
		_E("tel_process_power_command() error %d", ret);
		restart_ap_by_force((void *)-1);
		return 0;
	}

	poweroff_timer_id = ecore_timer_add(15, restart_ap_ecore, NULL);
	return 0;
}

static DBusMessage *dbus_power_handler(E_DBus_Object *obj, DBusMessage *msg)
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

	if (strncmp(type_str, PREDEF_ENTERSLEEP, strlen(PREDEF_ENTERSLEEP)) == 0)
		entersleep_def_predefine_action(0, NULL);
	else if(strncmp(type_str, PREDEF_LEAVESLEEP, strlen(PREDEF_LEAVESLEEP)) == 0)
		leavesleep_def_predefine_action(0, NULL);
	else if(strncmp(type_str, PREDEF_REBOOT, strlen(PREDEF_REBOOT)) == 0)
		restart_def_predefine_action(0, NULL);
	else if(strncmp(type_str, PREDEF_PWROFF_POPUP, strlen(PREDEF_PWROFF_POPUP)) == 0)
		launching_predefine_action(0, NULL);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_flightmode_handler(E_DBus_Object *obj, DBusMessage *msg)
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

	flight_mode_def_predefine_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	 { PREDEF_ENTERSLEEP, "si", "i", dbus_power_handler },
	 { PREDEF_LEAVESLEEP, "si", "i", dbus_power_handler },
	 { PREDEF_REBOOT, "si", "i", dbus_power_handler },
	 { PREDEF_PWROFF_POPUP, "si", "i", dbus_power_handler },
	 { PREDEF_FLIGHT_MODE, "sis", "i", dbus_flightmode_handler },
	 /* Add methods here */
};

static void power_init(void *data)
{
	int bTelReady = 0;
	int ret;

	/* init dbus interface*/
	ret = register_edbus_method(DEVICED_PATH_POWER, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	if (vconf_get_bool(VCONFKEY_TELEPHONY_READY,&bTelReady) == 0) {
		if (bTelReady == 1) {
			tapi_handle = tel_init(NULL);
			if (tapi_handle == NULL) {
				_E("tapi init error");
			}
		} else {
			vconf_notify_key_changed(VCONFKEY_TELEPHONY_READY, (void *)__tel_init_cb, NULL);
		}
	} else {
		_E("failed to get tapi vconf key");
	}

	action_entry_add_internal(PREDEF_ENTERSLEEP,
				     entersleep_def_predefine_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_POWEROFF,
				     poweroff_def_predefine_action, NULL, NULL);
	action_entry_add_internal(PREDEF_PWROFF_POPUP,
				     launching_predefine_action, NULL, NULL);
	action_entry_add_internal(PREDEF_LEAVESLEEP,
				     leavesleep_def_predefine_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_REBOOT,
				     restart_def_predefine_action, NULL, NULL);

	action_entry_add_internal(PREDEF_INTERNAL_POWEROFF,
				     internal_poweroff_def_predefine_action, NULL, NULL);

	action_entry_add_internal(PREDEF_FLIGHT_MODE,
				     flight_mode_def_predefine_action, NULL, NULL);

	if (vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb, NULL) < 0) {
		_E("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
	}

	register_edbus_signal_handler(OBJECT_PATH, INTERFACE_NAME,
			SIGNAL_NAME_POWEROFF_POPUP,
		    (void *)poweroff_popup_edbus_signal_handler);
	register_edbus_signal_handler(OBJECT_PATH, INTERFACE_NAME,
			SIGNAL_NAME_LCD_CONTROL,
		    (void *)lcd_control_edbus_signal_handler);
}

static const struct device_ops power_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "power",
	.init = power_init,
};

DEVICE_OPS_REGISTER(&power_device_ops)
