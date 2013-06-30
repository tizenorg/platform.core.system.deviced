/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
#include <ITapiModem.h>
#include <TelPower.h>
#include <tapi_event.h>
#include <tapi_common.h>
#include <fcntl.h>
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
		PRT_TRACE_ERR("there is no power off popup signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == 0) {
		PRT_TRACE_ERR("there is no message");
		return;
	}

	if (strncmp(str, PREDEF_PWROFF_POPUP, strlen(PREDEF_PWROFF_POPUP)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_POPUP;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_POWEROFF)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_DIRECT;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_REBOOT)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_RESTART;
	if (val == 0) {
		PRT_TRACE_ERR("not supported message : %s", str);
		return;
	}
	vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);
}

static void poweroff_control_cb(keynode_t *in_key, struct ss_main_data *ad)
{
	int val;
	if (vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &val) != 0)
		return;
	switch (val) {
	case VCONFKEY_SYSMAN_POWER_OFF_DIRECT:
		ss_action_entry_call_internal(PREDEF_POWEROFF, 0);
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_POPUP:
		ss_action_entry_call_internal(PREDEF_PWROFF_POPUP, 0);
		break;
	case VCONFKEY_SYSMAN_POWER_OFF_RESTART:
		ss_action_entry_call_internal(PREDEF_REBOOT, 0);
		break;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_POWEROFF, val);
}

static void remount_ro()
{
	struct mntent* mnt;
	const char* table = "/etc/mtab";
	const char mmtpoint[10][64];
	FILE* fp;
	int r = -1, foundmount=0;
	char buf[256];
	fp = setmntent(table, "r");

	if (!fp)
		return;

	while (mnt=getmntent(fp)) {
		if (foundmount >= 10)
			continue;
		if (!strcmp(mnt->mnt_type, "ext4") && strstr(mnt->mnt_opts, "rw")) {
			memset(mmtpoint[foundmount], 0, 64);
			strncpy(mmtpoint[foundmount], mnt->mnt_dir, 63);
			foundmount++;
		}
	}
	endmntent(fp);
	while (foundmount) {
		foundmount--;
		snprintf(buf, sizeof(buf), "fuser -c %s -k -15", mmtpoint[foundmount]);
		sleep(1);
		umount2(mmtpoint[foundmount], MNT_DETACH);
	}
}

static void enter_flight_mode_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	int bCurFlightMode = 0;
	if (result != TAPI_POWER_FLIGHT_MODE_ENTER) {
		PRT_TRACE_ERR("flight mode enter failed %d",result);
	} else {
		PRT_TRACE("enter flight mode result : %d",result);
		if (vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE,&bCurFlightMode) == 0) {
			PRT_TRACE("Flight Mode is %d", bCurFlightMode);
		} else {
			PRT_TRACE_ERR("failed to get vconf key");
		}
	}
}

static void leave_flight_mode_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	int bCurFlightMode = 0;
	if (result != TAPI_POWER_FLIGHT_MODE_LEAVE) {
		PRT_TRACE_ERR("flight mode leave failed %d",result);
	} else {
		PRT_TRACE("leave flight mode result : %d",result);
		if (vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE,&bCurFlightMode) == 0) {
			PRT_TRACE("Flight Mode is %d", bCurFlightMode);
		} else {
			PRT_TRACE_ERR("failed to get vconf key");
		}
	}
}

int flight_mode_def_predefine_action(int argc, char **argv)
{
	int bCurFlightMode;
	int err = TAPI_API_SUCCESS;
	if (argc != 1 || argv[0] == NULL) {
		PRT_TRACE_ERR("FlightMode Set predefine action failed");
		return -1;
	}
	bCurFlightMode = atoi(argv[0]);
	if (bCurFlightMode == 1) {
		err = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_LEAVE, leave_flight_mode_cb, NULL);
	} else if (bCurFlightMode == 0) {
		err = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_ENTER, enter_flight_mode_cb, NULL);
	}
	if (err != TAPI_API_SUCCESS)
		PRT_TRACE_ERR("FlightMode tel api action failed %d",err);
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
			PRT_TRACE_ERR("tapi init error");
		}
	} else {
		PRT_TRACE_ERR("tapi is not ready yet");
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

	PRT_TRACE("Power off by force\n");
	/* give a chance to be terminated for each process */
	power_off = 1;
	sync();
	remount_ro();
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
	PRT_TRACE("Power off \n");

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
	remount_ro();
	reboot(RB_POWER_OFF);
}
static void powerdown_res_cb(TapiHandle *handle, int result, void *data, void *user_data)
{
	PRT_TRACE("poweroff command request : %d",result);
}


