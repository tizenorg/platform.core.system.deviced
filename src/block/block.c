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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <vconf.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <tzplatform_config.h>

#include "core/log.h"
#include "core/device-notifier.h"
#include "core/devices.h"
#include "core/udev.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "block.h"

/**
 * TODO  Assumed root device is always mmcblk0*.
 * Should be fixed.
 */
#define MMC_PATH            "*/mmcblk[1-9]*"
#define MMC_PARTITION_PATH  "mmcblk[1-9]p[0-9]"
#define SCSI_PATH           "*/sd[a-z]"

#define MMC_MOUNT_POINT      tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard")
#define SCSI_MOUNT_POINT     tzplatform_mkpath(TZ_SYS_STORAGE,"usb")

#define SMACKFS_MAGIC	0x43415d53
#define SMACKFS_MNT		"/smack"

#ifndef ST_RDONLY
#define ST_RDONLY		0x0001
#endif

#define MMC_32GB_SIZE	61315072
#define FORMAT_RETRY	3
#define UNMOUNT_RETRY	5

enum block_operation {
	BLOCK_MOUNT,
	BLOCK_UNMOUNT,
	BLOCK_FORMAT,
	BLOCK_REMOVE,
};

static struct unmount_data {
	struct block_data *data;
	enum unmount_operation option;
};

static dd_list *fs_head;
static dd_list *block_dev_head;
static dd_list *block_data_list;
static bool smack = false;

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

void add_block_dev(const struct block_dev_ops *dev)
{
	DD_LIST_APPEND(block_dev_head, (void *)dev);
}

void remove_block_dev(const struct block_dev_ops *dev)
{
	DD_LIST_REMOVE(block_dev_head, (void *)dev);
}

static void broadcast_block_info(enum block_operation op,
		struct block_data *data, int result)
{
	struct block_dev_ops *dev;
	dd_list *elem;

	assert(data);

	DD_LIST_FOREACH(block_dev_head, elem, dev) {
		if (dev->type != data->type)
			continue;
		if (op == BLOCK_MOUNT)
			dev->mount(data, result);
		else if (op == BLOCK_UNMOUNT)
			dev->unmount(data, result);
		else if (op == BLOCK_FORMAT)
			dev->format(data, result);
	}
}

static int create_mount_point(const char *devpath, char *path, int len)
{
	const char *parent;
	char str[32];
	int cnt;

	if (!devpath || !path || len <= 0)
		return -EINVAL;

	if (!fnmatch(MMC_PATH, devpath, 0)) {
		parent = MMC_MOUNT_POINT;
		sscanf(devpath, "/dev/mmcblk%s", str);
		/* TODO will be removed.
		 * The storage directory policy is not fixed yet.
		 * So keep the original policy.
		 * The basic mount point is /usr/storage/sdcard. */
		cnt = DD_LIST_LENGTH(block_data_list);
		if (cnt == 0)
			memset(str, 0, sizeof(str));
	} else if (!fnmatch(SCSI_PATH, devpath, 0)) {
		parent = SCSI_MOUNT_POINT;
		sscanf(devpath, "/dev/sd%s", str);
	} else
		return -EINVAL;

	snprintf(path, len, "%s%s", parent, str);
	_D("mount point : %s(%s)", path, devpath);
	return 0;	
}

/* Whole data in struct block_data should be freed. */
static struct block_data *make_block_data(const char *devpath)
{
	struct block_data *data;
	char str[PATH_MAX];
	int ret;

	assert(devpath);

	data = calloc(1, sizeof(struct block_data));
	if (!data)
		return NULL;

	ret = create_mount_point(devpath, str, sizeof(str));
	if (ret < 0)
		return NULL;

	data->devpath = strdup(devpath);
	data->mount_point = strdup(str);
	if (!fnmatch(MMC_PATH, devpath, 0))
		data->type = BLOCK_MMC_DEV;
	else if (!fnmatch(SCSI_PATH, devpath, 0))
		data->type = BLOCK_SCSI_DEV;
	else {
		free(data);
		data = NULL;
	}

	return data;
}

static struct block_data *find_block_data(const char *devpath)
{
	struct block_data *data;
	dd_list *elem;

	DD_LIST_FOREACH(block_data_list, elem, data) {
		if (!data->deleted &&
		    !strncmp(data->devpath, devpath, strlen(devpath)+1))
			return data;
	}

