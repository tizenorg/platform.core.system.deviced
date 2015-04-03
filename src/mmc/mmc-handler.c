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


#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <vconf.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <tzplatform_config.h>

#include "core/log.h"
#include "core/device-notifier.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/udev.h"
#include "mmc-handler.h"
#include "config.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "core/config-parser.h"

#define VCONFKEY_INTERNAL_PRIVATE_MMC_ID	"db/private/sysman/mmc_device_id"

#define MMC_PARENT_PATH	tzplatform_getenv(TZ_SYS_STORAGE)
#define MMC_DEV			"/dev/mmcblk"
#define MMC_PATH        "*/mmcblk[0-9]"

#define SMACKFS_MAGIC	0x43415d53
#define SMACKFS_MNT		"/smack"

#ifndef ST_RDONLY
#define ST_RDONLY		0x0001
#endif

#define MMC_32GB_SIZE	61315072
#define FORMAT_RETRY	3
#define UNMOUNT_RETRY	5

#define SMACK_LABELING_TIME (0.5)

enum unmount_operation {
	UNMOUNT_NORMAL = 0,
	UNMOUNT_FORCE,
};

enum mmc_operation {
	MMC_MOUNT = 0,
	MMC_UNMOUNT,
	MMC_FORMAT,
	MMC_END,
};

static void *mount_start(void *arg);
static void *unmount_start(void *arg);
static void *format_start(void *arg);

static const struct mmc_thread_func {
	enum mmc_operation type;
	void *(*func) (void*);
} mmc_func[MMC_END] = {
	[MMC_MOUNT] = {.type = MMC_MOUNT, .func = mount_start},
	[MMC_UNMOUNT] = {.type = MMC_UNMOUNT, .func = unmount_start},
	[MMC_FORMAT] = {.type = MMC_FORMAT, .func = format_start},
};

struct mmc_data {
	int option;
	char *devpath;
};

static dd_list *fs_head;
static char *mmc_curpath;
static bool smack = false;
static bool mmc_disabled = false;
static Ecore_Timer *smack_timer = NULL;

static void __CONSTRUCTOR__ smack_check(void)
{
	struct statfs sfs;
	int ret;

	do {
		ret = statfs(SMACKFS_MNT, &sfs);
	} while (ret < 0 && errno == EINTR);

	if (ret == 0 && sfs.f_type == SMACKFS_MAGIC)
		smack = true;
	_I("smackfs check %d", smack);
}

void add_fs(const struct mmc_fs_ops *fs)
{
	DD_LIST_APPEND(fs_head, (void*)fs);
}

void remove_fs(const struct mmc_fs_ops *fs)
{
	DD_LIST_REMOVE(fs_head, (void*)fs);
}

const struct mmc_fs_ops *find_fs(enum mmc_fs_type type)
{
	struct mmc_fs_ops *fs;
	dd_list *elem;

	DD_LIST_FOREACH(fs_head, elem, fs) {
		if (fs->type == type)
			return fs;
	}
	return NULL;
}

bool mmc_check_mounted(const char *mount_point)
{
	struct stat parent_stat, mount_stat;
	char parent_path[PATH_MAX];

	snprintf(parent_path, sizeof(parent_path), "%s", MMC_PARENT_PATH);

	if (stat(mount_point, &mount_stat) != 0 || stat(parent_path, &parent_stat) != 0)
		return false;

	if (mount_stat.st_dev == parent_stat.st_dev)
		return false;

	return true;
}

static void launch_syspopup(char *str)
{
	manage_notification("MMC", str);
}

static int get_partition(const char *devpath, char *subpath)
{
	char path[NAME_MAX];
	int i;

	for (i = 1; i < 5; ++i) {
		snprintf(path, sizeof(path), "%sp%d", devpath, i);
		if (!access(path, R_OK)) {
			strncpy(subpath, path, strlen(path));
			return 0;
		}
	}
	return -ENODEV;
}

static int create_partition(const char *devpath)
{
	int r;
	char data[NAME_MAX];

	snprintf(data, sizeof(data), "\"n\\n\\n\\n\\n\\nw\" | fdisk %s", devpath);

	r = launch_evenif_exist("/usr/bin/printf", data);
	if (WIFSIGNALED(r) && (WTERMSIG(r) == SIGINT || WTERMSIG(r) == SIGQUIT || WEXITSTATUS(r)))
		return -1;

	return 0;
}

