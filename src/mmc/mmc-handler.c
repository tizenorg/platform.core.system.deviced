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
#include <bundle.h>
#include <signal.h>
#include <stdbool.h>
#include <mntent.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "mmc-handler.h"

#define VCONFKEY_INTERNAL_PRIVATE_MMC_ID	"db/private/sysman/mmc_device_id"

#define PREDEF_MOUNT_MMC		"mountmmc"
#define PREDEF_UNMOUNT_MMC		"unmountmmc"
#define PREDEF_FORMAT_MMC		"formatmmc"

#define MMC_PARENT_PATH	"/opt/storage"
#define MMC_DEV			"/dev/mmcblk"

#define SMACKFS_MAGIC	0x43415d53
#define SMACKFS_MNT		"/smack"

#ifndef ST_RDONLY
#define ST_RDONLY		0x0001
#endif

#define MMC_32GB_SIZE           61315072

#define UNMOUNT_RETRY	5

#define get_mmc_fs(ptr, type, member) \
((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define mmc_fs_search(pos, head) \
for (pos = (head)->next; pos != (head); \
pos = pos->next)

typedef enum {
	FS_MOUNT_ERR = -1,
	FS_MOUNT_FAIL = 0,
	FS_MOUNT_SUCCESS = 1,
} mmc_mount_type;

enum unmount_operation {
	UNMOUNT_NORMAL,
	UNMOUNT_FORCE,
};

static int smack = 0;
static int mmc_popup_pid = 0;
static enum mmc_fs_type inserted_type;
static bool mmc_disabled = false;
static char mmc_node[PATH_MAX];

static struct mmc_filesystem_ops *mmc_filesystem;

static struct mmc_list mmc_handler_list = { &(mmc_handler_list), &(mmc_handler_list)};

static void __CONSTRUCTOR__ smack_check(void)
{
	struct statfs sfs;
	int ret;

	do {
		ret = statfs(SMACKFS_MNT, &sfs);
	} while (ret < 0 && errno == EINTR);

	if (ret == 0 && sfs.f_type == SMACKFS_MAGIC)
		smack = 1;
	_I("smackfs check %d", smack);
}

static void mmc_list_add(struct mmc_list *new, struct mmc_list *prev, struct mmc_list *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static void mmc_fs_add(struct mmc_list *new, struct mmc_list *head)
{
	mmc_list_add(new, head->prev, head);
}

static int exec_process(const char **argv)
{
	int pid;
	int i;
	int r;

	if (!argv)
		return -1;

	pid = fork();
	if (pid == -1)
		return -1;

	if (!pid) {
		for (i = 0; i < _NSIG; ++i)
			signal(i, SIG_DFL);
		r = execv(argv[0], (char **)argv);
		if (r == -1) {
			_E("execv() error");
			exit(EXIT_FAILURE);
		}
	}
	return pid;
}

int get_mmcblk_num(void)
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
					_E("%s open error: %s", buf,
						      strerror(errno));
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
					_D("%d", mmcblk_num);

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
	_E("Failed to find mmc block number");
	return -1;
}

static int mmc_check_fs_type(void)
{
	int ret;
	struct mmc_filesystem_info *filesystem = NULL;
	struct mmc_list *tmp;
	inserted_type = FS_TYPE_NONE;

	mmc_fs_search(tmp, &mmc_handler_list) {
		filesystem = get_mmc_fs(tmp, struct mmc_filesystem_info, list);
		ret = filesystem->fs_ops->init((void *)mmc_node);
		if (ret == -EINVAL)
			continue;
		inserted_type = ret;
		mmc_filesystem = filesystem->fs_ops;
		return 0;
	}
	_D("set default file system");
	mmc_fs_search(tmp, &mmc_handler_list) {
		filesystem = get_mmc_fs(tmp, struct mmc_filesystem_info, list);
		if (strcmp(filesystem->name, "vfat"))
			continue;
		inserted_type = FS_TYPE_FAT;
		mmc_filesystem =  filesystem->fs_ops;
		mmc_filesystem->init((void *)mmc_node);
		return 0;
	}
	_E("fail to init mmc file system");
	return -EINVAL;
}

static int get_format_type(void)
{
	unsigned int size;
	struct mmc_list *tmp;
	struct mmc_filesystem_info *filesystem = NULL;

	if (mmc_check_fs_type())
		return -EINVAL;

	if (inserted_type == FS_TYPE_EXT4)
		return 0;

	_E("fail to init mmc file system");
	return -EINVAL;
}

static int check_mount_state(void)
{
	struct stat parent_stat, mount_stat;
	char parent_path[PATH_MAX];
	char mount_path[PATH_MAX];

	snprintf(parent_path, sizeof(parent_path), "%s", MMC_PARENT_PATH);
	snprintf(mount_path, sizeof(mount_path), "%s", MMC_MOUNT_POINT);

	if (stat(mount_path, &mount_stat) != 0 || stat(parent_path, &parent_stat) != 0) {
		_I("state : UNMOUNT (get stat error)");
		return 0;
	}

	if (mount_stat.st_dev == parent_stat.st_dev) {
		_I("state : UNMOUNT");
		return 0;
	}

	_I("state : MOUNT");
	return 1;
}

static int create_partition(const char *dev_path)
{
	int r;

	r = system("printf \"n\\n\\n\\n\\n\\nw\" | fdisk /dev/mmcblk1");
	if (WIFSIGNALED(r) && (WTERMSIG(r) == SIGINT || WTERMSIG(r) == SIGQUIT || WEXITSTATUS(r)))
		return -1;
	return 0;
}

static int kill_app_accessing_mmc(void)
{
	int pid;
	const char *argv[4] = {"/sbin/fuser", "-mk", MMC_MOUNT_POINT, NULL};
	char buf[256];
	int retry = 10;

	pid = exec_process(argv);
	if (pid < 0) {
		_E("%s fail");
		return -EINVAL;
	}

	snprintf(buf, sizeof(buf), "%s%d", "/proc/", pid);
	_D("child process : %s", buf);
	while (--retry) {
		usleep(100000);
		_D("killing app....");
		if (access(buf, R_OK) != 0)
			break;
	}

	return 0;
}

static void launch_syspopup(const char *str)
{
	bundle *b;
	int ret;
	b = bundle_create();
	if (!b) {
		_E("error bundle_create()");
		return;
	}
	bundle_add(b, "_SYSPOPUP_CONTENT_", str);
	ret = syspopup_launch("mmc-syspopup", b);
	if (ret < 0)
		_E("popup launch failed");
	bundle_free(b);
}

static int mmc_check(char* path)
{
	int ret = false;
	struct mntent* mnt;
	const char* table = "/etc/mtab";
	FILE* fp;

	fp = setmntent(table, "r");
	if (!fp)
		return ret;
	while (mnt=getmntent(fp)) {
		if (!strcmp(mnt->mnt_dir, path)) {
			ret = true;
			break;
		}
	}
	endmntent(fp);
	return ret;
}

static int mmc_check_and_unmount(char *path)
{
	int ret = 0;
	if (mmc_check(path))
		ret = umount(path);
	return ret;
}

static int mmc_umount(int option)
{
	int r, retry = UNMOUNT_RETRY;

	if (!check_mount_state())
		return 0;

	_I("Mounted, will be unmounted");
	r = mmc_check_and_unmount(MMC_MOUNT_POINT);
	if (!r || option == UNMOUNT_NORMAL) {
		_I("unmount mmc card, ret = %d, option = %d", r, option);
		return r;
	}

	_I("Execute force unmount!");

	/* it notify to other app who already access sdcard */
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);

	/* it takes some seconds til other app completely clean up */
	usleep(500000);

	while (--retry) {
		r = mmc_check_and_unmount(MMC_MOUNT_POINT);
		if (!r)
			break;
		usleep(500000);
		_I("umount retry : %d", retry);
		if (retry == UNMOUNT_RETRY -2)
			kill_app_accessing_mmc();
	}

	return r;
}

static int mmc_format_exec(char *path)
{
	unsigned int size;
	int mkfs_pid;
	const char **argv;
	char buf[NAME_MAX];
	int r;

	if (path == NULL) {
		_E("Invalid parameter");
		return -EINVAL;
	}

	if (get_format_type() != 0) {
		_E("Invalid parameter");
		return -EINVAL;
	}

	argv = mmc_filesystem->format((void *)path);

	if (argv == NULL) {
		_E("get_argument fail");
		return -EINVAL;
	}

	mkfs_pid = exec_process(argv);
	if (mkfs_pid < 0) {
		_E("%s fail");
		return -EINVAL;
	}

	snprintf(buf, sizeof(buf), "%s%d", "/proc/", mkfs_pid);
	_D("child process : %s", buf);
	while (1) {
		sleep(1);
		_D("formatting....");
		if (access(buf, R_OK) != 0)
			break;
	}
	return 0;
}

static int mmc_format(int blknum)
{
	char dev_mmcblk[NAME_MAX];
	char dev_mmcblkp[NAME_MAX];
	int r;

	snprintf(dev_mmcblk, sizeof(dev_mmcblk), "%s%d", MMC_DEV, blknum);
	snprintf(dev_mmcblkp, sizeof(dev_mmcblkp), "%sp1", dev_mmcblk);

	/* in case of no partition */
	if (access(dev_mmcblkp, R_OK) < 0) {
		_I("%s is not valid, create the primary partition", dev_mmcblkp);

		/* format default dev partition */
		r = mmc_format_exec(dev_mmcblk);
		if (r != 0) {
			_E("format_mmc(%s) fail", dev_mmcblk);
			return r;
		}

		/* create partition */
		r = create_partition(dev_mmcblk);
		if (r != 0) {
			_E("create_partition failed");
			return r;
		}
	}

	/* format first partition */
	r = mmc_format_exec(dev_mmcblkp);
	if (r != 0) {
		_E("format_mmc fail");
		return r;
	}

	return 0;
}

int mount_fs(char *path, const char *fs_name, const char *mount_data)
{
	int ret, retry = 0;

	do {
		if ((ret = mount(path, MMC_MOUNT_POINT, fs_name, 0, mount_data)) == 0) {
			_I("Mounted mmc card %s", fs_name);
			return 0;
		}
		usleep(100000);
	} while (ret == -1 && errno == ENOENT && retry++ < 10);

	return errno;
}

static int __ss_rw_mount(const char *szPath)
{
	struct statvfs mount_stat;
	if (!statvfs(szPath, &mount_stat)) {
		if ((mount_stat.f_flag & ST_RDONLY) == ST_RDONLY)
			return -1;
	}
	return 0;
}

static int mmc_check_node(void)
{
	int fd;
	int blk_num;
	int ret = 0;

	if (access(MMC_MOUNT_POINT, R_OK) != 0) {
		if (mkdir(MMC_MOUNT_POINT, 0755) < 0) {
			_E("Make Directory is failed");
			return errno;
		}
	}

	blk_num = get_mmcblk_num();
	if (blk_num  == -1) {
		_E("fail to check mmc block");
		return -1;
	}

	snprintf(mmc_node, sizeof(mmc_node), "%s%dp1", MMC_DEV, blk_num);
	fd = open(mmc_node, O_RDONLY);
	if (fd < 0) {
		_E("can't open the '%s(%d)': %s",
			mmc_node, sizeof(mmc_node), strerror(errno));
		snprintf(mmc_node, sizeof(mmc_node), "%s%d", MMC_DEV, blk_num);
		fd = open(mmc_node, O_RDONLY);
		if (fd < 0) {
			_E("can't open the '%s(%d)': %s",
				mmc_node, sizeof(mmc_node), strerror(errno));
			return -1;
		}
	}
	close(fd);
	return 0;
}

static void mmc_check_fs(void)
{
	const char **params;
	char buf[NAME_MAX];
	int pid;

	if (!mmc_filesystem) {
		_E("no inserted mmc");
		return;
	}
	params = mmc_filesystem->check();

	pid = exec_process(params);
	if (pid < 0) {
		_E("mmc checker failed");
		return;
	}

	snprintf(buf, sizeof(buf), "%s%d", "/proc/", pid);
	_D("child process : %s", buf);

	while (1) {
		_D("mmc checking ....");
		if (access(buf, R_OK) != 0)
			break;
		sleep(1);
	}
}

static int mmc_mount(void)
{
	int r;

	r = mmc_check_node();
	if (r != 0) {
		_E("fail to check mmc");
		return r;
	}

	if (mmc_check_fs_type())
		goto mount_fail;

	mmc_check_fs();

	r = mmc_filesystem->mount(smack, mmc_node);

	if (r == 0)
		goto mount_complete;
	else if (r == 1)
		goto mount_wait;
	else if (r == -1)
		goto mount_fail;

	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_FAILED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, errno);
	_E("Failed to mount mmc card");
	return -1;

mount_complete:
	r = __ss_rw_mount(MMC_MOUNT_POINT);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	if (r == -1)
		return -1;
	return 0;

mount_fail:
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_FAILED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, VCONFKEY_SYSMAN_MMC_EINVAL);
	return -1;

