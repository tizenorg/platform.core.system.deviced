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
#include "ss_log.h"
#include "ss_launch.h"
#include "ss_queue.h"
#include "ss_device_handler.h"
#include "device-node.h"
#include "ss_predefine.h"
#include "ss_procmgr.h"
#include "ss_vibrator.h"
#include "include/ss_data.h"
#include "ss_common.h"
#include "poll.h"
#include "setting.h"
#include "led/led.h"

#define PREDEFINE_SO_DIR		PREFIX"/lib/ss_predefine/"

#define CALL_EXEC_PATH			PREFIX"/bin/call"
#define LOWMEM_EXEC_PATH		PREFIX"/bin/lowmem-popup"
#define LOWBAT_EXEC_PATH		PREFIX"/bin/lowbatt-popup"
#define USBCON_EXEC_PATH		PREFIX"/bin/usb-server"
#define TVOUT_EXEC_PATH			PREFIX"/bin/tvout-selector"
#define PWROFF_EXEC_PATH		PREFIX"/bin/poweroff-popup"
#define MEMPS_EXEC_PATH			PREFIX"/bin/memps"
#define HDMI_NOTI_EXEC_PATH		PREFIX"/bin/hdmi_connection_noti"
#define LOWBAT_POPUP_NAME		"lowbat-syspopup"
#define POWEROFF_POPUP_NAME		"poweroff-syspopup"
#define HDMI_POPUP_NAME			"hdmi-syspopup"
#define LOWMEM_POPUP_NAME		"lowmem-syspopup"

/* wait for 5 sec as victim process be dead */
#define WAITING_INTERVAL		5

#define TVOUT_X_BIN			"/usr/bin/xberc"
#define TVOUT_FLAG			0x00000001
#define MEMPS_LOG_FILE			"/var/log/memps"
#define MAX_RETRY			2

#define POWEROFF_DURATION		2
#define POWEROFF_ANIMATION_PATH		"/usr/bin/boot-animation"
#define POWEROFF_NOTI_NAME		"power_off_start"

#define WM_READY_PATH			"/tmp/.wm_ready"

#define LOWBAT_OPT_WARNING		1
#define LOWBAT_OPT_POWEROFF		2
#define LOWBAT_OPT_CHARGEERR		3
#define LOWBAT_OPT_CHECK		4

static Ecore_Timer *lowbat_popup_id = NULL;
static int lowbat_popup_option = 0;

static struct timeval tv_start_poweroff;
static void powerdown_ap(TapiHandle *handle, const char *noti_id, void *data, void *user_data);
static void poweroff_control_cb(keynode_t *in_key, struct ss_main_data *ad);
int internal_poweroff_def_predefine_action(int argc, char **argv);

static int ss_flags = 0;

static Ecore_Timer *poweroff_timer_id = NULL;
static TapiHandle *tapi_handle = NULL;
static int power_off = 0;

static int __predefine_get_pid(const char *execpath)
{
	DIR *dp;
	struct dirent *dentry;
	int pid = -1, fd;
	char buf[PATH_MAX];
	char buf2[PATH_MAX];

	dp = opendir("/proc");
	if (!dp) {
		PRT_TRACE_ERR("open /proc");
		return -1;
	}

	while ((dentry = readdir(dp)) != NULL) {
		if (!isdigit(dentry->d_name[0]))
			continue;

		pid = atoi(dentry->d_name);

		snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;
		if (read(fd, buf2, PATH_MAX) < 0) {
			close(fd);
			continue;
		}
		close(fd);

		if (!strcmp(buf2, execpath)) {
			closedir(dp);
			return pid;
		}
	}

	errno = ESRCH;
	closedir(dp);
	return -1;
}

int is_power_off(void)
{
	return power_off;
}

