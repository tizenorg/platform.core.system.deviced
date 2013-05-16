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
#include <heynoti.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <time.h>

#include "device-node.h"
#include "core/log.h"
#include "core/noti.h"
#include "core/queue.h"
#include "core/predefine.h"
#include "core/data.h"
#include "core/devices.h"
#include "core/common.h"
#include "core/edbus-handler.h"

#define PREDEF_LOWMEM			"lowmem"
#define OOM_MEM_ACT			"oom_mem_act"
#define DELETE_SM		"sh -c "PREFIX"/bin/delete.sm"
#define LOWMEM_EXEC_PATH		PREFIX"/bin/lowmem-popup"
#define MEMPS_LOG_FILE			"/var/log/memps"
#define MEMPS_EXEC_PATH			PREFIX"/bin/memps"
/* wait for 5 sec as victim process be dead */
#define WAITING_INTERVAL		5

#define MEMNOTIFY_NORMAL	0x0000
#define MEMNOTIFY_LOW		0xfaac
#define MEMNOTIFY_CRITICAL	0xdead
#define MEMNOTIFY_REBOOT	0xb00f

#define _SYS_RES_CLEANUP	"RES_CLEANUP"

#define MEM_THRESHOLD_LV1	110
#define MEM_THRESHOLD_LV2	70

#define MEMORY_STATUS_USR_PATH			"/opt/usr"
#define MEMORY_MEGABYTE_VALUE			1048576
#define MEMORY_RESERVE_VALUE			(100*MEMORY_MEGABYTE_VALUE)

struct lowmem_process_entry {
	unsigned cur_mem_state;
	unsigned new_mem_state;
	int (*action) (void *);
};

static int lowmem_fd = -1;
static int cur_mem_state = MEMNOTIFY_NORMAL;

Ecore_Timer *oom_timer;
#define OOM_TIMER_INTERVAL	5

static int memory_low_act(void *ad);
static int memory_oom_act(void *ad);
static int memory_normal_act(void *ad);

static struct lowmem_process_entry lpe[] = {
	{MEMNOTIFY_NORMAL, MEMNOTIFY_LOW, memory_low_act},
	{MEMNOTIFY_NORMAL, MEMNOTIFY_CRITICAL, memory_oom_act},
	{MEMNOTIFY_LOW, MEMNOTIFY_CRITICAL, memory_oom_act},
	{MEMNOTIFY_CRITICAL, MEMNOTIFY_CRITICAL, memory_oom_act},
	{MEMNOTIFY_LOW, MEMNOTIFY_NORMAL, memory_normal_act},
	{MEMNOTIFY_CRITICAL, MEMNOTIFY_NORMAL, memory_normal_act},

};

unsigned int oom_delete_sm_time = 0;

static int remove_shm()
{
	int maxid, shmid, id;
	struct shmid_ds shmseg;
	struct shm_info shm_info;

	maxid = shmctl(0, SHM_INFO, (struct shmid_ds *)(void *)&shm_info);
	if (maxid < 0) {
		_E("shared mem error");
		return -1;
	}

	for (id = 0; id <= maxid; id++) {
		shmid = shmctl(id, SHM_STAT, &shmseg);
		if (shmid < 0)
			continue;
		if (shmseg.shm_nattch == 0) {
			_D("shared memory killer ==> %d killed", shmid);
			shmctl(shmid, IPC_RMID, NULL);
		}
	}
	return 0;
}

static char *convert_to_str(unsigned int mem_state)
{
	char *tmp;
	switch (mem_state) {
	case MEMNOTIFY_NORMAL:
		tmp = "mem normal";
		break;
	case MEMNOTIFY_LOW:
		tmp = "mem low";
		break;
	case MEMNOTIFY_CRITICAL:
		tmp = "mem critical";
		break;
	case MEMNOTIFY_REBOOT:
		tmp = "mem reboot";
		break;
	default:
		assert(0);
	}
	return tmp;
}

