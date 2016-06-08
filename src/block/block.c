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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <fnmatch.h>
#include <errno.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <vconf.h>
#include <ctype.h>
#include <tzplatform_config.h>

#include "core/log.h"
#include "core/config-parser.h"
#include "core/device-idler.h"
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
#define MMC_LINK_PATH       "*/sdcard/*"
#define SCSI_PATH           "*/sd[a-z]*"
#define SCSI_PARTITION_PATH "sd[a-z][0-9]"

#define FILESYSTEM          "filesystem"

#define DEV_PREFIX          "/dev/"

#define UNMOUNT_RETRY	5
#define TIMEOUT_MAKE_OBJECT 500 /* milliseconds */

#define BLOCK_DEVICE_ADDED      "DeviceAdded"
#define BLOCK_DEVICE_REMOVED    "DeviceRemoved"
#define BLOCK_DEVICE_BLOCKED    "DeviceBlocked"
#define BLOCK_DEVICE_CHANGED    "DeviceChanged"
#define BLOCK_DEVICE_CHANGED_2  "DeviceChanged2"

#define BLOCK_TYPE_MMC          "mmc"
#define BLOCK_TYPE_SCSI         "scsi"
#define BLOCK_TYPE_ALL          "all"

#define BLOCK_MMC_NODE_PREFIX   "SDCard"
#define BLOCK_SCSI_NODE_PREFIX  "USBDrive"

#define BLOCK_CONF_FILE         "/etc/deviced/block.conf"

/* Minimum value of block id */
#define BLOCK_ID_MIN 10

/* Maximum number of thread */
#define THREAD_MAX 5

enum block_dev_operation {
	BLOCK_DEV_MOUNT,
	BLOCK_DEV_UNMOUNT,
	BLOCK_DEV_FORMAT,
	BLOCK_DEV_INSERT,
	BLOCK_DEV_REMOVE,
	BLOCK_DEV_DEQUEUE,
};

struct operation_queue {
	enum block_dev_operation op;
	DBusMessage *msg;
	void *data;
	bool done;
};

struct block_device {
	struct block_data *data;
	dd_list *op_queue;
	int thread_id;		/* Current thread ID */
};

struct format_data {
	struct block_device *bdev;
	char *fs_type;
	enum unmount_operation option;
};

struct pipe_data {
	enum block_dev_operation op;
	struct block_device *bdev;
	int result;
};

static struct block_conf {
	bool multimount;
} block_conf[BLOCK_MMC_DEV + 1];

static struct manage_thread {
	dd_list *th_node_list;	/* list of devnode which thread dealt with. Only main thread access */
	pthread_t th;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int num_dev;		/* number of devices which thread holds. Only main thread access */
	int op_len;		/* number of operation of thread */
	int thread_id;
	bool start_th;
} th_manager[THREAD_MAX];

static dd_list *fs_head;
static dd_list *block_dev_list;
static dd_list *block_ops_list;
static bool smack;
static int pfds[2];
static Ecore_Fd_Handler *phandler;
static bool block_control = false;
static bool block_boot = false;

static pthread_mutex_t glob_mutex = PTHREAD_MUTEX_INITIALIZER;

static int add_operation(struct block_device *bdev,
		enum block_dev_operation operation,
		DBusMessage *msg, void *data);
static void remove_operation(struct block_device *bdev);

static void uevent_block_handler(struct udev_device *dev);
static struct uevent_handler uh = {
	.subsystem = BLOCK_SUBSYSTEM,
	.uevent_func = uevent_block_handler,
};

static void __CONSTRUCTOR__ smack_check(void)
{
	FILE *fp;
	char buf[128];

	fp = fopen("/proc/filesystems", "r");
	if (!fp)
		return;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strstr(buf, "smackfs")) {
			smack = true;
			break;
		}
	}

	fclose(fp);
}

void add_fs(const struct block_fs_ops *fs)
{
	DD_LIST_APPEND(fs_head, (void *)fs);
}

void remove_fs(const struct block_fs_ops *fs)
{
	DD_LIST_REMOVE(fs_head, (void *)fs);
}

const struct block_fs_ops *find_fs(enum block_fs_type type)
{
	struct block_fs_ops *fs;
	dd_list *elem;

	DD_LIST_FOREACH(fs_head, elem, fs) {
		if (fs->type == type)
			return fs;
	}
	return NULL;
}

void add_block_dev(const struct block_dev_ops *ops)
{
	DD_LIST_APPEND(block_ops_list, (void *)ops);
}

void remove_block_dev(const struct block_dev_ops *ops)
{
	DD_LIST_REMOVE(block_ops_list, (void *)ops);
}

static void broadcast_block_info(enum block_dev_operation op,
		struct block_data *data, int result)
{
	struct block_dev_ops *ops;
	dd_list *elem;

	DD_LIST_FOREACH(block_ops_list, elem, ops) {
		if (ops->block_type != data->block_type)
			continue;
		if (op == BLOCK_DEV_MOUNT)
			ops->mounted(data, result);
		else if (op == BLOCK_DEV_UNMOUNT)
			ops->unmounted(data, result);
		else if (op == BLOCK_DEV_FORMAT)
			ops->formatted(data, result);
		else if (op == BLOCK_DEV_INSERT)
			ops->inserted(data);
		else if (op == BLOCK_DEV_REMOVE)
			ops->removed(data);
	}
}

static int block_get_new_id(void)
{
	static int id = BLOCK_ID_MIN;
	struct block_device *bdev;
	dd_list *elem;
	bool found;
	int i;

	for (i = 0 ; i < INT_MAX ; i++) {
		found = false;
		DD_LIST_FOREACH(block_dev_list, elem, bdev) {
			if (bdev->data->id == id) {
				found = true;
				break;
			}
		}
		if (!found)
			return id++;

		if (++id == INT_MAX)
			id = BLOCK_ID_MIN;
	}

	return -ENOENT;
}

static void signal_device_blocked(struct block_device *bdev)
{
	struct block_data *data;
	char *arr[2];
	char *str_null = "";

	if (!bdev || !bdev->data)
		return;

	data = bdev->data;

	/* Broadcast outside with Block iface */
	arr[0] = (data->fs_uuid_enc ? data->fs_uuid_enc : str_null);
	arr[1] = (data->mount_point ? data->mount_point : str_null);

	broadcast_block_edbus_signal(DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER,
			BLOCK_DEVICE_BLOCKED,
			"ss", arr);
}

static void signal_device_changed(struct block_device *bdev,
		enum block_dev_operation op)
{
	struct block_data *data;
	char *arr[13];
	char str_block_type[32];
	char str_readonly[32];
	char str_state[32];
	char str_primary[32];
	char str_flags[32];
	char str_id[32];
	char *str_null = "";
	int flags;

	if (!bdev || !bdev->data)
		return;

	data = bdev->data;

	switch (op) {
	case BLOCK_DEV_MOUNT:
		BLOCK_GET_MOUNT_FLAGS(data, flags);
		break;
	case BLOCK_DEV_UNMOUNT:
		BLOCK_GET_UNMOUNT_FLAGS(data, flags);
		break;
	case BLOCK_DEV_FORMAT:
		BLOCK_GET_FORMAT_FLAGS(data, flags);
		break;
	default:
		flags = 0;
		break;
	}

	/* Broadcast outside with BlockManager iface */
	snprintf(str_block_type, sizeof(str_block_type),
			"%d", data->block_type);
	arr[0] = str_block_type;
	arr[1] = (data->devnode ? data->devnode : str_null);
	arr[2] = (data->syspath ? data->syspath : str_null);
	arr[3] = (data->fs_usage ? data->fs_usage : str_null);
	arr[4] = (data->fs_type ? data->fs_type : str_null);
	arr[5] = (data->fs_version ? data->fs_version : str_null);
	arr[6] = (data->fs_uuid_enc ? data->fs_uuid_enc : str_null);
	snprintf(str_readonly, sizeof(str_readonly),
			"%d", data->readonly);
	arr[7] = str_readonly;
	arr[8] = (data->mount_point ? data->mount_point : str_null);
	snprintf(str_state, sizeof(str_state),
			"%d", data->state);
	arr[9] = str_state;
	snprintf(str_primary, sizeof(str_primary),
			"%d", data->primary);
	arr[10] = str_primary;
	snprintf(str_flags, sizeof(str_flags), "%d", flags);
	arr[11] = str_flags;
	snprintf(str_id, sizeof(str_id), "%d", data->id);
	arr[12] = str_id;

