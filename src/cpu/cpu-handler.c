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
#include <device-node.h>
#include <vconf.h>

#include "core/list.h"
#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "proc/proc-handler.h"

#define SET_MAX_FREQ	"set_max_frequency"
#define SET_MIN_FREQ	"set_min_frequency"
#define SET_FREQ_LEN	17

#define RELEASE_MAX_FREQ	"release_max_frequency"
#define RELEASE_MIN_FREQ	"release_min_frequency"
#define RELEASE_FREQ_LEN	21

#define POWER_SAVING_CPU_FREQ_RATE	(0.7)

#define DEFAULT_MAX_CPU_FREQ		1200000
#define DEFAULT_MIN_CPU_FREQ		100000

enum emergency_type {
	EMERGENCY_UNLOCK = 0,
	EMERGENCY_LOCK = 1,
};

static int max_cpu_freq_limit = -1;
static int min_cpu_freq_limit = -1;
static int cur_max_cpu_freq = INT_MAX;
static int cur_min_cpu_freq = INT_MIN;
static int power_saving_freq = -1;

static dd_list *max_cpu_freq_list;
static dd_list *min_cpu_freq_list;

static int cpu_number_limit = -1;
static int cur_cpu_number = INT_MAX;
static dd_list *cpu_number_list;

struct cpu_freq_entry {
	int pid;
	int freq;
};

struct cpu_number_entry {
	int pid;
	int number;
};

static int is_entry_enable(int pid)
{
	char pid_path[PATH_MAX];

	snprintf(pid_path, PATH_MAX, "/proc/%d", pid);
	if (access(pid_path, F_OK) < 0) {
		return 0;
	}

	return 1;
}

static int write_min_cpu_freq(int freq)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_CPU, PROP_CPU_SCALING_MIN_FREQ, freq);
	if (ret < 0) {
		_E("set cpufreq min freq write error: %s", strerror(errno));
		return ret;
	}

	return 0;
}

static int write_max_cpu_freq(int freq)
{
	int ret;

	ret = device_set_property(DEVICE_TYPE_CPU, PROP_CPU_SCALING_MAX_FREQ, freq);
	if (ret < 0) {
		_E("set cpufreq max freq write error: %s", strerror(errno));
		return ret;
	}

	return 0;
}

static int remove_entry_from_min_cpu_freq_list(int pid)
{
	dd_list *tmp;
	struct cpu_freq_entry *entry;

	cur_min_cpu_freq = INT_MIN;

	DD_LIST_FOREACH(min_cpu_freq_list, tmp, entry) {
		if ((!is_entry_enable(entry->pid)) || (entry->pid == pid)) {
			DD_LIST_REMOVE(min_cpu_freq_list, entry);
			free(entry);
			continue;
		}
		if (entry->freq > cur_min_cpu_freq) {
			cur_min_cpu_freq = entry->freq;
		}
	}

	return 0;
}

static int remove_entry_from_max_cpu_freq_list(int pid)
{
	dd_list *tmp;
	struct cpu_freq_entry *entry;

	cur_max_cpu_freq = INT_MAX;

	DD_LIST_FOREACH(max_cpu_freq_list, tmp, entry) {
		if ((!is_entry_enable(entry->pid)) || (entry->pid == pid)) {
			DD_LIST_REMOVE(max_cpu_freq_list, entry);
			free(entry);
			continue;
		}
		if (entry->freq < cur_max_cpu_freq) {
			cur_max_cpu_freq = entry->freq;
		}
	}

	return 0;
}

int release_max_frequency_action(int argc, char **argv)
{
	int r;

	if (argc < 1)
		return -EINVAL;

	r = remove_entry_from_max_cpu_freq_list(atoi(argv[0]));
	if (r < 0) {
		_E("Remove entry failed");
		return r;
	}

	if (cur_max_cpu_freq == INT_MAX)
		cur_max_cpu_freq = max_cpu_freq_limit;

	r = write_max_cpu_freq(cur_max_cpu_freq);
	if (r < 0) {
		_E("Write freq failed");
		return r;
	}

	return 0;
}

int release_min_frequency_action(int argc, char **argv)
{
	int r;

	if (argc < 1)
		return -EINVAL;

	r = remove_entry_from_min_cpu_freq_list(atoi(argv[0]));
	if (r < 0) {
		_E("Remove entry failed");
		return r;
	}

	if (cur_min_cpu_freq == INT_MIN)
		cur_min_cpu_freq = min_cpu_freq_limit;

	r = write_min_cpu_freq(cur_min_cpu_freq);
	if (r < 0) {
		_E("Write entry failed");
		return r;
	}

	return 0;
}