static void print_lowmem_state(unsigned int mem_state)
{
	_D("[LOW MEM STATE] %s ==> %s", convert_to_str(cur_mem_state),
		  convert_to_str(mem_state));
}
#define BUF_MAX 1024
static int get_lowmemnotify_info(FILE *output_fp)
{
	FILE *fp;
	char line[BUF_MAX];

	if (output_fp == NULL)
		return -1;

	fp = fopen("/sys/class/memnotify/meminfo", "r");
	if (fp == NULL)
		return -1;
	_D("make LOWMEM_LOG");
	fprintf(output_fp,
		"====================================================================");
	fprintf(output_fp, "MEMORY INFO by lowmemnotify");

	while (fgets(line, BUF_MAX, fp) != NULL) {
		_D("%s", line);
		fputs(line, output_fp);
	}
	fclose(fp);

	return 0;
}

static void make_LMM_log(char *file, pid_t pid, char *victim_name)
{
	time_t now;
	struct tm *cur_tm;
	char new_log[NAME_MAX];
	static pid_t old_pid = 0;
	int ret=-1;
	FILE *output_file = NULL;

	if (old_pid == pid)
		return;
	old_pid = pid;

	now = time(NULL);
	cur_tm = (struct tm *)malloc(sizeof(struct tm));
	if (cur_tm == NULL) {
		_E("Fail to memory allocation");
		return;
	}

	if (localtime_r(&now, cur_tm) == NULL) {
		_E("Fail to get localtime");
		free(cur_tm);
		return;
	}

	_D("%s_%s_%d_%.4d%.2d%.2d_%.2d%.2d%.2d.log", file, victim_name,
		 pid, (1900 + cur_tm->tm_year), 1 + cur_tm->tm_mon,
		 cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);
	snprintf(new_log, sizeof(new_log),
		 "%s_%s_%d_%.4d%.2d%.2d_%.2d%.2d%.2d.log", file, victim_name,
		 pid, (1900 + cur_tm->tm_year), 1 + cur_tm->tm_mon,
		 cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);

	output_file = fopen(new_log, "w+");
	if(!output_file) {
		_E("cannot open output file(%s)",new_log);
		free(cur_tm);
		return;
	}
	get_lowmemnotify_info(output_file);
	fclose(output_file);
	free(cur_tm);
}



static int memory_low_act(void *data)
{
	char lowmem_noti_name[NAME_MAX];

	_I("[LOW MEM STATE] memory low state");
	make_LMM_log("/var/log/memps", 1, "LOWMEM_WARNING");
	remove_shm();
	heynoti_get_snoti_name(_SYS_RES_CLEANUP, lowmem_noti_name, NAME_MAX);
	ss_noti_send(lowmem_noti_name);
	vconf_set_int(VCONFKEY_SYSMAN_LOW_MEMORY,
		      VCONFKEY_SYSMAN_LOW_MEMORY_SOFT_WARNING);

	return 0;
}

static int memory_oom_act(void *data)
{
	unsigned int cur_time;
	char lowmem_noti_name[NAME_MAX];

	_I("[LOW MEM STATE] memory oom state");
	cur_time = time(NULL);
	_D("cur=%d, old=%d, cur-old=%d", cur_time, oom_delete_sm_time,
		  cur_time - oom_delete_sm_time);
	if (cur_time - oom_delete_sm_time > 15) {
		remove_shm();
		oom_delete_sm_time = cur_time;
		/* Also clean up unreturned memory of applications */
		heynoti_get_snoti_name(_SYS_RES_CLEANUP, lowmem_noti_name,
				       NAME_MAX);
		ss_noti_send(lowmem_noti_name);
	}
	action_entry_call_internal(PREDEF_LOWMEM, 1, OOM_MEM_ACT);

	vconf_set_int(VCONFKEY_SYSMAN_LOW_MEMORY,
		      VCONFKEY_SYSMAN_LOW_MEMORY_HARD_WARNING);

	return 1;
}

static int memory_normal_act(void *data)
{
	_I("[LOW MEM STATE] memory normal state");
	vconf_set_int(VCONFKEY_SYSMAN_LOW_MEMORY,
		      VCONFKEY_SYSMAN_LOW_MEMORY_NORMAL);
	return 0;
}

