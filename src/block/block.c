/*
 * deviced
 *
 * Copyright (c) 2012 - 2015 Samsung Electronics Co., Ltd.
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
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <errno.h>
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
 * TODO  Assume root device is always mmcblk0*.
 */
#define MMC_PATH            "*/mmcblk[1-9]*"
#define MMC_PARTITION_PATH  "mmcblk[1-9]p[0-9]"
#define SCSI_PATH           "*/sd[a-z]"
#define SCSI_PARTITION_PATH "sd[a-z][0-9]"

#define UNMOUNT_RETRY	5

struct block_device {
	int deleted;
	pthread_mutex_t mutex;
	struct block_data *data;
};

enum unmount_operation {
	UNMOUNT_NORMAL,
	UNMOUNT_FORCE,
};

struct unmount_data {
	struct block_device *bdev;
	enum unmount_operation option;
};

static dd_list *fs_head;
static dd_list *block_dev_list;
static bool smack = false;

static void __CONSTRUCTOR__ smack_check(void)
{
	FILE *fp;
	char *buf;
	size_t len;
	ssize_t num;

	fp = fopen("/proc/filesystems", "r");
	if (!fp)
		return;

	__fsetlocking(fp, FSETLOCKING_BYCALLER);

	num = getline(&buf, &len, fp);
	while (num != -1) {
		if (strstr(buf, "smackfs")) {
			smack = true;
			break;
		}
		num = getline(&buf, &len, fp);
	}

	free(buf);
	fclose(fp);
	return;
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

/* Whole data in struct block_data should be freed. */
static struct block_data *make_block_data(const char *devnode,
		const char *fs_usage,
		const char *fs_type,
		const char *fs_version,
		const char *fs_uuid_enc,
		const char *readonly)
{
	struct block_data *data;
	const char *str;

	data = calloc(1, sizeof(struct block_data));
	if (!data)
		return NULL;

	if (devnode)
		data->devnode = strdup(devnode);
	if (fs_usage)
		data->fs_usage = strdup(fs_usage);
	if (fs_type)
		data->fs_type = strdup(fs_type);
	if (fs_version)
		data->fs_version = strdup(fs_version);
	if (fs_uuid_enc) {
		data->fs_uuid_enc = strdup(fs_uuid_enc);
		str = tzplatform_mkpath(TZ_SYS_STORAGE, fs_uuid_enc);
		if (str)
			data->mount_point = strdup(str);
	}
	if (readonly)
		data->readonly = atoi(readonly);

	/* TODO should we know block dev type? */
	if (!fnmatch(MMC_PATH, devnode, 0))
		data->block_type = BLOCK_MMC_DEV;
	else if (!fnmatch(SCSI_PATH, devnode, 0))
		data->block_type = BLOCK_SCSI_DEV;

	return data;
}

static void free_block_data(struct block_data *data)
{
	if (!data)
		return;
	free(data->devnode);
	free(data->fs_usage);
	free(data->fs_type);
	free(data->fs_version);
	free(data->fs_uuid_enc);
	free(data->mount_point);
	free(data);
}

static struct block_device *make_block_device(void)
{
	struct block_device *bdev;

	bdev = calloc(1, sizeof(struct block_device));
	if (!bdev)
		return NULL;

	return bdev;
}

static int set_block_device(struct block_device **bdev,
		struct block_data *data)
{
	struct block_device *dev = *bdev;

	if (!bdev || !*bdev || !data)
		return -EINVAL;

	dev->data = data;
	return 0;
}

static struct block_device *find_block_device(const char *devnode)
{
	struct block_device *bdev;
	dd_list *elem;
	int len;

	len = strlen(devnode) + 1;
	DD_LIST_FOREACH(block_dev_list, elem, bdev) {
		if (!bdev->deleted &&
		    bdev->data &&
		    !strncmp(bdev->data->devnode, devnode, len))
			return bdev;
	}

	return NULL;
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

static int rw_mount(const char *szPath)
{
	struct statvfs mount_stat;
	if (!statvfs(szPath, &mount_stat)) {
		if ((mount_stat.f_flag & ST_RDONLY) == ST_RDONLY)
			return -1;
	}
	return 0;
}

static int block_mount(struct block_data *data)
{
	static const char *filesystem = "filesystem";
	struct mmc_fs_ops *fs;
	dd_list *elem;
	int r;
	int len;

	if (!data || !data->devnode || !data->mount_point)
		return -EINVAL;

	/* check existing mounted */
	if (mount_check(data->mount_point))
		return -EEXIST;

	/* create mount point */
	if (access(data->mount_point, R_OK) != 0) {
		if (mkdir(data->mount_point, 0755) < 0)
			return -errno;
	}

	/* check matched file system */
	fs = NULL;
	if (data->fs_usage &&
	    !strncmp(data->fs_usage, filesystem,
			strlen(filesystem) + 1)) {
		if (data->fs_type) {
			len = strlen(data->fs_type) + 1;
			DD_LIST_FOREACH(fs_head, elem, fs) {
				if (!strncmp(fs->name, data->fs_type, len))
					break;
			}
		}
	}

	if (!fs) {
		r = -ENODEV;
		goto out;
	}

	r = fs->mount(smack, data->devnode, data->mount_point);
	if (r < 0)
		goto out;

	r = rw_mount(data->mount_point);
	if (r < 0) {
		r = -EROFS;
		goto out;
	}

	return 0;

out:
	rmdir(data->mount_point);
	return r;
}

/* runs in thread */
static void *mount_start(void *arg)
{
	struct block_device *bdev = (struct block_device *)arg;
	struct block_data *data;
	struct storage_info *info;
	int r;

	assert(bdev);
	assert(bdev->data);

	data = bdev->data;
	_I("Mount Start : (%s -> %s)",
			data->devnode, data->mount_point);

	if (data->state == BLOCK_MOUNT) {
		_I("%s is already mounted", data->devnode);
		r = 0;
		goto out;
	}

	/* mount operation */
	r = block_mount(data);
	if (r != -EROFS && r < 0) {
		_E("fail to mount %s device : %d", data->devnode, r);
		goto out;
	}

	pthread_mutex_lock(&(bdev->mutex));
	if (r == -EROFS)
		data->readonly = true;
	data->state = BLOCK_MOUNT;
	pthread_mutex_unlock(&(bdev->mutex));

out:
	_I("%s result : %s, %d", __func__, data->devnode, r);
	return NULL;
}

static int block_unmount(struct block_data *data,
	   enum unmount_operation option)
{
	int r, retry = 0;

	if (!data || !data->mount_point)
		return -EINVAL;

	/* it must called before unmounting mmc */
	r = mmc_check_and_unmount(data->mount_point);
	if (!r)
		goto out;
	if (option == UNMOUNT_NORMAL) {
		_I("Failed to unmount with normal option : %d", r);
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
			terminate_process(data->mount_point, false);
			break;
		case 2:
			/* Last time, kill app with SIGKILL */
			_I("Kill app with SIGKILL");
			terminate_process(data->mount_point, true);
			break;
		default:
			if (umount2(data->mount_point, MNT_DETACH) != 0) {
				_I("Failed to unmount with lazy option : %d", errno);
				return -errno;
			}
			goto out;
		}

		/* it takes some seconds til other app completely clean up */
		usleep(500*1000);

		r = mmc_check_and_unmount(data->mount_point);
		if (!r)
			break;
	}

out:
	if (rmdir(data->mount_point) < 0)
		_E("fail to remove %s directory", data->mount_point);

	return r;
}

static void *unmount_start(void *arg)
{
	struct unmount_data *udata = (struct unmount_data *)arg;
	struct block_device *bdev;
	struct block_data *data;
	int r;

	assert(udata);
	assert(udata->bdev);
	assert(udata->bdev->data);

	bdev = udata->bdev;
	data = bdev->data;
	_I("Unmount Start : (%s -> %s)",
			data->devnode, data->mount_point);

	if (data->state == BLOCK_UNMOUNT) {
		_I("%s is already unmounted", data->devnode);
		r = 0;
		goto out;
	}

	r = block_unmount(data, udata->option);
	if (r < 0) {
		_E("fail to unmount %s device : %d", data->devnode, r);
		goto out;
	}

	pthread_mutex_lock(&(bdev->mutex));
	data->state = BLOCK_UNMOUNT;
	pthread_mutex_unlock(&(bdev->mutex));

out:
	free(udata);

	_I("%s result : %s, %d", __func__, data->devnode, r);
	return NULL;
}

static int mount_block_device(struct block_device *bdev)
{
	pthread_t th;
	int r;

	if (!bdev && !bdev->data)
		return -EINVAL;

	r = pthread_create(&th, NULL, mount_start, bdev);
	if (r != 0)
		return -EPERM;

	pthread_detach(th);
	return 0;
}

static int unmount_block_device(struct block_device *bdev,
		enum unmount_operation option)
{
	struct unmount_data *udata;

	if (!bdev && !bdev->data)
		return -EINVAL;

	udata = malloc(sizeof(struct unmount_data));
	if (!udata)
		return -errno;

	udata->bdev = bdev;
	udata->option = option;

	unmount_start(udata);
	return 0;
}

static bool disk_is_partitioned_by_kernel(struct udev_device *dev)
{
	DIR *dp;
	struct dirent *dir;
	const char *syspath;
	bool ret = false;

	syspath = udev_device_get_syspath(dev);
	if (!syspath)
		goto out;

	dp = opendir(syspath);
	if (!dp) {
		_E("fail to open %s", syspath);
		goto out;
	}

	/* TODO compare devname and d_name */
	while ((dir = readdir(dp)) != NULL) {
		if (!fnmatch(MMC_PARTITION_PATH, dir->d_name, 0) ||
		    !fnmatch(SCSI_PARTITION_PATH, dir->d_name, 0)) {
			ret = true;
			break;
		}
	}

	closedir(dp);

out:
	return ret;
}

static bool check_partition(struct udev_device *dev)
{
	static const char *fs = "filesystem";
	const char *devtype;
	const char *part_table_type;
	const char *fs_usage;
	bool ret = false;

	/* only consider disk type, never partitions */
	devtype = udev_device_get_devtype(dev);
	if (!devtype)
		goto out;

	if (strncmp(devtype, BLOCK_DEVTYPE_DISK,
				sizeof(BLOCK_DEVTYPE_DISK)) != 0)
		goto out;

	part_table_type = udev_device_get_property_value(dev,
			"ID_PART_TABLE_TYPE");
	if (part_table_type) {
		fs_usage = udev_device_get_property_value(dev,
				"ID_FS_USAGE");
		if (fs_usage && strncmp(fs_usage, fs, strlen(fs) + 1) == 0) {
			if (!disk_is_partitioned_by_kernel(dev))
					goto out;
		}
		ret = true;
		goto out;
	}

	if (disk_is_partitioned_by_kernel(dev)) {
		ret = true;
		goto out;
	}

out:
	return ret;
}

static int add_block_device(struct udev_device *dev, const char *devnode)
{
	struct block_data *data;
	struct block_device *bdev;
	bool partition;
	int ret;

	partition = check_partition(dev);
	if (partition) {
		/* if there is a partition, skip this request */
		_I("%s device has partitions, skip this time", devnode);
		return 0;
	}

	data = make_block_data(devnode,
			udev_device_get_property_value(dev, "ID_FS_USAGE"),
			udev_device_get_property_value(dev, "ID_FS_TYPE"),
			udev_device_get_property_value(dev, "ID_FS_VERSION"),
			udev_device_get_property_value(dev, "ID_FS_UUID_ENC"),
			udev_device_get_sysattr_value(dev, "ro"));
	if (!data) {
		_E("fail to make block data for %s", devnode);
		return -EPERM;
	}

	bdev = make_block_device();
	if (!bdev) {
		_E("fail to make block device for %s", devnode);
		free_block_data(data);
		return -EPERM;
	}

	set_block_device(&bdev, data);

	/**
	 * Add the data into list.
	 * The block data keep until the devnode is removed.
	 */
	DD_LIST_APPEND(block_dev_list, bdev);

	ret = mount_block_device(bdev);
	if (ret < 0) {
		_E("fail to mount block device for %s", devnode);
		return ret;
	}

	return 0;
}

static int remove_block_device(struct udev_device *dev, const char *devnode)
{
	struct block_device *bdev;
	int r;

	bdev = find_block_device(devnode);
	if (!bdev) {
		_E("fail to find block data for %s", devnode);
		return -ENODEV;
	}

	r = unmount_block_device(bdev, UNMOUNT_NORMAL);
	if (r < 0)
		_E("fail to unmount block device for %s", devnode);

	/* check deleted */
	while (pthread_mutex_trylock(&(bdev->mutex)) != 0)
		usleep(1);
	bdev->deleted = true;
	pthread_mutex_unlock(&(bdev->mutex));

	return 0;
}

static int block_init_from_udev_enumerate(void)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *list_entry;
	struct udev_device *dev;
	const char *syspath;
	const char *devnode;
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

		devnode = udev_device_get_devnode(dev);
		if (!devnode)
			continue;

		_D("%s device add", devnode);
		add_block_device(dev, devnode);

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return 0;
}

