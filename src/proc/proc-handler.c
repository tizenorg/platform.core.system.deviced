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
//#include <dirent.h>
//#include <sys/types.h>
//#include <device-node.h>
//#include <sys/un.h>
//#include <stdarg.h>
#include <errno.h>
#include <vconf.h>
//#include <fcntl.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "shared/score-defines.h"

#define OOMADJ_SET			"oomadj_set"
#define OOM_PMQOS_TIME		2000 /* ms */
#define SIGNAL_PROC_STATUS	"ProcStatus"

enum proc_status_type {
	PROC_STATUS_LAUNCH,
	PROC_STATUS_RESUME,
	PROC_STATUS_TERMINATE,
	PROC_STATUS_FOREGROUND,
	PROC_STATUS_BACKGROUND,
};

static void memcg_move_group(int pid, int oom_score_adj)
{
	char buf[100];
	FILE *f;
	int size;
	char exe_name[PATH_MAX];

	if (get_cmdline_name(pid, exe_name, PATH_MAX) != 0) {
		_E("fail to get process name(%d)", pid);
		return;
	}

	_SD("memcg_move_group : %s, pid = %d", exe_name, pid);
	if (oom_score_adj >= OOMADJ_BACKGRD_LOCKED)
		snprintf(buf, sizeof(buf), "/sys/fs/cgroup/memory/background/cgroup.procs");
	else if (oom_score_adj >= OOMADJ_FOREGRD_LOCKED && oom_score_adj < OOMADJ_BACKGRD_LOCKED)
		snprintf(buf, sizeof(buf), "/sys/fs/cgroup/memory/foreground/cgroup.procs");
	else
		return;

	f = fopen(buf, "w");
	if (f == NULL)
		return;
	size = snprintf(buf, sizeof(buf), "%d", pid);
	if (fwrite(buf, size, 1, f) != 1)
		_E("fwrite cgroup tasks : %d", pid);
	fclose(f);
}

int set_oom_score_adj_action(int argc, char **argv)
{
	FILE *fp;
	int pid = -1;
	int new_oom_score_adj = 0;

	if (argc < 2)
		return -1;
	pid = atoi(argv[0]);
	new_oom_score_adj = atoi(argv[1]);
	if (pid < 0 || new_oom_score_adj <= -20)
		return -1;

	_I("OOMADJ_SET : pid %d, new_oom_score_adj %d", pid, new_oom_score_adj);

	fp = open_proc_oom_score_adj_file(pid, "w");
	if (fp == NULL)
		return -1;
	if (new_oom_score_adj < OOMADJ_SU)
		new_oom_score_adj = OOMADJ_SU;
	fprintf(fp, "%d", new_oom_score_adj);
	fclose(fp);

	memcg_move_group(pid, new_oom_score_adj);
	return 0;
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
	else
		ret = -EINVAL;

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static void proc_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	int ret, type;
	pid_t pid;

	ret = dbus_message_is_signal(msg, RESOURCED_INTERFACE_PROCESS,
	    SIGNAL_PROC_STATUS);
	if (!ret) {
		_E("It's not active signal!");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &type,
	    DBUS_TYPE_INT32, &pid, DBUS_TYPE_INVALID) == 0) {
		_E("There's no arguments!");
		return;
	}

	if (type == PROC_STATUS_BACKGROUND)
		device_notify(DEVICE_NOTIFIER_PROCESS_BACKGROUND, &pid);
}

static const struct edbus_method edbus_methods[] = {
	{ OOMADJ_SET, "siss", "i", dbus_oom_handler },
};

static void proc_change_lowmemory(keynode_t *key, void *data)
{
	int state = 0;

	if (vconf_get_int(VCONFKEY_SYSMAN_LOW_MEMORY, &state))
		return;

	if (state == VCONFKEY_SYSMAN_LOW_MEMORY_HARD_WARNING)
		device_notify(DEVICE_NOTIFIER_PMQOS_OOM, (void *)OOM_PMQOS_TIME);
}

static void process_init(void *data)
{
	int ret;

	register_edbus_signal_handler(RESOURCED_PATH_PROCESS,
	    RESOURCED_INTERFACE_PROCESS, SIGNAL_PROC_STATUS, proc_signal_handler);

	ret = register_edbus_method(DEVICED_PATH_PROCESS, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	vconf_notify_key_changed(VCONFKEY_SYSMAN_LOW_MEMORY,
			proc_change_lowmemory, NULL);
}

static const struct device_ops process_device_ops = {
	.name     = "process",
	.init     = process_init,
};

DEVICE_OPS_REGISTER(&process_device_ops)