static int lowmem_process(unsigned int mem_state, void *ad)
{
	int i;
	for (i = 0; i < sizeof(lpe) / sizeof(struct lowmem_process_entry); i++) {
		if ((cur_mem_state == lpe[i].cur_mem_state)
		    && (mem_state == lpe[i].new_mem_state)) {

			if(oom_timer != NULL) {
				ecore_timer_del(oom_timer);
				oom_timer = NULL;
			}
			lpe[i].action(ad);
			if(mem_state == MEMNOTIFY_CRITICAL)
				oom_timer = ecore_timer_add(OOM_TIMER_INTERVAL,lpe[i].action, ad);
			return 0;
		}
	}
	return 0;
}

static unsigned int lowmem_read(int fd)
{
	unsigned int mem_state;
	if (read(fd, &mem_state, sizeof(mem_state)) < 0) {
		_E("error lowmem state");
		return -1;
	}
	return mem_state;
}
static int lowmem_cb(void *data, Ecore_Fd_Handler * fd_handler)
{
	int fd;
	struct ss_main_data *ad = (struct ss_main_data *)data;
	unsigned int mem_state;

	if (!ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ)) {
		_E("ecore_main_fd_handler_active_get error, return");
		return -1;
	}

	fd = ecore_main_fd_handler_fd_get(fd_handler);
	if (fd < 0) {
		_E("ecore_main_fd_handler_fd_get error, return");
		return -1;
	}
	mem_state = lowmem_read(fd);
	print_lowmem_state(mem_state);
	lowmem_process(mem_state, ad);
	cur_mem_state = mem_state;

	return 1;
}

static int set_threshold()
{
	if (device_set_property(DEVICE_TYPE_MEMORY, PROP_MEMORY_THRESHOLD_LV1, MEM_THRESHOLD_LV1) < 0) {
		_E("Set memnorify threshold lv1 failed");
		return -1;
	}

	if (device_set_property(DEVICE_TYPE_MEMORY, PROP_MEMORY_THRESHOLD_LV2, MEM_THRESHOLD_LV2) < 0) {
		_E("Set memnorify threshold lv2 failed");
		return -1;
	}

	return 0;
}

static void make_memps_log(char *file, pid_t pid, char *victim_name)
{
	time_t now;
	struct tm *cur_tm;
	char params[4096];
	char new_log[NAME_MAX];
	static pid_t old_pid = 0;
	int ret=-1;

	if (old_pid == pid)
		return;
	old_pid = pid;

	now = time(NULL);
	cur_tm = (struct tm *)malloc(sizeof(struct tm));
	if (cur_tm == NULL) {
		_E("Fail to memory allocation");
		return;
	}

	if (localtime_r(&now, cur_tm) == NULL) {
		_E("Fail to get localtime");
		free(cur_tm);
		return;
	}

	_D("%s_%s_%d_%.4d%.2d%.2d_%.2d%.2d%.2d.log", file, victim_name,
		 pid, (1900 + cur_tm->tm_year), 1 + cur_tm->tm_mon,
		 cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);
	snprintf(new_log, sizeof(new_log),
		 "%s_%s_%d_%.4d%.2d%.2d_%.2d%.2d%.2d.log", file, victim_name,
		 pid, (1900 + cur_tm->tm_year), 1 + cur_tm->tm_mon,
		 cur_tm->tm_mday, cur_tm->tm_hour, cur_tm->tm_min,
		 cur_tm->tm_sec);

	snprintf(params, sizeof(params), "-f %s", new_log);
	ret = ss_launch_evenif_exist(MEMPS_EXEC_PATH, params);
	/* will be removed, just for debugging */
	if(ret > 0) {
		FILE *fp;
		fp = open_proc_oom_adj_file(ret, "w");
		if (fp != NULL) {
			fprintf(fp, "%d", (-17));
			fclose(fp);
		}
	}
	free(cur_tm);
}

static int __fs_stat(double* pdTotal, double* pdAvail, const char* szPath)
{
        struct statvfs s;

        if (NULL == pdAvail) {
                _E("input param error");
                return 0;
        }

        if (!statvfs(szPath, &s)) {
                *pdTotal = (double)s.f_frsize * s.f_blocks;
                *pdAvail = (double)s.f_bsize * s.f_bavail;
        } else {
                _E("fail to get memory size");
                return 0;
        }

        return 1;
}