static int booting_done(void* data)
{
	/* if there is the attached device, try to mount */
	block_init_from_udev_enumerate();
	return 0;
}

static void show_block_device_list(void)
{
	struct block_device *bdev;
   struct block_data *data;
	dd_list *elem;

	DD_LIST_FOREACH(block_dev_list, elem, bdev) {
		data = bdev->data;
		if (!data)
			continue;
		_D("%s:", data->devnode);
		_D("\tBlock type: %s",
				(data->block_type == BLOCK_MMC_DEV ? "mmc" : "scsi"));
		_D("\tFs type: %s", data->fs_type);
		_D("\tFs usage: %s", data->fs_usage);
		_D("\tFs version: %s", data->fs_version);
		_D("\tFs uuid enc: %s", data->fs_uuid_enc);
		_D("\tReadonly: %s",
				(data->readonly ? "true" : "false"));
		_D("\tMount point: %s", data->mount_point);
		_D("\tMount state: %s",
				(data->state == BLOCK_MOUNT ? "mount" : "unmount"));
		_D("\tRemove: %s", (bdev->deleted ? "true" : "false"));
	}
}

static void remove_whole_block_device(void)
{
	struct block_device *bdev;
	struct block_data *data;
	dd_list *elem;
	dd_list *next;
	int r;

	DD_LIST_FOREACH_SAFE(block_dev_list, elem, next, bdev) {
		unmount_block_device(bdev, UNMOUNT_NORMAL);
		data = bdev->data;
		free_block_data(data);
		DD_LIST_REMOVE_LIST(block_dev_list, elem);
		free(bdev);
	}
}

