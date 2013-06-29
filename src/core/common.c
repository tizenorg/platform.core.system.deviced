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
#include <poll.h>
#include "log.h"

#define PERMANENT_DIR		"/tmp/permanent"
#define VIP_DIR			"/tmp/vip"

/**
 * Opens "/proc/$pid/oom_score_adj" file for w/r;
 * Return: FILE pointer or NULL
 */
FILE * open_proc_oom_score_adj_file(int pid, const char *mode)
{
        char buf[32];
        FILE *fp;

	snprintf(buf, sizeof(buf), "/proc/%d/oom_score_adj", pid);
	fp = fopen(buf, mode);
	return fp;
}

int get_exec_pid(const char *execpath)
{
	DIR *dp;
	struct dirent *dentry;
	int pid = -1, fd;
	char buf[PATH_MAX];
	char buf2[PATH_MAX];

	dp = opendir("/proc");
	if (!dp) {
		_E("FAIL: open /proc");
		return -1;
	}

	while ((dentry = readdir(dp)) != NULL) {
		if (!isdigit(dentry->d_name[0]))
			continue;

		pid = atoi(dentry->d_name);

		snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;
		if (read(fd, buf2, PATH_MAX) < 0) {
			close(fd);
			continue;
		}
		close(fd);

		if (!strcmp(buf2, execpath)) {
			closedir(dp);
			return pid;
		}
	}

	errno = ESRCH;
	closedir(dp);
	return -1;
}

int get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size)
{
	int fd, ret;
	char buf[PATH_MAX + 1];
	char *filename;

	snprintf(buf, sizeof(buf), "/proc/%d/cmdline", pid);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		errno = ESRCH;
		return -1;
	}

	ret = read(fd, buf, PATH_MAX);
	close(fd);
	buf[PATH_MAX] = '\0';

	filename = strrchr(buf, '/');
	if (filename == NULL)
		filename = buf;
	else
		filename = filename + 1;

	if (cmdline_size < strlen(filename) + 1) {
		errno = EOVERFLOW;
		return -1;
	}

	strncpy(cmdline, filename, cmdline_size - 1);
	cmdline[cmdline_size - 1] = '\0';
	return 0;
}

int is_vip(int pid)
{
	if (pid < 1)
		return -1;

	char buf[PATH_MAX];

	snprintf(buf, PATH_MAX, "%s/%d", VIP_DIR, pid);

	if (access(buf, R_OK) == 0)
		return 1;
	else
		return 0;
}