static int lowmem_get_victim_pid()
{
	pid_t pid;
	int fd;

	if (device_get_property(DEVICE_TYPE_MEMORY, PROP_MEMORY_VICTIM_TASK, &pid) < 0) {
		_E("Get victim task failed");
		return -1;
	}

	return pid;
}

int lowmem_def_predefine_action(int argc, char **argv)
{
	int pid, ret, oom_adj;
	char appname[PATH_MAX];

	if (argc < 1)
		return -1;

	if (!strcmp(argv[0], OOM_MEM_ACT)) {
		pid = lowmem_get_victim_pid();
		if (pid > 0 && pid != get_exec_pid(LOWMEM_EXEC_PATH) && pid != get_exec_pid(MEMPS_EXEC_PATH)) {
			if ((get_cmdline_name(pid, appname, PATH_MAX)) ==
			    0) {
				_E("we will kill, lowmem lv2 = %d (%s)", pid, appname);
				make_memps_log(MEMPS_LOG_FILE, pid, appname);

				if(get_app_oomadj(pid, &oom_adj) < 0) {
					_E("Failed to get oom_adj");
					return -1;
				}
				_D("%d will be killed with %d oom_adj value", pid, oom_adj);

				kill(pid, SIGTERM);

				if (check_oomadj(oom_adj) == 0)
					return 0;

				bundle *b = NULL;

				b = bundle_create();
				bundle_add(b, "_APP_NAME_", appname);
				ret = syspopup_launch("lowmem-syspopup", b);
				bundle_free(b);
				if (ret < 0) {
					_E("popup lauch failed");
					return -1;
				}

				if (set_su_oomadj(ret) < 0) {
					_E("Failed to set oom_adj");
				}
			}
		}
	}
	return 0;
}
static DBusMessage *e_dbus_getstatus(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	double dAvail = 0.0;
	double dTotal = 0.0;

	ret = __fs_stat(&dTotal, &dAvail, MEMORY_STATUS_USR_PATH);

	dAvail -= MEMORY_RESERVE_VALUE;
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &dTotal);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &dAvail);
	return reply;
}

static struct edbus_method {
	const char *member;
	const char *signature;
	const char *reply_signature;
	E_DBus_Method_Cb func;
} edbus_methods[] = {
	{ "getstorage",       NULL,   "i", e_dbus_getstatus },
	/* Add methods here */
};

static int lowmem_dbus_init(void)
{
	E_DBus_Interface *iface;
	int ret, i;

	iface = get_edbus_interface(DEVICED_PATH_STORAGE);

	_D("%s, %x", DEVICED_PATH_STORAGE, iface);

	if (!iface) {
		_E("fail to get edbus interface!");
		return -EPERM;
	}

	for (i = 0; i < ARRAY_SIZE(edbus_methods); i++) {
		ret = e_dbus_interface_method_add(iface,
				    edbus_methods[i].member,
				    edbus_methods[i].signature,
				    edbus_methods[i].reply_signature,
				    edbus_methods[i].func);
		if (!ret) {
			_E("fail to add method %s!", edbus_methods[i].member);
			return -EPERM;
		}
	}

	return 0;
}

static void lowmem_init(void *data)
{
	struct ss_main_data *ad = (struct ss_main_data*)data;
	char lowmem_dev_node[PATH_MAX];

	lowmem_dbus_init();
	action_entry_add_internal(PREDEF_LOWMEM, lowmem_def_predefine_action,
				     NULL, NULL);

	if (device_get_property(DEVICE_TYPE_MEMORY, PROP_MEMORY_NODE, lowmem_dev_node) < 0) {
		_E("Low memory handler fd init failed");
		return;
	}

	lowmem_fd = open(lowmem_dev_node, O_RDONLY);
	if (lowmem_fd < 0) {
		_E("ss_lowmem_init fd open failed");
		return;
	}

	oom_timer = NULL;
	ecore_main_fd_handler_add(lowmem_fd, ECORE_FD_READ, lowmem_cb, ad, NULL,
				  NULL);
	if (set_threshold() < 0) {
		_E("Setting lowmem threshold is failed");
		return;
	}
}

const struct device_ops lowmem_device_ops = {
	.init = lowmem_init,
};