	return NULL;
}

static void free_block_data(struct block_data *data)
{
	if (!data)
		return;
	free(data->devpath);
	free(data->mount_point);
	free(data);
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

static int block_mount(const char *devpath, const char *mount_point)
{
	struct mmc_fs_ops *fs;
	dd_list *elem;
	char path[NAME_MAX] = {0,};
	int r;

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

	/* give a transmutable attribute to mount_point */
	r = setxattr(MMC_MOUNT_POINT, "security.SMACK64TRANSMUTE", "TRUE", strlen("TRUE"), 0);
	if (r < 0)
		_E("setxattr error : %s", strerror(errno));

	return 0;
}

static void *mount_start(void *arg)
{
	struct block_data *data = (struct block_data *)arg;
	struct storage_info *info;
	char *devpath;
	char *mount_point;
	int r;

	assert(data);
	assert(data->devpath);
	assert(data->mount_point);

	devpath = data->devpath;
	mount_point = data->mount_point;

	_D("devpath : %s mount_point : %s", devpath, mount_point);
	/* clear previous filesystem */
	mmc_check_and_unmount(mount_point);

	/* check mount point */
	if (access(mount_point, R_OK) != 0) {
		if (mkdir(mount_point, 0755) < 0) {
			r = -errno;
			goto error;
		}
	}

	/* mount operation */
	r = block_mount(devpath, mount_point);
	if (r < 0)
		_E("fail to mount %s device : %s", devpath, strerror(-r));

error:
	/* broadcast to block devices */
	broadcast_block_info(BLOCK_MOUNT, data, r);

	/* check unmountable */
	if (r < 0)
		data->deleted = true;
	
	_I("%s result : %d", __func__, r);
	return NULL;
}

static int block_unmount(int option, const char *mount_point)
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
	struct unmount_data *udata = (struct unmount_data *)arg;
	struct block_data *data;
	char *mount_point;
	int r;

	assert(udata);
	assert(udata->data);

	data = udata->data;
	assert(data->devpath);
	assert(data->mount_point);

	mount_point = data->mount_point;

	/* TODO Should be fixed.... */
	if (data->type == BLOCK_MMC_DEV)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
				VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);

	r = block_unmount(UNMOUNT_FORCE, mount_point);
	if (r < 0)
		_E("fail to unmount %s device : %s", data->devpath, strerror(-r));

	/* broadcast to block devices */
	broadcast_block_info(BLOCK_UNMOUNT, data, r);

	/* check unmounted */
	if (r == 0)
		data->deleted = true;
	free(udata);

	_I("%s result : %d", __func__, r);
	return NULL;
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
	struct unmount_data *udata = (struct unmount_data *)arg;
	struct block_data *data;
	char *devpath;
	char *mount_point;
	int r, key = VCONFKEY_SYSMAN_MMC_MOUNTED;

	assert(udata);
	assert(udata->data);

	data = udata->data;
	assert(data->devpath);
	assert(data->mount_point);

	devpath = data->devpath;
	mount_point = data->mount_point;

	_I("Format Start");
	r = block_unmount(udata->option, mount_point);
	if (r < 0) {
		_E("fail to unmount %s device : %s", devpath, strerror(-r));
		goto error;
	}

	/* TODO should be fixed... */
	if (data->type == BLOCK_MMC_DEV)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS,
				VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NOW);
	r = format(devpath);
	/* TODO should be fixed... */
	if (data->type == BLOCK_MMC_DEV)
		vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS,
				VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NONE);

	block_mount(devpath, mount_point);
	if (r < 0)
		_E("fail to format %s device : %s", devpath, strerror(-r));

error:
	/* broadcast to block devices */
	broadcast_block_info(BLOCK_FORMAT, data, r);

	free(udata);

	_I("%s result : %d", __func__, r);
	return NULL;
}

int mount_block_device(const char *devpath)
{
	struct block_data *data;
	pthread_t th;
	int r;

	_I("request to mount %s block device", devpath);

	/* check if the device is already mounted */
	data = find_block_data(devpath);
	if (data) {
		_E("%s is already mounted on %s", devpath, data->mount_point);
		return -EEXIST;
	}

	data = make_block_data(devpath);
	if (!data) {
		_E("fail to make block data for %s", devpath);
		return -EPERM;
	}

	/**
	 * Add the data into list first.
	 * If the mount is failed, thread will mark the deleted value of the data.
	 * In idle state, the deleted item will be freed.
	 */
	DD_LIST_APPEND(block_data_list, data);

	r = pthread_create(&th, NULL, mount_start, data);
	if (r != 0) {
		_E("fail to create pthread for %s", devpath);
		data->deleted = true;
		return -EPERM;
	}
	
	pthread_detach(th);
	return 0;
}