	if (op == BLOCK_DEV_INSERT)
		broadcast_block_edbus_signal(DEVICED_PATH_BLOCK_MANAGER,
				DEVICED_INTERFACE_BLOCK_MANAGER,
				BLOCK_DEVICE_ADDED,
				"issssssisibii", arr);
	else if (op == BLOCK_DEV_REMOVE)
		broadcast_block_edbus_signal(DEVICED_PATH_BLOCK_MANAGER,
				DEVICED_INTERFACE_BLOCK_MANAGER,
				BLOCK_DEVICE_REMOVED,
				"issssssisibii", arr);
	else {
		broadcast_block_edbus_signal(DEVICED_PATH_BLOCK_MANAGER,
				DEVICED_INTERFACE_BLOCK_MANAGER,
				BLOCK_DEVICE_CHANGED,
				"issssssisibii", arr);
		broadcast_block_edbus_signal(DEVICED_PATH_BLOCK_MANAGER,
				DEVICED_INTERFACE_BLOCK_MANAGER,
				BLOCK_DEVICE_CHANGED_2,
				"issssssisibi", arr);
	}
}

static int get_mmc_mount_node(char *devnode, char *node, size_t len)
{
	char *name = devnode;
	int dev = -1, part = -1;
	char emul[32] = { 0, };
	int i;

	if (!name)
		return -EINVAL;

	/* Check Target */
	sscanf(name, "mmcblk%dp%d", &dev, &part);
	if (dev >= 0) {
		if (part < 0)
			snprintf(node, len, "%s%c", BLOCK_MMC_NODE_PREFIX, dev + 'A' - 1);
		else
			snprintf(node, len, "%s%c%d", BLOCK_MMC_NODE_PREFIX, dev + 'A' - 1, part);
		return 0;
	}

	/* Check Emulator */
	sscanf(name, "vd%s", emul);
	if (emul[0] == '\0')
		return -EINVAL;
	for (i = 0 ; i < strlen(emul) ; i++)
		emul[i] = toupper(emul[i]);
	snprintf(node, len, "%s%s", BLOCK_MMC_NODE_PREFIX, emul);
	return 0;
}

static int get_scsi_mount_node(char *devnode, char *node, size_t len)
{
	char dev[64], *name;
	int i;

	if (!devnode)
		return -EINVAL;

	snprintf(dev, sizeof(dev), "%s", devnode);

	if (!strstr(dev, "sd"))
		return -EINVAL;

	name = dev;
	name += strlen("sd");
	if (!name)
		return -EINVAL;

	for (i = 0 ; i < strlen(name) ; i++)
		name[i] = toupper(name[i]);
	snprintf(node, len, "%s%s", BLOCK_SCSI_NODE_PREFIX, name);

	return 0;
}

static char *generate_mount_path(struct block_data *data)
{
	const char *str;
	char *name, node[64];
	int ret;

	if (!data || !data->devnode)
		return NULL;

	name = strrchr(data->devnode, '/');
	if (!name)
		goto out;
	name++;

	switch (data->block_type) {
	case BLOCK_MMC_DEV:
		ret = get_mmc_mount_node(name, node, sizeof(node));
		break;
	case BLOCK_SCSI_DEV:
		ret = get_scsi_mount_node(name, node, sizeof(node));
		break;
	default:
		_E("Invalid block type (%d)", data->block_type);
		return NULL;
	}
	if (ret < 0)
		goto out;

	str = tzplatform_mkpath(TZ_SYS_MEDIA, node);
	if (!str)
		return NULL;
	return strdup(str);

out:
	_E("Invalid devnode (%s)", data->devnode ? data->devnode : "NULL");
	return NULL;
}

static bool check_primary_partition(const char *devnode)
{
	char str[PATH_MAX];
	char str2[PATH_MAX];
	int len;
	int i;

	/* if no partition */
	if (!fnmatch(MMC_LINK_PATH, devnode, 0) ||
		!fnmatch(MMC_PATH, devnode, 0) ||
		!fnmatch(SCSI_PATH, devnode, 0))
		return true;

	snprintf(str, sizeof(str), "%s", devnode);

	len = strlen(str);
	str[len - 1] = '\0';

	for (i = 1; i < 9; ++i) {
		snprintf(str2, sizeof(str2), "%s%d", str, i);
		if (access(str2, R_OK) == 0)
			break;
	}

	if (!strncmp(devnode, str2, strlen(str2) + 1))
		return true;

	return false;
}

/* Whole data in struct block_data should be freed. */
static struct block_data *make_block_data(const char *devnode,
		const char *syspath,
		const char *fs_usage,
		const char *fs_type,
		const char *fs_version,
		const char *fs_uuid_enc,
		const char *readonly)
{
	struct block_data *data;

	/* devnode is unique value so it should exist. */
	if (!devnode)
		return NULL;

	data = calloc(1, sizeof(struct block_data));
	if (!data)
		return NULL;

	data->devnode = strdup(devnode);
	if (syspath)
		data->syspath = strdup(syspath);
	if (fs_usage)
		data->fs_usage = strdup(fs_usage);
	if (fs_type)
		data->fs_type = strdup(fs_type);
	if (fs_version)
		data->fs_version = strdup(fs_version);
	if (fs_uuid_enc)
		data->fs_uuid_enc = strdup(fs_uuid_enc);
	if (readonly)
		data->readonly = atoi(readonly);
	data->primary = check_primary_partition(devnode);

	/* TODO should we know block dev type? */
	if (!fnmatch(MMC_LINK_PATH, devnode, 0))
		data->block_type = BLOCK_MMC_DEV;
	else if (!fnmatch(MMC_PATH, devnode, 0))
		data->block_type = BLOCK_MMC_DEV;
	else if (!fnmatch(SCSI_PATH, devnode, 0))
		data->block_type = BLOCK_SCSI_DEV;
	else
		data->block_type = -1;

	data->mount_point = generate_mount_path(data);
	BLOCK_FLAG_CLEAR_ALL(data);

	data->id = block_get_new_id();

	return data;
}

static void free_block_data(struct block_data *data)
{
	if (!data)
		return;
	free(data->devnode);
	free(data->syspath);
	free(data->fs_usage);
	free(data->fs_type);
	free(data->fs_version);
	free(data->fs_uuid_enc);
	free(data->mount_point);
	free(data);
}

static int update_block_data(struct block_data *data,
		const char *fs_usage,
		const char *fs_type,
		const char *fs_version,
		const char *fs_uuid_enc,
		const char *readonly)
{
	if (!data)
		return -EINVAL;

	free(data->fs_usage);
	data->fs_usage = NULL;
	if (fs_usage)
		data->fs_usage = strdup(fs_usage);

	free(data->fs_type);
	data->fs_type = NULL;
	if (fs_type)
		data->fs_type = strdup(fs_type);

	free(data->fs_version);
	data->fs_version = NULL;
	if (fs_version)
		data->fs_version = strdup(fs_version);

	free(data->fs_uuid_enc);
	data->fs_uuid_enc = NULL;
	if (fs_uuid_enc)
		data->fs_uuid_enc = strdup(fs_uuid_enc);

	/* generate_mount_path function should be invoked
	 * after fs_uuid_enc is updated */
	free(data->mount_point);
	data->mount_point = generate_mount_path(data);

	data->readonly = false;
	if (readonly)
		data->readonly = atoi(readonly);

	BLOCK_FLAG_MOUNT_CLEAR(data);

	return 0;
}

static struct block_device *make_block_device(struct block_data *data)
{
	struct block_device *bdev;

	if (!data)
		return NULL;

