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
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <poll.h>
#include <mntent.h>
#include <notification.h>

#include "log.h"
#include "common.h"

#define PERMANENT_DIR   "/tmp/permanent"
#define VIP_DIR         "/tmp/vip"
#define BUFF_MAX        255

/**
 * Opens "/proc/$pid/oom_score_adj" file for w/r;
 * Return: FILE pointer or NULL
 */
FILE *open_proc_oom_score_adj_file(int pid, const char *mode)
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
	struct dirent entry;
	struct dirent *dentry;
	int pid = -1, fd;
	int ret;
	char buf[PATH_MAX];
	char buf2[PATH_MAX];

	dp = opendir("/proc");
	if (!dp) {
		_E("FAIL: open /proc");
		return -1;
	}

	while (1) {
		ret = readdir_r(dp, &entry, &dentry);
		if (ret != 0 || dentry == NULL)
			break;

		if (!isdigit(dentry->d_name[0]))
			continue;

		pid = atoi(dentry->d_name);

		snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;
		ret = read(fd, buf2, PATH_MAX);
		close(fd);

		if (ret < 0 || ret >= PATH_MAX)
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
	if (ret < 0)
		return -1;

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

static int remove_dir_internal(int fd)
{
	DIR *dir;
	struct dirent entry;
	struct dirent *de;
	int subfd, ret = 0;

	dir = fdopendir(fd);
	if (!dir)
		return -1;
	while (1) {
		ret = readdir_r(dir, &entry, &de);
		if (ret != 0 || de == NULL)
			break;
		if (de->d_type == DT_DIR) {
			if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
				continue;
			subfd = openat(fd, de->d_name, O_RDONLY | O_DIRECTORY);
			if (subfd < 0) {
				_SE("Couldn't openat %s: %d\n", de->d_name, errno);
				ret = -1;
				continue;
			}
			if (remove_dir_internal(subfd))
				ret = -1;

			close(subfd);
			if (unlinkat(fd, de->d_name, AT_REMOVEDIR) < 0) {
				_SE("Couldn't unlinkat %s: %d\n", de->d_name, errno);
				ret = -1;
			}
		} else {
			if (unlinkat(fd, de->d_name, 0) < 0) {
				_SE("Couldn't unlinkat %s: %d\n", de->d_name, errno);
				ret = -1;
			}
		}
	}
	closedir(dir);
	return ret;
}

int remove_dir(const char *path, int del_dir)
{
	int fd, ret = 0;

	if (!path)
		return -1;
	fd = open(path, O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0) {
		_SE("Couldn't opendir %s: %d\n", path, errno);
		return -errno;
	}
	ret = remove_dir_internal(fd);
	close(fd);

	if (del_dir) {
		if (rmdir(path)) {
			_SE("Couldn't rmdir %s: %d\n", path, errno);
			ret = -1;
		}
	}
	return ret;
}

/*
 * Helper function
 * - Read from sysfs entry
 * - Write to sysfs entry
 */
int sys_check_node(char *path)
{
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;

	close(fd);
	return 0;
}

static int sys_read_buf(char *file, char *buf)
{
	int fd;
	int r;
	int ret = 0;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -ENOENT;

	r = read(fd, buf, BUFF_MAX);
	if ((r >= 0) && (r < BUFF_MAX))
		buf[r] = '\0';
	else
		ret = -EIO;

	close(fd);

	return ret;
}

static int sys_write_buf(char *file, char *buf)
{
	int fd;
	int r;
	int ret = 0;

	fd = open(file, O_WRONLY);
	if (fd == -1)
		return -ENOENT;

	r = write(fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;

	close(fd);

	return ret;
}

int sys_get_int(char *fname, int *val)
{
	char buf[BUFF_MAX];
	int ret = 0;

	if (sys_read_buf(fname, buf) == 0) {
		*val = atoi(buf);
	} else {
		*val = -1;
		ret = -EIO;
	}

	return ret;
}

int sys_set_int(char *fname, int val)
{
	char buf[BUFF_MAX];
	int ret = 0;

	snprintf(buf, sizeof(buf), "%d", val);

	if (sys_write_buf(fname, buf) != 0)
		ret = -EIO;

	return ret;
}

int sys_get_str(char *fname, char *str)
{
	char buf[BUFF_MAX] = {0};

	if (sys_read_buf(fname, buf) == 0) {
		strncpy(str, buf, strlen(buf));
		return 0;
	}

	return -1;
}

int sys_set_str(char *fname, char *val)
{
	int r = -1;

	if (val != NULL) {
		if (sys_write_buf(fname, val) == 0)
			r = 0;
	}

	return r;
}

int terminate_process(const char *partition, bool force)
{
	const char *argv[7] = {"/sbin/fuser", "-m", "-k", "-S", NULL, NULL, NULL};
	int argc;

	if (force)
		argv[4] = "-SIGKILL";
	else
		argv[4] = "-SIGTERM";
	argv[5] = partition;
	argc = sizeof(argv) / sizeof(argv[0]);
	return run_child(argc, argv);
}

int mount_check(const char *path)
{
	int ret = false;
	struct mntent *mnt;
	const char *table = "/etc/mtab";
	FILE *fp;

	fp = setmntent(table, "r");
	if (!fp)
		return ret;
	while (1) {
		mnt = getmntent(fp);
		if (mnt == NULL)
			break;
		if (!strcmp(mnt->mnt_dir, path)) {
			ret = true;
			break;
		}
	}
	endmntent(fp);
	return ret;
}

void print_time(const char *prefix)
{
	struct timeval tv;
	struct tm *tm;
	gettimeofday(&tv, NULL);
	tm = localtime(&(tv.tv_sec));
	_D("%s --> %d:%02d:%02d %d",
			prefix, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec);
}

int manage_notification(char *title, char *content)
{
	notification_h noti;

	noti = notification_create(NOTIFICATION_TYPE_NOTI);
	if (noti == NULL) {
		_E("fail to create %s notification (content:%s)", title, content);
		return -1;
	}

	notification_set_text(noti, NOTIFICATION_TEXT_TYPE_TITLE, title, NULL, NOTIFICATION_TYPE_NONE);
	notification_set_text(noti, NOTIFICATION_TEXT_TYPE_CONTENT, content, NULL, NOTIFICATION_TYPE_NONE);
	notification_insert(noti, NULL);
	notification_free(noti);
	return 0;
}
