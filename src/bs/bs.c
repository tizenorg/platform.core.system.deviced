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
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <grp.h>
#include <dirent.h>
#include <Ecore_File.h>
#include "core/log.h"
#include "core/launch.h"
#include "core/devices.h"
#include <tzplatform_config.h>

#define PREDEF_DUMP_LOG         "dump_log"
#define PREDEF_DELETE_DUMP      "delete_dump"

#define CRASH_WORKER_MAX	3
#define CRASH_PID_MAX 7
#define CRASH_MODE_MAX 2
#define CRASH_TIME_MAX 65
#define CRASH_POPUP_ON	1
#define CRASH_POPUP_OFF	0
#define CRASH_CHECK_SIZE (512 * 1024)
#define CRASH_CHECK_DISK_PATH   "/opt/usr"
#define CRASH_LIMIT_NUM 5
#define CRASH_ARG_NUM 6
#define CRASH_DELIMITER "|"
#define CRASH_VERIFY_MAX 5
#define CRASH_PROCESSNAME_MAX NAME_MAX
#define CRASH_EXEPATH_MAX NAME_MAX
#define CRASH_ARG_MAX (CRASH_PROCESSNAME_MAX + CRASH_EXEPATH_MAX + CRASH_TIME_MAX + CRASH_PID_MAX + CRASH_MODE_MAX + CRASH_VERIFY_MAX)
#define CRASH_NOTI_DIR		tzplatform_mkpath(TZ_SYS_SHARE, "crash")
#define CRASH_NOTI_FILE		"curbs.log"
#define CRASH_NOTI_PATH tzplatform_mkpath(TZ_SYS_SHARE, "crash/"CRASH_NOTI_FILE)
#define CRASH_COREDUMP_PATH		tzplatform_mkpath(TZ_USER_SHARE, "crash/core")
#define CRASH_DUMP_PATH		tzplatform_mkpath(TZ_USER_SHARE, "crash/dump")
#define CRASH_INFO_PATH		tzplatform_mkpath(TZ_SYS_SHARE, "crash/info")
#define CRASH_WORKER_PATH	"/usr/bin/crash-worker"
#define CRASH_POPUP_PATH	tzplatform_mkpath(TZ_SYS_RO_APP, \
		"org.tizen.crash-popup/bin/crash-popup")

static int noti_fd;
static int add_noti(void);
struct crash_arg {
	char crash_mode[CRASH_MODE_MAX];
	char crash_processname[CRASH_PROCESSNAME_MAX];
	char crash_timestr[CRASH_TIME_MAX];
	char crash_pid[CRASH_PID_MAX];
	char crash_exepath[CRASH_EXEPATH_MAX];
	char crash_verify[CRASH_VERIFY_MAX];
};