	bdev = calloc(1, sizeof(struct block_device));
	if (!bdev)
		return NULL;

	bdev->data = data;
	bdev->thread_id = -1;

	return bdev;
}

static void free_block_device(struct block_device *bdev)
{
	dd_list *l, *next;
	struct operation_queue *op;
	int thread_id = bdev->thread_id;

	if (!bdev)
		return;

	if (thread_id < 0 || thread_id >= THREAD_MAX)
		return;
	th_manager[thread_id].num_dev--;

	pthread_mutex_lock(&(th_manager[thread_id].mutex));
	free_block_data(bdev->data);

	DD_LIST_FOREACH_SAFE(bdev->op_queue, l, next, op) {
		th_manager[thread_id].op_len--;
		DD_LIST_REMOVE(bdev->op_queue, op);
		free(op);
	}
	pthread_mutex_unlock(&(th_manager[thread_id].mutex));

	free(bdev);
}

static struct block_device *find_block_device(const char *devnode)
{
	struct block_device *bdev;
	dd_list *elem;
	int len;

	len = strlen(devnode) + 1;
	DD_LIST_FOREACH(block_dev_list, elem, bdev) {
		if (bdev->data &&
		    !strncmp(bdev->data->devnode, devnode, len))
			return bdev;
	}

	return NULL;
}

static struct block_device *find_block_device_by_id(int id)
{
	struct block_device *bdev;
	dd_list *elem;

	DD_LIST_FOREACH(block_dev_list, elem, bdev) {
		if (!bdev->data)
			continue;
		if (bdev->data->id == id)
			return bdev;
	}

	return NULL;
}

static char *get_operation_char(enum block_dev_operation op,
		char *name, unsigned int len)
{
	char *str = "unknown";

	if (!name)
		return NULL;

	switch (op) {
	case BLOCK_DEV_MOUNT:
		str = "MOUNT";
		break;
	case BLOCK_DEV_UNMOUNT:
		str = "UNMOUNT";
		break;
	case BLOCK_DEV_FORMAT:
		str = "FORMAT";
		break;
	case BLOCK_DEV_INSERT:
		str = "INSERT";
		break;
	case BLOCK_DEV_REMOVE:
		str = "REMOVE";
		break;
	case BLOCK_DEV_DEQUEUE:
		str = "DEQUEUE";
		break;
	default:
		_E("invalid operation (%d)", op);
		break;
	}

	snprintf(name, len, "%s", str);
	return name;
}

static int pipe_trigger(enum block_dev_operation op,
		struct block_device *bdev, int result)
{
	struct pipe_data pdata = { op, bdev, result };
	int n;
	char name[16];

	_D("op : %s, bdev : %p, result : %d",
			get_operation_char(pdata.op, name, sizeof(name)),
			pdata.bdev, pdata.result);

	pthread_mutex_lock(&glob_mutex);

	n = write(pfds[1], &pdata, sizeof(struct pipe_data));

	pthread_mutex_unlock(&glob_mutex);

	return (n != sizeof(struct pipe_data)) ? -EPERM : 0;
}

static Eina_Bool pipe_cb(void *data, Ecore_Fd_Handler *fdh)
{
	struct pipe_data pdata = {0,};
	int fd;
	int n;
	char name[16];

	if (ecore_main_fd_handler_active_get(fdh, ECORE_FD_ERROR)) {
		_E("an error has occured. Ignore it.");
		goto out;
	}

	fd = ecore_main_fd_handler_fd_get(fdh);
	if (fd <= 0) {
		_E("fail to get fd");
		goto out;
	}

	n = read(fd, &pdata, sizeof(pdata));
	if (n != sizeof(pdata) || !pdata.bdev) {
		_E("fail to read struct pipe data");
		goto out;
	}

	_D("op : %s, bdev : %p, result : %d",
			get_operation_char(pdata.op, name, sizeof(name)),
			pdata.bdev, pdata.result);

	if (pdata.op == BLOCK_DEV_DEQUEUE) {
		remove_operation(pdata.bdev);
		goto out;
	}

	/* Broadcast to mmc and usb storage module */
	broadcast_block_info(pdata.op, pdata.bdev->data, pdata.result);

	/* Broadcast outside with Block iface */
	signal_device_changed(pdata.bdev, pdata.op);

	if (pdata.op == BLOCK_DEV_REMOVE) {
		DD_LIST_REMOVE(block_dev_list, pdata.bdev);
		free_block_device(pdata.bdev);
	}

out:
	return ECORE_CALLBACK_RENEW;

}

static int pipe_init(void)
{
	int ret;

	ret = pipe2(pfds, O_CLOEXEC);
	if (ret == -1)
		return -errno;

	phandler = ecore_main_fd_handler_add(pfds[0],
			ECORE_FD_READ | ECORE_FD_ERROR,
			pipe_cb, NULL, NULL, NULL);
	if (!phandler)
		return -EPERM;

	return 0;
}

static void pipe_exit(void)
{
	if (phandler) {
		ecore_main_fd_handler_del(phandler);
		phandler = NULL;
	}

	if (pfds[0])
		close(pfds[0]);
	if (pfds[1])
		close(pfds[1]);
}

static int mmc_check_and_unmount(const char *path)
{
	int ret = 0;
	int retry = 0;

	if (!path)
		return 0;

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

static bool check_rw_mount(const char *szPath)
{
	struct statvfs mount_stat;

	if (!statvfs(szPath, &mount_stat)) {
		if ((mount_stat.f_flag & ST_RDONLY) == ST_RDONLY)
			return false;
	}
	return true;
}

static int retrieve_udev_device(struct block_data *data)
{
	struct udev *udev;
	struct udev_device *dev;
	int r;

	if (!data)
		return -EINVAL;

	udev = udev_new();
	if (!udev) {
		_E("fail to create udev library context");
		return -EPERM;
	}

	dev = udev_device_new_from_syspath(udev, data->syspath);
	if (!dev) {
		_E("fail to create new udev device");
		udev_unref(udev);
		return -EPERM;
	}

	r = update_block_data(data,
			udev_device_get_property_value(dev, "ID_FS_USAGE"),
			udev_device_get_property_value(dev, "ID_FS_TYPE"),
			udev_device_get_property_value(dev, "ID_FS_VERSION"),
			udev_device_get_property_value(dev, "ID_FS_UUID_ENC"),
			udev_device_get_sysattr_value(dev, "ro"));
	if (r < 0)
		_E("fail to update block data for %s", data->devnode);

	udev_device_unref(dev);
	udev_unref(udev);
	return r;
}

static int block_mount(struct block_data *data)
{
	struct block_fs_ops *fs;
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
	if (!data->fs_usage ||
	    strncmp(data->fs_usage, FILESYSTEM,
		    sizeof(FILESYSTEM)) != 0) {
		r = -ENODEV;
		goto out;
	}

	if (!data->fs_type) {
		_E("There is no file system");
		BLOCK_FLAG_SET(data, FS_EMPTY);
		r = -ENODATA;
		goto out;
	}

	fs = NULL;
	len = strlen(data->fs_type) + 1;
	DD_LIST_FOREACH(fs_head, elem, fs) {
		if (!strncmp(fs->name, data->fs_type, len))
			break;
	}

	if (!fs) {
		_E("Not supported file system (%s)", data->fs_type);
		BLOCK_FLAG_SET(data, FS_NOT_SUPPORTED);
		r = -ENOTSUP;
		goto out;
	}

	r = fs->mount(smack, data->devnode, data->mount_point);

	if (r == -EIO)
		BLOCK_FLAG_SET(data, FS_BROKEN);

	if (r < 0)
		goto out;

	r = check_rw_mount(data->mount_point);
	if (!r)
		return -EROFS;

	return 0;

out:
	rmdir(data->mount_point);
	return r;
}

static int mount_start(struct block_device *bdev)
{
	struct block_data *data;
	int r;

	assert(bdev);
	assert(bdev->data);

	data = bdev->data;
	_I("Mount Start : (%s -> %s)",
			data->devnode, data->mount_point);

	/* mount operation */
	r = block_mount(data);
	if (r != -EROFS && r < 0) {
		_E("fail to mount %s device : %d", data->devnode, r);
		goto out;
	}

	if (r == -EROFS) {
		data->readonly = true;
		BLOCK_FLAG_SET(data, MOUNT_READONLY);
	}

	data->state = BLOCK_MOUNT;

out:
	_I("%s result : %s, %d", __func__, data->devnode, r);

	if (pipe_trigger(BLOCK_DEV_MOUNT, bdev, r) < 0)
		_E("fail to trigger pipe");

	return r;
}

static int change_mount_point(struct block_device *bdev,
		const char *mount_point)
{
	struct block_data *data;

	if (!bdev)
		return -EINVAL;

	data = bdev->data;
	free(data->mount_point);

	/* If the mount path already exists, the path cannot be used */
	if (mount_point &&
		access(mount_point, F_OK) != 0)
		data->mount_point = strdup(mount_point);
	else
		data->mount_point = generate_mount_path(data);

	return 0;
}

static int mount_block_device(struct block_device *bdev)
{
	struct block_data *data;
	int r;

	if (!bdev || !bdev->data)
		return -EINVAL;

	data = bdev->data;
	if (data->state == BLOCK_MOUNT) {
		_I("%s is already mounted", data->devnode);
		return 0;
	}

	if (!block_conf[data->block_type].multimount &&
	    !data->primary) {
		_I("Not support multi mount by config info");
		return 0;
	}

	r = mount_start(bdev);
	if (r < 0) {
		_E("Failed to mount (%d)", data->devnode);
		return r;
	}

	return 0;
}

static int block_unmount(struct block_device *bdev,
		enum unmount_operation option)
{
	struct block_data *data;
	int r, retry = 0;
	struct timespec time = {0,};

	if (!bdev || !bdev->data || !bdev->data->mount_point)
		return -EINVAL;

	data = bdev->data;

	signal_device_blocked(bdev);
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
			/* At first, notify to other app
			 * who already access sdcard */
			_I("Notify to other app who already access sdcard");

			/* Mobile specific:
			 * should unmount the below vconf key. */
			if (data->block_type == BLOCK_MMC_DEV && data->primary)
				vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
						VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
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
				_I("Failed to unmount with lazy option : %d",
						errno);
				return -errno;
			}
			goto out;
		}

		/* it takes some seconds til other app completely clean up */
		time.tv_nsec = 500 * NANO_SECOND_MULTIPLIER;
		nanosleep(&time, NULL);

		r = mmc_check_and_unmount(data->mount_point);
		if (!r) {
			_E("Failed to unmount (%d)", data->mount_point);
			break;
		}
	}

