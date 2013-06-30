/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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
#include <sys/smack.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <vconf.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <bundle.h>
#include <signal.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"

#define VCONFKEY_INTERNAL_PRIVATE_MMC_ID	"db/private/sysman/mmc_device_id"

#define PREDEF_MOUNT_MMC		"mountmmc"
#define PREDEF_UNMOUNT_MMC		"unmountmmc"
#define PREDEF_FORMAT_MMC		"formatmmc"
#define PREDEF_CHECK_SMACK_MMC	"checksmackmmc"
#define PREDEF_CHECK_MMC		"checkmmc"
#define PREDEF_CHECK_MMC_PROC	"checkmmcproc"

#define MMC_MOUNT_POINT			"/opt/storage/sdcard"

#define MMC_DEV			"/dev/mmcblk"
#define MOVINAND_DEV		"/dev/mmcblk0p1"
#define FORMAT_MMC		PREFIX"/sbin/mkfs.vfat "
#define FORMAT_MOVINAND		PREFIX"/bin/movi_format.sh"

#define FS_VFAT_NAME "mkdosfs"
#define FS_VFAT_MOUNT_OPT  "uid=0,gid=0,dmask=0000,fmask=0111,iocharset=iso8859-1,utf8,shortname=mixed"
#define FS_VFAT_CHECKER "/sbin/fsck.vfat"
#define FS_EXT4_CHECKER "/sbin/fsck.ext4"
#define FS_VFAT_CHECK_PARAM "-a %s"
#define FS_EXT4_CHECK_PARAM "-f -y %s"
#define FS_EXT4_SMACK_LABEL "mmc-smack-label "MMC_MOUNT_POINT

#define SMACKFS_MAGIC		0x43415d53
#define SMACKFS_MNT		"/smack"
#define SMACKFS_MOUNT_OPT	"smackfsroot=*,smackfsdef=*"

#ifndef ST_RDONLY
#define ST_RDONLY	0x0001
#endif

#define MMC_PARENT_PATH     "/opt/storage"

#define BUF_LEN             20

#define MMC_32GB_SIZE           61315072

typedef enum {
	FS_TYPE_NONE = 0,
	FS_TYPE_FAT = 1,
	FS_TYPE_EXT4
} mmc_fs_type;

typedef enum {
	FS_MOUNT_ERR = -1,
	FS_MOUNT_FAIL = 0,
	FS_MOUNT_SUCCESS = 1,
} mmc_mount_type;


struct fs_check {
	unsigned int type;
	char *name;
	unsigned int offset;
	unsigned int magic_sz;
	char magic[4];
};

static struct fs_check fs_types[2] = {
	{
		FS_TYPE_FAT,
		"vfat",
		0x1fe,
		2,
		{0x55, 0xaa}
	},
	{
		FS_TYPE_EXT4,
		"ext4",
		0x438,
		2,
		{0x53, 0xef}
	},
};


static const char *fsstr[] = {
    [FS_TYPE_FAT] = "mkdosfs",
};

static const char *vfat_arg[] = {
	"/sbin/mkfs.vfat",
	NULL, NULL,
};

static const char *ext4_arg[] = {
    "/sbin/mkfs.ext4",
    NULL, NULL,
};

static const char *vfat_check_arg[] = {
    "/sbin/fsck.vfat",
    "-a", NULL, NULL,
};

static const char *ext4_check_arg[] = {
    "/sbin/fsck.ext4",
    "-f", "-y", NULL, NULL,
};


static int smack;
static int mmc_popup_pid;
static mmc_fs_type inserted_type;

