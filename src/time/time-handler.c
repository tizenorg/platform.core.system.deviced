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

#include "core/data.h"
#include "core/queue.h"
#include "core/log.h"
#include "core/devices.h"
#include "display/poll.h"


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
static const char default_rtc0[] = "/dev/rtc0";
static const char default_rtc1[] = "/dev/rtc1";
static const char default_localtime[] = "/opt/etc/localtime";

static const time_t default_time = 2147483645; // max(32bit) -3sec
static Ecore_Fd_Handler *tfdh = NULL; // tfd change noti

static Eina_Bool tfd_cb(void *data, Ecore_Fd_Handler * fd_handler);
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
	const char *sympath = default_localtime;

	if (str == NULL)
		return -1;
	const char *tzpath = str;

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
	time_t before = 0;

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
		_E("Fail to get vconf value for pm state");
	if (ret == 1)
		pm_state = 0x1;
	else if (ret == 2)
		pm_state = 0x2;
	else
		pm_state = 0x4;

	pm_lock_internal(getpid(), pm_state, STAY_CUR_STATE, 0);
	ret = handle_date(argv[0]);
	pm_unlock_internal(getpid(), pm_state, STAY_CUR_STATE);
	return ret;
}

int set_timezone_action(int argc, char **argv)
{
	int ret;
	unsigned int pm_state;
	if (argc < 1)
		return -1;
	if (vconf_get_int(VCONFKEY_PM_STATE, &ret) != 0)
		_E("Fail to get vconf value for pm state");
	if (ret == 1)
		pm_state = 0x1;
	else if (ret == 2)
		pm_state = 0x2;
	else
		pm_state = 0x4;

	pm_lock_internal(getpid(), pm_state, STAY_CUR_STATE, 0);
	ret = handle_timezone(argv[0]);
	pm_unlock_internal(getpid(), pm_state, STAY_CUR_STATE);
	return ret;
}

static int timerfd_check_start(void)
{
	int tfd;
	struct itimerspec tmr;

	if ((tfd = timerfd_create(CLOCK_REALTIME,TFD_NONBLOCK|TFD_CLOEXEC)) == -1) {
		_E("error timerfd_create() %d", errno);
		tfdh = NULL;
		return -1;
	}

	tfdh = ecore_main_fd_handler_add(tfd,ECORE_FD_READ,tfd_cb,NULL,NULL,NULL);
	if (!tfdh) {
		_E("error ecore_main_fd_handler_add");
		return -1;
	}
	memset(&tmr, 0, sizeof(tmr));
	tmr.it_value.tv_sec = default_time;

	if (timerfd_settime(tfd,TFD_TIMER_ABSTIME|TFD_TIMER_CANCELON_SET,&tmr,NULL) < 0) {
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
	if (tfd >=0) {
		close(tfd);
		tfd = -1;
	}
	return 0;
}

static Eina_Bool tfd_cb(void *data, Ecore_Fd_Handler * fd_handler)
{
	int tfd = -1;
	u_int64_t ticks;
	int ret = -1;

	if (!ecore_main_fd_handler_active_get(fd_handler,ECORE_FD_READ)) {
		_E("error ecore_main_fd_handler_get()");
		goto out;
	}

	if((tfd = ecore_main_fd_handler_fd_get(fd_handler)) == -1) {
		_E("error ecore_main_fd_handler_fd_get()");
		goto out;
	}

	ret = read(tfd,&ticks,sizeof(ticks));
	if (ret < 0 && errno == ECANCELED) {
		vconf_set_int(VCONFKEY_SYSMAN_STIME, VCONFKEY_SYSMAN_STIME_CHANGED);
		timerfd_check_stop(tfd);
		_D("NOTIFICATION here");
		timerfd_check_start();
	} else {
		_E("unexpected read (err:%d)", errno);
	}
out:
	return EINA_TRUE;
}

static void time_init(void *data)
{
	action_entry_add_internal(PREDEF_SET_DATETIME, set_datetime_action,
				     NULL, NULL);
	action_entry_add_internal(PREDEF_SET_TIMEZONE, set_timezone_action,
				     NULL, NULL);
	if (timerfd_check_start() == -1) {
		_E("fail system time change detector init");
		return;
	}
}

static const struct device_ops time_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "time",
	.init     = time_init,
};

DEVICE_OPS_REGISTER(&time_device_ops)
