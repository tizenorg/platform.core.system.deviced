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
#include <tzplatform_config.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <vconf.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/timerfd.h>

#include "core/log.h"
#include "core/devices.h"
#include "display/poll.h"
#include "display/core.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "core/device-notifier.h"

#define PREDEF_SET_DATETIME		"set_datetime"
#define PREDEF_SET_TIMEZONE		"set_timezone"

#ifndef TFD_TIMER_CANCELON_SET
#define TFD_TIMER_CANCELON_SET (1<<1)
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC	0x2000000
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	0x4000
#endif

#ifndef TFD_CLOEXEC
#define TFD_CLOEXEC	O_CLOEXEC
#endif

#ifndef TFD_NONBLOCK
#define TFD_NONBLOCK	O_NONBLOCK
#endif

#define TIME_CHANGE_SIGNAL     "STimeChanged"

static const char default_rtc0[] = "/dev/rtc0";
static const char default_rtc1[] = "/dev/rtc1";

static const time_t default_time = 2147483645; /* max(32bit) -3sec */
static Ecore_Fd_Handler *tfdh; /* tfd change noti */

static Eina_Bool tfd_cb(void *data, Ecore_Fd_Handler *fd_handler);
static int timerfd_check_stop(int fd);
static int timerfd_check_start(void);

char *substring(const char *str, size_t begin, size_t len)
{
	if (str == 0 || strlen(str) == 0 || strlen(str) < begin
	    || strlen(str) < (begin + len))
		return 0;

	return strndup(str + begin, len);
}

int handle_timezone(char *str)
{
	int ret;
	struct stat sts;
	time_t now;
	struct tm *ts;
	const char *sympath, *tzpath;

	if (str == NULL)
		return -1;

	tzpath = str;
	sympath = tzplatform_mkpath(TZ_SYS_ETC, "localtime");

	_D("TZPATH = %s", tzpath);

	if (stat(tzpath, &sts) == -1 && errno == ENOENT) {
		_E("invalid tzpath(%s)", tzpath);
		return -EINVAL;
	}

	/* FIXME for debugging purpose */
	time(&now);
	ts = localtime(&now);
	_D("cur local time is %s", asctime(ts));

	/* unlink current link
	 * eg. rm /opt/etc/localtime */
	if (stat(sympath, &sts) == -1 && errno == ENOENT) {
		/* DO NOTHING */
	} else {
		ret = unlink(sympath);
		if (ret < 0) {
			_E("unlink error : [%d]%s", ret,
				  strerror(errno));
			return -1;
		}
		_D("unlink success");
	}

	/* symlink new link
	 * eg. ln -s /usr/share/zoneinfo/Asia/Seoul /opt/etc/localtime */
	ret = symlink(tzpath, sympath);
	if (ret < 0) {
		_E("symlink error : [%d]%s", ret, strerror(errno));
		return -1;
	}
	_D("symlink success");

	tzset();

	/* FIXME for debugging purpose */
	ts = localtime(&now);
	_D("new local time is %s", asctime(ts));
	return 0;
}

/*
 * TODO : error handling code should be added here.
 */
int handle_date(char *str)
{
	long int tmp = 0;
	time_t timet = 0;

	if (str == NULL)
		return -1;

	tmp = (long int)atoi(str);
	timet = (time_t) tmp;

	_D("ctime = %s", ctime(&timet));
	vconf_set_int(VCONFKEY_SYSTEM_TIMECHANGE, timet);

	return 0;
}

int set_datetime_action(int argc, char **argv)
{
	int ret = 0;
	unsigned int pm_state;
	if (argc < 1)
		return -1;
	if (vconf_get_int(VCONFKEY_PM_STATE, &ret) != 0)
		_E("Fail to get vconf value for pm state\n");
	if (ret == 1)
		pm_state = 0x1;
	else if (ret == 2)
		pm_state = 0x2;
	else
		pm_state = 0x4;

	pm_lock_internal(INTERNAL_LOCK_TIME, pm_state, STAY_CUR_STATE, 0);
	ret = handle_date(argv[0]);
	pm_unlock_internal(INTERNAL_LOCK_TIME, pm_state, STAY_CUR_STATE);
	return ret;
}