static int mmc_check_and_unmount(const char *path)
{
	int ret = 0, retry = 0;
	while (mount_check(path)) {
		ret = umount(path);
		if (ret < 0) {
			retry++;
			if (retry > UNMOUNT_RETRY)
				return -errno;
		}
	}
	return ret;
}

int get_block_number(void)
{
	DIR *dp;
	struct dirent *dir;
	struct stat stat;
	char buf[255];
	int fd;
	int r;
	int mmcblk_num;
	char *pre_mmc_device_id = NULL;
	int mmc_dev_changed = 0;

	if ((dp = opendir("/sys/block")) == NULL) {
		_E("Can not open directory..");
		return -1;
	}

	r = chdir("/sys/block");
	if (r < 0) {
		_E("Fail to change the directory..");
		closedir(dp);
		return r;
	}

	while ((dir = readdir(dp)) != NULL) {
		memset(&stat, 0, sizeof(struct stat));
		if(lstat(dir->d_name, &stat) < 0) {continue;}
		if (S_ISDIR(stat.st_mode) || S_ISLNK(stat.st_mode)) {
			if (strncmp(".", dir->d_name, 1) == 0
			    || strncmp("..", dir->d_name, 2) == 0)
				continue;
			if (strncmp("mmcblk", dir->d_name, 6) == 0) {
				snprintf(buf, 255, "/sys/block/%s/device/type",
					 dir->d_name);

				fd = open(buf, O_RDONLY);
				if (fd == -1) {
					continue;
				}
				r = read(fd, buf, 10);
				if ((r >= 0) && (r < 10))
					buf[r] = '\0';
				else
					_E("%s read error: %s", buf,
						      strerror(errno));
				close(fd);
				if (strncmp("SD", buf, 2) == 0) {
					char *str_mmcblk_num = strndup((dir->d_name) + 6, 1);
					if (str_mmcblk_num == NULL) {
						_E("Memory Allocation Failed");
						closedir(dp);
						return -1;
					}
					mmcblk_num =
					    atoi(str_mmcblk_num);

					free(str_mmcblk_num);
					closedir(dp);
					_I("%d", mmcblk_num);

					snprintf(buf, 255, "/sys/block/%s/device/cid", dir->d_name);

					fd = open(buf, O_RDONLY);
					if (fd == -1) {
						_E("%s open error", buf, strerror(errno));
						return mmcblk_num;
					}
					r = read(fd, buf, 255);
					if ((r >=0) && (r < 255)) {
						buf[r] = '\0';
					} else {
						_E("%s read error: %s", buf,strerror(errno));
					}
					close(fd);
					pre_mmc_device_id = vconf_get_str(VCONFKEY_INTERNAL_PRIVATE_MMC_ID);
					if (pre_mmc_device_id) {
						if (strcmp(pre_mmc_device_id, "") == 0) {
							vconf_set_str(VCONFKEY_INTERNAL_PRIVATE_MMC_ID, buf);
						} else if (strncmp(pre_mmc_device_id,buf,33) == 0) {
							if ( vconf_get_int(VCONFKEY_SYSMAN_MMC_DEVICE_CHANGED,&mmc_dev_changed) == 0
							&& mmc_dev_changed != VCONFKEY_SYSMAN_MMC_NOT_CHANGED) {
								vconf_set_int(VCONFKEY_SYSMAN_MMC_DEVICE_CHANGED, VCONFKEY_SYSMAN_MMC_NOT_CHANGED);
							}
						} else if (strncmp(pre_mmc_device_id,buf,32) != 0) {
							vconf_set_str(VCONFKEY_INTERNAL_PRIVATE_MMC_ID, buf);
							vconf_set_int(VCONFKEY_SYSMAN_MMC_DEVICE_CHANGED, VCONFKEY_SYSMAN_MMC_CHANGED);
						}
						free(pre_mmc_device_id);
					} else {
						_E("failed to get pre_mmc_device_id");
					}
					return mmcblk_num;
				}
			}

		}
	}
	closedir(dp);
	_E("failed to find mmc block number");
	return -1;
}

static int find_mmc_node(char devpath[])
{
	int num;

	num = get_block_number();
	if (num < 0)
		return -ENODEV;

	snprintf(devpath, NAME_MAX, "%s%d", MMC_DEV, num);
	return 0;
}

static int get_mmc_size(const char *devpath)
{
	int fd, r;
	unsigned long long ullbytes;
	unsigned int nbytes;

	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		_E("open error");
		return -EINVAL;
	}

	r = ioctl(fd, BLKGETSIZE64, &ullbytes);
	close(fd);

	if (r < 0) {
		_E("ioctl BLKGETSIZE64 error");
		return -EINVAL;
	}

	nbytes = ullbytes/512;
	_I("block size(64) : %d", nbytes);
	return nbytes;
}