out:
	data->state = BLOCK_UNMOUNT;

	if (rmdir(data->mount_point) < 0)
		_E("fail to remove %s directory", data->mount_point);

	return r;
}

static int unmount_block_device(struct block_device *bdev,
		enum unmount_operation option)
{
	struct block_data *data;
	int r;

	if (!bdev || !bdev->data)
		return -EINVAL;

	data = bdev->data;
	if (data->state == BLOCK_UNMOUNT) {
		_I("%s is already unmounted", data->devnode);
		r = mmc_check_and_unmount(data->mount_point);
		if (r < 0)
			_E("The path was existed, but could not delete it(%s)",
					data->mount_point);
		return 0;
	}

	_I("Unmount Start : (%s -> %s)",
			data->devnode, data->mount_point);

	r = block_unmount(bdev, option);
	if (r < 0) {
		_E("fail to unmount %s device : %d", data->devnode, r);
		goto out;
	}

	BLOCK_FLAG_MOUNT_CLEAR(data);

out:
	_I("%s result : %s, %d", __func__, data->devnode, r);

	if (pipe_trigger(BLOCK_DEV_UNMOUNT, bdev, r) < 0)
		_E("fail to trigger pipe");

	return r;
}

static int block_format(struct block_data *data,
		const char *fs_type)
{
	const struct block_fs_ops *fs;
	dd_list *elem;
	int len;
	int r;

	if (!data || !data->devnode || !data->mount_point)
		return -EINVAL;

	if (!fs_type)
		fs_type = data->fs_type;

	fs = NULL;
	len = strlen(fs_type);
	DD_LIST_FOREACH(fs_head, elem, fs) {
		if (!strncmp(fs->name, fs_type, len))
			break;
	}

	if (!fs) {
		BLOCK_FLAG_SET(data, FS_NOT_SUPPORTED);
		_E("not supported file system(%s)", fs_type);
		return -ENOTSUP;
	}

	_I("format path : %s", data->devnode);
	fs->check(data->devnode);
	r = fs->format(data->devnode);
	if (r < 0) {
		_E("fail to format block data for %s", data->devnode);
		goto out;
	}

	/* need to update the partition data.
	 * It can be changed in doing format. */
	retrieve_udev_device(data);

out:
	return r;
}

static int format_block_device(struct block_device *bdev,
		const char *fs_type,
		enum unmount_operation option)
{
	struct block_data *data;
	int r;

	assert(bdev);
	assert(bdev->data);

	data = bdev->data;

	_I("Format Start : (%s -> %s)",
			data->devnode, data->mount_point);

	if (data->state == BLOCK_MOUNT) {
		r = block_unmount(bdev, option);
		if (r < 0) {
			_E("fail to unmount %s device : %d", data->devnode, r);
			goto out;
		}
	}

	r = block_format(data, fs_type);
	if (r < 0)
		_E("fail to format %s device : %d", data->devnode, r);

out:
	_I("%s result : %s, %d", __func__, data->devnode, r);

	r = pipe_trigger(BLOCK_DEV_FORMAT, bdev, r);
	if (r < 0)
		_E("fail to trigger pipe");

	return r;
}

static struct format_data *get_format_data(
		const char *fs_type, enum unmount_operation option)
{
	struct format_data *fdata;

	fdata = (struct format_data *)malloc(sizeof(struct format_data));
	if (!fdata) {
		_E("fail to allocate format data");
		return NULL;
	}

	if (fs_type)
		fdata->fs_type = strdup(fs_type);
	else
		fdata->fs_type = NULL;
	fdata->option = option;

	return fdata;
}

static void release_format_data(struct format_data *data)
{
	if (data) {
		free(data->fs_type);
		free(data);
	}
}

static int block_mount_device(struct block_device *bdev, void *data)
{
	dd_list *l;
	int ret;

	if (!bdev)
		return -EINVAL;

	l = DD_LIST_FIND(block_dev_list, bdev);
	if (!l) {
		_E("(%d) does not exist in the device list", bdev->data->devnode);
		return -ENOENT;
	}

	/* mount automatically */
	ret = mount_block_device(bdev);
	if (ret < 0)
		_E("fail to mount block device for %s", bdev->data->devnode);

	return ret;
}

static int block_format_device(struct block_device *bdev, void *data)
{
	dd_list *l;
	int ret;
	struct format_data *fdata = (struct format_data *)data;

	if (!bdev || !fdata) {
		ret = -EINVAL;
		goto out;
	}

	l = DD_LIST_FIND(block_dev_list, bdev);
	if (!l) {
		_E("(%d) does not exist in the device list", bdev->data->devnode);
		ret = -ENOENT;
		goto out;
	}

	ret = format_block_device(bdev, fdata->fs_type, fdata->option);
	if (ret < 0)
		_E("fail to mount block device for %s", bdev->data->devnode);

out:
	release_format_data(fdata);

	return ret;
}

static int block_unmount_device(struct block_device *bdev, void *data)
{
	int ret;
	long option = (long)data;

	if (!bdev)
		return -EINVAL;

	ret = unmount_block_device(bdev, option);
	if (ret < 0) {
		_E("Failed to unmount block device (%s)", bdev->data->devnode);
		return ret;
	}

	return 0;
}