static void __attribute__ ((constructor)) smack_check(void)
{
	struct statfs sfs;
	int ret;

	do {
		ret = statfs(SMACKFS_MNT, &sfs);
	} while (ret < 0 && errno == EINTR);

	if (ret == 0 && sfs.f_type == SMACKFS_MAGIC)
		smack = 1;
	PRT_TRACE_ERR("smackfs check %d", smack);
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
		r = execv(argv[0], argv);
		if (r == -1) {
			PRT_TRACE_ERR("execv() error");
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
		PRT_TRACE_ERR("Can not open directory..\n");
		return -1;
	}
	chdir("/sys/block");

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
					PRT_TRACE_ERR("%s open error: %s", buf,
						      strerror(errno));
					continue;
				}
				r = read(fd, buf, 10);
				if ((r >= 0) && (r < 10))
					buf[r] = '\0';
				else
					PRT_TRACE_ERR("%s read error: %s", buf,
						      strerror(errno));
				close(fd);
				if (strncmp("SD", buf, 2) == 0) {
					char *str_mmcblk_num = strndup((dir->d_name) + 6, 1);
					if (str_mmcblk_num == NULL) {
						PRT_TRACE_ERR("Memory Allocation Failed");
						closedir(dp);
						return -1;
					}
					mmcblk_num =
					    atoi(str_mmcblk_num);

					free(str_mmcblk_num);
					closedir(dp);
					PRT_TRACE("%d \n", mmcblk_num);

					snprintf(buf, 255, "/sys/block/%s/device/cid", dir->d_name);

					fd = open(buf, O_RDONLY);
					if (fd == -1) {
						PRT_TRACE_ERR("%s open error", buf, strerror(errno));
						return mmcblk_num;
					}
					r = read(fd, buf, 255);
					if ((r >=0) && (r < 255)) {
						buf[r] = '\0';
					} else {
						PRT_TRACE_ERR("%s read error: %s", buf,strerror(errno));
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
						PRT_TRACE_ERR("failed to get pre_mmc_device_id");
					}
					return mmcblk_num;
				}
			}

		}
	}
	closedir(dp);
	PRT_TRACE_ERR("Failed to find mmc block number\n");
	return -1;
}

static int __umount_fs(void)
{
	int ret;
	if ((ret = umount2(MMC_MOUNT_POINT, MNT_DETACH)) != 0) {
		PRT_TRACE_ERR("Failed to unmount mmc card : %s\n", strerror(errno));
	}
	return ret;
}

static int __check_mmc_fs_type(const char *path)
{
	int fd, ret, i;
	char buf[20];
	int len = 20;
	char *tmpbuf = buf;
	int cnr = 0;
	struct statfs sfs;

	inserted_type = FS_TYPE_NONE;
	if ((fd = open(path, O_RDONLY)) < 0) {
		PRT_TRACE_ERR("can't open the '%s': %s", path, strerror(errno));
		return -1;
	}

	while (len != 0 && (ret = read(fd, tmpbuf, len)) != 0) {
		if (ret == -1) {
			if (errno == EINTR) {
				PRT_TRACE_ERR("check_mmc_fs error(%d)", errno);
				continue;
			}
			PRT_TRACE_ERR("Can't read the '%s': %s", path, strerror(errno));
			inserted_type = FS_TYPE_FAT;
			goto check_return;
		}
		len -= ret;
		tmpbuf += ret;
		cnr += ret;
	}
	PRT_TRACE("mmc search path: %s, %s", path, buf);

	/* check fs type with magic code */
	for (i = 0; i < ARRAY_SIZE(fs_types); i++)
	{
		lseek(fd, fs_types[i].offset, SEEK_SET);
		ret = read(fd, buf, 2);
		PRT_TRACE("mmc search magic : 0x%2x, 0x%2x", buf[0],buf[1]);
		if (!memcmp(buf, fs_types[i].magic, fs_types[i].magic_sz)) {
			inserted_type =  fs_types[i].type;
			PRT_TRACE("mmc type : %s", fs_types[i].name);
			goto check_return;
		}
	}

	if (inserted_type == FS_TYPE_NONE)
		inserted_type = FS_TYPE_FAT;

check_return:
	close(fd);
	return 0;
}

static int get_format_type(const char *path)
{
	unsigned int size;

	if (__check_mmc_fs_type(path) != 0) {
		PRT_TRACE_ERR("fail to check mount point %s", path);
		return -1;
	}
	if (inserted_type == FS_TYPE_EXT4)
		return 0;

	return 0;
}

static const char **get_argument(const char *path)
{
	int argc;

	switch (inserted_type) {
	case FS_TYPE_FAT:
		argc = ARRAY_SIZE(vfat_arg);
		vfat_arg[argc - 2] = path;
		return vfat_arg;
	case FS_TYPE_EXT4:
		argc = ARRAY_SIZE(ext4_arg);
		ext4_arg[argc - 2] = path;
		return ext4_arg;
	default:
		break;
	}
	return NULL;
}

