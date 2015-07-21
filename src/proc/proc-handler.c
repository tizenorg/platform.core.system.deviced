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
#include <sys/un.h>
#include <stdarg.h>
#include <errno.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <fcntl.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "proc-handler.h"
#include "core/edbus-handler.h"

#define TEMPERATURE_DBUS_INTERFACE	"org.tizen.trm.siop"
#define TEMPERATURE_DBUS_PATH		"/Org/Tizen/Trm/Siop"
#define TEMPERATURE_DBUS_SIGNAL		"ChangedTemperature"

#define LIMITED_PROCESS_OOMADJ 15

#define PROCESS_VIP		"process_vip"
#define PROCESS_PERMANENT	"process_permanent"
#define OOMADJ_SET			"oomadj_set"

#define PREDEF_BACKGRD			"backgrd"
#define PREDEF_FOREGRD			"foregrd"
#define PREDEF_ACTIVE			"active"
#define PREDEF_INACTIVE			"inactive"
#define PROCESS_GROUP_SET		"process_group_set"

#define VCONFKEY_INTERNAL_PRIVATE_SIOP_DISABLE	"memory/private/sysman/siop_disable"

#define BUFF_MAX	255
#define SIOP_CTRL_LEVEL_MASK	0xFFFF
#define SIOP_CTRL_LEVEL(val)	((val & SIOP_CTRL_LEVEL_MASK) << 16)
#define SIOP_CTRL_VALUE(s, r)	(s | (r << 4))
#define SIOP_VALUE(d, val)	((d) * (val & 0xF))
#define REAR_VALUE(val)	(val >> 4)

#define SIGNAL_SIOP_CHANGED	"ChangedSiop"
#define SIGNAL_REAR_CHANGED	"ChangedRear"
#define SIOP_LEVEL_GET		"GetSiopLevel"
#define REAR_LEVEL_GET		"GetRearLevel"
#define SIOP_LEVEL_SET		"SetSiopLevel"

#define SIGNAL_NAME_OOMADJ_SET	"OomadjSet"

enum SIOP_DOMAIN_TYPE {
	SIOP_NEGATIVE = -1,
	SIOP_POSITIVE = 1,
};

struct siop_data {
	int siop;
	int rear;
};

static int siop;
static int mode;
static int siop_domain = SIOP_POSITIVE;

enum siop_scenario {
	MODE_NONE = 0,
	MODE_LCD = 1,
};
#ifdef NOUSE
struct Node {
	pid_t pid;
	struct Node *next;
};

static struct Node *head;

static struct Node *find_node(pid_t pid)
{
	struct Node *t = head;

	while (t != NULL) {
		if (t->pid == pid)
			break;
		t = t->next;
	}
	return t;
}

static struct Node *add_node(pid_t pid)
{
	struct Node *n;

	n = (struct Node *) malloc(sizeof(struct Node));
	if (n == NULL) {
		_E("Not enough memory, add cond. fail");
		return NULL;
	}

	n->pid = pid;
	n->next = head;
	head = n;

	return n;
}

static int del_node(struct Node *n)
{
	struct Node *t;
	struct Node *prev;

	if (n == NULL)
		return 0;

	t = head;
	prev = NULL;
	while (t != NULL) {
		if (t == n) {
			if (prev != NULL)
				prev->next = t->next;
			else
				head = head->next;
			free(t);
			break;
		}
		prev = t;
		t = t->next;
	}
	return 0;
}
#endif

int cur_siop_level(void)
{
	return  SIOP_VALUE(siop_domain, siop);
}