static void remove_operation(struct block_device *bdev)
{
	struct operation_queue *op;
	dd_list *l, *next;
	char name[16];
	int thread_id;

	assert(bdev);

	thread_id = bdev->thread_id;
	if (thread_id < 0 || thread_id >= THREAD_MAX)
		return;

	/* LOCK
	 * during removing queue and checking the queue length */
	pthread_mutex_lock(&(th_manager[thread_id].mutex));

	DD_LIST_FOREACH_SAFE(bdev->op_queue, l, next, op) {
		if (op->done) {
			_D("Remove operation (%s, %s)",
					get_operation_char(op->op, name, sizeof(name)),
					bdev->data->devnode);

			th_manager[thread_id].op_len--;
			DD_LIST_REMOVE(bdev->op_queue, op);
			free(op);
		}
	}

	pthread_mutex_unlock(&(th_manager[thread_id].mutex));
	/* UNLOCK */
}

static void block_send_dbus_reply(DBusMessage *msg, int result)
{
	DBusMessage *rep;
	int ret;
	static DBusConnection *conn = NULL;

	if (!msg)
		return;

	if (!conn) {
		conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
		if (!conn) {
			_E("dbus_bus_get error");
			return;
		}
	}

	rep = make_reply_message(msg, result);
	ret = dbus_connection_send(conn, rep, NULL);
	dbus_message_unref(msg);
	dbus_message_unref(rep);

	if (ret != TRUE)
		_E("Failed to send reply");
}

static bool check_removed(struct block_device *bdev, dd_list **queue, struct operation_queue **op)
{
	struct operation_queue *temp;
	dd_list *l;
	int thread_id;
	bool removed = false;

	if (!bdev)
		return false;

	if (!queue)
		return false;

	if (!op)
		return false;

	thread_id = bdev->thread_id;
	if (thread_id < 0 || thread_id >= THREAD_MAX)
		return removed;

	pthread_mutex_lock(&(th_manager[thread_id].mutex));
	DD_LIST_FOREACH(*queue, l, temp) {
		if (temp->op == BLOCK_DEV_REMOVE) {
			removed = true;
			_D("Operation queue has remove operation");
			break;
		}
	}
	pthread_mutex_unlock(&(th_manager[thread_id].mutex));

	if (!removed)
		return removed;

	pthread_mutex_lock(&(th_manager[thread_id].mutex));

	DD_LIST_FOREACH(*queue, l, temp) {
		if (temp->op == BLOCK_DEV_REMOVE) {
			*op = temp;
			break;
		}
		temp->done = true;
		block_send_dbus_reply((*op)->msg, 0);

		if (pipe_trigger(BLOCK_DEV_DEQUEUE, bdev, 0) < 0)
			_E("fail to trigger pipe");
	}

	pthread_mutex_unlock(&(th_manager[thread_id].mutex));

	return removed;
}

static bool check_unmount(struct block_device *bdev, dd_list **queue, struct operation_queue **op)
{
	struct operation_queue *temp;
	dd_list *l;
	int thread_id;
	bool unmounted = false;

	if (!bdev)
		return false;

	if (!queue)
		return false;

	if (!op)
		return false;

	thread_id = bdev->thread_id;
	if (thread_id < 0 || thread_id >= THREAD_MAX)
		return unmounted;

	pthread_mutex_lock(&(th_manager[thread_id].mutex));
	DD_LIST_FOREACH(*queue, l, temp) {
		if (temp->op == BLOCK_DEV_UNMOUNT) {
			unmounted = true;
			_D("Operation queue has unmount operation");
			break;
		}
	}
	pthread_mutex_unlock(&(th_manager[thread_id].mutex));

	if (!unmounted)
		return unmounted;

	pthread_mutex_lock(&(th_manager[thread_id].mutex));

	DD_LIST_FOREACH(*queue, l, temp) {
		if (temp->op == BLOCK_DEV_UNMOUNT) {
			*op = temp;
			break;
		}
		temp->done = true;
		block_send_dbus_reply((*op)->msg, 0);

		if (pipe_trigger(BLOCK_DEV_DEQUEUE, bdev, 0) < 0)
			_E("fail to trigger pipe");
	}

	pthread_mutex_unlock(&(th_manager[thread_id].mutex));

	return unmounted;
}

static void trigger_operation(struct block_device *bdev)
{
	struct operation_queue *op;
	dd_list *queue;
	int ret = 0;
	int thread_id;
	char devnode[PATH_MAX];
	char name[16];
	enum block_dev_operation operation;
	bool removed = false;
	bool unmounted = false;

	assert(bdev);

	if (!bdev->op_queue)
		return;

	thread_id = bdev->thread_id;
	if (thread_id < 0 || thread_id >= THREAD_MAX)
		return;

	pthread_mutex_lock(&(th_manager[thread_id].mutex));
	queue = bdev->op_queue;
	pthread_mutex_unlock(&(th_manager[thread_id].mutex));

	snprintf(devnode, sizeof(devnode), "%s", bdev->data->devnode);

	do {
		pthread_mutex_lock(&(th_manager[thread_id].mutex));
		op = DD_LIST_NTH(queue, 0);
		if (!op) {
			_D("Operation queue is empty");
			pthread_mutex_unlock(&(th_manager[thread_id].mutex));
			break;
		}
		if (op->done) {
			queue = DD_LIST_NEXT(queue);
			pthread_mutex_unlock(&(th_manager[thread_id].mutex));
			continue;
		}
		pthread_mutex_unlock(&(th_manager[thread_id].mutex));

		operation = op->op;

		_D("Trigger operation (%s, %s)",
			get_operation_char(operation, name, sizeof(name)), devnode);

		removed = false;
		unmounted = false;
		if (operation == BLOCK_DEV_INSERT) {
			removed = check_removed(bdev, &queue, &op);
			if (removed) {
				operation = op->op;
				_D("Trigger operation again (%s, %s)",
					get_operation_char(operation, name, sizeof(name)), devnode);
			}
		}
		if (operation == BLOCK_DEV_MOUNT) {
			unmounted = check_unmount(bdev, &queue, &op);
			if (unmounted) {
				operation = op->op;
				_D("Trigger operation again (%s, %s)",
					get_operation_char(operation, name, sizeof(name)), devnode);
			}
		}

		switch (operation) {
		case BLOCK_DEV_INSERT:
			break;
		case BLOCK_DEV_MOUNT:
			ret = block_mount_device(bdev, op->data);
			_D("Mount (%s) result:(%d)", devnode, ret);
			break;
		case BLOCK_DEV_FORMAT:
			ret = block_format_device(bdev, op->data);
			_D("Format (%s) result:(%d)", devnode, ret);
			break;
		case BLOCK_DEV_UNMOUNT:
			ret = block_unmount_device(bdev, op->data);
			_D("Unmount (%s) result:(%d)", devnode, ret);
			break;
		case BLOCK_DEV_REMOVE:
			/* Do nothing */
			break;
		default:
			_E("Operation type is invalid (%d)", op->op);
			ret = -EINVAL;
			break;
		}

		/* LOCK
		 * during checking the queue length */
		pthread_mutex_lock(&(th_manager[thread_id].mutex));

		op->done = true;

		block_send_dbus_reply(op->msg, ret);

		queue = DD_LIST_NEXT(queue);

		pthread_mutex_unlock(&(th_manager[thread_id].mutex));
		/* UNLOCK */

		if (pipe_trigger(BLOCK_DEV_DEQUEUE, bdev, ret) < 0)
			_E("fail to trigger pipe");

		if (operation == BLOCK_DEV_INSERT || operation == BLOCK_DEV_REMOVE) {
			if (pipe_trigger(operation, bdev, 0) < 0)
				_E("fail to trigger pipe");
		}

	} while(true);

}

