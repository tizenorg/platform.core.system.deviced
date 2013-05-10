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

#include "dd-deviced.h"
#include "deviced-priv.h"
#include "log.h"

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
#define PREDEF_MOUNT_MMC			"mountmmc"
#define PREDEF_UNMOUNT_MMC			"unmountmmc"
#define PREDEF_FORMAT_MMC			"formatmmc"

#define PREDEF_SET_MAX_FREQUENCY		"set_max_frequency"
#define PREDEF_SET_MIN_FREQUENCY		"set_min_frequency"
#define PREDEF_RELEASE_MAX_FREQUENCY		"release_max_frequency"
#define PREDEF_RELEASE_MIN_FREQUENCY		"release_min_frequency"

enum deviced_noti_cmd {
	ADD_DEVICED_ACTION,
	CALL_DEVICED_ACTION
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

static int deviced_noti_send(struct sysnoti *msg)
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
		_E("%s: socket create failed\n", __FUNCTION__);
		return -1;
	}
	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	strncpy(clientaddr.sun_path, SYSTEM_NOTI_SOCKET_PATH, sizeof(clientaddr.sun_path) - 1);
	client_len = sizeof(clientaddr);

	if (connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len) <
	    0) {
		_E("%s: connect failed\n", __FUNCTION__);
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
	msg->cmd = CALL_DEVICED_ACTION;
	msg->type = (char *)type;
	msg->path = NULL;

	msg->argc = num;
	va_start(argptr, num);
	for (i = 0; i < num; i++) {
		args = va_arg(argptr, char *);
		msg->argv[i] = args;
	}
	va_end(argptr);

	ret = deviced_noti_send(msg);
	free(msg);

	return ret;
}

API int deviced_inform_foregrd(void)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", getpid());
	return deviced_call_predef_action(PREDEF_FOREGRD, 1, buf);
}

API int deviced_inform_backgrd(void)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", getpid());
	return deviced_call_predef_action(PREDEF_BACKGRD, 1, buf);
}

API int deviced_inform_active(pid_t pid)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", pid);
	return deviced_call_predef_action(PREDEF_ACTIVE, 1, buf);
}

API int deviced_inform_inactive(pid_t pid)
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%d", pid);
	return deviced_call_predef_action(PREDEF_INACTIVE, 1, buf);
}

API int deviced_request_poweroff(void)
{
	return deviced_call_predef_action(PREDEF_PWROFF_POPUP, 0);
}

API int deviced_request_entersleep(void)
{
	return deviced_call_predef_action(PREDEF_ENTERSLEEP, 0);
}

API int deviced_request_leavesleep(void)
{
	return deviced_call_predef_action(PREDEF_LEAVESLEEP, 0);
}

API int deviced_request_reboot(void)
{
	return deviced_call_predef_action(PREDEF_REBOOT, 0);
}

API int deviced_set_datetime(time_t timet)
{
	if (timet < 0L)
		return -1;
	char buf[255] = { 0 };
	snprintf(buf, sizeof(buf), "%ld", timet);
	return deviced_call_predef_action(PREDEF_SET_DATETIME, 1, buf);
}

API int deviced_set_timezone(char *tzpath_str)
{
	if (tzpath_str == NULL)
		return -1;
	char buf[255];
	snprintf(buf, sizeof(buf), "%s", tzpath_str);
	return deviced_call_predef_action(PREDEF_SET_TIMEZONE, 1, buf);
}

static int deviced_noti_mount_mmc_cb(keynode_t *key_nodes, void *data)
{
	struct mmc_contents *mmc_data;
	int mmc_err = 0;
	mmc_data = (struct mmc_contents *)data;
	_D("mountmmc_cb called");
	if (vconf_keynode_get_int(key_nodes) ==
	    VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED) {
		_D("mount ok");
		(mmc_data->mmc_cb)(0, mmc_data->user_data);
	} else if (vconf_keynode_get_int(key_nodes) ==
		   VCONFKEY_SYSMAN_MMC_MOUNT_ALREADY) {
		_D("mount already");
		(mmc_data->mmc_cb)(-2, mmc_data->user_data);
	} else {
		_D("mount fail");
		vconf_get_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, &mmc_err);
		(mmc_data->mmc_cb)(mmc_err, mmc_data->user_data);
	}
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_MOUNT,
				 (void *)deviced_noti_mount_mmc_cb);
	return 0;
}