static int rw_mount(const char *szPath)
{
	struct statvfs mount_stat;
	if (!statvfs(szPath, &mount_stat)) {
		if ((mount_stat.f_flag & ST_RDONLY) == ST_RDONLY)
			return -1;
	}
	return 0;
}

static Eina_Bool smack_timer_cb(void *data)
{
	if (smack_timer) {
		ecore_timer_del(smack_timer);
		smack_timer = NULL;
	}
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	return EINA_FALSE;
}

void mmc_mount_done(void)
{
	smack_timer = ecore_timer_add(SMACK_LABELING_TIME,
			smack_timer_cb, NULL);
	if (smack_timer) {
		_I("Wait to check");
		return;
	}
	_E("Fail to add abnormal check timer");
	smack_timer_cb(NULL);
}

static int mmc_mount(const char *devpath, const char *mount_point)
{
	struct mmc_fs_ops *fs;
	dd_list *elem;
	char path[NAME_MAX] = {0,};
	int r;

	/* mmc_disabled set by start/stop func. */
	if (mmc_disabled)
		return -EWOULDBLOCK;

	if (!devpath)
		return -ENODEV;

	/* check partition */
	r = get_partition(devpath, path);
	if (!r)
		devpath = path;

	DD_LIST_FOREACH(fs_head, elem, fs) {
		if (fs->match(devpath))
			break;
	}

	if (!fs)
		return -EINVAL;

	_I("devpath : %s", devpath);
	r = fs->check(devpath);
	if (r < 0)
		_E("failt to check devpath : %s", devpath);

	r = fs->mount(smack, devpath, mount_point);
	if (r < 0)
		return r;

	r = rw_mount(mount_point);
	if (r < 0)
		return -EROFS;

	return 0;
}

static void *mount_start(void *arg)
{
	struct mmc_data *data = (struct mmc_data*)arg;
	char *devpath;
	int r;

	devpath = data->devpath;
	if (!devpath) {
		r = -EINVAL;
		goto error;
	}

	/* clear previous filesystem */
	mmc_check_and_unmount(MMC_MOUNT_POINT);

	/* check mount point */
	if (access(MMC_MOUNT_POINT, R_OK) != 0) {
		if (mkdir(MMC_MOUNT_POINT, 0755) < 0) {
			r = -errno;
			goto error;
		}
	}

	/* mount operation */
	r = mmc_mount(devpath, MMC_MOUNT_POINT);
	if (r == -EROFS)
		launch_syspopup("mountrdonly");
	/* Do not need to show error popup, if mmc is disabled */
	else if (r == -EWOULDBLOCK)
		goto error_without_popup;
	else if (r < 0)
		goto error;

	mmc_set_config(MAX_RATIO);

	free(devpath);
	free(data);

	/* give a transmutable attribute to mount_point */
	r = setxattr(MMC_MOUNT_POINT, "security.SMACK64TRANSMUTE", "TRUE", strlen("TRUE"), 0);
	if (r < 0)
		_E("setxattr error : %s", strerror(errno));

	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	return 0;

error:
	launch_syspopup("mounterr");

error_without_popup:
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_FAILED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);

	free(devpath);
	free(data);
	_E("failed to mount device : %s", strerror(-r));
	return (void *)r;
}

static int mmc_unmount(int option, const char *mount_point)
{
	int r, retry = 0;
	int kill_op;

	/* it must called before unmounting mmc */
	r = mmc_check_and_unmount(mount_point);
	if (!r)
		return r;
	if (option == UNMOUNT_NORMAL) {
		_I("Failed to unmount with normal option : %s", strerror(-r));
		return r;
	}

	_I("Execute force unmount!");
	/* Force Unmount Scenario */
	while (1) {
		switch (retry++) {
		case 0:
			/* At first, notify to other app who already access sdcard */
			_I("Notify to other app who already access sdcard");
			vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
			break;
		case 1:
			/* Second, kill app with SIGTERM */
			_I("Kill app with SIGTERM");
			terminate_process(MMC_MOUNT_POINT, false);
			break;
		case 2:
			/* Last time, kill app with SIGKILL */
			_I("Kill app with SIGKILL");
			terminate_process(MMC_MOUNT_POINT, true);
			break;
		default:
			if (umount2(mount_point, MNT_DETACH) != 0) {
				_I("Failed to unmount with lazy option : %s", strerror(errno));
				return -errno;
			}
			return 0;
		}

		/* it takes some seconds til other app completely clean up */
		usleep(500*1000);

		r = mmc_check_and_unmount(mount_point);
		if (!r)
			break;
	}

	return r;
}