int unmount_block_device(const char *devpath, enum unmount_operation option)
{
	struct block_data *data;
	struct unmount_data *udata;

	_I("request to unmount %s block device", devpath);

	data = find_block_data(devpath);
	if (!data) {
		_E("fail to find block data for %s", devpath);
		return -ENODEV;
	}

	udata = malloc(sizeof(struct unmount_data));
	if (!udata) {
		_E("fail to allocate unmount data for %s", devpath);
		return -errno;
	}

	udata->data = data;
	udata->option = option;

	return unmount_start(udata);
}

int format_block_device(const char *devpath, enum unmount_operation option)
{
	struct block_data *data;
	struct unmount_data *udata;
	pthread_t th;
	int r;

	_I("request to format %s block device", devpath);

	data = find_block_data(devpath);
	if (!data) {
		_E("fail to find block data for %s", devpath);
		return -ENODEV;
	}

	udata = malloc(sizeof(struct unmount_data));
	if (!udata) {
		_E("fail to allocate unmount data for %s", devpath);
		return -errno;
	}

	udata->data = data;
	udata->option = option;

	r = pthread_create(&th, NULL, format_start, udata);
	if (r != 0) {
		_E("fail to create pthread for %s", devpath);
		free(udata);
		return -EPERM;
	}

	pthread_detach(th);
	return 0;
}

int add_block_device(const char *devpath)
{
	return mount_block_device(devpath);
}

int remove_block_device(const char *devpath)
{
	struct block_data *data;
	int r;

	_I("request to remove %s block device", devpath);

	data = find_block_data(devpath);
	if (!data) {
		_E("fail to find block data for %s", devpath);
		return -ENODEV;
	}

	r = block_unmount(UNMOUNT_NORMAL, data->mount_point);
	if (r < 0) {
		_E("fail to unmount %s device : %s", data->devpath, strerror(-r));
		return r;
	}

	/* broadcast to block devices */
	broadcast_block_info(BLOCK_REMOVE, data, r);

	/* check deleted */
	data->deleted = true;

	return 0;
}

static bool has_partition(const char *syspath)
{
	DIR *dp;
	struct dirent *dir;
	bool find = false;

	dp = opendir(syspath);
	if (!dp) {
		_E("fail to open %s", syspath);
		return find;
	}

	while ((dir = readdir(dp)) != NULL) {
		if (!fnmatch(MMC_PARTITION_PATH, dir->d_name, 0)) {
			find = true;
			break;
		}
	}

	closedir(dp);
	return find;
}

static int block_init_from_udev_enumerate(void)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *list_entry;
	struct udev_device *dev;
	const char *syspath;
	const char *devnode;
	const char *devtype;
	bool partition;
	int ret;

	udev = udev_new();
	if (!udev) {
		_E("fail to create udev library context");
		return -EPERM;
	}

	/* create a list of the devices in the 'usb' subsystem */
	enumerate = udev_enumerate_new(udev);
	if (!enumerate) {
		_E("fail to create an enumeration context");
		return -EPERM;
	}

	udev_enumerate_add_match_subsystem(enumerate, BLOCK_SUBSYSTEM);
	udev_enumerate_add_match_property(enumerate,
			UDEV_DEVTYPE, BLOCK_DEVTYPE_DISK);
	udev_enumerate_add_match_property(enumerate,
			UDEV_DEVTYPE, BLOCK_DEVTYPE_PARTITION);
	udev_enumerate_scan_devices(enumerate);

	udev_list_entry_foreach(list_entry,
			udev_enumerate_get_list_entry(enumerate)) {
		syspath = udev_list_entry_get_name(list_entry);
		if (!syspath)
			continue;

		dev = udev_device_new_from_syspath(udev_enumerate_get_udev(enumerate),
				syspath);
		if (!dev)
			continue;

		if (fnmatch(MMC_PATH, syspath, 0) &&
			fnmatch(SCSI_PATH, syspath, 0))
			continue;

		devtype = udev_device_get_devtype(dev);
		devnode = udev_device_get_devnode(dev);
		if (!devtype || !devnode)
			continue;

		/* in case of block devtype, check the partition count */
		if (!strncmp(devtype, BLOCK_DEVTYPE_DISK, sizeof(BLOCK_DEVTYPE_DISK))) {
			/* check if there is partition */
			partition = has_partition(syspath);
			if (partition)
				continue;
			/* if there is no partition, try to mount */
			_I("%s device has no partition", devnode);
		}

		_D("%s(%s) device add", devnode, syspath);
		add_block_device(devnode);

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return 0;
}