static int is_running_process(pid_t pid)
{
	char buf[PATH_MAX + 1];
	snprintf(buf, sizeof(buf), "/proc/%d", pid);
	if (!access(buf, R_OK))
		return 1;
	return 0;
}
static int make_noti_file(const char *path, const char *file)
{
	int fd;
	char buf[PATH_MAX];
	gid_t group_id;
	struct group *group_entry;
	mode_t old_mask;

	snprintf(buf, sizeof(buf), "%s/%s", path, file);	/* buf - full path to file */
	if (access(buf, F_OK) == 0)				/* if file exists then return -1 */
		return -1;

	/* save old mask and set new calling process's file mode creation mask */
	old_mask = umask(0);

	mkdir(path, 0775);		/* make directory, if exest then errno==EEXIST */
	group_entry = getgrnam("crash");		/* need to find out group ID of "crash" group name */
	if (group_entry == NULL) {
		umask(old_mask);	/* restore old file mask */
		return -1;
	}
	chown(path, 0, group_entry->gr_gid);			/* chown root:crash */
	if ((fd = open(buf, O_CREAT, 0666)) < 0) {	/* create crash file */
		umask(old_mask);			/* restore old file mask */
		return -1;
	}
	fchown(fd, 0, group_entry->gr_gid);	/* chown root:crash */
	close(fd);
	umask(old_mask);		/* restore old file mask */

	return 0;
}
static int make_coredump_dir(void)
{
	mode_t old_mask;
	gid_t group_id;
	struct group *group_entry;

	if (access(CRASH_COREDUMP_PATH, F_OK) == 0)				/* if file exists then return -1 */
		return -1;

	/* save old mask and set new calling process's file mode creation mask */
	old_mask = umask(0);

	mkdir(CRASH_COREDUMP_PATH, 0775);		/* make directory, if exest then errno==EEXIST */
	group_entry = getgrnam("crash");		/* need to find out group ID of "crash" group name*/
	if (group_entry == NULL) {
		umask(old_mask);	/* restore old file mask */
		return -1;
	}
	chown(CRASH_COREDUMP_PATH, 0, group_entry->gr_gid);			/* chown root:crash */
	umask(old_mask);		/* restore old file mask */

	return 0;
}
static int make_info_dir(void)
{
	mode_t old_mask;
	gid_t group_id;
	struct group *group_entry;

	if (access(CRASH_INFO_PATH, F_OK) == 0)				/* if file exists then return -1 */
		return -1;

	/* save old mask and set new calling process's file mode creation mask */
	old_mask = umask(0);

	mkdir(CRASH_INFO_PATH, 0775);		/* make directory, if exest then errno==EEXIST */
	group_entry = getgrnam("crash");		/* need to find out group ID of "crash" group name*/
	if (group_entry == NULL) {
		umask(old_mask);	/* restore old file mask */
		return -1;
	}
	chown(CRASH_INFO_PATH, 0, group_entry->gr_gid);			/* chown root:crash */
	umask(old_mask);		/* restore old file mask */

	return 0;
}
static int clean_coredump_dir(void)
{
	DIR *dir;
	struct dirent *dp;
	int dfd;
	dir = opendir(CRASH_COREDUMP_PATH);
	if (!dir) {
		_E("opendir failed");
		return 0;
	}
	dfd = dirfd(dir);
	if (dfd < 0)
		return 0;
	while ((dp = readdir(dir)) != NULL) {
		const char *name = dp->d_name;
		/* always skip "." and ".." */
		if (name[0] == '.') {
			if (name[1] == 0)
				continue;
			if ((name[1] == '.') && (name[2] == 0))
				continue;
		}
		if (unlinkat(dfd, name, 0) < 0) {
			_SE("FAIL: clean_coredump_dir (%s)", name);
			continue;
		}
	}
	closedir(dir);
	return 1;
}
static int clean_dump_dir(void)
{
	DIR *dir;
	struct dirent *dp;
	char dirname[PATH_MAX];

	dir = opendir(CRASH_DUMP_PATH);
	if (!dir) {
		_E("opendir failed");
		return 0;
	}
	while ((dp = readdir(dir)) != NULL) {
		if (dp->d_type == DT_DIR) {
			const char *name = dp->d_name;
			/* always skip "." and ".." */
			if (name[0] == '.') {
				if (name[1] == 0)
					continue;
				if ((name[1] == '.') && (name[2] == 0))
					continue;
			}
			snprintf(dirname, sizeof(dirname), "%s/%s", CRASH_DUMP_PATH, name);
			if (ecore_file_recursive_rm(dirname) == EINA_FALSE) {
				_SE("FAIL: clean_dump_dir (%s)", dirname);
				continue;
			}
		}
	}
	closedir(dir);
	return 1;
}
static int clean_info_dir(void)
{
	DIR *dir;
	struct dirent *dp;
	int dfd;
	dir = opendir(CRASH_INFO_PATH);
	if (!dir) {
		_E("opendir failed");
		return 0;
	}
	dfd = dirfd(dir);
	if (dfd < 0)
		return 0;
	while ((dp = readdir(dir)) != NULL) {
		const char *name = dp->d_name;
		/* always skip "." and ".." */
		if (name[0] == '.') {
			if (name[1] == 0)
				continue;
			if ((name[1] == '.') && (name[2] == 0))
				continue;
		}
		if (unlinkat(dfd, name, 0) < 0) {
			_E("FAIL: clean_info_dir (%s)", name);
			continue;
		}
	}
	closedir(dir);
	return 1;
}
static int crash_arg_parser(char *linebuffer, struct crash_arg *arg)
{
	char *ptr = NULL;
	int verify_num = 0;
	int verify_arg_num = 0;

	if (linebuffer == NULL || arg == NULL) {
		_E("crash_arg_parser input arguments is NULL");
		return -1;
	}
	ptr = strtok(linebuffer, CRASH_DELIMITER);
	if (ptr == NULL) {
		_SE("can't strtok linebuffer ptr(%s)", ptr);
		return -1;
	}
	snprintf(arg->crash_mode, CRASH_MODE_MAX, "%s",  ptr);
	ptr = strtok(NULL, CRASH_DELIMITER);
	if (ptr == NULL) {
		_SE("can't strtok linebuffer ptr(%s)", ptr);
		return -1;
	}
	snprintf(arg->crash_processname, CRASH_PROCESSNAME_MAX, "%s",  ptr);
	ptr = strtok(NULL, CRASH_DELIMITER);
	if (ptr == NULL) {
		_SE("can't strtok linebuffer ptr(%s)", ptr);
		return -1;
	}
	snprintf(arg->crash_timestr, CRASH_TIME_MAX, "%s", ptr);
	ptr = strtok(NULL, CRASH_DELIMITER);
	if (ptr == NULL) {
		_SE("can't strtok linebuffer ptr(%s)", ptr);
		return -1;
	}
	snprintf(arg->crash_pid, CRASH_PID_MAX, "%s", ptr);
	ptr = strtok(NULL, CRASH_DELIMITER);
	if (ptr == NULL) {
		_SE("can't strtok linebuffer ptr(%s)", ptr);
		return -1;
	}
	snprintf(arg->crash_exepath, CRASH_EXEPATH_MAX, "%s", ptr);
	ptr = strtok(NULL, CRASH_DELIMITER);
	if (ptr == NULL) {
		_SE("can't strtok linebuffer ptr(%s)", ptr);
		return -1;
	}
	snprintf(arg->crash_verify, CRASH_VERIFY_MAX, "%s", ptr);
	verify_num = strlen(arg->crash_processname) + strlen(arg->crash_exepath);
	verify_arg_num = atoi(arg->crash_verify);
	_SD("vnum %d vanum %d", verify_num, verify_arg_num);
	if (verify_num == verify_arg_num)
		return 1;
	else
		return 0;
}

