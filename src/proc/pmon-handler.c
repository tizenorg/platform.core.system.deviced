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
#include "core/data.h"
#include "core/common.h"
#include "core/devices.h"

#define PMON_PERMANENT_DIR	"/tmp/permanent"

static Ecore_Fd_Handler *pmon_efd = NULL;

static int __pmon_start(struct ss_main_data *ad);
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
	char buf[PATH_MAX];
	struct stat st;
	char *cmdline = NULL;

	snprintf(buf, sizeof(buf), "%s/%d", PMON_PERMANENT_DIR, pid);
	fd = open(buf, O_RDONLY);
	if (fd == -1) {
		PRT_TRACE_ERR("file open error");
		return NULL;
	}

	if (fstat(fd, &st) < 0) {
		PRT_TRACE_ERR("fstat error");
		close(fd);
		return NULL;
	}
	PRT_TRACE("size = %d", (int)st.st_size);

	cmdline = malloc(st.st_size + 1);
	if (cmdline == NULL) {
		PRT_TRACE_ERR("Not enough memory");
		close(fd);
		return NULL;
	}
	memset(cmdline, 0, st.st_size + 1);

	read(fd, cmdline, st.st_size);
	/* TODO - must change more smarter */
	replace_char(st.st_size - 1, cmdline);
	close(fd);

	return cmdline;
}

static void print_pmon_state(unsigned int dead_pid)
{
	PRT_TRACE("[Process MON] %d killed", dead_pid);
}

static int pmon_process(int pid, void *ad)
{
	char *cmdline;
	int new_pid;
	char old_file[PATH_MAX];
	int fd;
	int r;

	if (is_vip(pid)) {
		PRT_TRACE_ERR("=======================================");
		PRT_TRACE_ERR("[Process MON] VIP process dead.");
		PRT_TRACE_ERR("=======================================");
	}
	/* If there is NOT a .hibernation_start file, run following codes 
	 * On hibernation processing, just ignore relaunching */
	else if (access("/tmp/.hibernation_start", R_OK) != 0) {
		cmdline = pmon_get_permanent_pname(pid);
		if (cmdline != NULL) {
			PRT_TRACE("[Process MON] %s relaunch", cmdline);
			new_pid = ss_launch_evenif_exist(cmdline, "");
			free(cmdline);
			if (new_pid > 0) {
				/* TODO - set oom */
				char buf[PATH_MAX];
				char filepath[PATH_MAX];
				int cnt;

				if (access(PMON_PERMANENT_DIR, R_OK) < 0) {
					PRT_TRACE("no predefined matrix dir = %s, so created", PMON_PERMANENT_DIR);
					r = mkdir(PMON_PERMANENT_DIR, 0777);
					if(r < 0) {
						PRT_TRACE("Make Directory is failed");
						return -1;
					}
				}

				snprintf(filepath, sizeof(filepath), "%s/%d", PMON_PERMANENT_DIR, pid);
				fd = open(filepath, O_RDONLY);
				if (fd == -1) {
					PRT_TRACE("Failed to open");
					return -1;
				}
				cnt = read(fd, buf, PATH_MAX);
				close(fd);

				if (cnt <= 0) {
					PRT_TRACE("Failed to read");
					return -1;
				}

				snprintf(filepath, sizeof(filepath), "%s/%d", PMON_PERMANENT_DIR, new_pid);

				fd = open(filepath, O_CREAT | O_WRONLY, 0644);
				if (fd == -1) {
					PRT_TRACE("Failed to open");
					return -1;
				}
				if (write(fd, buf, cnt) == -1) {
					PRT_TRACE("Failed to write");
					close(fd);
					return -1;
				}
				close(fd);
				if ( device_set_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_MP_PNP, new_pid) < 0) {
					PRT_TRACE_ERR("Write new pid failed");
				}
				PRT_TRACE("[Process MON] %d ", new_pid);

				FILE *fp;

				PRT_TRACE
				    ("[Process MON] OOMADJ_SET : pid %d, new_oomadj %d",
				     new_pid, (-17));
				
				fp = open_proc_oom_adj_file(new_pid, "w");
				if (fp == NULL)
					return -1;
				fprintf(fp, "%d", (-17));
				fclose(fp);

				snprintf(old_file, sizeof(old_file), "%s/%d",
					 PMON_PERMANENT_DIR, pid);
				unlink(old_file);
			} else { 
				PRT_TRACE_ERR("[Process MON] failed relaunching");
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

static int pmon_cb(void *data, Ecore_Fd_Handler * fd_handler)
{
	int fd;
	struct ss_main_data *ad = (struct ss_main_data *)data;
	int dead_pid;
	if (!ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ)) {
		PRT_TRACE_ERR
		    ("ecore_main_fd_handler_active_get error , return\n");
		return -1;
	}

	fd = ecore_main_fd_handler_fd_get(fd_handler);

	if (fd < 0) {
		PRT_TRACE_ERR("ecore_main_fd_handler_fd_get error , return");
		return -1;
	}
	if (read(fd, &dead_pid, sizeof(dead_pid)) < 0) {
		__pmon_stop(fd);
		PRT_TRACE_ERR("Reading DEAD_PID failed, restart ecore fd");
		__pmon_start(ad);
		return -1;
	}
		
	print_pmon_state(dead_pid);
	pmon_process(dead_pid, ad);

	return 1;
}

static int __pmon_start(struct ss_main_data *ad)
{
	int pmon_fd = -1;
	char pmon_dev_node[PATH_MAX];
	if (device_get_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_NODE, pmon_dev_node) < 0) {
		PRT_TRACE_ERR("ss_pmon_init get dev node path failed");
		return -1;
	}

	pmon_fd = open(pmon_dev_node, O_RDONLY);
	if (pmon_fd < 0) {
		PRT_TRACE_ERR("ss_pmon_init fd open failed");
		return -1;
	}
	pmon_efd = ecore_main_fd_handler_add(pmon_fd, ECORE_FD_READ, pmon_cb, ad, NULL, NULL);
	if (!pmon_efd) {
		PRT_TRACE_ERR("error ecore_main_fd_handler_add");
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
	struct ss_main_data *ad = (struct ss_main_data*)data;
	int ret = -1;

	if (pmon_efd) {
		ecore_main_fd_handler_del(pmon_efd);
		pmon_efd = NULL;
	}
	if (__pmon_start(ad) == -1) {
		PRT_TRACE_ERR("fail pmon control fd init");
		return;
	}
}

const struct device_ops pmon_device_ops = {
	.init = pmon_init,
};