int internal_poweroff_def_predefine_action(int argc, char **argv)
{
	int ret;

	system("/etc/rc.d/rc.shutdown &");
	sync();

	gettimeofday(&tv_start_poweroff, NULL);
	if (tapi_handle) {
		ret = tel_register_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER, powerdown_ap, NULL);

		if (ret != TAPI_API_SUCCESS) {
			PRT_TRACE_ERR
			    ("tel_register_event is not subscribed. error %d\n", ret);
			powerdown_ap_by_force(NULL);
			return 0;
		}

		ret = tel_process_power_command(tapi_handle, TAPI_PHONE_POWER_OFF, powerdown_res_cb, NULL);
		if (ret != TAPI_API_SUCCESS) {
			PRT_TRACE_ERR("tel_process_power_command() error %d\n", ret);
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

	PRT_INFO("Restart\n");
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
	remount_ro();

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


	if(tapi_handle != NULL)
	{
		tel_deinit(tapi_handle);
		tapi_handle = NULL;
	}

	PRT_INFO("Restart\n");
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
	remount_ro();

	reboot(RB_AUTOBOOT);
}

int entersleep_def_predefine_action(int argc, char **argv)
{
	int ret;

	pm_change_internal(getpid(), LCD_NORMAL);
	sync();

	ret = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_ENTER, enter_flight_mode_cb, NULL);
	PRT_TRACE_ERR("request for changing into flight mode : %d\n", ret);

	system("/etc/rc.d/rc.entersleep");
	pm_change_internal(getpid(), POWER_OFF);

	return 0;
}

int poweroff_def_predefine_action(int argc, char **argv)
{
	int retry_count = 0;

	heynoti_publish(POWEROFF_NOTI_NAME);

	while (retry_count < MAX_RETRY) {
		if (ss_action_entry_call_internal(PREDEF_INTERNAL_POWEROFF, 0) < 0) {
			PRT_TRACE_ERR("failed to request poweroff to system_server \n");
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
		PRT_TRACE_ERR("poweroff-syspopup launch failed");
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
	PRT_TRACE_ERR("request for changing into flight mode : %d\n", ret);

	return 0;
}

int restart_def_predefine_action(int argc, char **argv)
{
	int ret;

	heynoti_publish(POWEROFF_NOTI_NAME);
	pm_change_internal(getpid(), LCD_NORMAL);
	system("/etc/rc.d/rc.shutdown &");
	sync();

	gettimeofday(&tv_start_poweroff, NULL);

	ret =
	    tel_register_noti_event(tapi_handle, TAPI_NOTI_MODEM_POWER, restart_ap, NULL);
	if (ret != TAPI_API_SUCCESS) {
		PRT_TRACE_ERR
		    ("tel_register_event is not subscribed. error %d\n", ret);
		restart_ap_by_force((void *)-1);
		return 0;
	}


	ret = tel_process_power_command(tapi_handle, TAPI_PHONE_POWER_OFF, powerdown_res_cb, NULL);
	if (ret != TAPI_API_SUCCESS) {
		PRT_TRACE_ERR("tel_process_power_command() error %d\n", ret);
		restart_ap_by_force((void *)-1);
		return 0;
	}

	poweroff_timer_id = ecore_timer_add(15, restart_ap_ecore, NULL);
	return 0;
}

static void power_init(void *data)
{
	int bTelReady = 0;

	if (vconf_get_bool(VCONFKEY_TELEPHONY_READY,&bTelReady) == 0) {
		if (bTelReady == 1) {
			tapi_handle = tel_init(NULL);
			if (tapi_handle == NULL) {
				PRT_TRACE_ERR("tapi init error");
			}
		} else {
			vconf_notify_key_changed(VCONFKEY_TELEPHONY_READY, (void *)__tel_init_cb, NULL);
		}
	} else {
		PRT_TRACE_ERR("failed to get tapi vconf key");
	}

	ss_action_entry_add_internal(PREDEF_ENTERSLEEP,
				     entersleep_def_predefine_action, NULL,
				     NULL);
	ss_action_entry_add_internal(PREDEF_POWEROFF,
				     poweroff_def_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_PWROFF_POPUP,
				     launching_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_LEAVESLEEP,
				     leavesleep_def_predefine_action, NULL,
				     NULL);
	ss_action_entry_add_internal(PREDEF_REBOOT,
				     restart_def_predefine_action, NULL, NULL);

	ss_action_entry_add_internal(PREDEF_INTERNAL_POWEROFF,
				     internal_poweroff_def_predefine_action, NULL, NULL);

	ss_action_entry_add_internal(PREDEF_FLIGHT_MODE,
				     flight_mode_def_predefine_action, NULL, NULL);

	if (vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb, NULL) < 0) {
		PRT_TRACE_ERR("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
	}

	register_edbus_signal_handler(OBJECT_PATH, INTERFACE_NAME,
			SIGNAL_NAME_POWEROFF_POPUP,
		    (void *)poweroff_popup_edbus_signal_handler);
	register_edbus_signal_handler(OBJECT_PATH, INTERFACE_NAME,
			SIGNAL_NAME_LCD_CONTROL,
		    (void *)lcd_control_edbus_signal_handler);
}

const struct device_ops power_device_ops = {
	.init = power_init,
};