int set_timezone_action(int argc, char **argv)
{
	int ret;
	unsigned int pm_state;
	if (argc < 1)
		return -1;
	if (vconf_get_int(VCONFKEY_PM_STATE, &ret) != 0)
		_E("Fail to get vconf value for pm state\n");
	if (ret == 1)
		pm_state = 0x1;
	else if (ret == 2)
		pm_state = 0x2;
	else
		pm_state = 0x4;

	pm_lock_internal(INTERNAL_LOCK_TIME, pm_state, STAY_CUR_STATE, 0);
	ret = handle_timezone(argv[0]);
	pm_unlock_internal(INTERNAL_LOCK_TIME, pm_state, STAY_CUR_STATE);
	return ret;
}

static void time_changed_broadcast(void)
{
	broadcast_edbus_signal(DEVICED_PATH_TIME, DEVICED_INTERFACE_TIME,
			TIME_CHANGE_SIGNAL, NULL, NULL);
}

static int timerfd_check_start(void)
{
	int tfd;
	int ret;
	struct itimerspec tmr;

	tfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK|TFD_CLOEXEC);
	if (tfd == -1) {
		_E("error timerfd_create() %d", errno);
		tfdh = NULL;
		return -1;
	}

	tfdh = ecore_main_fd_handler_add(tfd, ECORE_FD_READ, tfd_cb, NULL, NULL, NULL);
	if (!tfdh) {
		_E("error ecore_main_fd_handler_add");
		return -1;
	}
	memset(&tmr, 0, sizeof(tmr));
	tmr.it_value.tv_sec = default_time;
	ret = timerfd_settime(tfd, TFD_TIMER_ABSTIME|TFD_TIMER_CANCELON_SET, &tmr, NULL);
	if (ret < 0) {
		_E("error timerfd_settime() %d", errno);
		return -1;
	}
	return 0;
}

static int timerfd_check_stop(int tfd)
{
	if (tfdh) {
		ecore_main_fd_handler_del(tfdh);
		tfdh = NULL;
	}
	if (tfd >= 0) {
		close(tfd);
		tfd = -1;
	}
	return 0;
}

static Eina_Bool tfd_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	int tfd = -1;
	u_int64_t ticks;
	int ret = -1;

	ret = ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ);
	if (!ret) {
		_E("error ecore_main_fd_handler_get()");
		goto out;
	}

	tfd = ecore_main_fd_handler_fd_get(fd_handler);
	if (tfd == -1) {
		_E("error ecore_main_fd_handler_fd_get()");
		goto out;
	}

	ret = read(tfd, &ticks, sizeof(ticks));
	if (ret < 0 && errno == ECANCELED) {
		vconf_set_int(VCONFKEY_SYSMAN_STIME, VCONFKEY_SYSMAN_STIME_CHANGED);
		time_changed_broadcast();
		timerfd_check_stop(tfd);
		_D("NOTIFICATION here");
		timerfd_check_start();
	} else {
		_E("unexpected read (err:%d)", errno);
	}
out:
	return EINA_TRUE;
}

static DBusMessage *dbus_time_handler(E_DBus_Object *obj, DBusMessage *msg)
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

	if (strncmp(type_str, PREDEF_SET_DATETIME, strlen(PREDEF_SET_DATETIME)) == 0)
		ret = set_datetime_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_SET_TIMEZONE, strlen(PREDEF_SET_TIMEZONE)) == 0)
		ret = set_timezone_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_SET_DATETIME, "sis", "i", dbus_time_handler },
	{ PREDEF_SET_TIMEZONE, "sis", "i", dbus_time_handler },

};

static int time_lcd_changed_cb(void *data)
{
	int lcd_state;
	int tfd = -1;

	if (!data)
		goto out;

	lcd_state = *(int *)data;

	if (lcd_state < S_LCDOFF)
		goto restart;

	lcd_state = check_lcdoff_lock_state();
	if (lcd_state || !tfdh)
		goto out;
	tfd = ecore_main_fd_handler_fd_get(tfdh);
	if (tfd == -1)
		goto out;

	_D("stop tfd");
	timerfd_check_stop(tfd);
	goto out;
restart:
	if (tfdh)
		return 0;
	_D("restart tfd");
	timerfd_check_start();
out:
	return 0;
}

static void time_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	if (timerfd_check_start() == -1)
		_E("fail system time change detector init");
	register_notifier(DEVICE_NOTIFIER_LCD, time_lcd_changed_cb);
}

static const struct device_ops time_device_ops = {
	.name     = "time",
	.init     = time_init,
};

DEVICE_OPS_REGISTER(&time_device_ops)