static int check_mount_state(void)
{
	struct stat parent_stat, mount_stat;
	char parent_path[PATH_MAX];
	char mount_path[PATH_MAX];

	snprintf(parent_path, sizeof(parent_path), "%s", MMC_PARENT_PATH);
	snprintf(mount_path, sizeof(mount_path), "%s", MMC_MOUNT_POINT);

	if (stat(mount_path, &mount_stat) != 0 || stat(parent_path, &parent_stat) != 0) {
		PRT_TRACE_ERR("get stat error");
		return 0;
	}

	if (mount_stat.st_dev == parent_stat.st_dev) {
		PRT_TRACE_ERR("state : UNMOUNT");
		return 0;
	}

	PRT_TRACE_ERR("state : MOUNT");
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

static int format_mmc(const char *path)
{
	mmc_fs_type type;
	unsigned int size;
	int mkfs_pid;
	const char **argv;
	char buf[NAME_MAX];
	int r;

	if (path == NULL) {
		PRT_TRACE_ERR("Invalid parameter");
		return -1;
	}

	if (get_format_type(path) < 0) {
		PRT_TRACE_ERR("Invalid parameter");
		return -1;
	}
	argv = get_argument(path);
	if (argv == NULL) {
		PRT_TRACE_ERR("get_argument fail");
		return -1;
	}

	mkfs_pid = exec_process(argv);
	if (mkfs_pid < 0) {
		PRT_TRACE_ERR("%s fail");
		return -1;
	}

	snprintf(buf, sizeof(buf), "%s%d", "/proc/", mkfs_pid);
	PRT_TRACE_ERR("child process : %s", buf);
	while (1) {
		sleep(1);
		PRT_TRACE_ERR("formatting....");
		if (access(buf, R_OK) != 0)
			break;
	}

	return 0;
}

static int format_exec(int blknum)
{
	char dev_mmcblk[NAME_MAX];
	char dev_mmcblkp[NAME_MAX];
	int r;

	if (check_mount_state() == 1) {
		PRT_TRACE_ERR("Mounted, will be unmounted");
		r = __umount_fs();
		if (r != 0) {
			PRT_TRACE_ERR("unmount_mmc fail");
			vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_FAILED);
			return -1;
		}
	}

	snprintf(dev_mmcblk, sizeof(dev_mmcblk), "%s%d", MMC_DEV, blknum);
	snprintf(dev_mmcblkp, sizeof(dev_mmcblkp), "%sp1", dev_mmcblk);
	if (access(dev_mmcblkp, R_OK) < 0) {
		PRT_TRACE_ERR("%s is not valid, create the primary partition", dev_mmcblkp);
		r = create_partition(dev_mmcblk);
		if (r < 0) {
			PRT_TRACE_ERR("create_partition failed");
			vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_FAILED);
			heynoti_publish("mmcblk_remove");
			return -1;
		}
	}

	r = format_mmc(dev_mmcblkp);
	if (r < 0) {
		PRT_TRACE_ERR("format_mmc fail");
		vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_FAILED);
		heynoti_publish("mmcblk_remove");
		return -1;
	}

	PRT_TRACE_ERR("Format Successful");
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT, VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED);
	return 0;
}

static const char **get_check_argument(const char *path)
{
	int argc;

	switch (inserted_type) {
	case FS_TYPE_EXT4:
		argc = ARRAY_SIZE(ext4_check_arg);
		ext4_check_arg[argc - 2] = path;
		return ext4_check_arg;
	default:
		break;
	}
	return NULL;
}

static int mmc_check_process_launch(int argc,  char **argv)
{
	const char **params;
	char buf[NAME_MAX];
	int pid;

	params = get_check_argument((const char*)argv[1]);
	if ((pid = exec_process(params)) < 0) {
		PRT_TRACE_ERR("mmc checker failed");
		goto run_mount;
	}

	snprintf(buf, sizeof(buf), "%s%d", "/proc/", pid);
	PRT_TRACE_ERR("child process : %s", buf);

	while (1) {
		sleep(1);
		PRT_TRACE_ERR("mmc checking ....");
		if (access(buf, R_OK) != 0)
			break;
	}
run_mount:
	ss_action_entry_call_internal(PREDEF_CHECK_MMC,0);
	return 0;

}