static void siop_level_action(int level)
{
	int val = SIOP_CTRL_LEVEL(level);
	static int old;
	static int siop_level;
	static int rear_level;
	static int initialized;
	static int domain;
	char *arr[1];
	char str_level[32];

	if (initialized && siop == level && mode == old && domain == siop_domain)
		return;
	initialized = 1;
	siop = level;
	domain = siop_domain;
	if (siop_domain == SIOP_NEGATIVE)
		val = 0;
	old = mode;
	val |= old;

	val = SIOP_VALUE(siop_domain, level);
	if (siop_level != val) {
		siop_level = val;
		snprintf(str_level, sizeof(str_level), "%d", siop_level);
		arr[0] = str_level;
		_I("broadcast siop %s", str_level);
		broadcast_edbus_signal(DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			SIGNAL_SIOP_CHANGED, "i", arr);
	}

	val = REAR_VALUE(level);
	if (rear_level != val) {
		rear_level = val;
		snprintf(str_level, sizeof(str_level), "%d", rear_level);
		arr[0] = str_level;
		_I("broadcast rear %s", str_level);
		broadcast_edbus_signal(DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			SIGNAL_REAR_CHANGED, "i", arr);
	}
	_I("level is d:%d(0x%x) s:%d r:%d", siop_domain, siop, siop_level, rear_level);
}

static int siop_changed(int argc, char **argv)
{
	int siop_level = 0;
	int rear_level = 0;
	int level;

	if (argc != 2 || argv[0] == NULL) {
		_E("fail to check value");
		return -1;
	}

	if (argv[0] == NULL)
		goto out;

	level = atoi(argv[0]);
	if (level <= SIOP_NEGATIVE)
		siop_domain = SIOP_NEGATIVE;
	else
		siop_domain = SIOP_POSITIVE;
	siop_level = siop_domain * level;

	if (argv[1] == NULL)
		goto out;

	level = atoi(argv[1]);
	rear_level = level;

out:
	level = SIOP_CTRL_VALUE(siop_level, rear_level);
	siop_level_action(level);
	return 0;
}

static int siop_mode_lcd(keynode_t *key_nodes, void *data)
{
	int pm_state;
	if (vconf_get_int(VCONFKEY_PM_STATE, &pm_state) != 0)
		_E("Fail to get vconf value for pm state\n");
	if (pm_state == VCONFKEY_PM_STATE_LCDOFF)
		mode = MODE_LCD;
	else
		mode = MODE_NONE;
	siop_level_action(siop);
	return 0;
}

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
		sprintf(buf, "/sys/fs/cgroup/memory/background/cgroup.procs");
	else if (oom_score_adj >= OOMADJ_FOREGRD_LOCKED && oom_score_adj < OOMADJ_BACKGRD_LOCKED)
		sprintf(buf, "/sys/fs/cgroup/memory/foreground/cgroup.procs");
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

int get_oom_score_adj(int pid, int *oom_score_adj)
{
	char buf[PATH_MAX];
	FILE *fp;

	if (pid < 0)
		return -1;

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
	FILE *fp;
	int old_oom_score_adj;
	char exe_name[PATH_MAX];

	if (get_cmdline_name(pid, exe_name, PATH_MAX) < 0)
		snprintf(exe_name, sizeof(exe_name), "Unknown (maybe dead)");

	if (get_oom_score_adj(pid, &old_oom_score_adj) < 0)
		return -1;

	_SI("Process %s, pid %d, old_oom_score_adj %d new_oom_score_adj %d",
		exe_name, pid, old_oom_score_adj, new_oom_score_adj);

	if (new_oom_score_adj < OOMADJ_SU)
		new_oom_score_adj = OOMADJ_SU;

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

int set_active_action(int argc, char **argv)
{
	int pid = -1;
	int ret = 0;
	int oom_score_adj = 0;

	if (argc < 1)
		return -1;
	pid = atoi(argv[0]);
	if (pid < 0)
		return -1;
	ret = get_oom_score_adj(pid, &oom_score_adj);
	if (ret < 0)
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
		if (oom_score_adj > OOMADJ_BACKGRD_UNLOCKED) {
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
	pid = atoi(argv[0]);
	if (pid < 0)
		return -1;

	ret = get_oom_score_adj(pid, &oom_score_adj);
	if (ret < 0)
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
		if (oom_score_adj > OOMADJ_BACKGRD_UNLOCKED) {
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
	pid = atoi(argv[0]);
	if (pid < 0)
		return -1;

	return ret;
}

int set_process_group_action(int argc, char **argv)
{
	int pid = -1;
	int ret = -1;

	if (argc != 2)
		return -1;
	pid = atoi(argv[0]);
	if (pid < 0)
		return -1;

	if (strncmp(argv[1], PROCESS_VIP, strlen(PROCESS_VIP)) == 0)
		ret = device_set_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_MP_VIP, pid);
	else if (strncmp(argv[1], PROCESS_PERMANENT, strlen(PROCESS_PERMANENT)) == 0)
		ret = device_set_property(DEVICE_TYPE_PROCESS, PROP_PROCESS_MP_PNP, pid);

	if (ret == 0)
		_I("%s : pid %d", argv[1], pid);
	else
		_E("fail to set %s : pid %d", argv[1], pid);
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

static DBusMessage *dbus_get_siop_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int level;

	level = SIOP_VALUE(siop_domain, siop);
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &level);
	return reply;
}

static DBusMessage *dbus_get_rear_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int level;

	level = REAR_VALUE(siop);
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &level);
	return reply;
}