static void make_memps_log(char *file, pid_t pid, char *victim_name)
{
	time_t now;
	struct tm *cur_tm;
	char params[4096];
	char new_log[NAME_MAX];
	static pid_t old_pid = 0;
	int ret=-1;

	if (old_pid == pid)
		return;
	old_pid = pid;

	now = time(NULL);
	cur_tm = (struct tm *)malloc(sizeof(struct tm));
	if (cur_tm == NULL) {
		PRT_TRACE_ERR("Fail to memory allocation");
		return;
	}

	if (localtime_r(&now, cur_tm) == NULL) {
		PRT_TRACE_ERR("Fail to get localtime");
		free(cur_tm);
		return;
	}

	PRT_TRACE("%s_%s_%d_%.4d%.2d%.2d_%.2d%.2d%.2d.log", file, victim_name,
		 pid, (1900 + cur_tm->tm_year), 1 + cur_tm->tm_mon,
		 cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);
	snprintf(new_log, sizeof(new_log),
		 "%s_%s_%d_%.4d%.2d%.2d_%.2d%.2d%.2d.log", file, victim_name,
		 pid, (1900 + cur_tm->tm_year), 1 + cur_tm->tm_mon,
		 cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);

	snprintf(params, sizeof(params), "-f %s", new_log);
	ret = ss_launch_evenif_exist(MEMPS_EXEC_PATH, params);
	/* will be removed, just for debugging */
	if(ret > 0) {
		FILE *fp;
		fp = open_proc_oom_adj_file(ret, "w");
		if (fp != NULL) {
			fprintf(fp, "%d", (-17));
			fclose(fp);
		}
	}
	free(cur_tm);
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

static int lowmem_get_victim_pid()
{
	pid_t pid;
	int fd;

	if (device_get_property(DEVICE_TYPE_MEMORY, PROP_MEMORY_VICTIM_TASK, &pid) < 0) {
		PRT_TRACE_ERR("Get victim task failed");
		return -1;
	}

	return pid;
}

int lowmem_def_predefine_action(int argc, char **argv)
{
	int pid, ret, oom_adj;
	char appname[PATH_MAX];

	if (argc < 1)
		return -1;

	if (!strcmp(argv[0], OOM_MEM_ACT)) {
		pid = lowmem_get_victim_pid();
		if (pid > 0 && pid != get_exec_pid(LOWMEM_EXEC_PATH) && pid != get_exec_pid(MEMPS_EXEC_PATH)) {
			if ((get_cmdline_name(pid, appname, PATH_MAX)) ==
			    0) {
				PRT_TRACE_EM
				    ("we will kill, lowmem lv2 = %d (%s)\n",
				     pid, appname);
	
				make_memps_log(MEMPS_LOG_FILE, pid, appname);

				if(get_app_oomadj(pid, &oom_adj) < 0) {
					PRT_TRACE_ERR("Failed to get oom_adj");
					return -1;
				}
				PRT_TRACE("%d will be killed with %d oom_adj value", pid, oom_adj);

				kill(pid, SIGTERM);

				if (oom_adj != OOMADJ_FOREGRD_LOCKED && oom_adj != OOMADJ_FOREGRD_UNLOCKED) {
					return 0;
				}

				bundle *b = NULL;

				b = bundle_create();
				bundle_add(b, "_APP_NAME_", appname);
				ret = syspopup_launch("lowmem-syspopup", b);
				bundle_free(b);
				if (ret < 0) {
					PRT_TRACE_EM("popup lauch failed\n");
					return -1;
				}
				
				if (set_app_oomadj(ret, OOMADJ_SU) < 0) {	
					PRT_TRACE_ERR("Failed to set oom_adj");
				}
			}
		}
	}
	return 0;
}

int usbcon_def_predefine_action(int argc, char **argv)
{
	int pid;
	int val = -1;
	int ret = -1;
	int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;

	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ONLINE, &val) == 0) {
		if (val == 0) {
			vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS,
				      VCONFKEY_SYSMAN_USB_DISCONNECTED);
			pm_unlock_internal(getpid(), LCD_OFF, STAY_CUR_STATE);
			return 0;
		}

		if ( vconf_get_int(VCONFKEY_SYSMAN_USB_STATUS, &val) == 0 && val == VCONFKEY_SYSMAN_USB_AVAILABLE)
			return 0;

		vconf_set_int(VCONFKEY_SYSMAN_USB_STATUS,
			      VCONFKEY_SYSMAN_USB_AVAILABLE);
		pm_lock_internal(getpid(), LCD_OFF, STAY_CUR_STATE, 0);
		pid = ss_launch_if_noexist(USBCON_EXEC_PATH, NULL);
		if (pid < 0) {
			PRT_TRACE_ERR("usb predefine action failed\n");
			return -1;
		}
		return pid;
	}
	PRT_TRACE_ERR("failed to get usb status\n");
	return -1;
}