int ss_mmc_unmounted(int argc, char **argv)
{
	int option = -1;
	int mmc_err = 0;
	if (argc < 1) {
		PRT_TRACE_ERR("Option is wong");
		return -1;
	}
	if ((option = atoi(argv[0])) < 0) {
		PRT_TRACE_ERR("Option is wong : %d", option);
		return -1;
	}

	if (umount2(MMC_MOUNT_POINT, option) != 0) {
		mmc_err = errno;
		PRT_TRACE_ERR("Failed to unmount mmc card\n");
		vconf_set_int(VCONFKEY_SYSMAN_MMC_UNMOUNT,
			      VCONFKEY_SYSMAN_MMC_UNMOUNT_FAILED);
		vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS,
				mmc_err);
		return -1;

	}
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS,
		      VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_UNMOUNT,
		      VCONFKEY_SYSMAN_MMC_UNMOUNT_COMPLETED);
	return 0;
}


static int __ss_rw_mount(const char* szPath)
{
	struct statvfs mount_stat;
	if (!statvfs(szPath, &mount_stat)) {
		if ((mount_stat.f_flag & ST_RDONLY) == ST_RDONLY)
			return -1;
	}
	return 0;
}

static int __ss_check_smack_popup(void)
{
	bundle *b = NULL;
	int ret = -1;
	int val = -1;

	b = bundle_create();
	bundle_add(b, "_SYSPOPUP_CONTENT_", "checksmack");
	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &val);
	if (val == 1 || ret != 0) {
		if ((mmc_popup_pid = syspopup_launch("mmc-syspopup", b)) < 0) {
			PRT_TRACE_EM("popup launch failed\n");
		}
	}
	bundle_free(b);

	if (ss_action_entry_call_internal(PREDEF_CHECK_SMACK_MMC, 0) < 0) {
		PRT_TRACE_ERR("fail to launch mmc checker,direct mount mmc");
	}
	return 0;

}

static int __mount_fs(char *path, const char *fs_name, const char *mount_data)
{
	int ret, retry = 0;

	do {
		if ((ret = mount(path, MMC_MOUNT_POINT, fs_name, 0, mount_data)) == 0) {
			PRT_TRACE_ERR("Mounted mmc card %s", fs_name);
			return 0;
		}
		usleep(100000);
	} while (ret == -1 && errno == ENOENT && retry++ < 10);

	return errno;
}

static int __mmc_mount_fs(void)
{
	char buf[NAME_MAX];
	char params[NAME_MAX];
	char options[NAME_MAX];
	int fd;
	int blk_num;

	if (access(MMC_MOUNT_POINT, R_OK) != 0) {
		if (mkdir(MMC_MOUNT_POINT, 0755) < 0) {
			PRT_TRACE_ERR("Make Directory is failed");
			return errno;
		}
	}

	blk_num = get_mmcblk_num();
	if (blk_num  == -1) {
		PRT_TRACE_ERR("fail to check mmc block");
		return -1;
	}

	snprintf(buf, sizeof(buf), "%s%dp1", MMC_DEV, blk_num);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		PRT_TRACE_ERR("can't open the '%s': %s", buf, strerror(errno));
		snprintf(buf, sizeof(buf), "%s%d", MMC_DEV, blk_num);
	} else
		close(fd);

	switch (inserted_type) {
	case FS_TYPE_FAT:
		if (smack)
			snprintf(options, sizeof(options), "%s,%s", FS_VFAT_MOUNT_OPT, SMACKFS_MOUNT_OPT);
		else
			snprintf(options, sizeof(options), "%s", FS_VFAT_MOUNT_OPT);
		if (__mount_fs(buf, "vfat", options) != 0)
			return -1;
		break;
	case FS_TYPE_EXT4:
		if (__mount_fs(buf, "ext4", NULL) != 0)
			return errno;
		if (smack) {
			if (__ss_check_smack_popup() != 0)
				return -1;
		}
		return smack;
	}
	return 0;
}

