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

#include "util.h"
#include "core.h"
#include "poll.h"
#include "core/devices.h"

#define SHIFT_UNLOCK                    4
#define SHIFT_UNLOCK_PARAMETER          12
#define SHIFT_CHANGE_STATE              8
#define SHIFT_CHANGE_TIMEOUT            20

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

	if (device_get_status(&display_device_ops) != DEVICE_OPS_STATUS_START) {
		LOGERR("display is not started!");
		return EINA_FALSE;
	}

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
	indev *new_dev = NULL;

	g_pm_callback = pm_callback;

	LOGINFO
	    ("initialize pm poll - input devices and domain socket(deviced)");

	pm_input_env = getenv("PM_INPUT");
	if ((pm_input_env != NULL) && (strlen(pm_input_env) < 1024)) {
		LOGINFO("Getting input device path from environment: %s",
		       pm_input_env);
		/* Add 2 bytes for following strncat() */
		dev_paths_size =  strlen(pm_input_env) + strlen(SOCK_PATH) + strlen(DEV_PATH_DLM) + 1;
		dev_paths = (char *)malloc(dev_paths_size);
		if (!dev_paths)
			return -1;
		snprintf(dev_paths, dev_paths_size, "%s", pm_input_env);
	} else {
		/* Add 2 bytes for following strncat() */
		dev_paths_size = strlen(DEFAULT_DEV_PATH) + strlen(SOCK_PATH) + strlen(DEV_PATH_DLM) + 1;
		dev_paths = (char *)malloc(dev_paths_size);
		if (!dev_paths)
			return -1;
		snprintf(dev_paths, dev_paths_size, "%s", DEFAULT_DEV_PATH);
	}

	/* add the UNIX domain socket file path */
	strncat(dev_paths, DEV_PATH_DLM, strlen(DEV_PATH_DLM));
	strncat(dev_paths, SOCK_PATH, strlen(SOCK_PATH));
	dev_paths[dev_paths_size - 1] = '\0';

	path_tok = strtok_r(dev_paths, DEV_PATH_DLM, &save_ptr);
	if (path_tok == NULL) {
		LOGERR("Device Path Tokeninzing Failed");
		goto err_devpaths;
	}

	do {
		char *path, *new_path;
		int len;

		if (strcmp(path_tok, SOCK_PATH) == 0) {
			fd = init_sock(SOCK_PATH);
			path = SOCK_PATH;
			LOGINFO("pm_poll domain socket file: %s, fd: %d",
			       path_tok, fd);
		} else {
			fd = open(path_tok, O_RDONLY);
			path = path_tok;
			LOGINFO("pm_poll input device file: %s, fd: %d",
			       path_tok, fd);
		}

		if (fd == -1) {
			LOGERR("Cannot open the file: %s", path_tok);
			goto err_devpaths;
		}

		fd_handler = ecore_main_fd_handler_add(fd,
				    ECORE_FD_READ|ECORE_FD_ERROR,
				    pm_handler, fd, NULL, NULL);
		if (fd_handler == NULL) {
			LOGERR("Failed ecore_main_handler_add() in init_pm_poll()");
			goto err_fd;
		}

		new_dev = (indev *)malloc(sizeof(indev));
		if (!new_dev) {
			LOGERR("Fail to malloc for new_dev %s", path);
			goto err_fdhandler;
		}

		memset(new_dev, 0, sizeof(indev));
		len = strlen(path) + 1;
		new_path = (char*) malloc(len);
		if (!new_path) {
			LOGERR("Fail to malloc for dev_path %s", path);
			goto err_dev;
		}

		strncpy(new_path, path, len);
		new_dev->dev_path = new_path;
		new_dev->fd = fd;
		new_dev->dev_fd = fd_handler;
		indev_list = eina_list_append(indev_list, new_dev);

	} while ((path_tok = strtok_r(NULL, DEV_PATH_DLM, &save_ptr)));

	free(dev_paths);
	return 0;

err_dev:
	free(new_dev);
err_fdhandler:
	ecore_main_fd_handler_del(fd_handler);
err_fd:
	fclose(fd);
err_devpaths:
	free(dev_paths);
	return -1;
}

int exit_pm_poll(void)
{
	Eina_List *l = NULL;
	Eina_List *l_next = NULL;
	indev *data = NULL;

	EINA_LIST_FOREACH_SAFE(indev_list, l, l_next, data) {
		ecore_main_fd_handler_del(data->dev_fd);
		close(data->fd);
		free(data->dev_path);
		free(data);
		indev_list = eina_list_remove_list(indev_list, l);
	}

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

int pm_lock_internal(pid_t pid, int s_bits, int flag, int timeout)
{
	if (!g_pm_callback)
		return -1;

	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}
	if (flag & GOTO_STATE_NOW)
		/* if the flag is true, go to the locking state directly */
		s_bits = s_bits | (s_bits << SHIFT_CHANGE_STATE);

	recv_data.pid = pid;
	recv_data.cond = s_bits;
	recv_data.timeout = timeout;

	(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

int pm_unlock_internal(pid_t pid, int s_bits, int flag)
{
	if (!g_pm_callback)
		return -1;

	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}

	s_bits = (s_bits << SHIFT_UNLOCK);
	s_bits = (s_bits | (flag << SHIFT_UNLOCK_PARAMETER));

	recv_data.pid = pid;
	recv_data.cond = s_bits;

	(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

int pm_change_internal(pid_t pid, int s_bits)
{
	if (!g_pm_callback)
		return -1;

	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}

	recv_data.pid = pid;
	recv_data.cond = s_bits << SHIFT_CHANGE_STATE;

	(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);

	return 0;
}

void lcd_control_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int pid = -1;
	char *lock_str = NULL;
	char *state_str = NULL;
	int state = -1;
	int timeout = -1;

	if (dbus_message_is_signal(msg, INTERFACE_NAME, SIGNAL_NAME_LCD_CONTROL) == 0) {
		LOGERR("there is lcd control signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err,
		    DBUS_TYPE_INT32, &pid,
		    DBUS_TYPE_STRING, &lock_str,
		    DBUS_TYPE_STRING, &state_str,
		    DBUS_TYPE_INT32, &timeout, DBUS_TYPE_INVALID) == 0) {
		LOGERR("there is no message");
		return;
	}

	if (pid == -1 || !lock_str || !state_str) {
		LOGERR("message is invalid!!");
		return;
	}

	if (kill(pid, 0) == -1) {
		LOGERR("%d process does not exist, dbus ignored!", pid);
		return;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else {
		LOGERR("%d process does not exist, dbus ignored!", pid);
		return;
	}

	if (!strcmp(lock_str, PM_LOCK_STR)) {
		if (timeout < 0) {
			LOGERR("pm_lock timeout is invalid! %d", timeout);
			return;
		}
		pm_lock_internal(pid, state, STAY_CUR_STATE, timeout);
	} else if (!strcmp(lock_str, PM_UNLOCK_STR)) {
		pm_unlock_internal(pid, state, PM_SLEEP_MARGIN);
	} else if (!strcmp(lock_str, PM_CHANGE_STR)) {
		pm_change_internal(pid, state);
	} else {
		LOGERR("%s process does not exist, dbus ignored!", pid);
		return;
	}
	LOGINFO("dbus call success from %d\n", pid);
}