static void *block_th_start(void *arg)
{
	struct block_device *temp;
	struct manage_thread *th = (struct manage_thread *)arg;
	dd_list *elem;
	int thread_id;

	assert(th);

	thread_id = th->thread_id;
	if (thread_id < 0 || thread_id >= THREAD_MAX) {
		_D("Thread Number: %d", th->thread_id);
		return NULL;
	}

	do {
		pthread_mutex_lock(&glob_mutex);
		if (th_manager[th->thread_id].op_len == 0) {
			pthread_mutex_unlock(&glob_mutex);
			_D("Operation queue of thread is empty");
			pthread_mutex_lock(&(th_manager[thread_id].mutex));
			pthread_cond_wait(&(th_manager[thread_id].cond), &(th_manager[thread_id].mutex));
			_D("Wake up %d", thread_id);
			pthread_mutex_unlock(&(th_manager[thread_id].mutex));
			continue;
		}
		pthread_mutex_unlock(&glob_mutex);

		DD_LIST_FOREACH(block_dev_list, elem, temp) {
			if (temp->thread_id == thread_id) {
				trigger_operation(temp);
			}
		}

	} while (true);
	return NULL;
}

static int find_thread(char *devnode)
{
	dd_list *elem;
	char *th_node;
	int i, len, min, min_num;

	len = strlen(devnode);
	min_num = 1000;
	min = -1;
	for (i = 0; i < THREAD_MAX; i++) {
		DD_LIST_FOREACH(th_manager[i].th_node_list, elem, th_node) {
			if (!th_node)
				continue;
			if (!strncmp(th_node, devnode, len))
				return i;
		}
		if (th_manager[i].num_dev < min_num) {
			min_num = th_manager[i].num_dev;
			min = i;
		}
	}

	th_node = strdup(devnode);
	if (min >= 0 && min < THREAD_MAX) {
		DD_LIST_APPEND(th_manager[min].th_node_list, th_node);
		return min;
	}

	// TODO return error or 0?
	DD_LIST_APPEND(th_manager[0].th_node_list, th_node);
	return 0;
}

/* Only Main thread is permmited */
static int add_operation(struct block_device *bdev,
		enum block_dev_operation operation,
		DBusMessage *msg, void *data)
{
	struct operation_queue *op;
	int ret;
	int thread_id;
	bool start_th;
	char name[16];

	if (!bdev)
		return -EINVAL;

	_D("Add operation (%s, %s)",
			get_operation_char(operation, name, sizeof(name)),
			bdev->data->devnode);

	op = (struct operation_queue *)malloc(sizeof(struct operation_queue));
	if (!op) {
		_E("malloc failed");
		return -ENOMEM;
	}

	op->op = operation;
	op->data = data;
	op->done = false;

	if (msg)
		msg = dbus_message_ref(msg);
	op->msg = msg;

	if (operation == BLOCK_DEV_INSERT) {
		thread_id = find_thread(bdev->data->devnode);
		if (thread_id < 0 || thread_id >= THREAD_MAX) {
			_E("Fail to find thread to add");
			return -EPERM;
		}
		th_manager[thread_id].num_dev++;
		bdev->thread_id = thread_id;
	} else
		thread_id = bdev->thread_id;

	if (thread_id < 0 || thread_id >= THREAD_MAX) {
		_E("Fail to find thread to add");
		return -EPERM;
	}

	/* LOCK
	 * during adding queue and checking the queue length */
	pthread_mutex_lock(&(th_manager[thread_id].mutex));

	start_th = th_manager[thread_id].start_th;
	DD_LIST_APPEND(bdev->op_queue, op);
	th_manager[thread_id].op_len++;

	if (th_manager[thread_id].op_len == 1 && !start_th) {
		pthread_cond_signal(&(th_manager[thread_id].cond));
	}

	pthread_mutex_unlock(&(th_manager[thread_id].mutex));
	/* UNLOCK */

	if (start_th) {
		_D("Start New thread for block device");
		th_manager[thread_id].start_th = false;
		ret = pthread_create(&(th_manager[thread_id].th), NULL, block_th_start, &th_manager[thread_id]);
		if (ret != 0) {
			_E("fail to create thread for %s", bdev->data->devnode);
			return -EPERM;
		}

		pthread_detach(th_manager[thread_id].th);
	}

	return 0;
}

static bool disk_is_partitioned_by_kernel(struct udev_device *dev)
{
	DIR *dp;
	struct dirent entry;
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
	while (readdir_r(dp, &entry, &dir) == 0 && dir != NULL) {
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
		if (fs_usage &&
		    strncmp(fs_usage, FILESYSTEM, sizeof(FILESYSTEM)) == 0) {
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
			udev_device_get_syspath(dev),
			udev_device_get_property_value(dev, "ID_FS_USAGE"),
			udev_device_get_property_value(dev, "ID_FS_TYPE"),
			udev_device_get_property_value(dev, "ID_FS_VERSION"),
			udev_device_get_property_value(dev, "ID_FS_UUID_ENC"),
			udev_device_get_sysattr_value(dev, "ro"));
	if (!data) {
		_E("fail to make block data for %s", devnode);
		return -EPERM;
	}

	bdev = make_block_device(data);
	if (!bdev) {
		_E("fail to make block device for %s", devnode);
		free_block_data(data);
		return -EPERM;
	}

	DD_LIST_APPEND(block_dev_list, bdev);

	ret = add_operation(bdev, BLOCK_DEV_INSERT, NULL, (void *)data);
	if (ret < 0) {
		_E("Failed to add operation (mount %s)", devnode);
		return ret;
	}

	ret = add_operation(bdev, BLOCK_DEV_MOUNT, NULL, NULL);
	if (ret < 0) {
		_E("Failed to add operation (mount %s)", devnode);
		return ret;
	}

	return 0;
}

static int remove_block_device(struct udev_device *dev, const char *devnode)
{
	struct block_device *bdev;
	int ret;

	bdev = find_block_device(devnode);
	if (!bdev) {
		_E("fail to find block data for %s", devnode);
		return -ENODEV;
	}

	BLOCK_FLAG_SET(bdev->data, UNMOUNT_UNSAFE);

	ret = add_operation(bdev, BLOCK_DEV_UNMOUNT, NULL, (void *)UNMOUNT_FORCE);
	if (ret < 0) {
		_E("Failed to add operation (unmount %s)", devnode);
		return ret;
	}

	ret = add_operation(bdev, BLOCK_DEV_REMOVE, NULL, NULL);
	if (ret < 0) {
		_E("Failed to add operation (remove %s)", devnode);
		return ret;
	}

	return 0;
}

static int block_init_from_udev_enumerate(void)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *list_entry, *list_sub_entry;
	struct udev_device *dev;
	const char *syspath;
	const char *devnode;

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

		dev = udev_device_new_from_syspath(
				udev_enumerate_get_udev(enumerate),
				syspath);
		if (!dev)
			continue;

		devnode = NULL;
		udev_list_entry_foreach(list_sub_entry,
				udev_device_get_devlinks_list_entry(dev)) {
			const char *devlink = udev_list_entry_get_name(list_sub_entry);
			if (!fnmatch(MMC_LINK_PATH, devlink, 0)) {
				devnode = devlink;
				break;
			}
		}

		if (!devnode) {
			devnode = udev_device_get_devnode(dev);
			if (!devnode)
				continue;

			if (fnmatch(MMC_PATH, devnode, 0) &&
			    fnmatch(SCSI_PATH, devnode, 0))
				continue;
		}

		_D("%s device add", devnode);
		add_block_device(dev, devnode);

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);
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
		_D("\tSyspath: %s", data->syspath);
		_D("\tBlock type: %s",
				(data->block_type == BLOCK_MMC_DEV ?
				 BLOCK_TYPE_MMC : BLOCK_TYPE_SCSI));
		_D("\tFs type: %s", data->fs_type);
		_D("\tFs usage: %s", data->fs_usage);
		_D("\tFs version: %s", data->fs_version);
		_D("\tFs uuid enc: %s", data->fs_uuid_enc);
		_D("\tReadonly: %s",
				(data->readonly ? "true" : "false"));
		_D("\tMount point: %s", data->mount_point);
		_D("\tMount state: %s",
				(data->state == BLOCK_MOUNT ?
				 "mount" : "unmount"));
		_D("\tPrimary: %s",
				(data->primary ? "true" : "false"));
		_D("\tID: %d", data->id);
	}
}

