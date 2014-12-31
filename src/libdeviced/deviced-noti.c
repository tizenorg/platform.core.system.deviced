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


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <limits.h>

#include "dd-deviced.h"
#include "deviced-priv.h"
#include "log.h"
#include "dbus.h"

#define PREDEF_PWROFF_POPUP			"pwroff-popup"
#define PREDEF_ENTERSLEEP			"entersleep"
#define PREDEF_LEAVESLEEP			"leavesleep"
#define PREDEF_REBOOT				"reboot"
#define PREDEF_BACKGRD				"backgrd"
#define PREDEF_FOREGRD				"foregrd"
#define PREDEF_ACTIVE				"active"
#define PREDEF_INACTIVE				"inactive"
#define PREDEF_SET_DATETIME			"set_datetime"
#define PREDEF_SET_TIMEZONE			"set_timezone"

#define PREDEF_SET_MAX_FREQUENCY		"set_max_frequency"
#define PREDEF_SET_MIN_FREQUENCY		"set_min_frequency"
#define PREDEF_RELEASE_MAX_FREQUENCY		"release_max_frequency"
#define PREDEF_RELEASE_MIN_FREQUENCY		"release_min_frequency"

#define ALARM_BUS_NAME		"com.samsung.alarm.manager"
#define ALARM_PATH_NAME		"/com/samsung/alarm/manager"
#define ALARM_INTERFACE_NAME	ALARM_BUS_NAME
#define ALARM_SET_TIME_METHOD	"alarm_set_time"

enum deviced_noti_cmd {
	ADD_deviced_ACTION,
	CALL_deviced_ACTION
};

#define SYSTEM_NOTI_SOCKET_PATH "/tmp/sn"
#define RETRY_READ_COUNT	10

static inline int send_int(int fd, int val)
{
	return write(fd, &val, sizeof(int));
}

static inline int send_str(int fd, char *str)
{
	int len;
	int ret;
	if (str == NULL) {
		len = 0;
		ret = write(fd, &len, sizeof(int));
	} else {
		len = strlen(str);
		if (len > SYSTEM_NOTI_MAXSTR)
			len = SYSTEM_NOTI_MAXSTR;
		write(fd, &len, sizeof(int));
		ret = write(fd, str, len);
	}
	return ret;
}

static int noti_send(struct sysnoti *msg)
{
	int client_len;
	int client_sockfd;
	int result;
	int r;
	int retry_count = 0;
	struct sockaddr_un clientaddr;
	int i;

	client_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client_sockfd == -1) {
		_E("socket create failed");
		return -1;
	}
	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	strncpy(clientaddr.sun_path, SYSTEM_NOTI_SOCKET_PATH, sizeof(clientaddr.sun_path) - 1);
	client_len = sizeof(clientaddr);

	if (connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len) <
	    0) {
		_E("connect failed");
		close(client_sockfd);
		return -1;
	}

	send_int(client_sockfd, msg->pid);
	send_int(client_sockfd, msg->cmd);
	send_str(client_sockfd, msg->type);
	send_str(client_sockfd, msg->path);
	send_int(client_sockfd, msg->argc);
	for (i = 0; i < msg->argc; i++)
		send_str(client_sockfd, msg->argv[i]);

	while (retry_count < RETRY_READ_COUNT) {
		r = read(client_sockfd, &result, sizeof(int));
		if (r < 0) {
			if (errno == EINTR) {
				_E("Re-read for error(EINTR)");
				retry_count++;
				continue;
			}
			_E("Read fail for str length");
			result = -1;
			break;

		}
		break;
	}
	if (retry_count == RETRY_READ_COUNT) {
		_E("Read retry failed");
	}

	close(client_sockfd);
	return result;
}

static int dbus_flightmode_handler(char *type, char *buf)
{
	DBusError err;
	DBusMessage *msg;
	char *pa[3];
	int ret, val;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_POWER, DEVICED_INTERFACE_POWER,
			pa[0], "sis", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_POWER, pa[0], val);
	return val;
}

API int deviced_call_predef_action(const char *type, int num, ...)
{
	struct sysnoti *msg;
	int ret;
	va_list argptr;
	int i;
	char *args = NULL;

	if (type == NULL || num > SYSTEM_NOTI_MAXARG) {
		errno = EINVAL;
		return -1;
	}

	msg = malloc(sizeof(struct sysnoti));

	if (msg == NULL) {
		/* Do something for not enought memory error */
		return -1;
	}

	msg->pid = getpid();
	msg->cmd = CALL_deviced_ACTION;
	msg->type = (char *)type;
	msg->path = NULL;

	msg->argc = num;
	va_start(argptr, num);
	for (i = 0; i < num; i++) {
		args = va_arg(argptr, char *);
		msg->argv[i] = args;
	}
	va_end(argptr);

	ret = noti_send(msg);
	free(msg);

	return ret;
}

static int dbus_proc_handler(char *type, char *buf)
{
	DBusError err;
	DBusMessage *msg;
	char *pa[3];
	int ret, val;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			pa[0], "sis", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_PROCESS, pa[0], val);
	return val;
}

API int deviced_inform_foregrd(void)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", getpid());
	return dbus_proc_handler(PREDEF_FOREGRD, buf);
}

API int deviced_inform_backgrd(void)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", getpid());
	return dbus_proc_handler(PREDEF_BACKGRD, buf);
}

API int deviced_inform_active(pid_t pid)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", pid);
	return dbus_proc_handler(PREDEF_ACTIVE, buf);
}