static int mmc_check_mount(int argc,  char **argv)
{
	int r;
	r = __mmc_mount_fs();
	if (r == 0)
		goto mount_complete;
	else if (r == 1)
		goto mount_wait;
	else if (r == -1)
		goto mount_fail;
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_FAILED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, errno);
	PRT_TRACE_ERR("Failed to mount mmc card\n");
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
	return 0;
mount_wait:
	PRT_TRACE_ERR("wait ext4 smack rule checking");
	return 0;
}

static int __check_mmc_fs(void)
{
	char buf[NAME_MAX];
	char params[NAME_MAX];
	char options[NAME_MAX];
	int fd;
	int blk_num;
	int ret = 0;

	if (access(MMC_MOUNT_POINT, R_OK) != 0) {
		if (mkdir(MMC_MOUNT_POINT, 0755) < 0) {
			PRT_TRACE_ERR("Make Directory is failed");
			return errno;
		}
	}

	blk_num = get_mmcblk_num();
	if (blk_num  == -1) {
		PRT_TRACE_ERR("fail to check mmc block");
		return -1;
	}

	snprintf(buf, sizeof(buf), "%s%dp1", MMC_DEV, blk_num);
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		PRT_TRACE_ERR("can't open the '%s': %s", buf, strerror(errno));
		snprintf(buf, sizeof(buf), "%s%d", MMC_DEV, blk_num);
	}
	close(fd);

	if (__check_mmc_fs_type(buf) != 0) {
		PRT_TRACE_ERR("fail to check mount point %s", buf);
		return -1;
	}
	snprintf(params, sizeof(params), "%d", inserted_type);
	if (ss_action_entry_call_internal(PREDEF_CHECK_MMC_PROC, 2, params, buf) < 0) {
		PRT_TRACE_ERR("fail to launch mmc checker,direct mount mmc");
		ret = mmc_check_mount(0, NULL);
	}
	return ret;
}

int ss_mmc_inserted(void)
{
	int mmc_status;
	int ret;

	vconf_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &mmc_status);

	if (mmc_status == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		PRT_DBG("Mmc is already mounted.\n");
		vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_ALREADY);
		return 0;
	}

	if ((ret = __check_mmc_fs()) != 0)
		PRT_TRACE_ERR("fail to check mmc");
	return ret;
}

int ss_mmc_removed(void)
{
	int mmc_err = 0;
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_REMOVED);
	mmc_err = __umount_fs();
	vconf_set_int(VCONFKEY_SYSMAN_MMC_ERR_STATUS, mmc_err);

	return 0;
}

static int ss_mmc_format(int argc, char **argv)
{
	PRT_TRACE_ERR("mmc format called");
	__umount_fs();

	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS, VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NOW);
	format_exec(get_mmcblk_num());
	vconf_set_int(VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS, VCONFKEY_SYSMAN_MMC_FORMAT_PROGRESS_NONE);

	if (__check_mmc_fs() != 0)
		PRT_TRACE_ERR("fail to check mmc");

	return 0;
}

static int ss_mmc_check_smack(int argc, char **argv)
{
	int smack_checked;
	int pid;
	system(FS_EXT4_SMACK_LABEL);
	PRT_TRACE_ERR("smack labeling script is run(%d)", smack_checked);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	if (mmc_popup_pid > 0) {
		PRT_TRACE_ERR("will be killed mmc-popup(%d)", mmc_popup_pid);
		kill(mmc_popup_pid, SIGTERM);
	}
	return 0;
}

static void mmc_init(void *data)
{
	ss_action_entry_add_internal(PREDEF_MOUNT_MMC, ss_mmc_inserted, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_UNMOUNT_MMC, ss_mmc_unmounted, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_FORMAT_MMC, ss_mmc_format, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_CHECK_SMACK_MMC, ss_mmc_check_smack, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_CHECK_MMC, mmc_check_mount, NULL, NULL);
	ss_action_entry_add_internal(PREDEF_CHECK_MMC_PROC, mmc_check_process_launch, NULL, NULL);
	/* mmc card mount */
	if (__check_mmc_fs() != 0)
		PRT_TRACE_ERR("fail to check mmc");
}

const struct device_ops mmc_device_ops = {
	.init = mmc_init,
};
