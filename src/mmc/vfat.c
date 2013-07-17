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

#include "mmc-handler.h"

#define FS_VFAT_MOUNT_OPT  "uid=0,gid=0,dmask=0000,fmask=0111,iocharset=iso8859-1,utf8,shortname=mixed"

static const char *vfat_arg[] = {
	"/sbin/mkfs.vfat",
	NULL, NULL,
};

static const char *vfat_check_arg[] = {
	"/sbin/fsck.vfat",
	"-a", NULL, NULL,
};

static struct fs_check fs_vfat_type = {
	FS_TYPE_FAT,
	"vfat",
	0x1fe,
	2,
	{0x55, 0xaa},
};

static int vfat_init(void *data)
{
	int fd, ret, argc;
	char buf[BUF_LEN];
	char *path = (char *)data;

	argc = ARRAY_SIZE(vfat_check_arg);
	vfat_check_arg[argc - 2] = path;

	if ((fd = open(path, O_RDONLY)) < 0) {
		PRT_TRACE_ERR("can't open the '%s': %s", path, strerror(errno));
		return -EINVAL;
	}
	/* check fs type with magic code */
	lseek(fd, fs_vfat_type.offset, SEEK_SET);
	ret = read(fd, buf, 2);
	PRT_TRACE("mmc search magic : 0x%2x, 0x%2x", buf[0],buf[1]);
	if (!memcmp(buf, fs_vfat_type.magic, fs_vfat_type.magic_sz)) {
		PRT_TRACE("mmc type : %s", fs_vfat_type.name);
		close(fd);
		return fs_vfat_type.type;
	}
	close(fd);
	return -EINVAL;
}

static const char ** vfat_check(void)
{
	return vfat_check_arg;
}

static int vfat_mount(int smack, void *data)
{
	char options[NAME_MAX];

	PRT_TRACE_ERR("vfat_mount");
	if (smack)
		snprintf(options, sizeof(options), "%s,%s", FS_VFAT_MOUNT_OPT, SMACKFS_MOUNT_OPT);
	else
		snprintf(options, sizeof(options), "%s", FS_VFAT_MOUNT_OPT);
	if (mount_fs((char *)data, "vfat", options) != 0)
		return -1;
	return 0;
}

static const char **vfat_format(void *data)
{
	int argc;
	char *path = (char *)data;
	argc = ARRAY_SIZE(vfat_arg);
	vfat_arg[argc - 2] = path;
	return vfat_arg;
}

const struct mmc_filesystem_ops vfat_ops = {
	.init = vfat_init,
	.check = vfat_check,
	.mount = vfat_mount,
	.format = vfat_format,
};

