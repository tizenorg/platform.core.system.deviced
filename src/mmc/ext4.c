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

#define FS_EXT4_SMACK_LABEL "mmc-smack-label "MMC_MOUNT_POINT

static const char *ext4_arg[] = {
	"/sbin/mkfs.ext4",
	NULL, NULL,
};

static const char *ext4_check_arg[] = {
	"/sbin/fsck.ext4",
	"-f", "-y", NULL, NULL,
};

static struct fs_check fs_ext4_type = {
	FS_TYPE_EXT4,
	"ext4",
	0x438,
	2,
	{0x53, 0xef},
};

static int mmc_popup_pid;

static int mmc_check_smack(void)
{
	system(FS_EXT4_SMACK_LABEL);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, VCONFKEY_SYSMAN_MMC_MOUNTED);
	vconf_set_int(VCONFKEY_SYSMAN_MMC_MOUNT, VCONFKEY_SYSMAN_MMC_MOUNT_COMPLETED);
	if (mmc_popup_pid > 0) {
		PRT_TRACE_ERR("will be killed mmc-popup(%d)", mmc_popup_pid);
		kill(mmc_popup_pid, SIGTERM);
	}
	return 0;
}

static int check_smack_popup(void)
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

	mmc_check_smack();
	return 0;
}

static int ext4_init(void *data)
{
	int fd, ret;
	int argc;
	char buf[BUF_LEN];
	char *path = (char *)data;

	argc = ARRAY_SIZE(ext4_check_arg);
	ext4_check_arg[argc - 2] = path;

	if ((fd = open(path, O_RDONLY)) < 0) {
		PRT_TRACE_ERR("can't open the '%s': %s", path, strerror(errno));
		return -EINVAL;
	}
	/* check fs type with magic code */
	lseek(fd, fs_ext4_type.offset, SEEK_SET);
	ret = read(fd, buf, 2);
	PRT_TRACE("mmc search magic : 0x%2x, 0x%2x", buf[0],buf[1]);
	if (!memcmp(buf, fs_ext4_type.magic, fs_ext4_type.magic_sz)) {
		PRT_TRACE("mmc type : %s", fs_ext4_type.name);
		close(fd);

		return fs_ext4_type.type;
	}
	close(fd);
	return -EINVAL;
}

static const char **ext4_check(void)
{
	return ext4_check_arg;
}

static int ext4_mount(int smack, void *data)
{
	PRT_TRACE_ERR("ext4_mount");
	if (mount_fs((char *)data, "ext4", NULL) != 0)
		return errno;
	if (smack) {
		if (check_smack_popup() != 0)
			return -1;
	}
	return smack;
}

static const char **ext4_format(void *data)
{
	int argc;
	char *path = (char *)data;
	argc = ARRAY_SIZE(ext4_arg);
	ext4_arg[argc - 2] = path;
	return ext4_arg;
}

const struct mmc_filesystem_ops ext4_ops = {
	.init = ext4_init,
	.check = ext4_check,
	.mount = ext4_mount,
	.format = ext4_format,
};