mount_wait:
	_E("wait ext4 smack rule checking");
	return 0;
}

int ss_mmc_inserted(void)
{
	int mmc_status;

	if (mmc_disabled) {
		_I("mmc is blocked!");
		vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
		return -ENODEV;
	}

	vconf_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &mmc_status);
	if (mmc_status == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		_I("Mmc is already mounted");
		vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_ALREADY);
		return 0;
	}

	return mmc_mount();
}

int ss_mmc_removed(void)
{
	int mmc_err = 0;

	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	mmc_err = mmc_umount(UNMOUNT_NORMAL);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, mmc_err);
	mmc_filesystem = NULL;
	return 0;
}

static int ss_mmc_unmounted(int argc, char **argv)
{
	int option = -1;

	if (argc < 1) {
		_E("Option is wong");
		return -1;
	}

	option = atoi(argv[0]);
	if (option < 0) {
		_E("Option is wong : %d", option);
		return -1;
	}

	if (mmc_umount(UNMOUNT_FORCE) != 0) {
		_E("Failed to unmount mmc card");
		vconf_set_int(VCONFKEY_SYSMAN_MMC_UNMOUNT,
			VCONFKEY_SYSMAN_MMC_UNMOUNT_FAILED);
		vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, errno);
		return -1;

	}
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
		VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_UNMOUNT,
		VCONFKEY_SYSMAN_MMC_UNMOUNT_COMPLETED);
	return 0;
}