API int deviced_request_mount_mmc(struct mmc_contents *mmc_data)
{
	if (mmc_data != NULL && mmc_data->mmc_cb != NULL)
		vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_MOUNT,
					 (void *)deviced_noti_mount_mmc_cb, (void *)mmc_data);
	return deviced_call_predef_action(PREDEF_MOUNT_MMC, 0);
}

static int deviced_noti_unmount_mmc_cb(keynode_t *key_nodes, void *data)
{
	struct mmc_contents *mmc_data;
	int mmc_err = 0;
	mmc_data = (struct mmc_contents *)data;
	_D("unmountmmc_cb called");
	if (vconf_keynode_get_int(key_nodes) ==
	    VCONFKEY_SYSMAN_MMC_UNMOUNT_COMPLETED) {
		_D("unmount ok");
		(mmc_data->mmc_cb)(0, mmc_data->user_data);
	} else {
		_D("unmount fail");
		vconf_get_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, &mmc_err);
		(mmc_data->mmc_cb)(mmc_err, mmc_data->user_data);
	}
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_UNMOUNT,
				 (void *)deviced_noti_unmount_mmc_cb);
	return 0;
}

API int deviced_request_unmount_mmc(struct mmc_contents *mmc_data, int option)
{
	char buf[255];
	if (option != 1 && option != 2) {
		_D("option is wrong. default option 1 will be used");
		option = 1;
	}
	snprintf(buf, sizeof(buf), "%d", option);

	if (mmc_data != NULL && mmc_data->mmc_cb != NULL)
		vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_UNMOUNT,
					 (void *)deviced_noti_unmount_mmc_cb,
					 (void *)mmc_data);
	return deviced_call_predef_action(PREDEF_UNMOUNT_MMC, 1, buf);
}

static int deviced_noti_format_mmc_cb(keynode_t *key_nodes, void *data)
{
	struct mmc_contents *mmc_data;
	mmc_data = (struct mmc_contents *)data;
	_D("format_cb called");
	if (vconf_keynode_get_int(key_nodes) ==
	    VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED) {
		_D("format ok");
		(mmc_data->mmc_cb)(0, mmc_data->user_data);

	} else {
		_D("format fail");
		(mmc_data->mmc_cb)(-1, mmc_data->user_data);
	}
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_FORMAT,
				 (void *)deviced_noti_format_mmc_cb);
	return 0;
}

API int deviced_request_format_mmc(struct mmc_contents *mmc_data)
{
	if (mmc_data != NULL && mmc_data->mmc_cb != NULL)
		vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_FORMAT,
					 (void *)deviced_noti_format_mmc_cb,
					 (void *)mmc_data);
	return deviced_call_predef_action(PREDEF_FORMAT_MMC, 0);
}

API int deviced_request_set_cpu_max_frequency(int val)
{
	char buf_pid[8];
	char buf_freq[256];

	// to do - need to check new frequncy is valid
	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_freq, sizeof(buf_freq), "%d", val * 1000);

	return deviced_call_predef_action(PREDEF_SET_MAX_FREQUENCY, 2, buf_pid, buf_freq);
}

API int deviced_request_set_cpu_min_frequency(int val)
{
	char buf_pid[8];
	char buf_freq[256];

	// to do - need to check new frequncy is valid
	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_freq, sizeof(buf_freq), "%d", val * 1000);

	return deviced_call_predef_action(PREDEF_SET_MIN_FREQUENCY, 2, buf_pid, buf_freq);
}

API int deviced_release_cpu_max_frequency()
{
	char buf_pid[8];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());

	return deviced_call_predef_action(PREDEF_RELEASE_MAX_FREQUENCY, 1, buf_pid);
}

API int deviced_release_cpu_min_frequency()
{
	char buf_pid[8];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());

	return deviced_call_predef_action(PREDEF_RELEASE_MIN_FREQUENCY, 1, buf_pid);
}