static void remove_whole_block_device(void)
{
	struct block_device *bdev;
	dd_list *elem;
	dd_list *next;
	int r;

	DD_LIST_FOREACH_SAFE(block_dev_list, elem, next, bdev) {
		DD_LIST_REMOVE(block_dev_list, bdev);

		r = add_operation(bdev, BLOCK_DEV_UNMOUNT, NULL, (void *)UNMOUNT_NORMAL);
		if (r < 0)
			_E("Failed to add operation (unmount %s)", bdev->data->devnode);

		r = add_operation(bdev, BLOCK_DEV_REMOVE, NULL, NULL);
		if (r < 0)
			_E("Failed to add operation (remove %s)", bdev->data->devnode);
	}
}

static int booting_done(void *data)
{
	/* if there is the attached device, try to mount */
	block_init_from_udev_enumerate();
	block_control = true;
	block_boot = true;
	return 0;
}

static int block_poweroff(void *data)
{
	/* unregister mmc uevent control routine */
	unregister_udev_uevent_control(&uh);
	remove_whole_block_device();
	return 0;
}

static void uevent_block_handler(struct udev_device *dev)
{
	const char *devnode = NULL;
	const char *action;
	struct udev_list_entry *list_entry;

	udev_list_entry_foreach(list_entry, udev_device_get_devlinks_list_entry(dev)) {
		const char *devlink = udev_list_entry_get_name(list_entry);
		if (!fnmatch(MMC_LINK_PATH, devlink, 0)) {
			devnode = devlink;
			break;
		}
	}

	if (!devnode) {
		devnode = udev_device_get_devnode(dev);
		if (!devnode)
			return;

		if (fnmatch(MMC_PATH, devnode, 0) &&
		    fnmatch(SCSI_PATH, devnode, 0))
			return;
	}

	action = udev_device_get_action(dev);
	if (!action)
		return;

	_D("%s device %s", devnode, action);
	if (!strncmp(action, UDEV_ADD, sizeof(UDEV_ADD)))
		add_block_device(dev, devnode);
	else if (!strncmp(action, UDEV_REMOVE, sizeof(UDEV_REMOVE)))
		remove_block_device(dev, devnode);
}

static DBusMessage *request_mount_block(E_DBus_Object *obj,
		DBusMessage *msg)
{
	struct block_device *bdev;
	char *mount_point;
	int id;
	int ret = -EBADMSG;

	if (!obj || !msg)
		goto out;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &id,
			DBUS_TYPE_STRING, &mount_point,
			DBUS_TYPE_INVALID);
	if (!ret)
		goto out;
	bdev = find_block_device_by_id(id);
	if (!bdev) {
		_E("Failed to find (%d) in the device list", id);
		ret = -ENOENT;
		goto out;
	}

	if (bdev->data->state == BLOCK_MOUNT) {
		ret = -EEXIST;
		goto out;
	}

	/* if requester want to use a specific mount point */
	if (mount_point && strncmp(mount_point, "", 1) != 0) {
		ret = change_mount_point(bdev, mount_point);
		if (ret < 0) {
			ret = -EPERM;
			goto out;
		}
	}

	ret = add_operation(bdev, BLOCK_DEV_MOUNT, msg, NULL);
	if (ret < 0) {
		_E("Failed to add operation (mount %s)", bdev->data->devnode);
		goto out;
	}

	return NULL;

out:
	return make_reply_message(msg, ret);
}

static DBusMessage *request_unmount_block(E_DBus_Object *obj,
		DBusMessage *msg)
{
	struct block_device *bdev;
	long option;
	int id;
	int ret = -EBADMSG;

	if (!obj || !msg)
		goto out;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &id,
			DBUS_TYPE_INT32, &option,
			DBUS_TYPE_INVALID);
	if (!ret)
		goto out;
	bdev = find_block_device_by_id(id);
	if (!bdev) {
		_E("Failed to find (%d) in the device list", id);
		ret = -ENOENT;
		goto out;
	}

	ret = add_operation(bdev, BLOCK_DEV_UNMOUNT, msg, (void *)option);
	if (ret < 0) {
		_E("Failed to add operation (unmount %s)", bdev->data->devnode);
		goto out;
	}

	return NULL;

out:
	return make_reply_message(msg, ret);
}

static DBusMessage *request_format_block(E_DBus_Object *obj,
		DBusMessage *msg)
{
	struct block_device *bdev;
	struct format_data *fdata;
	int id;
	int option;
	int ret = -EBADMSG;
	int prev_state;

	if (!obj || !msg)
		goto out;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &id,
			DBUS_TYPE_INT32, &option,
			DBUS_TYPE_INVALID);
	if (!ret)
		goto out;
	bdev = find_block_device_by_id(id);
	if (!bdev) {
		_E("Failed to find (%d) in the device list", id);
		goto out;
	}

	fdata = get_format_data(NULL, option);
	if (!fdata) {
		_E("Failed to get format data");
		goto out;
	}

	prev_state =  bdev->data->state;
	if (prev_state == BLOCK_MOUNT) {
		ret = add_operation(bdev, BLOCK_DEV_UNMOUNT, NULL, (void *)UNMOUNT_FORCE);
		if (ret < 0) {
			_E("Failed to add operation (unmount %s)", bdev->data->devnode);
			release_format_data(fdata);
			goto out;
		}
	}

	ret = add_operation(bdev, BLOCK_DEV_FORMAT, msg, (void *)fdata);
	if (ret < 0) {
		_E("Failed to add operation (format %s)", bdev->data->devnode);
		release_format_data(fdata);
	}

	/* Maintain previous state of mount/unmount */
	if (prev_state == BLOCK_MOUNT) {
		if (add_operation(bdev, BLOCK_DEV_MOUNT, NULL, NULL) < 0) {
			_E("Failed to add operation (mount %s)", bdev->data->devnode);
			goto out;
		}
	}

	return NULL;

out:
	return make_reply_message(msg, ret);
}

static int add_device_to_iter(struct block_data *data, DBusMessageIter *iter)
{
	DBusMessageIter piter;
	char *str_null = "";

	if (!data || !iter)
		return -EINVAL;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &piter);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->block_type));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->devnode ? &(data->devnode) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->syspath ? &(data->syspath) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_usage ? &(data->fs_usage) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_type ? &(data->fs_type) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_version ? &(data->fs_version) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_uuid_enc ? &(data->fs_uuid_enc) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->readonly));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->mount_point ? &(data->mount_point) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->state));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_BOOLEAN,
			&(data->primary));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->flags));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->id));
	dbus_message_iter_close_container(iter, &piter);

	return 0;
}

static int add_device_to_iter_2(struct block_data *data, DBusMessageIter *iter)
{
	DBusMessageIter piter;
	char *str_null = "";

	if (!data || !iter)
		return -EINVAL;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &piter);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->block_type));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->devnode ? &(data->devnode) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->syspath ? &(data->syspath) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_usage ? &(data->fs_usage) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_type ? &(data->fs_type) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_version ? &(data->fs_version) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->fs_uuid_enc ? &(data->fs_uuid_enc) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->readonly));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_STRING,
			data->mount_point ? &(data->mount_point) : &str_null);
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->state));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_BOOLEAN,
			&(data->primary));
	dbus_message_iter_append_basic(&piter, DBUS_TYPE_INT32,
			&(data->flags));
	dbus_message_iter_close_container(iter, &piter);

	return 0;
}

static DBusMessage *request_get_device_info(E_DBus_Object *obj,
		DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	struct block_device *bdev;
	struct block_data *data;
	int ret, id;

	if (!obj || !msg)
		return NULL;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		goto out;

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &id,
			DBUS_TYPE_INVALID);
	if (!ret)
		goto out;

	bdev = find_block_device_by_id(id);
	if (!bdev)
		goto out;
	data = bdev->data;
	if (!data)
		goto out;

	dbus_message_iter_init_append(reply, &iter);
	add_device_to_iter(data, &iter);

out:
	return reply;
}

static DBusMessage *request_show_device_list(E_DBus_Object *obj,
		DBusMessage *msg)
{
	show_block_device_list();
	return dbus_message_new_method_return(msg);
}