static void *unmount_start(void *arg)
{
	struct mmc_data *data = (struct mmc_data*)arg;
	int option, r;

	option = data->option;

	assert(option == UNMOUNT_NORMAL || option == UNMOUNT_FORCE);

	r = mmc_unmount(option, MMC_MOUNT_POINT);
	if (r < 0)
		goto error;

	free(data);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	return 0;

error:
	free(data);
	_E("Failed to unmount device : %s", strerror(-r));
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	return (void *)r;
}

static int format(const char *devpath)
{
	const struct mmc_fs_ops *fs = NULL;
	dd_list *elem;
	char path[NAME_MAX] = {0,};
	int r, size, retry;

	if (!devpath)
		return -ENODEV;

	/* check partition */
	r = get_partition(devpath, path);
	if (!r) {
		/* if there is partition, find partition file system */
		DD_LIST_FOREACH(fs_head, elem, fs) {
			if (fs->match(path))
				break;
		}
	} else {
		/* if there isn't partition, create partition */
		create_partition(devpath);
		r = get_partition(devpath, path);
		if (r < 0)
			memcpy(path, devpath, strlen(devpath));
	}

	_I("format partition : %s", path);

	if (!fs) {
		/* find root file system */
		DD_LIST_FOREACH(fs_head, elem, fs) {
			if (fs->match(devpath))
				break;
		}
	}

	if (!fs) {
		/* cannot find root and partition file system,
		   find suitable file system */
		size = get_mmc_size(path);
		fs = find_fs(FS_TYPE_VFAT);
	}

	if (!fs)
		return -EINVAL;

	for (retry = FORMAT_RETRY; retry > 0; --retry) {
		fs->check(devpath);
		_I("format path : %s", path);
		r = fs->format(path);
		if (!r)
			break;
	}
	return r;
}

static void *format_start(void *arg)
{
	struct mmc_data *data = (struct mmc_data*)arg;
	char *devpath;
	int option, r, key = VCONFKEY_SYSMAN_MMC_MOUNTED;
	bool format_ret = true;

	option = data->option;
	devpath = data->devpath;

	assert(devpath);
	assert(option == UNMOUNT_NORMAL || option == UNMOUNT_FORCE);

	_I("Format Start (option:%d)", option);
	r = mmc_unmount(option, MMC_MOUNT_POINT);
	if (r < 0)
		goto release_memory;

	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS, VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NOW);
	r = format(devpath);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS, VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NONE);
	if (r != 0)
		format_ret = false;

	mount_start(arg);
	if (!format_ret)
		goto error;
	_I("Format Successful");
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED);
	return 0;

release_memory:
	free(devpath);
	free(data);
error:
	_E("Format Failed : %s", strerror(-r));
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_FAILED);
	return (void*)r;
}

static int mmc_make_thread(int type, int option, const char *devpath)
{
	pthread_t th;
	struct mmc_data *pdata;
	int r;

	if (type < 0 || type >= MMC_END)
		return -EINVAL;

	pdata = malloc(sizeof(struct mmc_data));
	if (!pdata) {
		_E("malloc failed");
		return -errno;
	}

	if (option >= 0)
		pdata->option = option;
	if (devpath)
		pdata->devpath = strdup(devpath);
	r = pthread_create(&th, NULL, mmc_func[type].func, pdata);
	if (r != 0) {
		_E("pthread create failed");
		free(pdata->devpath);
		free(pdata);
		return -EPERM;
	}

	pthread_detach(th);
	return 0;
}

static int mmc_inserted(const char *devpath)
{
	int r;
	_I("MMC inserted : %s", devpath);
	mmc_curpath = strdup(devpath);
	r = mmc_make_thread(MMC_MOUNT, -1, devpath);
	if (r < 0)
		return r;
	return 0;
}

static int mmc_removed(void)
{
	_I("MMC removed");
	/* unmount */
	mmc_check_and_unmount((const char *)MMC_MOUNT_POINT);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	free(mmc_curpath);
	mmc_curpath = NULL;
	return 0;
}

static int mmc_booting_done(void* data)
{
	char devpath[NAME_MAX] = {0,};
	int r;

	/* check mmc exists */
	r = find_mmc_node(devpath);
	if (r < 0)
		return 0;

	/* if MMC exists */
	return mmc_inserted(devpath);
}