static int ss_mmc_format(int argc, char **argv)
{
	int r;
	int option;

	if (argc < 1) {
		_E("Option is wong");
		r = -EINVAL;
		goto error;
	}

	option = atoi(argv[0]);
	if (option < 0) {
		_E("Option is wong : %d", option);
		r = -EINVAL;
		goto error;
	}

	_I("Format Start (option:%d)", option);
	r = mmc_umount(option);
	if (r != 0)
		goto error;

	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS, VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NOW);
	r = mmc_format(get_mmcblk_num());
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS, VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NONE);
	if (r != 0)
		goto error;

	r = mmc_mount();
	if (r != 0)
		goto error;

	_I("Format Successful");
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED);
	return 0;

error:
	_E("Format Failed");
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_FAILED);
	return r;
}

int register_mmc_handler(const char *name, const struct mmc_filesystem_ops filesystem_type)
{
	struct mmc_list *tmp;
	struct mmc_filesystem_info *entry;

	entry = malloc(sizeof(struct mmc_filesystem_info));

	if (!entry) {
		_E("Malloc failed");
		return -1;
	}

	entry->name = strndup(name, strlen(name));

	if (!entry->name) {
		_E("Malloc failed");
		free(entry);
		return -1;
	}

	entry->fs_ops = malloc(sizeof(struct mmc_filesystem_ops));
	if (!entry->fs_ops) {
		_E("Malloc failed");
		return -1;
	}

	entry->fs_ops->init = filesystem_type.init;
	entry->fs_ops->check = filesystem_type.check;
	entry->fs_ops->mount = filesystem_type.mount;
	entry->fs_ops->format = filesystem_type.format;

	mmc_fs_add(&entry->list, &mmc_handler_list);

	mmc_fs_search(tmp, &mmc_handler_list) {
		entry = get_mmc_fs(tmp, struct mmc_filesystem_info, list);
	}
	return 0;
}

