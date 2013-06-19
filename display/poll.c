/*
 * power-manager
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


/**
 * @file	poll.c
 * @brief	 Power Manager poll implementation (input devices & a domain socket file)
 *
 * This file includes the input device poll implementation.
 * Default input devices are /dev/event0 and /dev/event1
 * User can use "PM_INPUT" for setting another input device poll in an environment file (/etc/profile).
 * (ex: PM_INPUT=/dev/event0:/dev/event1:/dev/event5 )
 */

#include <stdio.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <Ecore.h>

#include "core.h"
#include "poll.h"

#define DEV_PATH_DLM	":"

PMMsg recv_data;
int (*g_pm_callback) (int, PMMsg *);

#ifdef ENABLE_KEY_FILTER
extern int check_key_filter(int length, char buf[]);
#  define CHECK_KEY_FILTER(a, b) do {\
							if (check_key_filter(a, b) != 0)\
								return EINA_TRUE;\
							} while (0);

#else
#  define CHECK_KEY_FILTER(a, b)
#endif

#define DEFAULT_DEV_PATH "/dev/event1:/dev/event0"

static int sockfd;

static Eina_Bool pm_handler(void *data, Ecore_Fd_Handler *fd_handler)
{
	char buf[1024];
	struct sockaddr_un clientaddr;

	int *fd = (int *)data;
	int ret;
	int clilen = sizeof(clientaddr);

	if (g_pm_callback == NULL) {
		return EINA_FALSE;
	}
	if (fd == sockfd) {
		ret =
		    recvfrom(fd, &recv_data, sizeof(recv_data), 0,
			     (struct sockaddr *)&clientaddr,
			     (socklen_t *)&clilen);
		(*g_pm_callback) (PM_CONTROL_EVENT, &recv_data);
	} else {
		ret = read(fd, buf, sizeof(buf));
		CHECK_KEY_FILTER(ret, buf);
		(*g_pm_callback) (INPUT_POLL_EVENT, NULL);
	}

	return EINA_TRUE;
}

