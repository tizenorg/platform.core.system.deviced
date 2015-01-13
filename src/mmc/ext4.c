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
#include <stdlib.h>
#include <limits.h>
#include <vconf.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "core/common.h"
#include "core/devices.h"
#include "core/log.h"
#include "mmc-handler.h"

#define FS_EXT4_NAME	"ext4"

#define FS_EXT4_SMACK_LABEL "/usr/bin/mmc-smack-label"

static const char *ext4_arg[] = {
	"/sbin/mkfs.ext4",
	NULL, NULL,
};

static const char *ext4_check_arg[] = {
	"/sbin/fsck.ext4",
	"-f", "-y", NULL, NULL,
};

static struct fs_check ext4_info = {
	FS_TYPE_EXT4,
	"ext4",
	0x438,
	2,
	{0x53, 0xef},
};

static int mmc_popup_pid;

static bool ext4_match(const char *devpath)
{
	char buf[4];
	int fd, r;

	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		_E("failed to open fd(%s) : %s", devpath, strerror(errno));
		return false;
	}

	/* check fs type with magic code */
	r = lseek(fd, ext4_info.offset, SEEK_SET);
	if (r < 0)
		goto error;

	r = read(fd, buf, 2);
	if (r < 0)
		goto error;

	_I("mmc search magic : 0x%2x, 0x%2x", buf[0],buf[1]);
	if (memcmp(buf, ext4_info.magic, ext4_info.magic_sz))
		goto error;

	close(fd);
	_I("MMC type : %s", ext4_info.name);
	return true;

error:
	close(fd);
	_E("failed to match with ext4(%s)", devpath);
	return false;
}

static int ext4_check(const char *devpath)
{
	int argc;
	argc = ARRAY_SIZE(ext4_check_arg);
	ext4_check_arg[argc - 2] = devpath;
	return run_child(argc, ext4_check_arg);
}

static int mmc_check_smack(const char *mount_point)
{
	char buf[NAME_MAX] = {0,};

	snprintf(buf, sizeof(buf), "%s", mount_point);
	launch_evenif_exist(FS_EXT4_SMACK_LABEL, buf);

	if (mmc_popup_pid > 0) {
		_E("will be killed mmc-popup(%d)", mmc_popup_pid);
		kill(mmc_popup_pid, SIGTERM);
	}
	return 0;
}

static int check_smack_popup(void)
{
	int ret = -1;
	int val = -1;
	static const struct device_ops *apps = NULL;
	notification_h noti;

	ret = vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &val);
	if (val == 1 || ret != 0) {

		FIND_DEVICE_INT(apps, "apps");

		ret = manage_notification(noti, MMC_POPUP_NAME, MMC_POPUP_SMACK_VALUE);
		if (ret == -1)
			return -1;
	}

	return 0;
}

static int ext4_mount(bool smack, const char *devpath, const char *mount_point)
{
	int r, retry = RETRY_COUNT;

	do {
		r = mount(devpath, mount_point, "ext4", 0, NULL);
		if (!r) {
			_I("Mounted mmc card [ext4]");
			if (smack) {
				check_smack_popup();
				mmc_check_smack(mount_point);
			}
			return 0;
		}
		_I("mount fail : r = %d, err = %d", r, errno);
		usleep(100000);
	} while (r < 0 && errno == ENOENT && retry-- > 0);

	return -errno;
}

static int ext4_format(const char *devpath)
{
	int argc;
	argc = ARRAY_SIZE(ext4_arg);
	ext4_arg[argc - 2] = devpath;
	return run_child(argc, ext4_arg);
}

static const struct mmc_fs_ops ext4_ops = {
	.type = FS_TYPE_EXT4,
	.name = "ext4",
	.match = ext4_match,
	.check = ext4_check,
	.mount = ext4_mount,
	.format = ext4_format,
};

static void __CONSTRUCTOR__ module_init(void)
{
	add_fs(&ext4_ops);
}
/*
static void __DESTRUCTOR__ module_exit(void)
{
	remove_fs(&ext4_ops);
}
*/