static DBusMessage *dbus_set_siop_level(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}
	ret = siop_changed(2, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static void dbus_proc_oomadj_set_signal_handler(void *data, DBusMessage *msg)

{
	DBusError err;
	pid_t pid;
	int ret = -EINVAL;
	int argc;
	char *type_str;
	char *argv;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		return;
	}

	if (argc < 0) {
		_E("message is invalid!");
		return;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		return;
	}

	if (strncmp(type_str, PREDEF_FOREGRD, strlen(PREDEF_FOREGRD)) == 0)
		ret = set_process_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_BACKGRD, strlen(PREDEF_BACKGRD)) == 0)
		ret = set_process_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_ACTIVE, strlen(PREDEF_ACTIVE)) == 0)
		ret = set_active_action(argc, (char **)&argv);
	else if (strncmp(type_str, PREDEF_INACTIVE, strlen(PREDEF_INACTIVE)) == 0)
		ret = set_inactive_action(argc, (char **)&argv);

	if (ret < 0)
		_E("set_process_action error!");
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_FOREGRD, "sis", "i", dbus_proc_handler },
	{ PREDEF_BACKGRD, "sis", "i", dbus_proc_handler },
	{ PREDEF_ACTIVE, "sis", "i", dbus_proc_handler },
	{ PREDEF_INACTIVE, "sis", "i", dbus_proc_handler },
	{ OOMADJ_SET, "siss", "i", dbus_oom_handler },
	{ PROCESS_GROUP_SET, "siss", "i", dbus_oom_handler },
	{ SIOP_LEVEL_GET, NULL, "i", dbus_get_siop_level },
	{ REAR_LEVEL_GET, NULL, "i", dbus_get_rear_level },
	{ SIOP_LEVEL_SET, "ss", "i", dbus_set_siop_level },
};

static int proc_booting_done(void *data)
{
	static int done;

	if (data == NULL)
		goto out;
	done = *(int *)data;
	if (vconf_notify_key_changed(VCONFKEY_PM_STATE, (void *)siop_mode_lcd, NULL) < 0)
		_E("Vconf notify key chaneged failed: KEY(%s)", VCONFKEY_PM_STATE);
	siop_mode_lcd(NULL, NULL);
out:
	return done;
}

static int process_execute(void *data)
{
	struct siop_data *key_data = (struct siop_data *)data;
	int siop_level = 0;
	int rear_level = 0;
	int level;
	int booting_done;

	booting_done = proc_booting_done(NULL);
	if (!booting_done)
		return 0;

	if (key_data == NULL)
		goto out;

	level = key_data->siop;
	if (level <= SIOP_NEGATIVE)
		siop_domain = SIOP_NEGATIVE;
	else
		siop_domain = SIOP_POSITIVE;
	siop_level = siop_domain * level;

	level = key_data->rear;
	rear_level = level;

out:
	level = SIOP_CTRL_VALUE(siop_level, rear_level);
	siop_level_action(level);
	return 0;
}

static void process_init(void *data)
{
	int ret;

	register_edbus_signal_handler(DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS,
			SIGNAL_NAME_OOMADJ_SET,
		    dbus_proc_oomadj_set_signal_handler);

	ret = register_edbus_method(DEVICED_PATH_PROCESS, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, proc_booting_done);
}

static const struct device_ops process_device_ops = {
	.name     = PROC_OPS_NAME,
	.init     = process_init,
	.execute  = process_execute,
};

DEVICE_OPS_REGISTER(&process_device_ops)