static int init_sock(char *sock_path)
{
	struct sockaddr_un serveraddr;
	int fd;

	LOGINFO("initialize pm_socket for pm_control library");

	if (sock_path == NULL || strcmp(sock_path, SOCK_PATH)) {
		LOGERR("invalid sock_path= %s");
		return -1;
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		LOGERR("socket error");
		return -1;
	}

	unlink(sock_path);

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strncpy(serveraddr.sun_path, sock_path, sizeof(serveraddr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		LOGERR("bind error");
		close(fd);
		return -1;
	}

	if (chmod(sock_path, (S_IRWXU | S_IRWXG | S_IRWXO)) < 0)	/* 0777 */
		LOGERR("failed to change the socket permission");

	if (!strcmp(sock_path, SOCK_PATH))
		sockfd = fd;

	LOGINFO("init sock() sueccess!");
	return fd;
}

int init_pm_poll(int (*pm_callback) (int, PMMsg *))
{
	char *dev_paths, *path_tok, *pm_input_env, *save_ptr;
	int dev_paths_size;

	Ecore_Fd_Handler *fd_handler;
	int fd = -1;

	g_pm_callback = pm_callback;

	LOGINFO
	    ("initialize pm poll - input devices and domain socket(libpmapi)");

	pm_input_env = getenv("PM_INPUT");
	if ((pm_input_env != NULL) && (strlen(pm_input_env) < 1024)) {
		LOGINFO("Getting input device path from environment: %s",
		       pm_input_env);
		/* Add 2 bytes for following strncat() */
		dev_paths_size =  strlen(pm_input_env) + strlen(SOCK_PATH) + strlen(DEV_PATH_DLM) + 1;
		dev_paths = (char *)malloc(dev_paths_size);
		snprintf(dev_paths, dev_paths_size, "%s", pm_input_env);
	} else {
		/* Add 2 bytes for following strncat() */
		dev_paths_size = strlen(DEFAULT_DEV_PATH) + strlen(SOCK_PATH) + strlen(DEV_PATH_DLM) + 1;
		dev_paths = (char *)malloc(dev_paths_size);
		snprintf(dev_paths, dev_paths_size, "%s", DEFAULT_DEV_PATH);
	}

	/* add the UNIX domain socket file path */
	strncat(dev_paths, DEV_PATH_DLM, strlen(DEV_PATH_DLM));
	strncat(dev_paths, SOCK_PATH, strlen(SOCK_PATH));
	dev_paths[dev_paths_size - 1] = '\0';

	path_tok = strtok_r(dev_paths, DEV_PATH_DLM, &save_ptr);
	if (path_tok == NULL) {
		LOGERR("Device Path Tokeninzing Failed");
		free(dev_paths);
		return -1;
	}

	do {
		if (strcmp(path_tok, SOCK_PATH) == 0) {
			fd = init_sock(SOCK_PATH);
			LOGINFO("pm_poll domain socket file: %s, fd: %d",
			       path_tok, fd);
		} else {
			fd = open(path_tok, O_RDONLY);
			LOGINFO("pm_poll input device file: %s, fd: %d",
			       path_tok, fd);
		}

		if (fd == -1) {
			LOGERR("Cannot open the file: %s", path_tok);
			free(dev_paths);
			return -1;
		}

		fd_handler = ecore_main_fd_handler_add(fd,
				    ECORE_FD_READ|ECORE_FD_ERROR,
				    pm_handler, fd, NULL, NULL);
		if (fd_handler == NULL) {
			LOGERR("Failed ecore_main_handler_add() in init_pm_poll()");
			free(dev_paths);
			return -1;
		}

	} while ((path_tok = strtok_r(NULL, DEV_PATH_DLM, &save_ptr)));

	free(dev_paths);
	return 0;
}

int exit_pm_poll(void)
{
	close(sockfd);
	unlink(SOCK_PATH);
	LOGINFO("pm_poll is finished");
	return 0;
}

int init_pm_poll_input(int (*pm_callback)(int , PMMsg * ), const char *path)
{
	indev *new_dev = NULL;
	indev *data = NULL;
	Ecore_Fd_Handler *fd_handler = NULL;
	Eina_List *l = NULL;
	Eina_List *l_next = NULL;
	int fd = -1;
	char *dev_path = NULL;

	if (!pm_callback || !path) {
		LOGERR("argument is NULL! (%x,%x)", pm_callback, path);
		return -1;
	}

	EINA_LIST_FOREACH_SAFE(indev_list, l, l_next, data)
		if(!strcmp(path, data->dev_path)) {
			LOGERR("%s is already polled!", path);
			return -1;
		}

	LOGINFO("initialize pm poll for bt %s", path);

	g_pm_callback = pm_callback;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		LOGERR("Cannot open the file for BT: %s", path);
		return -1;
	}

	dev_path = (char*)malloc(strlen(path) + 1);
	if (!dev_path) {
		LOGERR("Fail to malloc for dev_path");
		close(fd);
		return -1;
	}
	strncpy(dev_path, path, strlen(path) +1);

	fd_handler = ecore_main_fd_handler_add(fd,
			    ECORE_FD_READ|ECORE_FD_ERROR,
			    pm_handler, fd, NULL, NULL);
	if (!fd_handler) {
		LOGERR("Fail to ecore fd handler add! %s", path);
		close(fd);
		free(dev_path);
		return -1;
	}

	new_dev = (indev *)malloc(sizeof(indev));
	if (!new_dev) {
		LOGERR("Fail to malloc for new_dev %s", path);
		ecore_main_fd_handler_del(fd_handler);
		close(fd);
		free(dev_path);
		return -1;
	}
	new_dev->dev_path = dev_path;
	new_dev->fd = fd;
	new_dev->dev_fd = fd_handler;

	LOGINFO("pm_poll for BT input device file(path: %s, fd: %d",
		    new_dev->dev_path, new_dev->fd);
	indev_list = eina_list_append(indev_list, new_dev);

	return 0;
}
