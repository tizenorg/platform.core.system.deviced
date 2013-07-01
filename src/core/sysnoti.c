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


#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "data.h"
#include "log.h"
#include "queue.h"
#include "common.h"
#include "devices.h"

#define SYSNOTI_SOCKET_PATH "/tmp/sn"
#define RETRY_READ_COUNT	5
enum sysnoti_cmd {
	ADD_SYSMAN_ACTION,
	CALL_SYSMAN_ACTION
};

static Ecore_Fd_Handler *sysnoti_efd = NULL;

static int __sysnoti_start(void);
static int __sysnoti_stop(int fd);

static void print_sysnoti_msg(const char *title, struct sysnoti *msg)
{
	int i;
	char exe_name[PATH_MAX];

	if (get_cmdline_name(msg->pid, exe_name, PATH_MAX) < 0)
		snprintf(exe_name, sizeof(exe_name), "Unknown (maybe dead)");

	_E("pid : %d name: %s cmd : %d type : %s path : %s",
			msg->pid, exe_name, msg->cmd, msg->type, msg->path);
}

static inline int recv_int(int fd)
{
	int val, r = -1;
	int retry_count = 0;
	while (retry_count < RETRY_READ_COUNT) {
		r = read(fd, &val, sizeof(int));
		if (r < 0) {
			if(errno == EINTR) {
				_E("Re-read for error(EINTR)");
				retry_count++;
				continue;
			}
			else {
				_E("Read fail for int");
				return -1;
			}
		} else {
			return val;
		}
	}
	return -1;
}

static inline char *recv_str(int fd)
{
	int len, r = -1;
	int retry_count = 0;
	char *str;

	while (retry_count < RETRY_READ_COUNT) {
		r = read(fd, &len, sizeof(int));
		if (r < 0) {
			if(errno == EINTR) {
				_E("Re-read for error(EINTR)");
				retry_count++;
				continue;
			}
			else {
				_E("Read fail for str length");
				return NULL;
			}
		} else
			break;
	}
	if (retry_count == RETRY_READ_COUNT) {
		_E("Read retry failed");
		return NULL;
	}
	if (len <= 0) {
		_E("str is null");
		return NULL;
	}

	if (len >= INT_MAX) {
		_E("size is over INT_MAX");
		return NULL;
	}

	str = (char *)malloc(len + 1);
	if (str == NULL) {
		_E("Not enough memory");
		return NULL;
	}
	retry_count = 0;
	while (retry_count < RETRY_READ_COUNT) {
		r = read(fd, str, len);
		if(r < 0) {
			if(errno == EINTR) {
				_E("Re-read for error(EINTR)");
				retry_count++;
				continue;                       
			}                                               
			else {      
				_E("Read fail for str");
				free(str);
				str = NULL;
				return NULL;
			}                                                           
		} else
			break;
	}
	if (retry_count == RETRY_READ_COUNT) {
		_E("Read retry failed");
		free(str);
		str = NULL;
		return NULL;
	}

	str[len] = 0;

	return str;
}

static int read_message(int fd, struct sysnoti *msg)
{
	int i;

	if ((msg->pid = recv_int(fd)) == -1)
		return -1;
	if ((msg->cmd = recv_int(fd)) == -1)
		return -1;
	if ((msg->type = recv_str(fd)) == NULL)
		return -1;
	msg->path = recv_str(fd);
	msg->argc = recv_int(fd);

	if (msg->argc < 0)
		return -1;
	for (i = 0; i < msg->argc; i++)
		msg->argv[i] = recv_str(fd);

	return 0;
}

static inline void internal_free(char *str)
{
	if (str)
		free(str);
}

static inline void free_message(struct sysnoti *msg)
{
	internal_free(msg->type);
	internal_free(msg->path);
	free(msg);
}

