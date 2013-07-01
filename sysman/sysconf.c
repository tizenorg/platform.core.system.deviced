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
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>

#include "sysman.h"
#include "sysman-priv.h"

#define PERMANENT_DIR		"/tmp/permanent"
#define VIP_DIR			"/tmp/vip"

#define OOMADJ_SET		"oomadj_set"
#define PROCESS_GROUP_SET	"process_group_set"
#define PROCESS_VIP		"process_vip"
#define PROCESS_PERMANENT	"process_permanent"

enum mp_entry_type {
	MP_VIP,
	MP_PERMANENT,
	MP_NONE
};

int util_oomadj_set(int pid, int oomadj_val)
{
	char buf1[255];
	char buf2[255];

	snprintf(buf1, sizeof(buf1), "%d", pid);
	snprintf(buf2, sizeof(buf2), "%d", oomadj_val);
	return sysman_call_predef_action(OOMADJ_SET, 2, buf1, buf2);
}

int util_process_group_set(const char* name, int pid)
{
	char buf[255];

	if (strncmp(PROCESS_VIP, name, strlen(name)) != 0 &&
	    strncmp(PROCESS_PERMANENT, name, strlen(name)) != 0) {
		ERR("fail to insert at %s group", name);
		return -1;
	}

	snprintf(buf, sizeof(buf), "%d", pid);
	ERR("pid(%d) is inserted at vip", pid);

	return sysman_call_predef_action(PROCESS_GROUP_SET, 2, buf, name);
}

API int sysconf_set_mempolicy_bypid(int pid, enum mem_policy mempol)
{
	if (pid < 1)
		return -1;

	int oomadj_val = 0;

	switch (mempol) {
	case OOM_LIKELY:
		oomadj_val = 1;
		break;
	case OOM_IGNORE:
		oomadj_val = -17;
		break;
	default:
		return -1;
	}

	return util_oomadj_set(pid, oomadj_val);
}

API int sysconf_set_mempolicy(enum mem_policy mempol)
{
	return sysconf_set_mempolicy_bypid(getpid(), mempol);
}

static int already_permanent(int pid)
{
	char buf[BUFF_MAX];

	snprintf(buf, BUFF_MAX, "%s/%d", PERMANENT_DIR, pid);

	if (access(buf, R_OK) == 0) {
		DBG("already_permanent process : %d", pid);
		return 1;
	}
	return 0;
}

static int copy_cmdline(int pid)
{
	char buf[PATH_MAX];
	char filepath[PATH_MAX];
	int fd;
	int cnt;
	int r;

	if (access(PERMANENT_DIR, R_OK) < 0) {
		DBG("no predefined matrix dir = %s, so created", PERMANENT_DIR);
		r = mkdir(PERMANENT_DIR, 0777);
		if(r < 0) {
			ERR("permanent directory mkdir is failed");
			return -1;
		}
	}

	snprintf(filepath, PATH_MAX, "/proc/%d/cmdline", pid);

	fd = open(filepath, O_RDONLY);
	if (fd == -1) {
		DBG("Failed to open");
		return -1;
	}

	cnt = read(fd, buf, PATH_MAX);
	close(fd);

	if (cnt <= 0) {
		/* Read /proc/<pid>/cmdline error */
		DBG("Failed to read");
		return -1;
	}

	snprintf(filepath, PATH_MAX, "%s/%d", PERMANENT_DIR, pid);

	fd = open(filepath, O_CREAT | O_WRONLY, 0644);
	if (fd == -1) {
		DBG("Failed to open");
		return -1;
	}

	if (write(fd, buf, cnt) == -1) {
		DBG("Failed to write");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

API int sysconf_set_vip(int pid)
{
	char buf[BUFF_MAX];
	int fd;
	int r;

	if (pid < 1)
		return -1;

	if (access(VIP_DIR, R_OK) < 0) {
		DBG("no predefined matrix dir = %s, so created", VIP_DIR);
		r = mkdir(VIP_DIR, 0777);
		if(r < 0) {
			ERR("sysconf_set_vip vip mkdir is failed");
			return -1;
		}
	}

	snprintf(buf, BUFF_MAX, "%s/%d", VIP_DIR, pid);
	fd = open(buf, O_CREAT | O_RDWR, 0644);
	if (fd < 0) {
		ERR("sysconf_set_vip fd open failed");
		return -1;
	}
	close(fd);
	if (util_process_group_set(PROCESS_VIP, pid) < 0) {
		ERR("set vip failed");
		return -1;
	}

	return 0;
}

API int sysconf_is_vip(int pid)
{
	if (pid < 1)
		return -1;

	char buf[BUFF_MAX];

	snprintf(buf, BUFF_MAX, "%s/%d", VIP_DIR, pid);

	if (access(buf, R_OK) == 0)
		return 1;
	else
		return 0;
}

API int sysconf_set_permanent_bypid(int pid)
{
	int fd;
	if (already_permanent(pid))
		goto MEMPOL_SET;

	if (copy_cmdline(pid) < 0)
		return -1;

	if (util_process_group_set(PROCESS_PERMANENT, pid) < 0) {
		ERR("set vip failed");
		return -1;
	}

 MEMPOL_SET:
	util_oomadj_set(pid, -17);

	return 0;
}

API int sysconf_set_permanent()
{
	pid_t pid = getpid();
	return sysconf_set_permanent_bypid(pid);
}