static void launch_crash_worker(const char *filename, int popup_on)
{
	static int popup_pid;
	FILE *fp;
	int ret = -1;
	int len, count;
	char linebuffer[CRASH_ARG_MAX] = {0,};
	char crash_worker_args[CRASH_ARG_MAX] = {0,};
	struct crash_arg parsing_arg;
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	}
	count = 0;
	/* launch crash process */
	while (fgets(linebuffer, CRASH_ARG_MAX, fp) != NULL) {
		len = strlen(linebuffer);
		if (!len || linebuffer[len - 1] != '\n') {
			_E("crash inoti msg  must be terminated with new line character\n");
			break;
		}
		/* change last caracter from \n to \0 */
		linebuffer[strlen(linebuffer) - 1] = '\0';
		if (crash_arg_parser(linebuffer, &parsing_arg) != 1)
			continue;
		snprintf(crash_worker_args, sizeof(crash_worker_args), "%s %s %s %s %s",
				parsing_arg.crash_mode, parsing_arg.crash_processname,
				parsing_arg.crash_timestr, parsing_arg.crash_pid, parsing_arg.crash_exepath);
		_SD("crash_worker args(%s)", crash_worker_args);
		_SD("(%s%s%s)", parsing_arg.crash_mode,
				parsing_arg.crash_processname, parsing_arg.crash_timestr);
		ret = ss_launch_evenif_exist(CRASH_WORKER_PATH, crash_worker_args);
		if (ret > 0) {
			char buf[PATH_MAX];
			FILE *fpAdj;
			snprintf(buf, sizeof(buf), "/proc/%d/oom_adj", ret);
			fpAdj = fopen(buf, "w");
			if (fpAdj != NULL) {
				fprintf(fpAdj, "%d", (-17));
				fclose(fpAdj);
			}
			count++;
			if (CRASH_WORKER_MAX <= count)
				break;
		}
		if (popup_on) {
			if (!is_running_process(popup_pid))
				popup_pid = ss_launch_evenif_exist(CRASH_POPUP_PATH, parsing_arg.crash_processname);
		}

		if (popup_pid < 0) {
			_E("popup failed)\n");
			break;
		}
	}
	fclose(fp);
	if (ret != -1) {
		fp = fopen(filename, "w");
		if (fp == NULL) {
			return;
		}
		fclose(fp);
	}
	return;
}