static void mmc_poweron_changed_cb(keynode_t *key, void* data)
{
	int val;
	if (vconf_get_int(VCONFKEY_DEVICED_BOOT_POWER_ON_STATUS, &val) < 0)
		return;

	/* unregister poweron callback */
	vconf_ignore_key_changed(VCONFKEY_DEVICED_BOOT_POWER_ON_STATUS,
		    mmc_poweron_changed_cb);
}

static void mmc_init(void *data)
{
	action_entry_add_internal(PREDEF_MOUNT_MMC, ss_mmc_inserted, NULL, NULL);
	action_entry_add_internal(PREDEF_UNMOUNT_MMC, ss_mmc_unmounted, NULL, NULL);
	action_entry_add_internal(PREDEF_FORMAT_MMC, ss_mmc_format, NULL, NULL);

	/* register vconf callback */
	vconf_notify_key_changed(VCONFKEY_DEVICED_BOOT_POWER_ON_STATUS,
			mmc_poweron_changed_cb, NULL);	/* mmc card mount */

	/* mmc card mount */
	mmc_mount();
}

static int mmc_start(void)
{
	mmc_disabled = false;
	_D("start");
	return 0;
}

static int mmc_stop(void)
{
	mmc_disabled = true;
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	_D("stop");
	return 0;
}

const struct device_ops mmc_device_ops = {
	.init = mmc_init,
	.start = mmc_start,
	.stop = mmc_stop,
};
