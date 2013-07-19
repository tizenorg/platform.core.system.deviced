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
#include "core/edbus-handler.h"

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

int get_oom_score_adj(int pid, int *oom_score_adj)
{
	if (pid < 0)
		return -1;

	char buf[PATH_MAX];
	FILE *fp = NULL;

	fp = open_proc_oom_score_adj_file(pid, "r");
	if (fp == NULL)
		return -1;
	if (fgets(buf, PATH_MAX, fp) == NULL) {
		fclose(fp);
		return -1;
	}

	*oom_score_adj = atoi(buf);
	fclose(fp);
	return 0;
}

int set_oom_score_adj(pid_t pid, int new_oom_score_adj)
{
	char buf[PATH_MAX];
	FILE *fp;
	int old_oom_score_adj;
	char exe_name[PATH_MAX];

	if (get_cmdline_name(pid, exe_name, PATH_MAX) < 0)
		snprintf(exe_name, sizeof(exe_name), "Unknown (maybe dead)");

	if (get_oom_score_adj(pid, &old_oom_score_adj) < 0)
		return -1;

	_SI("Process %s, pid %d, old_oom_score_adj %d new_oom_score_adj %d",
		exe_name, pid, old_oom_score_adj, new_oom_score_adj);

	if (old_oom_score_adj < OOMADJ_APP_LIMIT)
		return 0;

	fp = open_proc_oom_score_adj_file(pid, "w");
	if (fp == NULL)
		return -1;

	fprintf(fp, "%d", new_oom_score_adj);
	fclose(fp);

	return 0;
}

int set_su_oom_score_adj(pid_t pid)
{
	return set_oom_score_adj(pid, OOMADJ_SU);
}

int check_oom_score_adj(int oom_score_adj)
{
	if (oom_score_adj != OOMADJ_FOREGRD_LOCKED && oom_score_adj != OOMADJ_FOREGRD_UNLOCKED)
		return 0;
	return -1;
}

int set_oom_score_adj_action(int argc, char **argv)
{
	FILE *fp;
	int pid = -1;
	int new_oom_score_adj = 0;

	if (argc < 2)
		return -1;
	if ((pid = atoi(argv[0])) < 0 || (new_oom_score_adj = atoi(argv[1])) <= -20)
		return -1;

	_I("OOMADJ_SET : pid %d, new_oom_score_adj %d", pid, new_oom_score_adj);

	fp = open_proc_oom_score_adj_file(pid, "w");
	if (fp == NULL)
		return -1;
	if (new_oom_score_adj < OOMADJ_SU)
		new_oom_score_adj = OOMADJ_SU;
	fprintf(fp, "%d", new_oom_score_adj);
	fclose(fp);

	return 0;
}

int set_active_action(int argc, char **argv)
{
	int pid = -1;
	int ret = 0;
	int oom_score_adj = 0;

	if (argc < 1)
		return -1;
	if ((pid = atoi(argv[0])) < 0)
		return -1;

	if (get_oom_score_adj(pid, &oom_score_adj) < 0)
		return -1;

	switch (oom_score_adj) {
	case OOMADJ_FOREGRD_LOCKED:
	case OOMADJ_BACKGRD_LOCKED:
	case OOMADJ_SU:
		ret = 0;
		break;
	case OOMADJ_FOREGRD_UNLOCKED:
		ret = set_oom_score_adj((pid_t) pid, OOMADJ_FOREGRD_LOCKED);
		break;
	case OOMADJ_BACKGRD_UNLOCKED:
		ret = set_oom_score_adj((pid_t) pid, OOMADJ_BACKGRD_LOCKED);
		break;
	case OOMADJ_INIT:
		ret = set_oom_score_adj((pid_t) pid, OOMADJ_BACKGRD_LOCKED);
		break;
	default:
		if(oom_score_adj > OOMADJ_BACKGRD_UNLOCKED) {
			ret = set_oom_score_adj((pid_t) pid, OOMADJ_BACKGRD_LOCKED);
		} else {
			_E("Unknown oom_score_adj value (%d) !", oom_score_adj);
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
	int oom_score_adj = 0;

	if (argc < 1)
		return -1;
	if ((pid = atoi(argv[0])) < 0)
		return -1;

	if (get_oom_score_adj(pid, &oom_score_adj) < 0)
		return -1;

	switch (oom_score_adj) {
	case OOMADJ_FOREGRD_UNLOCKED:
	case OOMADJ_BACKGRD_UNLOCKED:
	case OOMADJ_SU:
		ret = 0;
		break;
	case OOMADJ_FOREGRD_LOCKED:
		ret = set_oom_score_adj((pid_t) pid, OOMADJ_FOREGRD_UNLOCKED);
		break;
	case OOMADJ_BACKGRD_LOCKED:
		ret = set_oom_score_adj((pid_t) pid, OOMADJ_BACKGRD_UNLOCKED);
		break;
	case OOMADJ_INIT:
		ret = set_oom_score_adj((pid_t) pid, OOMADJ_BACKGRD_UNLOCKED);
		break;
	default:
		if(oom_score_adj > OOMADJ_BACKGRD_UNLOCKED) {
			ret = 0;
		} else {
			_E("Unknown oom_score_adj value (%d) !", oom_score_adj);
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

static DBusMessage *dbus_proc_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv;
	
	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		 _E("message is invalid!");
		 ret = -EINVAL;
		 goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (strncmp(type_str, PREDEF_FOREGRD, strlen(PREDEF_FOREGRD)) == 0)
		ret = set_process_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_BACKGRD, strlen(PREDEF_BACKGRD)) == 0)
		ret = set_process_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_ACTIVE, strlen(PREDEF_ACTIVE)) == 0)
		ret = set_active_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_INACTIVE, strlen(PREDEF_INACTIVE)) == 0)
		ret = set_inactive_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_oom_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (strncmp(type_str, OOMADJ_SET, strlen(OOMADJ_SET)) == 0)
		ret = set_oom_score_adj_action(argc, (char **)&argv);
	else if (strncmp(type_str, PROCESS_GROUP_SET, strlen(PROCESS_GROUP_SET)) == 0)
		ret = set_process_group_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_FOREGRD, "sis", "i", dbus_proc_handler },
	{ PREDEF_BACKGRD, "sis", "i", dbus_proc_handler },
	{ PREDEF_ACTIVE, "sis", "i", dbus_proc_handler },
	{ PREDEF_INACTIVE, "sis", "i", dbus_proc_handler },
	{ OOMADJ_SET, "siss", "i", dbus_oom_handler },
	{ PROCESS_GROUP_SET, "siss", "i", dbus_oom_handler },
};

static void process_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	action_entry_add_internal(PREDEF_FOREGRD, set_process_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_BACKGRD, set_process_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_ACTIVE, set_active_action, NULL,
				     NULL);
	action_entry_add_internal(PREDEF_INACTIVE, set_inactive_action, NULL,
				     NULL);
	action_entry_add_internal(OOMADJ_SET, set_oom_score_adj_action, NULL, NULL);
	action_entry_add_internal(PROCESS_GROUP_SET, set_process_group_action, NULL, NULL);
}

static const struct device_ops process_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "process",
	.init = process_init,
};

DEVICE_OPS_REGISTER(&process_device_ops)