int earjackcon_def_predefine_action(int argc, char **argv)
{
	int val;

	PRT_TRACE_EM("earjack_normal predefine action\n");
	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARJACK_ONLINE, &val) == 0) {
		return vconf_set_int(VCONFKEY_SYSMAN_EARJACK, val);
	}

	return -1;
}

int lowbat_popup(void *data)
{
	int ret = -1, state = 0;
	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &state);
	if (state == 1 || ret != 0) {
		bundle *b = NULL;
		b = bundle_create();
		if (lowbat_popup_option == LOWBAT_OPT_WARNING || lowbat_popup_option == LOWBAT_OPT_CHECK) {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "warning");
		} else if(lowbat_popup_option == LOWBAT_OPT_POWEROFF) {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "poweroff");
		} else if(lowbat_popup_option == LOWBAT_OPT_CHARGEERR) {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "chargeerr");
		} else {
			bundle_add(b, "_SYSPOPUP_CONTENT_", "check");
		}

		ret = syspopup_launch("lowbat-syspopup", b);
		if (ret < 0) {
			PRT_TRACE_EM("popup lauch failed\n");
			bundle_free(b);
			return 1;
		}
		lowbat_popup_id = NULL;
		lowbat_popup_option = 0;
		bundle_free(b);
	} else {
		PRT_TRACE_EM("boot-animation running yet");
		return 1;
	}

	return 0;
}