static int booting_done(void* data)
{
	_E("%s", __func__);
	/* if there is the attached device, try to mount */
	block_init_from_udev_enumerate();
	return 0;
}

static void show_block_device_list(void)
{
	struct block_data *data;
	dd_list *elem;
	int cnt = 0;

	_D("     devpath\t mount point\t deleted");
	DD_LIST_FOREACH(block_data_list, elem, data) {
		_D("[%2d] %s\t %s\t %d",
				cnt++, data->devpath, data->mount_point, data->deleted);
	}
}

static int remove_unmountable_blocks(void *user_data)
{
	struct block_data *data;
	dd_list *elem;
	dd_list *next;

	DD_LIST_FOREACH_SAFE(block_data_list, elem, next, data) {
		if (data->deleted) {
			DD_LIST_REMOVE_LIST(block_data_list, elem);
			free(data->devpath);
			free(data->mount_point);
			free(data);
		}
	}
	return 0;
}

static void uevent_block_handler(struct udev_device *dev)
{
	const char *action;
	const char *devnode;
	const char *devtype;
	const char *syspath;
	bool partition;
	struct block_data *data;

	syspath = udev_device_get_syspath(dev);
	if (!syspath)
		return;

	if (fnmatch(MMC_PATH, syspath, 0) &&
	    fnmatch(SCSI_PATH, syspath, 0))
		return;

	action = udev_device_get_action(dev);
	devnode = udev_device_get_devnode(dev);
	devtype = udev_device_get_devtype(dev);
	if (!action || !devtype || !devnode)
		return;

	_D("%s(%s) device %s", devnode, syspath, action);
	if (!strncmp(action, UDEV_ADD, sizeof(UDEV_ADD))) {
		/* in case of block devtype, check the partition counts */
		if (!strncmp(devtype, BLOCK_DEVTYPE_DISK,
					sizeof(BLOCK_DEVTYPE_DISK))) {
			/* check if there is partition */
			partition = has_partition(syspath);
			if (partition)
				return;
			/* if there is no partition, try to mount */
			_I("%s device has no partition", devnode);
		}
		add_block_device(devnode);
	} else if (!strncmp(action, UDEV_REMOVE, sizeof(UDEV_REMOVE)))
		remove_block_device(devnode);

	/* add idler queue to remove unmountable block datas */
	add_idle_request(remove_unmountable_blocks, NULL);
}

static DBusMessage *request_show_device_list(E_DBus_Object *obj, DBusMessage *msg)
{
	show_block_device_list();
	return dbus_message_new_method_return(msg);
}

static struct uevent_handler uh = {
	.subsystem = BLOCK_SUBSYSTEM,
	.uevent_func = uevent_block_handler,
};

static const struct edbus_method edbus_methods[] = {
	{ "ShowDeviceList", NULL, NULL, request_show_device_list },
};

static void block_init(void *data)
{
	struct block_dev_ops *dev;
	dd_list *elem;
	int ret;

	/* invoke init function in block_dev_heads */
	DD_LIST_FOREACH(block_dev_head, elem, dev) {
		if (dev->init)
			dev->init(NULL);
	}
	
	ret = register_edbus_interface_and_method(DEVICED_PATH_BLOCK,
			DEVICED_INTERFACE_BLOCK,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	/* register mmc uevent control routine */
	ret = register_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register extcon uevent : %d", ret);

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static void block_exit(void *data)
{
	int ret;

	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);

	/* unregister mmc uevent control routine */
	ret = unregister_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister extcon uevent : %d", ret);
}

const struct device_ops block_device_ops = {
	.name     = "block",
	.init     = block_init,
	.exit     = block_exit,
};

DEVICE_OPS_REGISTER(&block_device_ops)
