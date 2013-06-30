/*
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


#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <vconf.h>

#include <syspopup_caller.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <mntent.h>
#include <sys/mount.h>
#include "dd-deviced.h"
#include "log.h"
#include "launch.h"
#include "queue.h"
#include "device-handler.h"
#include "device-node.h"
#include "predefine.h"
#include "proc/proc-handler.h"
#include "data.h"
#include "common.h"
#include "poll.h"
#include "setting.h"
#include "devices.h"

#define PREDEFINE_SO_DIR		PREFIX"/lib/ss_predefine/"

#define LOWBAT_EXEC_PATH		PREFIX"/bin/lowbatt-popup"

#define HDMI_NOTI_EXEC_PATH		PREFIX"/bin/hdmi_connection_noti"
#define LOWBAT_POPUP_NAME		"lowbat-syspopup"
#define POWEROFF_POPUP_NAME		"poweroff-syspopup"
#define HDMI_POPUP_NAME			"hdmi-syspopup"
#define LOWMEM_POPUP_NAME		"lowmem-syspopup"

static int __predefine_get_pid(const char *execpath)
{
	DIR *dp;
	struct dirent *dentry;
	int pid = -1, fd;
	char buf[PATH_MAX];
	char buf2[PATH_MAX];

	dp = opendir("/proc");
	if (!dp) {
		PRT_TRACE_ERR("open /proc");
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

int predefine_control_launch(char *name, bundle *b, int option)
{
	int pid;
	static int launched_poweroff = 0;
	//lowbat-popup
	if (strncmp(name, LOWBAT_POPUP_NAME, strlen(LOWBAT_POPUP_NAME)) == 0) {
		if (launched_poweroff == 1) {
			PRT_TRACE_ERR("will be foreced power off");
			internal_poweroff_def_predefine_action(0,NULL);
			return 0;
		}

		if (option == 2)
			launched_poweroff = 1;

		pid = __predefine_get_pid(LOWBAT_EXEC_PATH);
		if (pid > 0) {
			PRT_TRACE_ERR("pre launched %s destroy", LOWBAT_EXEC_PATH);
			kill(pid, SIGTERM);
		}
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	//poweroff-popup
	if (strncmp(name, POWEROFF_POPUP_NAME, strlen(POWEROFF_POPUP_NAME)) == 0) {
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	//hdmi-popup
	if (strncmp(name, HDMI_POPUP_NAME, strlen(HDMI_POPUP_NAME)) == 0) {
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	//hdmi-noti
	if (strncmp(name, HDMI_NOTI_EXEC_PATH, strlen(HDMI_NOTI_EXEC_PATH)) == 0) {
		if (ss_launch_if_noexist(name, "1") < 0)
			return -1;
	}
	//user mem lowmem-popup
	if (strncmp(name, LOWMEM_POPUP_NAME, strlen(LOWMEM_POPUP_NAME)) == 0) {
		if (syspopup_launch(name, b) < 0)
			return -1;
	}
	return 0;
}

void predefine_pm_change_state(unsigned int s_bits)
{
	pm_change_internal(getpid(), s_bits);
}

static void ss_action_entry_load_from_sodir()
{
	DIR *dp;
	struct dirent *dentry;
	struct sysnoti *msg;
	char *ext;
	char tmp[128];

	dp = opendir(PREDEFINE_SO_DIR);
	if (!dp) {
		ERR("fail open %s", PREDEFINE_SO_DIR);
		return;
	}

	msg = malloc(sizeof(struct sysnoti));
	if (msg == NULL) {
		ERR("Malloc failed");
		closedir(dp);
		return;
	}

	msg->pid = getpid();

	while ((dentry = readdir(dp)) != NULL) {
		if ((ext = strstr(dentry->d_name, ".so")) == NULL)
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", PREDEFINE_SO_DIR,
			 dentry->d_name);
		msg->path = tmp;
		*ext = 0;
		msg->type = &(dentry->d_name[3]);
		ss_action_entry_add(msg);
	}
	free(msg);

	closedir(dp);
}

static void predefine_init(void *data)
{
	ss_action_entry_load_from_sodir();
}

const struct device_ops predefine_device_ops = {
	.init = predefine_init,
};