static void uevent_block_handler(struct udev_device *dev)
{
	const char *devpath;
	const char *action;
	const char *devnode;

	devpath = udev_device_get_devpath(dev);
	if (fnmatch(MMC_PATH, devpath, 0))
		return;

	action = udev_device_get_action(dev);
	devnode = udev_device_get_devnode(dev);
	if (!action || !devnode)
		return;

	_D("mmc action : %s", action);
	if (!strncmp(action, UDEV_ADD, sizeof(UDEV_ADD)))
		mmc_inserted(devnode);
	else if (!strncmp(action, UDEV_REMOVE, sizeof(UDEV_REMOVE)))
		mmc_removed();
}

static DBusMessage *edbus_request_mount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	struct mmc_data *pdata;
	int ret;

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	pdata = malloc(sizeof(struct mmc_data));
	if (!pdata) {
		_E("malloc failed");
		ret = -errno;
		goto error;
	}

	pdata->devpath = strdup(mmc_curpath);
	if (!pdata->devpath) {
		free(pdata);
		ret = -errno;
		goto error;
	}

	ret = (int)mount_start(pdata);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_unmount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;
	char params[NAME_MAX];
	struct mmc_data *pdata;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	pdata = malloc(sizeof(struct mmc_data));
	if (!pdata) {
		_E("malloc failed");
		ret = -errno;
		goto error;
	}

	pdata->option = opt;
	ret = (int)unmount_start(pdata);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_format(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;
	struct mmc_data *pdata;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	if (!mmc_curpath) {
		ret = -ENODEV;
		goto error;
	}

	pdata = malloc(sizeof(struct mmc_data));
	if (!pdata) {
		_E("malloc failed");
		ret = -errno;
		goto error;
	}

	pdata->option = opt;
	pdata->devpath = strdup(mmc_curpath);
	if (!pdata->devpath) {
		free(pdata);
		ret = -errno;
		goto error;
	}

	ret = (int)format_start(pdata);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_insert(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char *devpath;
	int ret;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &devpath, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}

	ret = mmc_inserted(devpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_remove(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = mmc_removed();

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_change_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int opt, ret;
	char params[NAME_MAX];
	struct mmc_data *pdata;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &opt, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EBADMSG;
		goto error;
	}
	if (opt == VCONFKEY_SYSMAN_MMC_MOUNTED)
		mmc_mount_done();
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

int get_mmc_devpath(char devpath[])
{
	if (mmc_disabled)
		return -EWOULDBLOCK;
	if (!mmc_curpath)
		return -ENODEV;
	snprintf(devpath, NAME_MAX, "%s", mmc_curpath);
	return 0;
}

static struct uevent_handler uh = {
	.subsystem = BLOCK_SUBSYSTEM,
	.uevent_func = uevent_block_handler,
};

static const struct edbus_method edbus_methods[] = {
	{ "RequestMount",         NULL, "i", edbus_request_mount },
	{ "RequestUnmount",        "i", "i", edbus_request_unmount },
	{ "RequestFormat",         "i", "i", edbus_request_format },
	{ "RequestInsert",         "s", "i", edbus_request_insert },
	{ "RequestRemove",        NULL, "i", edbus_request_remove },
	{ "ChangeStatus",          "i", "i", edbus_change_status },
};

static void mmc_init(void *data)
{
	int ret;

	mmc_load_config();
	ret = register_edbus_interface_and_method(DEVICED_PATH_MMC,
			DEVICED_INTERFACE_MMC,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	/* register mmc uevent control routine */
	ret = register_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register extcon uevent : %d", ret);

	/* register notifier if mmc exist or not */
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, mmc_booting_done);
}

static void mmc_exit(void *data)
{
	int ret;

	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, mmc_booting_done);

	/* unregister mmc uevent control routine */
	ret = unregister_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister extcon uevent : %d", ret);
}

static int mmc_start(enum device_flags flags)
{
	mmc_disabled = false;
	_I("start");
	return 0;
}

static int mmc_stop(enum device_flags flags)
{
	mmc_disabled = true;
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	_I("stop");
	return 0;
}

const struct device_ops mmc_device_ops = {
	.name     = "mmc",
	.init     = mmc_init,
	.exit     = mmc_exit,
	.start    = mmc_start,
	.stop     = mmc_stop,
};

DEVICE_OPS_REGISTER(&mmc_device_ops)
