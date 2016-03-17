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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <poll.h>

#include "log.h"
#include "dd-deviced.h"
#include "deviced-priv.h"

API int deviced_get_pid(const char *execpath)
{
	DIR *dp;
	struct dirent entry;
	struct dirent *dentry;
	int pid = -1, fd;
	char buf[BUFF_MAX];
	char buf2[BUFF_MAX];
	int ret;

	dp = opendir("/proc");
	if (!dp) {
		_E("open /proc");
		return -1;
	}

	if ((readdir_r(dp, &entry, &dentry)) != 0)
		dentry = NULL;

	while (dentry != NULL) {
		if (!isdigit(dentry->d_name[0]))
			continue;

		pid = atoi(dentry->d_name);

		snprintf(buf, BUFF_MAX, "/proc/%d/cmdline", pid);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;

		ret = read(fd, buf2, BUFF_MAX);
		close(fd);

		if (ret < 0 || ret >=BUFF_MAX)
			continue;

		buf2[ret] = '\0';

		if (!strcmp(buf2, execpath)) {
			closedir(dp);
			return pid;
		}
	}

	errno = ESRCH;
	closedir(dp);
	return -1;
}

API int deviced_get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size)
{
	int fd, ret;
	char buf[PATH_MAX + 1];
	char *filename;

	if (cmdline == NULL) {
		errno = EINVAL;
		return -EINVAL;
	}

	snprintf(buf, sizeof(buf), "/proc/%d/cmdline", pid);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		errno = ESRCH;
		return -ESRCH;
	}

	ret = read(fd, buf, PATH_MAX);
	close(fd);
	if (ret < 0) {
		ret = -errno;
		_E("read() failed (%d)", ret);
		return ret;
	}
	buf[PATH_MAX] = '\0';

	filename = strrchr(buf, '/');
	if (filename == NULL)
		filename = buf;
	else
		filename = filename + 1;

	if (cmdline_size < strlen(filename) + 1) {
		errno = EOVERFLOW;
		return -EOVERFLOW;
	}

	strncpy(cmdline, filename, cmdline_size - 1);
	cmdline[cmdline_size - 1] = '\0';
	return 0;
}

API int deviced_get_apppath(pid_t pid, char *app_path, size_t app_path_size)
{
	char buf[PATH_MAX];
	int ret;

	snprintf(buf, PATH_MAX, "/proc/%d/exe", pid);
	if (app_path == NULL
	    || (ret = readlink(buf, app_path, app_path_size)) == -1)
		return -1;
	if (app_path_size == ret) {
		app_path[ret - 1] = '\0';
		errno = EOVERFLOW;
		return -1;
	}

	app_path[ret] = '\0';
	return 0;
}