static int sysnoti_cb(void *data, Ecore_Fd_Handler * fd_handler)
{
	int fd;
	struct sysnoti *msg;
	int ret = -1;
	struct sockaddr_un client_address;
	int client_sockfd;
	int client_len;

	if (!ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ)) {
		_E
		    ("ecore_main_fd_handler_active_get error , return\n");
		return 1;
	}

	fd = ecore_main_fd_handler_fd_get(fd_handler);
	msg = malloc(sizeof(struct sysnoti));
	if (msg == NULL) {
		_E("%s : Not enough memory", __FUNCTION__);
		return 1;
	}

	client_len = sizeof(client_address);
	client_sockfd = accept(fd, (struct sockaddr *)&client_address, (socklen_t *)&client_len);

	if (client_sockfd == -1) {
		_E("socket accept error");
		free(msg);
		return -1;
	}
	if (read_message(client_sockfd, msg) < 0) {
		_E("%s : recv error msg", __FUNCTION__);
		print_sysnoti_msg(__FUNCTION__, msg);
		free_message(msg);
		write(client_sockfd, &ret, sizeof(int));
		close(client_sockfd);
		__sysnoti_stop(fd);
		__sysnoti_start();
		return -1;
	}

	print_sysnoti_msg(__FUNCTION__, msg);
	if (msg->argc > SYSMAN_MAXARG) {
		_E("%s : error argument", __FUNCTION__);
		free_message(msg);
		write(client_sockfd, &ret, sizeof(int));
		close(client_sockfd);
		return -1;
	}

	switch (msg->cmd) {
	case CALL_SYSMAN_ACTION:
		ret = ss_action_entry_call(msg, client_sockfd);
		break;
	default:
		ret = -1;
	}


	write(client_sockfd, &ret, sizeof(int));
	close(client_sockfd);

	free_message(msg);

	return 1;
}

static int ss_sysnoti_server_init(void)
{
	int fd;
	struct sockaddr_un serveraddr;

	if (access(SYSNOTI_SOCKET_PATH, F_OK) == 0)
		unlink(SYSNOTI_SOCKET_PATH);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		_E("%s: socket create failed\n", __FUNCTION__);
		return -1;
	}
	if((fsetxattr(fd, "security.SMACK64IPOUT", "@", 2, 0)) < 0 ) {
		_E("%s: Socket SMACK labeling failed\n", __FUNCTION__);
		if(errno != EOPNOTSUPP) {
			close(fd);
			return -1;
		}
	}

	if((fsetxattr(fd, "security.SMACK64IPIN", "*", 2, 0)) < 0 ) {
		_E("%s: Socket SMACK labeling failed\n", __FUNCTION__);
		if(errno != EOPNOTSUPP) {
			close(fd);
			return -1;
		}
	}

	bzero(&serveraddr, sizeof(struct sockaddr_un));
	serveraddr.sun_family = AF_UNIX;
	strncpy(serveraddr.sun_path, SYSNOTI_SOCKET_PATH,
		sizeof(serveraddr.sun_path));

	if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr)) < 0) {
		_E("%s: socket bind failed\n", __FUNCTION__);
		close(fd);
		return -1;
	}

	if (chmod(SYSNOTI_SOCKET_PATH, (S_IRWXU | S_IRWXG | S_IRWXO)) < 0)	/* 0777 */
		_E("failed to change the socket permission");

	if (listen(fd, 5) < 0) {
		_E("failed to listen");
		close(fd);
		return -1;
	}
	_I("socket create & listen ok\n");

	return fd;
}

static int __sysnoti_start(void)
{
	int fd;
	fd = ss_sysnoti_server_init();
	if ( fd < 0 )
		return -1;
	sysnoti_efd = ecore_main_fd_handler_add(fd, ECORE_FD_READ, sysnoti_cb, NULL, NULL,
				  NULL);
	if (!sysnoti_efd) {
		_E("error ecore_main_fd_handler_add");
		return -1;
	}
	return fd;
}

static int __sysnoti_stop(int fd)
{
	if (sysnoti_efd) {
		ecore_main_fd_handler_del(sysnoti_efd);
		sysnoti_efd = NULL;
	}
	if (fd >=0) {
		close(fd);
		fd = -1;
	}
	return 0;
}

static void sysnoti_init(void *data)
{
	struct ss_main_data *ad = (struct ss_main_data*)data;

	ad->sysnoti_fd = __sysnoti_start();
}

const struct device_ops sysnoti_device_ops = {
	.init = sysnoti_init,
};