int predefine_control_launch(char *name, bundle *b)
{
	int pid;
	static int launched_poweroff = 0;
	//lowbat-popup
	if (strncmp(name, LOWBAT_POPUP_NAME, strlen(LOWBAT_POPUP_NAME)) == 0) {
		if (launched_poweroff == 1) {
			PRT_TRACE_ERR("will be foreced power off");
			internal_poweroff_def_predefine_action(0,NULL);
			return 0;
		}

		if (lowbat_popup_option == LOWBAT_OPT_POWEROFF)
			launched_poweroff = 1;

		pid = __predefine_get_pid(LOWBAT_EXEC_PATH);
		if (pid > 0) {
			PRT_TRACE_ERR("pre launched %s destroy", LOWBAT_EXEC_PATH);
			kill(pid, SIGTERM);
		}
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	//poweroff-popup
	if (strncmp(name, POWEROFF_POPUP_NAME, strlen(POWEROFF_POPUP_NAME)) == 0) {
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	//hdmi-popup
	if (strncmp(name, HDMI_POPUP_NAME, strlen(HDMI_POPUP_NAME)) == 0) {
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	//hdmi-noti
	if (strncmp(name, HDMI_NOTI_EXEC_PATH, strlen(HDMI_NOTI_EXEC_PATH)) == 0) {
		if (ss_launch_if_noexist(name, "1") < 0)
			return -1;
	}
	//user mem lowmem-popup
	if (strncmp(name, LOWMEM_POPUP_NAME, strlen(LOWMEM_POPUP_NAME)) == 0) {
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	return 0;
}

void predefine_pm_change_state(unsigned int s_bits)
{
	pm_change_internal(getpid(), s_bits);
}

int lowbat_def_predefine_action(int argc, char **argv)
{
	int ret, state=0;
	char argstr[128];
	char* option = NULL;

	if (argc < 1)
		return -1;

	if (lowbat_popup_id != NULL) {
		ecore_timer_del(lowbat_popup_id);
		lowbat_popup_id = NULL;
	}

	bundle *b = NULL;
	b = bundle_create();
	if (!strcmp(argv[0],CRITICAL_LOW_BAT_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "warning");
		lowbat_popup_option = LOWBAT_OPT_CHECK;
	} else if(!strcmp(argv[0],WARNING_LOW_BAT_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "warning");
		lowbat_popup_option = LOWBAT_OPT_WARNING;
	} else if(!strcmp(argv[0],POWER_OFF_BAT_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "poweroff");
		lowbat_popup_option = LOWBAT_OPT_POWEROFF;
	} else if(!strcmp(argv[0],CHARGE_ERROR_ACT)) {
		bundle_add(b, "_SYSPOPUP_CONTENT_", "chargeerr");
		lowbat_popup_option = LOWBAT_OPT_CHARGEERR;
	}

	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &state);
	if (state == 1 || ret != 0) {
		if (predefine_control_launch("lowbat-syspopup", b) < 0) {
				PRT_TRACE_ERR("popup lauch failed\n");
				bundle_free(b);
				lowbat_popup_option = 0;
				return -1;
		}
	} else {
		PRT_TRACE_EM("boot-animation running yet");
		lowbat_popup_id = ecore_timer_add(1, lowbat_popup, NULL);
	}
	bundle_free(b);
	return 0;
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

int leavesleep_def_predefine_action(int argc, char **argv)
{
	int ret;

	pm_change_internal(getpid(), LCD_NORMAL);
	sync();

	ret = tel_set_flight_mode(tapi_handle, TAPI_POWER_FLIGHT_MODE_LEAVE, leave_flight_mode_cb, NULL);
	PRT_TRACE_ERR("request for changing into flight mode : %d\n", ret);

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

int launching_predefine_action(int argc, char **argv)
{
	int ret;

	if (argc < 0)
		return -1;

	/* current just launching poweroff-popup */
	if (predefine_control_launch("poweroff-syspopup", NULL) < 0) {
		PRT_TRACE_ERR("poweroff-syspopup launch failed");
		return -1;
	}
	return 0;
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

static void ss_action_entry_load_from_sodir()
{
	DIR *dp;
	struct dirent *dentry;
	struct sysnoti *msg;
	char *ext;
	char tmp[128];

	dp = opendir(PREDEFINE_SO_DIR);
	if (!dp) {
		ERR("fail open %s", PREDEFINE_SO_DIR);
		return;
	}

	msg = malloc(sizeof(struct sysnoti));
	if (msg == NULL) {
		ERR("Malloc failed");
		closedir(dp);
		return;
	}

	msg->pid = getpid();

	while ((dentry = readdir(dp)) != NULL) {
		if ((ext = strstr(dentry->d_name, ".so")) == NULL)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", PREDEFINE_SO_DIR,
			 dentry->d_name);
		msg->path = tmp;
		*ext = 0;
		msg->type = &(dentry->d_name[3]);
		ss_action_entry_add(msg);
	}
	free(msg);

	closedir(dp);
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

void ss_predefine_internal_init(void)
{

	/* telephony initialize */
	int ret = 0;
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
#ifdef NOUSE
	ss_action_entry_add_internal(PREDEF_CALL, call_predefine_action, NULL,
				     NULL);
#endif
	ss_action_entry_add_internal(PREDEF_LOWMEM, lowmem_def_predefine_action,
				     NULL, NULL);
	ss_action_entry_add_internal(PREDEF_LOWBAT, lowbat_def_predefine_action,
				     NULL, NULL);
	ss_action_entry_add_internal(PREDEF_USBCON, usbcon_def_predefine_action,
				     NULL, NULL);
	ss_action_entry_add_internal(PREDEF_EARJACKCON,
				     earjackcon_def_predefine_action, NULL,
				     NULL);
	ss_action_entry_add_internal(PREDEF_POWEROFF,
				     poweroff_def_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_PWROFF_POPUP,
				     launching_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_REBOOT,
				     restart_def_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_FLIGHT_MODE,
				     flight_mode_def_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_INTERNAL_POWEROFF,
				     internal_poweroff_def_predefine_action, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_HAPTIC, haptic_def_predefine_action,
					NULL, NULL);

	ss_action_entry_add_internal(PREDEF_LED,
					led_def_predefine_action, NULL, NULL);

	if (vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, (void *)poweroff_control_cb, NULL) < 0) {
		PRT_TRACE_ERR("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
	}
	ss_action_entry_load_from_sodir();

	/* check and set earjack init status */
	earjackcon_def_predefine_action(0, NULL);
}
