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
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "core/common.h"
#include "core/log.h"
#include "block.h"

#define FS_VFAT_NAME	"mkdosfs"

#define FS_VFAT_MOUNT_OPT  "uid=0,gid=0,dmask=0000,fmask=0111,iocharset=iso8859-1,utf8,shortname=mixed"

static const char *vfat_arg[] = {
	"/usr/bin/newfs_msdos",
	"-F", "32", "-O", "tizen", "-c", "8", NULL, NULL,
};

static const char *vfat_check_arg[] = {
	"/usr/bin/fsck_msdosfs",
	"-pf", NULL, NULL,
};

static struct fs_check vfat_info = {
	FS_TYPE_VFAT,
	"vfat",
	0x1fe,
	2,
	{0x55, 0xAA},
};

static int vfat_check(const char *devpath)
{
	int argc, r, pass = 0;

	argc = ARRAY_SIZE(vfat_check_arg);
	vfat_check_arg[argc - 2] = devpath;

	do {
		r = run_child(argc, vfat_check_arg);

		switch (r) {
		case 0:
			_I("filesystem check completed OK");
			return 0;
		case 2:
			_I("file system check failed (not a FAT filesystem)");
			errno = ENODATA;
			return -1;
		case 4:
			if (pass++ <= 2) {
				_I("filesystem modified - rechecking (pass : %d)", pass);
				continue;
			}
			_I("failing check after rechecks, but file system modified");
			return 0;
		default:
			_I("filesystem check failed (unknown exit code %d)", r);
			errno = EIO;
			return -1;
		}
	} while (1);

	return 0;
}

static bool vfat_match(const char *devpath)
{
	int r;

	r = vfat_check(devpath);
	if (r < 0) {
		_E("failed to match with vfat(%s)", devpath);
		return false;
	}

	_I("MMC type : %s", vfat_info.name);
	return true;
}

static int vfat_mount(bool smack, const char *devpath, const char *mount_point)
{
	char options[NAME_MAX];
	int r, retry = RETRY_COUNT;
	struct timespec time = {0,};

	if (smack)
		snprintf(options, sizeof(options), "%s,%s", FS_VFAT_MOUNT_OPT, SMACKFS_MOUNT_OPT);
	else
		snprintf(options, sizeof(options), "%s", FS_VFAT_MOUNT_OPT);

	do {
		r = mount(devpath, mount_point, "vfat", 0, options);
		if (!r) {
			_I("Mounted mmc card [vfat]");
			return 0;
		}
		_I("mount fail : r = %d, err = %d", r, errno);
		time.tv_nsec = 100 * NANO_SECOND_MULTIPLIER;
		nanosleep(&time, NULL);
	} while (r < 0 && errno == ENOENT && retry-- > 0);

	return -errno;
}

static int vfat_format(const char *devpath)
{
	int argc;
	argc = ARRAY_SIZE(vfat_arg);
	vfat_arg[argc - 2] = devpath;
	return run_child(argc, vfat_arg);
}

static const struct block_fs_ops vfat_ops = {
	.type = FS_TYPE_VFAT,
	.name = "vfat",
	.match = vfat_match,
	.check = vfat_check,
	.mount = vfat_mount,
	.format = vfat_format,
};

static void __CONSTRUCTOR__ module_init(void)
{
	add_fs(&vfat_ops);
}
/*
static void __DESTRUCTOR__ module_exit(void)
{
	_I("module exit");
	remove_fs(&vfat_ops);
}
*/