static int remove_unmountable_blocks(void *user_data)
{
	struct block_device *bdev;
	struct block_data *data;
	dd_list *elem;
	dd_list *next;

	DD_LIST_FOREACH_SAFE(block_dev_list, elem, next, bdev) {
		if (bdev->deleted) {
			pthread_mutex_lock(&(bdev->mutex));
			data = bdev->data;
			free_block_data(data);
			pthread_mutex_unlock(&(bdev->mutex));
			DD_LIST_REMOVE_LIST(block_dev_list, elem);
			free(bdev);
		}
	}
	return 0;
}

static void uevent_block_handler(struct udev_device *dev)
{
	const char *devnode;
	const char *action;

	devnode = udev_device_get_devnode(dev);
	if (!devnode)
		return;

	if (fnmatch(MMC_PATH, devnode, 0) &&
	    fnmatch(SCSI_PATH, devnode, 0))
		return;

	action = udev_device_get_action(dev);
	if (!action)
		return;

	_D("%s device %s", devnode, action);
	if (!strncmp(action, UDEV_ADD, sizeof(UDEV_ADD)))
		add_block_device(dev, devnode);
	else if (!strncmp(action, UDEV_REMOVE, sizeof(UDEV_REMOVE)))
		remove_block_device(dev, devnode);

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

	ret = register_edbus_interface_and_method(DEVICED_PATH_BLOCK,
			DEVICED_INTERFACE_BLOCK,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	/* register mmc uevent control routine */
	ret = register_udev_uevent_control(&uh);
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
	ret = unregister_udev_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister extcon uevent : %d", ret);

	/* remove remaining blocks */
	remove_whole_block_device();
}

const struct device_ops block_device_ops = {
	.name     = "block",
	.init     = block_init,
	.exit     = block_exit,
};

DEVICE_OPS_REGISTER(&block_device_ops)