static Ecore_File_Monitor *crash_file_monitor;

static Ecore_File_Monitor_Cb __crash_file_cb(void *data, Ecore_File_Monitor *em, Ecore_File_Event event, const char *path)
{
	switch (event) {
	case ECORE_FILE_EVENT_DELETED_DIRECTORY:
	case ECORE_FILE_EVENT_DELETED_SELF:
		if (make_noti_file(CRASH_NOTI_DIR, CRASH_NOTI_FILE) < 0) {
			launch_crash_worker(path, CRASH_POPUP_ON);
		}
		break;
	case ECORE_FILE_EVENT_MODIFIED:
	default:
		launch_crash_worker(path, CRASH_POPUP_ON);
		break;
	}
	return NULL;
}

static int _get_file_count(const char *path)
{
	DIR *dir;
	struct dirent *dp;
	int count = 0;

	dir = opendir(path);
	if (!dir)
		return 0;
	while ((dp = readdir(dir)) != NULL) {
		const char *name = dp->d_name;
		/* always skip "." and ".." */
		if (name[0] == '.') {
			if (name[1] == 0)
				continue;
			if ((name[1] == '.') && (name[2] == 0))
				continue;
		}
		count++;
	}
	closedir(dir);
	return count;
}

/* check disk available size */
static int _check_disk_available(void)
{
	struct statfs lstatfs;
	int avail_size = 0;

	if (statfs(CRASH_CHECK_DISK_PATH, &lstatfs) < 0)
		return -1;
	avail_size = (int)(lstatfs.f_bavail * (lstatfs.f_bsize/1024));
	if (CRASH_CHECK_SIZE > avail_size)
		return -1;
	return 1;
}

static void bs_init(void *data)
{
	if (make_noti_file(CRASH_NOTI_DIR, CRASH_NOTI_FILE) < 0) {
		_E("make_noti_file() failed");
		launch_crash_worker(CRASH_NOTI_PATH, CRASH_POPUP_OFF);
	}
	if (make_info_dir() < 0) {
		if (CRASH_LIMIT_NUM < _get_file_count(CRASH_INFO_PATH))
			clean_info_dir();
	}
	if (make_coredump_dir() < 0) {
		if (CRASH_LIMIT_NUM < _get_file_count(CRASH_COREDUMP_PATH)
					|| _check_disk_available() < 0)
			clean_coredump_dir();
	}
	if (CRASH_LIMIT_NUM < _get_file_count(CRASH_DUMP_PATH))
		clean_dump_dir();

	if (ecore_file_init() == 0) {
		_E("ecore_file_init() failed");
		launch_crash_worker(CRASH_NOTI_PATH, CRASH_POPUP_OFF);
	}
	crash_file_monitor = ecore_file_monitor_add(CRASH_NOTI_PATH, (void *) __crash_file_cb, NULL);
	if (!crash_file_monitor) {
		_E("ecore_file_monitor_add() failed");
		launch_crash_worker(CRASH_NOTI_PATH, CRASH_POPUP_OFF);
		return;
	}
}

static const struct device_ops bs_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "bs",
	.init     = bs_init,
};

DEVICE_OPS_REGISTER(&bs_device_ops)
