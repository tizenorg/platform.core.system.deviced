/*
 * devman
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/poll.h>

#include "devlog.h"
#include "devman_internal.h"

#define WD_RESPONSE_TIMEOUT		100 /* 0.1 seconds */
#define DISPLAY_WD_PATH			"/usr/bin/display_wd"

static int fifo_fd;

API int display_register_postjob(void)
{
	int ret, i;
	long open_max;
	pid_t pid;
	char buf[PATH_MAX];
	char fifo_path[NAME_MAX];
	struct pollfd fifo_pollfd;

	snprintf(fifo_path, NAME_MAX, "%s.%d", DISPLAY_WD_FIFO, getpid());
	if (access(fifo_path, F_OK) == 0) {
		DEVERR("Already registered!");
		return -1;
	}

	if (mkfifo(fifo_path, 0700) != 0) {
		DEVERR("mkfifo error : %s", strerror(errno));
		return -1;
	}

	pid = fork();
	if(pid < 0) {
		DEVERR("Failed to fork child process for LCD On/Off");
		unlink(fifo_path);
		return -1;
	}
	if (pid == 0) {
		open_max = sysconf(_SC_OPEN_MAX);
		for (i = 0; i < open_max; i++) {
			close(i);
		}

		execl(DISPLAY_WD_PATH, DISPLAY_WD_PATH, NULL);
	}

	fifo_pollfd.fd = open(fifo_path, O_RDWR | O_NONBLOCK);
	if (fifo_pollfd.fd < 0) {
		DEVERR("Cannot open fifo file");
		unlink(fifo_path);
		return -1;
	}

	/* get the watch dog ready message. */
	fifo_pollfd.events = POLLIN;
	if (poll(&fifo_pollfd, 1, WD_RESPONSE_TIMEOUT) < 0) {
		DEVERR("Cannot poll the fifo file");
		DEVLOG("fifo file path is %s", fifo_path);
		close(fifo_pollfd.fd);
		unlink(fifo_path);
		return -1;
	}
	read(fifo_pollfd.fd, buf, sizeof(buf));

	fifo_fd = fifo_pollfd.fd;

	return 0;
}

API int display_cancel_postjob(void)
{
	char buf[PATH_MAX];
	int ret;

	snprintf(buf, PATH_MAX, "%s.%d", DISPLAY_WD_FIFO, getpid());
	if (access(buf, F_OK) != 0) {
		DEVERR("No registered the post job!");
		return -1;
	}

	if (fifo_fd < 0)
		fifo_fd = open(buf, O_WRONLY);
	if (fifo_fd < 0) {
		DEVERR("Cannot open the fifo file");
		DEVLOG("fifo file path is %s", buf);
		return -1;
	}
	ret = DISPLAY_WD_CANCEL;
	write(fifo_fd, &ret, sizeof(int));
	close(fifo_fd);
	unlink(buf);
	fifo_fd = -1;

	return 0;
}