static DBusMessage *request_get_device_list(E_DBus_Object *obj,
		DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessageIter aiter;
	DBusMessage *reply;
	struct block_device *bdev;
	struct block_data *data;
	dd_list *elem;
	char *type = NULL;
	int ret = -EBADMSG;
	int block_type;

	reply = dbus_message_new_method_return(msg);

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &type,
			DBUS_TYPE_INVALID);
	if (!ret) {
		_E("Failed to get args");
		goto out;
	}

	if (!type) {
		_E("Delivered type is NULL");
		goto out;
	}

	_D("Block (%s) device list is requested", type);

	if (!strncmp(type, BLOCK_TYPE_SCSI, sizeof(BLOCK_TYPE_SCSI)))
		block_type = BLOCK_SCSI_DEV;
	else if (!strncmp(type, BLOCK_TYPE_MMC, sizeof(BLOCK_TYPE_MMC)))
		block_type = BLOCK_MMC_DEV;
	else if (!strncmp(type, BLOCK_TYPE_ALL, sizeof(BLOCK_TYPE_ALL)))
		block_type = -1;
	else {
		_E("Invalid type (%s) is requested", type);
		goto out;
	}

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(issssssisibii)", &aiter);

	DD_LIST_FOREACH(block_dev_list, elem, bdev) {
		data = bdev->data;
		if (!data)
			continue;

		switch (block_type) {
		case BLOCK_SCSI_DEV:
		case BLOCK_MMC_DEV:
			if (data->block_type != block_type)
				continue;
			break;
		default:
			break;
		}

		add_device_to_iter(data, &aiter);
	}
	dbus_message_iter_close_container(&iter, &aiter);

out:
	return reply;
}

static DBusMessage *request_get_device_list_2(E_DBus_Object *obj,
		DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessageIter aiter;
	DBusMessage *reply;
	struct block_device *bdev;
	struct block_data *data;
	dd_list *elem;
	char *type = NULL;
	int ret = -EBADMSG;
	int block_type;

	reply = dbus_message_new_method_return(msg);

	ret = dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &type,
			DBUS_TYPE_INVALID);
	if (!ret) {
		_E("Failed to get args");
		goto out;
	}

	if (!type) {
		_E("Delivered type is NULL");
		goto out;
	}

	_D("Block (%s) device list is requested", type);

	if (!strncmp(type, BLOCK_TYPE_SCSI, sizeof(BLOCK_TYPE_SCSI)))
		block_type = BLOCK_SCSI_DEV;
	else if (!strncmp(type, BLOCK_TYPE_MMC, sizeof(BLOCK_TYPE_MMC)))
		block_type = BLOCK_MMC_DEV;
	else if (!strncmp(type, BLOCK_TYPE_ALL, sizeof(BLOCK_TYPE_ALL)))
		block_type = -1;
	else {
		_E("Invalid type (%s) is requested", type);
		goto out;
	}

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(issssssisibi)", &aiter);

	DD_LIST_FOREACH(block_dev_list, elem, bdev) {
		data = bdev->data;
		if (!data)
			continue;

		switch (block_type) {
		case BLOCK_SCSI_DEV:
		case BLOCK_MMC_DEV:
			if (data->block_type != block_type)
				continue;
			break;
		default:
			break;
		}

		add_device_to_iter_2(data, &aiter);
	}
	dbus_message_iter_close_container(&iter, &aiter);

out:
	return reply;
}

static const struct edbus_method manager_methods[] = {
	{ "ShowDeviceList", NULL, NULL, request_show_device_list },
	{ "GetDeviceList" , "s", "a(issssssisibii)" , request_get_device_list },
	{ "GetDeviceList2", "s", "a(issssssisibi)", request_get_device_list_2 },
	{ "Mount",    "is",  "i", request_mount_block },
	{ "Unmount",  "ii",  "i", request_unmount_block },
	{ "Format",   "ii",  "i", request_format_block },
	{ "GetDeviceInfo"  , "i", "(issssssisibii)" , request_get_device_info },
};

static int load_config(struct parse_result *result, void *user_data)
{
	int index;

	if (MATCH(result->section, "Block"))
		return 0;

	if (MATCH(result->section, "SCSI"))
		index = BLOCK_SCSI_DEV;
	else if (MATCH(result->section, "MMC"))
		index = BLOCK_MMC_DEV;
	else
		return -EINVAL;

	if (MATCH(result->name, "Multimount"))
		block_conf[index].multimount =
			(MATCH(result->value, "yes") ? true : false);

	return 0;
}

#ifdef BLOCK_TMPFS
static int mount_root_path_tmpfs(void)
{
	int ret;
	char *root;

	root = tzplatform_getenv(TZ_SYS_MEDIA);
	if (!root)
		return -ENOTSUP;

	if (access(root, F_OK) != 0)
		return -ENODEV;

	if (mount_check(root))
		return 0;

	ret = mount("tmpfs", root, "tmpfs", 0, "smackfsroot=System::Shared");
	if (ret < 0) {
		ret = -errno;
		_E("tmpfs mount failed (%d)", ret);
		return ret;
	}

	return 0;
}
#else
#define mount_root_path_tmpfs() 0
#endif

static void block_init(void *data)
{
	int ret;
	int i;

	/* load config */
	ret = config_parse(BLOCK_CONF_FILE, load_config, NULL);
	if (ret < 0)
		_E("fail to load %s, Use default value", BLOCK_CONF_FILE);

	ret = mount_root_path_tmpfs();
	if (ret < 0)
		_E("Failed to mount tmpfs to root mount path (%d)", ret);

	/* register block manager object and interface */
	ret = register_block_edbus_interface_and_method(DEVICED_PATH_BLOCK_MANAGER,
			DEVICED_INTERFACE_BLOCK_MANAGER,
			manager_methods, ARRAY_SIZE(manager_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);

	/* init pipe */
	ret = pipe_init();
	if (ret < 0)
		_E("fail to init pipe");

	/* register mmc uevent control routine */
	ret = register_udev_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register block uevent : %d", ret);

	/* register notifier */
	register_notifier(DEVICE_NOTIFIER_POWEROFF, block_poweroff);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);

	for (i = 0; i < THREAD_MAX; i++) {
		th_manager[i].num_dev = 0;
		th_manager[i].op_len = 0;
		th_manager[i].start_th = true;
		th_manager[i].thread_id = i;
		pthread_mutex_init(&(th_manager[i].mutex), NULL);
		pthread_cond_init(&(th_manager[i].cond), NULL);
	}
}

static void block_exit(void *data)
{
	int ret;

	/* unregister notifier */
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF, block_poweroff);
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);

	/* exit pipe */
	pipe_exit();

	/* unregister mmc uevent control routine */
	ret = unregister_udev_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister block uevent : %d", ret);

	/* remove remaining blocks */
	remove_whole_block_device();

	block_control = false;
}

static int block_start(enum device_flags flags)
{
	int ret;

	if (!block_boot) {
		_E("Cannot be started. Booting is not ready");
		return -ENODEV;
	}

	if (block_control) {
		_I("Already started");
		return 0;
	}

	/* register mmc uevent control routine */
	ret = register_udev_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register block uevent : %d", ret);

	block_init_from_udev_enumerate();

	block_control = true;

	_I("start");
	return 0;
}

static int block_stop(enum device_flags flags)
{
	if (!block_boot) {
		_E("Cannot be stopped. Booting is not ready");
		return -ENODEV;
	}

	if (!block_control) {
		_I("Already stopped");
		return 0;
	}

	/* unregister mmc uevent control routine */
	unregister_udev_uevent_control(&uh);

	/* remove the existing blocks */
	remove_whole_block_device();

	block_control = false;

	_I("stop");
	return 0;
}

const struct device_ops block_device_ops = {
	.name     = "block",
	.init     = block_init,
	.exit     = block_exit,
	.start    = block_start,
	.stop     = block_stop,
};

DEVICE_OPS_REGISTER(&block_device_ops)