API int deviced_inform_inactive(pid_t pid)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", pid);
	return dbus_proc_handler(PREDEF_INACTIVE, buf);
}

static int dbus_power_handler(char *type)
{
	DBusError err;
	DBusMessage *msg;
	char *pa[2];
	int ret, val;

	pa[0] = type;
	pa[1] = "0";

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_POWER, DEVICED_INTERFACE_POWER,
			pa[0], "si", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_POWER, pa[0], val);
	return val;
}

API int deviced_request_poweroff(void)
{
	return dbus_power_handler(PREDEF_PWROFF_POPUP);
}

API int deviced_request_entersleep(void)
{
	return dbus_power_handler(PREDEF_ENTERSLEEP);
}

API int deviced_request_leavesleep(void)
{
	return dbus_power_handler(PREDEF_LEAVESLEEP);
}

API int deviced_request_reboot(void)
{
	return dbus_power_handler(PREDEF_REBOOT);
}

static int dbus_time_handler(char *type, char *buf)
{
	DBusError err;
	DBusMessage *msg;
	pid_t pid;
	char name[PATH_MAX];
	char *pa[3];
	int ret, val;

	pa[0] = type;
	pa[1] = "1";
	pa[2] = buf;

	pid = getpid();
	ret = deviced_get_cmdline_name(pid, name, sizeof(name));
	if (ret != 0)
		snprintf(name, sizeof(name), "%d", pid);

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			pa[0], "sis", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_SI("[%s] %s-%s(%s) : %d", name, DEVICED_INTERFACE_SYSNOTI, pa[0], pa[2], val);

	return val;
}

static DBusMessage *alarm_set_time_sync_with_reply(time_t timet)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	int r;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		_E("dbus_bus_get error");
		return NULL;
	}

	msg = dbus_message_new_method_call(ALARM_BUS_NAME, ALARM_PATH_NAME, ALARM_INTERFACE_NAME, ALARM_SET_TIME_METHOD);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)",
			ALARM_PATH_NAME, ALARM_INTERFACE_NAME, ALARM_SET_TIME_METHOD);
		return NULL;
	}

	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &timet);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
	if (!reply) {
		_E("dbus_connection_send error(No reply) %s %s:%s-%s",
			ALARM_BUS_NAME, ALARM_PATH_NAME, ALARM_INTERFACE_NAME, ALARM_SET_TIME_METHOD);
	}

	if (dbus_error_is_set(&err)) {
		_E("dbus_connection_send error(%s:%s) %s %s:%s-%s",
			err.name, err.message, ALARM_BUS_NAME, ALARM_PATH_NAME, ALARM_INTERFACE_NAME, ALARM_SET_TIME_METHOD);
		dbus_error_free(&err);
		reply = NULL;
	}

	dbus_message_unref(msg);
	return reply;
}

static int alarm_set_time(time_t timet)
{
	DBusError err;
	DBusMessage *msg;
	pid_t pid;
	char name[PATH_MAX];
	int ret, val;

	pid = getpid();
	ret = deviced_get_cmdline_name(pid, name, sizeof(name));
	if (ret != 0)
		snprintf(name, sizeof(name), "%d", pid);
	_SI("[%s]start %s %ld", name, ALARM_INTERFACE_NAME, timet);

	msg = alarm_set_time_sync_with_reply(timet);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_SI("[%s]end %s %ld, %d", name, ALARM_INTERFACE_NAME, timet, val);
	return val;
}

API int deviced_set_datetime(time_t timet)
{
	if (timet < 0L)
		return -1;
	return alarm_set_time(timet);
}

API int deviced_set_timezone(char *tzpath_str)
{
	if (tzpath_str == NULL)
		return -1;
	char buf[255];
	snprintf(buf, sizeof(buf), "%s", tzpath_str);
	return dbus_time_handler(PREDEF_SET_TIMEZONE, buf);
}

static int dbus_cpu_handler(char *type, char *buf_pid, char *buf_freq)
{
	DBusError err;
	DBusMessage *msg;
	char *pa[4];
	int ret, val;

	pa[0] = type;
	pa[1] = "2";
	pa[2] = buf_pid;
	pa[3] = buf_freq;

	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			pa[0], "siss", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE_SYSNOTI, pa[0], val);
	return val;
}

API int deviced_request_set_cpu_max_frequency(int val)
{
	char buf_pid[8];
	char buf_freq[256];

	/* to do - need to check new frequncy is valid */
	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_freq, sizeof(buf_freq), "%d", val * 1000);

	return dbus_cpu_handler(PREDEF_SET_MAX_FREQUENCY, buf_pid, buf_freq);
}

API int deviced_request_set_cpu_min_frequency(int val)
{
	char buf_pid[8];
	char buf_freq[256];

	/* to do - need to check new frequncy is valid */
	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_freq, sizeof(buf_freq), "%d", val * 1000);

	return dbus_cpu_handler(PREDEF_SET_MIN_FREQUENCY, buf_pid, buf_freq);
}

API int deviced_release_cpu_max_frequency()
{
	char buf_pid[8];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());

	return dbus_cpu_handler(PREDEF_RELEASE_MAX_FREQUENCY, buf_pid, "2");
}

API int deviced_release_cpu_min_frequency()
{
	char buf_pid[8];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());

	return dbus_cpu_handler(PREDEF_RELEASE_MIN_FREQUENCY, buf_pid, "2");
}
