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
#include <vconf.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include <device-node.h>
#include "dd-deviced.h"
#include "core/log.h"
#include "core/launch.h"
#include "core/device-notifier.h"
#include "core/device-idler.h"
#include "core/common.h"
#include "core/devices.h"
#include "proc/proc-handler.h"
#include "display/poll.h"
#include "display/setting.h"
#include "core/edbus-handler.h"
#include "display/core.h"
#include "power-handler.h"

#define SIGNAL_NAME_POWEROFF_POPUP	"poweroffpopup"
#define SIGNAL_BOOTING_DONE		"BootingDone"

#define POWEROFF_DURATION		2
#define MAX_RETRY			2

#define SIGNAL_POWEROFF_STATE	"ChangeState"

#define UMOUNT_RW_PATH "/opt/usr"

static struct timeval tv_start_poweroff;

static int power_off = 0;
static const struct device_ops *telephony = NULL;

static int power_execute(void *data);

static void telephony_init(void)
{
	FIND_DEVICE_VOID(telephony, "telephony");
	_I("telephony (%d)", telephony);
}

static void telephony_start(void)
{
	telephony_init();
	device_start(telephony);
}

static void telephony_stop(void)
{
	device_stop(telephony);
}

static int telephony_exit(void *data)
{
	int ret;

	ret = device_exit(telephony, data);
	return ret;
}

static void poweroff_start_animation(void)
{
	char params[128];
	snprintf(params, sizeof(params), "/usr/bin/boot-animation --stop --clear");
	launch_app_cmd_with_nice(params, -20);
	launch_evenif_exist("/usr/bin/sound_server", "--poweroff");
	device_notify(DEVICE_NOTIFIER_POWEROFF_HAPTIC, NULL);
}

int previous_poweroff(void)
{
	int ret;
	static const struct device_ops *display_device_ops = NULL;

	telephony_start();

	FIND_DEVICE_INT(display_device_ops, "display");

	display_device_ops->exit(NULL);
	sync();

	gettimeofday(&tv_start_poweroff, NULL);

	ret = telephony_exit(POWER_POWEROFF);

	if (ret < 0) {
		powerdown_ap(NULL);
		return 0;
	}
	return ret;
}

static int poweroff(void)
{
	int retry_count = 0;
	poweroff_start_animation();
	while (retry_count < MAX_RETRY) {
		if (previous_poweroff() < 0) {
			_E("failed to request poweroff to deviced");
			retry_count++;
			continue;
		}
		return 0;
	}
	return -1;
}

static int pwroff_popup(void)
{
	int ret;

	ret = manage_notification("Poweroff", "Poweroff");
	if (ret == -1)
		return -1;

	return 0;
}

static int power_reboot(void)
{
	int ret;

	const struct device_ops *display_device_ops = NULL;
	poweroff_start_animation();
	telephony_start();

	FIND_DEVICE_INT(display_device_ops, "display");

	pm_change_internal(getpid(), LCD_NORMAL);
	display_device_ops->exit(NULL);
	sync();

	gettimeofday(&tv_start_poweroff, NULL);

	ret = telephony_exit(POWER_REBOOT);
	if (ret < 0) {
		restart_ap(NULL);
		return 0;
	}
	return ret;
}

static void poweroff_popup_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *str;
	int val = 0;

	if (dbus_message_is_signal(msg, DEVICED_INTERFACE_NAME, SIGNAL_NAME_POWEROFF_POPUP) == 0) {
		_E("there is no power off popup signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == 0) {
		_E("there is no message");
		return;
	}

	power_execute(str);
}

static int booting_done(void *data)
{
	static int done = 0;

	if (data == NULL)
		goto out;

	done = *(int*)data;
	telephony_init();
out:
	return done;
}

static void booting_done_edbus_signal_handler(void *data, DBusMessage *msg)
{
	int done;

	if (!dbus_message_is_signal(msg, DEVICED_INTERFACE_CORE, SIGNAL_BOOTING_DONE)) {
		_E("there is no bootingdone signal");
		return;
	}
	done = booting_done(NULL);
	if (done)
		return;

	_I("signal booting done");
	done = TRUE;
	device_notify(DEVICE_NOTIFIER_BOOTING_DONE, &done);
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
			SIGNAL_POWEROFF_STATE, "i", arr);
}

static void poweroff_stop_systemd_service(void)
{
	char buf[256];
	_D("systemd service stop");
	umount2("/sys/fs/cgroup", MNT_FORCE |MNT_DETACH);
}

