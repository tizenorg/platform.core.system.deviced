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


#ifndef __MMC_HANDLER_H__
#define __MMC_HANDLER_H__

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <vconf.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <bundle.h>
#include <signal.h>
#include <stdbool.h>

#include "core/log.h"
#include "core/device-handler.h"
#include "core/common.h"
#include "core/devices.h"
#include <tzplatform_config.h>

#define BUF_LEN             20

#define SMACKFS_MOUNT_OPT		"smackfsroot=*,smackfsdef=*"
#define MMC_MOUNT_POINT		tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard")

struct mmc_list {
struct mmc_list *prev, *next;
};

enum mmc_fs_type {
	FS_TYPE_NONE,
	FS_TYPE_FAT,
	FS_TYPE_EXFAT,
	FS_TYPE_EXT4,
};

struct fs_check {
	int type;
	char *name;
	unsigned int offset;
	unsigned int magic_sz;
	char magic[4];
};

struct mmc_filesystem_ops {
	int (*init) (void *data);
	const char ** (*check) (void);
	int (*mount) (int smack, void *data);
	const char ** (*format) (void *data);
};

struct mmc_filesystem_info {
	char *name;
	struct mmc_filesystem_ops *fs_ops;
	struct mmc_list list;
};

int mount_fs(char *path, const char *fs_name, const char *mount_data);
int register_mmc_handler(const char *name, struct mmc_filesystem_ops filesystem_type);

#endif /* __MMC_HANDLER_H__ */
