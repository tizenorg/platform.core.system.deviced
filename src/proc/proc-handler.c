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
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <device-node.h>

#include "core/data.h"
#include "core/queue.h"
#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "proc-handler.h"

#define LIMITED_BACKGRD_NUM 15
#define MAX_BACKGRD_OOMADJ (OOMADJ_BACKGRD_UNLOCKED + LIMITED_BACKGRD_NUM)
#define PROCESS_VIP		"process_vip"
#define PROCESS_PERMANENT	"process_permanent"
#define OOMADJ_SET			"oomadj_set"

#define PREDEF_BACKGRD			"backgrd"
#define PREDEF_FOREGRD			"foregrd"
#define PREDEF_ACTIVE			"active"
#define PREDEF_INACTIVE			"inactive"
#define PROCESS_GROUP_SET		"process_group_set"

#define SIOP_LEVEL_MASK	0xFFFF
#define SIOP_LEVEL(val)			((val & SIOP_LEVEL_MASK) << 16)
static int siop = 0;

int get_app_oomadj(int pid, int *oomadj)
{
	if (pid < 0)
		return -1;

	char buf[PATH_MAX];
	FILE *fp = NULL;

	snprintf(buf, sizeof(buf), "/proc/%d/oom_adj", pid);
	fp = fopen(buf, "r");
	if (fp == NULL)
		return -1;
	if (fgets(buf, PATH_MAX, fp) == NULL) {
		fclose(fp);
		return -1;
	}

	(*oomadj) = atoi(buf);
	fclose(fp);
	return 0;
}

int set_app_oomadj(pid_t pid, int new_oomadj)
{
	char buf[PATH_MAX];
	FILE *fp;
	int old_oomadj;
	char exe_name[PATH_MAX];

	if (get_cmdline_name(pid, exe_name, PATH_MAX) < 0)
		snprintf(exe_name, sizeof(exe_name), "Unknown (maybe dead)");

	if (get_app_oomadj(pid, &old_oomadj) < 0)
		return -1;

	_I("Process %s, pid %d, old_oomadj %d", exe_name, pid, old_oomadj);

	if (old_oomadj < OOMADJ_APP_LIMIT)
		return 0;

	_I("Process %s, pid %d, new_oomadj %d", exe_name, pid, new_oomadj);
	snprintf(buf, sizeof(buf), "/proc/%d/oom_adj", pid);
	fp = fopen(buf, "w");
	if (fp == NULL)
		return -1;

	fprintf(fp, "%d", new_oomadj);
	fclose(fp);

	return 0;
}

int set_su_oomadj(pid_t pid)
{
	return set_app_oomadj(pid, OOMADJ_SU);
}

int check_oomadj(int oom_adj)
{
	if (oom_adj != OOMADJ_FOREGRD_LOCKED && oom_adj != OOMADJ_FOREGRD_UNLOCKED)
		return 0;
	return -1;
}

int set_oomadj_action(int argc, char **argv)
{
	int pid = -1;
	int new_oomadj = -20;

	if (argc < 2)
		return -1;
	if ((pid = atoi(argv[0])) < 0 || (new_oomadj = atoi(argv[1])) <= -20)
		return -1;

	char buf[255];
	FILE *fp;
	_I("OOMADJ_SET : pid %d, new_oomadj %d", pid, new_oomadj);

	snprintf(buf, sizeof(buf), "/proc/%d/oom_adj", pid);
	fp = fopen(buf, "w");
	if (fp == NULL)
		return -1;
	fprintf(fp, "%d", new_oomadj);
	fclose(fp);

	return 0;
}

static int update_backgrd_app_oomadj(pid_t pid, int new_oomadj)
{
	char buf[PATH_MAX];
	FILE* fp;

	snprintf(buf, sizeof(buf), "/proc/%d/oom_adj", pid);
	fp = fopen(buf, "w");
	if (fp == NULL)
		return -1;

	fprintf(fp, "%d", new_oomadj);
	fclose(fp);

	return 0;
}

