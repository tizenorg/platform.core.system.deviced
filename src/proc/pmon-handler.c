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


#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <device-node.h>

#include "core/log.h"
#include "core/launch.h"
#include "core/common.h"
#include "core/devices.h"

#define PMON_PERMANENT_DIR	"/tmp/permanent"

static Ecore_Fd_Handler *pmon_efd = NULL;

static int __pmon_start(void);
static int __pmon_stop(int fd);
static int replace_char(int size, char *t)
{
	while (size > 0) {
		if (*t == 0)
			*t = ' ';
		size--;
		t++;
	}
	return 0;
}

static char *pmon_get_permanent_pname(int pid)
{
	int fd;
	int ret;
	char buf[PATH_MAX];
	struct stat st;
	char *cmdline = NULL;

	snprintf(buf, sizeof(buf), "%s/%d", PMON_PERMANENT_DIR, pid);
	fd = open(buf, O_RDONLY);
	if (fd == -1) {
		_E("file open error");
		return NULL;
	}

	if (fstat(fd, &st) < 0) {
		_E("fstat error");
		close(fd);
		return NULL;
	}
	_D("size = %d", (int)st.st_size);

	cmdline = malloc(st.st_size + 1);
	if (cmdline == NULL) {
		_E("Not enough memory");
		close(fd);
		return NULL;
	}
	memset(cmdline, 0, st.st_size + 1);

	ret = read(fd, cmdline, st.st_size);
	if (ret >= 0 && ret < st.st_size) {
		/* TODO - must change more smarter */
		replace_char(st.st_size - 1, cmdline);
	}
	close(fd);

	return cmdline;
}

static void print_pmon_state(unsigned int dead_pid)
{
	_D("[Process MON] %d killed", dead_pid);
}

static int pmon_process(int pid)
{
	char *cmdline;
	int new_pid;
	char old_file[PATH_MAX];
	int fd;
	int r;

	if (is_vip(pid)) {
		_I("=======================================");
		_I("[Process MON] VIP process dead.");
		_I("=======================================");
	}
	/* If there is NOT a .hibernation_start file, run following codes 
	 * On hibernation processing, just ignore relaunching */
	else if (access("/tmp/.hibernation_start", R_OK) != 0) {
		cmdline = pmon_get_permanent_pname(pid);
		if (cmdline != NULL) {
			_I("[Process MON] %s relaunch", cmdline);
			new_pid = launch_evenif_exist(cmdline, "");
			free(cmdline);
			if (new_pid > 0) {
				/* TODO - set oom */
				char buf[PATH_MAX];
				char filepath[PATH_MAX];
				int cnt;

				if (access(PMON_PERMANENT_DIR, R_OK) < 0) {
					_I("no predefined matrix dir = %s, so created", PMON_PERMANENT_DIR);
					r = mkdir(PMON_PERMANENT_DIR, 0777);
					if(r < 0) {
						_E("Make Directory is failed");
						return -1;
					}
				}

				snprintf(filepath, sizeof(filepath), "%s/%d", PMON_PERMANENT_DIR, pid);
				fd = open(filepath, O_RDONLY);
				if (fd == -1) {
					_E("Failed to open");
					return -1;
				}
				cnt = read(fd, buf, PATH_MAX);
				close(fd);

				if (cnt <= 0) {
					_E("Failed to read");
					return -1;
				}

				snprintf(filepath, sizeof(filepath), "%s/%d", PMON_PERMANENT_DIR, new_pid);

				fd = open(filepath, O_CREAT | O_WRONLY, 0644);
				if (fd == -1) {
					_E("Failed to open");
					return -1;
				}
				if (write(fd, buf, cnt) == -1) {
					_E("Failed to write");
					close(fd);
					return -1;
				}
				close(fd);
				if ( device_set_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_MP_PNP, new_pid) < 0) {
					_E("Write new pid failed");
				}
				_I("[Process MON] %d ", new_pid);

				FILE *fp;

				_I("[Process MON] OOMADJ_SET : pid %d, new_oomadj %d",
				     new_pid, (-17));

				fp = open_proc_oom_score_adj_file(new_pid, "w");
				if (fp == NULL)
					return -1;
				fprintf(fp, "%d",0);
				fclose(fp);

				snprintf(old_file, sizeof(old_file), "%s/%d",
					 PMON_PERMANENT_DIR, pid);
				unlink(old_file);
			} else {
				_I("[Process MON] failed relaunching");
			}
		}
	}
	return 0;
}
/*
static unsigned int pmon_read(int fd)
{
	unsigned int pid;
	read(fd, &pid, sizeof(pid));
	return pid;
}
*/

static Eina_Bool pmon_cb(void *data, Ecore_Fd_Handler * fd_handler)
{
	int fd;
	int dead_pid;
	char pid_str[PATH_MAX];
	int ret;

	if (!ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ)) {
		_E("ecore_main_fd_handler_active_get error , return");
		goto out;
	}

	fd = ecore_main_fd_handler_fd_get(fd_handler);

	if (fd < 0) {
		_E("ecore_main_fd_handler_fd_get error , return");
		goto out;
	}

	ret = read(fd, pid_str, PATH_MAX);

	if (ret < 0 || ret >= PATH_MAX) {
		__pmon_stop(fd);
		_E("Reading DEAD_PID failed, restart ecore fd");
		__pmon_start();
		goto out;
	}

	pid_str[ret] = '\0';
	dead_pid = strtoul(pid_str, NULL, 10);
	print_pmon_state(dead_pid);
	pmon_process(dead_pid);
out:
	return EINA_TRUE;
}

static int __pmon_start(void)
{
	int pmon_fd = -1;
	char pmon_dev_node[PATH_MAX];
	if (device_get_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_NODE,
		(int *)pmon_dev_node) < 0) {
		_E("ss_pmon_init get dev node path failed");
		return -1;
	}

	pmon_fd = open(pmon_dev_node, O_RDONLY);
	if (pmon_fd < 0) {
		_E("ss_pmon_init fd open failed");
		return -1;
	}
	pmon_efd = ecore_main_fd_handler_add(pmon_fd, ECORE_FD_READ, pmon_cb, ad, NULL, NULL);
	if (!pmon_efd) {
		_E("error ecore_main_fd_handler_add");
		return -1;
	}
	return 0;
}
static int __pmon_stop(int fd)
{
	if (pmon_efd) {
		ecore_main_fd_handler_del(pmon_efd);
		pmon_efd = NULL;
	}
	if (fd >=0) {
		close(fd);
		fd = -1;
	}
	return 0;
}

static void pmon_init(void *data)
{
	int ret = -1;

	if (pmon_efd) {
		ecore_main_fd_handler_del(pmon_efd);
		pmon_efd = NULL;
	}
	if (__pmon_start() == -1) {
		_E("fail pmon control fd init");
	}
}

static const struct device_ops pmon_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "pmon",
	.init     = pmon_init,
};

DEVICE_OPS_REGISTER(&pmon_device_ops)