static int add_entry_to_max_cpu_freq_list(int pid, int freq)
{
	int r;
	struct cpu_freq_entry *entry;

	r = remove_entry_from_max_cpu_freq_list(pid);
	if (r < 0) {
		_E("Remove duplicated entry failed");
	}

	entry = malloc(sizeof(struct cpu_freq_entry));
	if (!entry) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	entry->pid = pid;
	entry->freq = freq;

	DD_LIST_PREPEND(max_cpu_freq_list, entry);
	if (!max_cpu_freq_list) {
		_E("eina_list_prepend failed");
		return -ENOSPC;
	}
	if (freq < cur_max_cpu_freq) {
		cur_max_cpu_freq = freq;
	}
	return 0;
}

static int add_entry_to_min_cpu_freq_list(int pid, int freq)
{
	int r;
	struct cpu_freq_entry *entry;

	r = remove_entry_from_min_cpu_freq_list(pid);
	if (r < 0) {
		_E("Remove duplicated entry failed");
	}

	entry = malloc(sizeof(struct cpu_freq_entry));
	if (!entry) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	entry->pid = pid;
	entry->freq = freq;

	DD_LIST_PREPEND(min_cpu_freq_list, entry);
	if (!min_cpu_freq_list) {
		_E("eina_list_prepend failed");
		return -ENOSPC;
	}
	if (freq > cur_min_cpu_freq) {
		cur_min_cpu_freq = freq;
	}
	return 0;
}

int set_max_frequency_action(int argc, char **argv)
{
	int r;

	if (argc < 2)
		return -EINVAL;

	r = add_entry_to_max_cpu_freq_list(atoi(argv[0]), atoi(argv[1]));
	if (r < 0) {
		_E("Add entry failed");
		return r;
	}

	r = write_max_cpu_freq(cur_max_cpu_freq);
	if (r < 0) {
		_E("Write entry failed");
		return r;
	}

	return 0;
}

int set_min_frequency_action(int argc, char **argv)
{
	int r;

	if (argc < 2)
		return -EINVAL;

	r = add_entry_to_min_cpu_freq_list(atoi(argv[0]), atoi(argv[1]));
	if (r < 0) {
		_E("Add entry failed");
		return r;
	}

	r = write_min_cpu_freq(cur_min_cpu_freq);
	if (r < 0) {
		_E("Write entry failed");
		return r;
	}

	return 0;
}

static int power_saving_cpu_cb(keynode_t *key_nodes, void *data)
{
	int ret = 0;
	int val = 0;
	int power_saving_cpu_stat = -1;

	power_saving_cpu_stat = vconf_keynode_get_bool(key_nodes);
	if (power_saving_cpu_stat == 1) {
		val = 1;
		ret = add_entry_to_max_cpu_freq_list(getpid(), power_saving_freq);
		if (ret < 0) {
			_E("Add entry failed");
			goto out;
		}
	} else {
		ret = remove_entry_from_max_cpu_freq_list(getpid());
		if (ret < 0) {
			_E("Remove entry failed");
			goto out;
		}
		if (cur_max_cpu_freq == INT_MAX)
			cur_max_cpu_freq = max_cpu_freq_limit;
	}
	ret = write_max_cpu_freq(cur_max_cpu_freq);
	if (ret < 0)
		_E("Write failed");
out:
	device_notify(DEVICE_NOTIFIER_PMQOS_POWERSAVING, (void*)val);
	return ret;
}

static void set_emergency_limit(void)
{
	int ret, val;

	ret = vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &val);
	if (ret < 0) {
		_E("failed to get vconf key");
		return;
	}
	if (val == SETTING_PSMODE_EMERGENCY) {
		val = EMERGENCY_LOCK;
		device_notify(DEVICE_NOTIFIER_PMQOS_EMERGENCY, (void*)val);
	}
}

static int emergency_cpu_cb(keynode_t *key_nodes, void *data)
{
	int val;

	val = vconf_keynode_get_int(key_nodes);
	if (val == SETTING_PSMODE_EMERGENCY)
		val = EMERGENCY_LOCK;
	else
		val = EMERGENCY_UNLOCK;

	device_notify(DEVICE_NOTIFIER_PMQOS_EMERGENCY, (void*)val);
	return 0;
}