int set_active_action(int argc, char **argv)
{
	int pid = -1;
	int ret = 0;
	int oomadj = 0;

	if (argc < 1)
		return -1;
	if ((pid = atoi(argv[0])) < 0)
		return -1;

	if (get_app_oomadj(pid, &oomadj) < 0)
		return -1;

	switch (oomadj) {
	case OOMADJ_FOREGRD_LOCKED:
	case OOMADJ_BACKGRD_LOCKED:
	case OOMADJ_SU:
		ret = 0;
		break;
	case OOMADJ_FOREGRD_UNLOCKED:
		ret = set_app_oomadj((pid_t) pid, OOMADJ_FOREGRD_LOCKED);
		break;
	case OOMADJ_BACKGRD_UNLOCKED:
		ret = set_app_oomadj((pid_t) pid, OOMADJ_BACKGRD_LOCKED);
		break;
	case OOMADJ_INIT:
		ret = set_app_oomadj((pid_t) pid, OOMADJ_BACKGRD_LOCKED);
		break;
	default:
		if(oomadj > OOMADJ_BACKGRD_UNLOCKED) {
			ret = set_app_oomadj((pid_t) pid, OOMADJ_BACKGRD_LOCKED);
		} else {
			_E("Unknown oomadj value (%d) !", oomadj);
			ret = -1;
		}
		break;
	}
	return ret;
}

int set_inactive_action(int argc, char **argv)
{
	int pid = -1;
	int ret = 0;
	int oomadj = 0;

	if (argc < 1)
		return -1;
	if ((pid = atoi(argv[0])) < 0)
		return -1;

	if (get_app_oomadj(pid, &oomadj) < 0)
		return -1;

	switch (oomadj) {
	case OOMADJ_FOREGRD_UNLOCKED:
	case OOMADJ_BACKGRD_UNLOCKED:
	case OOMADJ_SU:
		ret = 0;
		break;
	case OOMADJ_FOREGRD_LOCKED:
		ret = set_app_oomadj((pid_t) pid, OOMADJ_FOREGRD_UNLOCKED);
		break;
	case OOMADJ_BACKGRD_LOCKED:
		ret = set_app_oomadj((pid_t) pid, OOMADJ_BACKGRD_UNLOCKED);
		break;
	case OOMADJ_INIT:
		ret = set_app_oomadj((pid_t) pid, OOMADJ_BACKGRD_UNLOCKED);
		break;
	default:
		if(oomadj > OOMADJ_BACKGRD_UNLOCKED) {
			ret = 0;
		} else {
			_E("Unknown oomadj value (%d) !", oomadj);
			ret = -1;
		}
		break;

	}
	return ret;
}

int set_process_action(int argc, char **argv)
{
	int pid = -1;
	int ret = 0;
	int oomadj = 0;

	if (argc < 1)
		return -1;
	if ((pid = atoi(argv[0])) < 0)
		return -1;

	return ret;
}

int set_process_group_action(int argc, char **argv)
{
	int pid = -1;
	int ret = -1;

	if (argc != 2)
		return -1;
	if ((pid = atoi(argv[0])) < 0)
		return -1;

	if (strncmp(argv[1], PROCESS_VIP, strlen(PROCESS_VIP)) == 0)
		ret = device_set_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_MP_VIP, pid);
	else if (strncmp(argv[1], PROCESS_PERMANENT, strlen(PROCESS_PERMANENT)) == 0)
		ret = device_set_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_MP_PNP, pid);

	if (ret == 0)
		_I("%s : pid %d", argv[1], pid);
	else
		_E("fail to set %s : pid %d",argv[1], pid);
	return 0;
}

static void process_init(void *data)
{
	action_entry_add_internal(PREDEF_FOREGRD, set_process_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_BACKGRD, set_process_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_ACTIVE, set_active_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_INACTIVE, set_inactive_action, NULL,
				     NULL);
	action_entry_add_internal(OOMADJ_SET, set_oomadj_action, NULL, NULL);
	action_entry_add_internal(PROCESS_GROUP_SET, set_process_group_action, NULL, NULL);
}

const struct device_ops process_device_ops = {
	.init = process_init,
};
