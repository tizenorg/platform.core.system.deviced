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
#include <dd-deviced.h>
#include <dd-mmc.h>

#include "sysman-priv.h"

enum sysnoti_cmd {
	ADD_SYSMAN_ACTION,
	CALL_SYSMAN_ACTION
};

#define SYSNOTI_SOCKET_PATH "/tmp/sn"
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
		if (len > SYSMAN_MAXSTR)
			len = SYSMAN_MAXSTR;
		write(fd, &len, sizeof(int));
		ret = write(fd, str, len);
	}
	return ret;
}

static int sysnoti_send(struct sysnoti *msg)
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
		ERR("%s: socket create failed\n", __FUNCTION__);
		return -1;
	}
	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	strncpy(clientaddr.sun_path, SYSNOTI_SOCKET_PATH, sizeof(clientaddr.sun_path) - 1);
	client_len = sizeof(clientaddr);

	if (connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len) <
	    0) {
		ERR("%s: connect failed\n", __FUNCTION__);
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
				ERR("Re-read for error(EINTR)");
				retry_count++;
				continue;
			}
			ERR("Read fail for str length");
			result = -1;
			break;

		}
		break;
	}
	if (retry_count == RETRY_READ_COUNT) {
		ERR("Read retry failed");
	}

	close(client_sockfd);
	return result;
}

API int sysman_call_predef_action(const char *type, int num, ...)
{
	struct sysnoti *msg;
	int ret;
	va_list argptr;

	int i;
	char *args = NULL;

	if (type == NULL || num > SYSMAN_MAXARG) {
		errno = EINVAL;
		return -1;
	}

	msg = malloc(sizeof(struct sysnoti));

	if (msg == NULL) {
		/* Do something for not enought memory error */
		return -1;
	}

	msg->pid = getpid();
	msg->cmd = CALL_SYSMAN_ACTION;
	msg->type = (char *)type;
	msg->path = NULL;

	msg->argc = num;
	va_start(argptr, num);
	for (i = 0; i < num; i++) {
		args = va_arg(argptr, char *);
		msg->argv[i] = args;
	}
	va_end(argptr);

	ret = sysnoti_send(msg);
	free(msg);

	return ret;
}

API int sysman_inform_foregrd(void)
{
	return deviced_inform_foregrd();
}

API int sysman_inform_backgrd(void)
{
	return deviced_inform_backgrd();
}

API int sysman_inform_active(pid_t pid)
{
	return deviced_inform_active(pid);
}

API int sysman_inform_inactive(pid_t pid)
{
	return deviced_inform_inactive(pid);
}

API int sysman_request_poweroff(void)
{
	return deviced_request_poweroff();
}

API int sysman_request_entersleep(void)
{
	return deviced_request_entersleep();
}

API int sysman_request_leavesleep(void)
{
	return deviced_request_leavesleep();
}

API int sysman_request_reboot(void)
{
	return deviced_request_reboot();
}

API int sysman_set_datetime(time_t timet)
{
	return deviced_set_datetime(timet);
}

API int sysman_set_timezone(char *tzpath_str)
{
	return deviced_set_timezone(tzpath_str);
}

API int sysman_request_mount_mmc(struct mmc_contents *mmc_data)
{
	return deviced_request_mount_mmc(mmc_data);
}

API int sysman_request_unmount_mmc(struct mmc_contents *mmc_data, int option)
{
	return deviced_request_unmount_mmc(mmc_data, option);
}

API int sysman_request_format_mmc(struct mmc_contents *mmc_data)
{
	return deviced_request_format_mmc(mmc_data);
}

API int sysman_request_set_cpu_max_frequency(int val)
{
	return deviced_request_set_cpu_min_frequency(val);
}

API int sysman_request_set_cpu_min_frequency(int val)
{
	return deviced_request_set_cpu_min_frequency(val);
}

API int sysman_release_cpu_max_frequency()
{
	return deviced_release_cpu_max_frequency();
}

API int sysman_release_cpu_min_frequency()
{
	return deviced_release_cpu_min_frequency();
}