static void set_freq_limit(void)
{
	int ret = 0;
	int val = 0;
	int power_saving_stat = -1;
	int power_saving_cpu_stat = -1;

	ret = device_get_property(DEVICE_TYPE_CPU, PROP_CPU_CPUINFO_MAX_FREQ,
			&max_cpu_freq_limit);
	if (ret < 0) {
		_E("get cpufreq cpuinfo max readerror: %s", strerror(errno));
		max_cpu_freq_limit = DEFAULT_MAX_CPU_FREQ;
	}

	ret = device_get_property(DEVICE_TYPE_CPU, PROP_CPU_CPUINFO_MIN_FREQ,
			&min_cpu_freq_limit);
	if (ret < 0) {
		_E("get cpufreq cpuinfo min readerror: %s", strerror(errno));
		min_cpu_freq_limit = DEFAULT_MIN_CPU_FREQ;
	}
	power_saving_freq = (int)(max_cpu_freq_limit * POWER_SAVING_CPU_FREQ_RATE);
	_I("max(%d) , ps(%d), min(%d)",
		max_cpu_freq_limit,
		power_saving_freq,
		min_cpu_freq_limit);

	ret = vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU,
			&power_saving_cpu_stat);
	if (ret < 0) {
		_E("failed to get vconf key");
		return;
	}
	if (power_saving_cpu_stat != 1)
		return;
	val = 1;
	ret = add_entry_to_max_cpu_freq_list(getpid(), power_saving_freq);
	if (ret < 0) {
		_E("Add entry failed");
		goto out;
	}
	ret = write_max_cpu_freq(cur_max_cpu_freq);
	if (ret < 0)
		_E("Write entry failed");
out:
	_I("init");
	device_notify(DEVICE_NOTIFIER_PMQOS_POWERSAVING, (void*)val);
}

static int remove_entry_from_cpu_number_list(int pid)
{
	dd_list *tmp;
	struct cpu_number_entry *entry;

	cur_cpu_number = INT_MAX;

	DD_LIST_FOREACH(cpu_number_list, tmp, entry) {
		if ((!is_entry_enable(entry->pid)) || (entry->pid == pid)) {
			DD_LIST_REMOVE(cpu_number_list, entry);
			free(entry);
			continue;
		}
		if (entry->number < cur_cpu_number) {
			cur_cpu_number = entry->number;
		}
	}

	return 0;
}

static int add_entry_to_cpu_number_list(int pid, int number)
{
	int r;
	struct cpu_number_entry *entry;

	r = remove_entry_from_cpu_number_list(pid);
	if (r < 0) {
		_E("Remove duplicated entry failed");
	}



	entry = malloc(sizeof(struct cpu_number_entry));
	if (!entry) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	entry->pid = pid;
	entry->number = number;

	DD_LIST_PREPEND(cpu_number_list, entry);
	if (!cpu_number_list) {
		_E("eina_list_prepend failed");
		return -ENOSPC;
	}
	if (number < cur_cpu_number) {
		cur_cpu_number = number;
	}
	return 0;
}

static int booting_done(void *data)
{
	set_freq_limit();
	set_emergency_limit();
	return 0;
}

static DBusMessage *dbus_cpu_handler(E_DBus_Object *obj, DBusMessage *msg)
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

	if (!strncmp(type_str, SET_MAX_FREQ, SET_FREQ_LEN))
		ret = set_max_frequency_action(argc, (char **)&argv);
	else if (!strncmp(type_str, SET_MIN_FREQ, SET_FREQ_LEN))
		ret = set_min_frequency_action(argc, (char **)&argv);
	else if (!strncmp(type_str, RELEASE_MAX_FREQ, RELEASE_FREQ_LEN))
		ret = release_max_frequency_action(argc, (char **)&argv);
	else if (!strncmp(type_str, RELEASE_MIN_FREQ, RELEASE_FREQ_LEN))
		ret = release_min_frequency_action(argc, (char **)&argv);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ SET_MAX_FREQ,     "siss", "i", dbus_cpu_handler },
	{ SET_MIN_FREQ,     "siss", "i", dbus_cpu_handler },
	{ RELEASE_MAX_FREQ, "siss", "i", dbus_cpu_handler },
	{ RELEASE_MIN_FREQ, "siss", "i", dbus_cpu_handler },
};

static void cpu_init(void *data)
{
	int ret;

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	vconf_notify_key_changed(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_CPU, (void *)power_saving_cpu_cb, NULL);
	vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE, (void *)emergency_cpu_cb, NULL);
}

static const struct device_ops cpu_device_ops = {
	.name     = "cpu",
	.init     = cpu_init,
};

DEVICE_OPS_REGISTER(&cpu_device_ops)