static int poweroff_idler_cb(void *data)
{
	enum poweroff_type val = (int)data;
	int ret;
	int recovery;

	telephony_start();

	pm_lock_internal(INTERNAL_LOCK_POWEROFF, LCD_OFF, STAY_CUR_STATE, 0);
	poweroff_stop_systemd_service();

	if (val == POWER_OFF_DIRECT || val == POWER_OFF_RESTART) {
		poweroff_send_broadcast(val);
		device_notify(DEVICE_NOTIFIER_POWEROFF, &val);
	}

	/* TODO for notify. will be removed asap. */
	vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);

	switch (val) {
	case POWER_OFF_DIRECT:
		poweroff();
		break;
	case POWER_OFF_POPUP:
		pwroff_popup();
		break;
	case POWER_OFF_RESTART:
		power_reboot();
		break;
	}

	if (update_pm_setting)
		update_pm_setting(SETTING_POWEROFF, val);

	return 0;
}

static int power_execute(void *data)
{
	int ret;
	int val;

	if (!data) {
		_E("Invalid parameter : data(NULL)");
		return -EINVAL;
	}

	if (strncmp(POWER_POWEROFF, (char *)data, POWER_POWEROFF_LEN) == 0)
		val = POWER_OFF_DIRECT;
	else if (strncmp(PWROFF_POPUP, (char *)data, PWROFF_POPUP_LEN) == 0)
		val = POWER_OFF_POPUP;
	else if (strncmp(POWER_REBOOT, (char *)data, POWER_REBOOT_LEN) == 0)
		val = POWER_OFF_RESTART;
	else {
		_E("Invalid parameter : data(%s)", (char *)data);
		return -EINVAL;
	}

	ret = add_idle_request(poweroff_idler_cb, (int*)val);
	if (ret < 0) {
		_E("fail to add poweroff idle request : %d", ret);
		return ret;
	}

	return 0;
}

/* umount usr data partition */
static void unmount_rw_partition()
{
	int retry = 0;
	sync();
#ifdef MICRO_DD
	if (!mount_check(UMOUNT_RW_PATH))
		return;
#endif
	while (1) {
		switch (retry++) {
		case 0:
			/* Second, kill app with SIGTERM */
			_I("Kill app with SIGTERM");
			terminate_process(UMOUNT_RW_PATH, false);
			sleep(3);
			break;
		case 1:
			/* Last time, kill app with SIGKILL */
			_I("Kill app with SIGKILL");
			terminate_process(UMOUNT_RW_PATH, true);
			sleep(1);
			break;
		default:
			if (umount2(UMOUNT_RW_PATH, 0) != 0) {
				_I("Failed to unmount %s", UMOUNT_RW_PATH);
				return;
			}
			_I("%s unmounted successfully", UMOUNT_RW_PATH);
			return;
		}
		if (umount2(UMOUNT_RW_PATH, 0) == 0) {
			_I("%s unmounted successfully", UMOUNT_RW_PATH);
			return;
		}
	}
}

static void powerdown(void)
{
	static int wait = 0;
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	int check_duration = 0;
	char *buf;

	if (power_off == 1) {
		_E("during power off");
		return;
	}
	/* if this fails, that's OK */
	telephony_stop();
	power_off = 1;
	sync();

	buf = getenv("PWROFF_DUR");
	if (buf != NULL && strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60)
		poweroff_duration = POWEROFF_DURATION;
	gettimeofday(&now, NULL);
	check_duration = now.tv_sec - tv_start_poweroff.tv_sec;
	while (check_duration < poweroff_duration) {
		if (wait == 0) {
			_I("wait poweroff %d %d", check_duration, poweroff_duration);
			wait = 1;
		}
		usleep(100000);
		gettimeofday(&now, NULL);
		check_duration = now.tv_sec - tv_start_poweroff.tv_sec;
		if (check_duration < 0)
			break;
	}
#ifndef EMULATOR
	unmount_rw_partition();
#endif
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

	ret = power_execute(type_str);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

void powerdown_ap(void *data)
{
	_I("Power off");
	powerdown();
	reboot(RB_POWER_OFF);
}

void restart_ap(void *data)
{
	_I("Restart");
	powerdown();
	reboot(RB_AUTOBOOT);
}

static const struct edbus_method edbus_methods[] = {
	{ POWER_REBOOT, "si", "i", dbus_power_handler },
	{ PWROFF_POPUP, "si", "i", dbus_power_handler },
	/* Add methods here */
};

static void power_init(void *data)
{
	int bTelReady = 0;
	int ret;

	/* init dbus interface */
	ret = register_edbus_method(DEVICED_PATH_POWER, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	register_edbus_signal_handler(DEVICED_OBJECT_PATH, DEVICED_INTERFACE_NAME,
			SIGNAL_NAME_POWEROFF_POPUP,
		    poweroff_popup_edbus_signal_handler);
	register_edbus_signal_handler(DEVICED_PATH_CORE,
		    DEVICED_INTERFACE_CORE,
		    SIGNAL_BOOTING_DONE,
		    booting_done_edbus_signal_handler);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static const struct device_ops power_device_ops = {
	.name     = POWER_OPS_NAME,
	.init     = power_init,
	.execute  = power_execute,
};

DEVICE_OPS_REGISTER(&power_device_ops)
