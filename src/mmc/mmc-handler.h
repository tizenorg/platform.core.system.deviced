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

#include <stdbool.h>
#include <tzplatform_config.h>

#define SMACKFS_MOUNT_OPT		"smackfsroot=*,smackfsdef=*"
#define MMC_MOUNT_POINT		tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard")

#define BUF_LEN		20
#define RETRY_COUNT	10

enum mmc_fs_type {
	FS_TYPE_VFAT = 0,
	FS_TYPE_EXT4,
};

struct mmc_fs_ops {
	enum mmc_fs_type type;
	const char *name;
	bool (*match) (const char *);
	int (*check) (const char *);
	int (*mount) (bool, const char *, const char *);
	int (*format) (const char *);
};

struct fs_check {
	int type;
	char *name;
	unsigned int offset;
	unsigned int magic_sz;
	char magic[4];
};

void add_fs(const struct mmc_fs_ops *fs);
void remove_fs(const struct mmc_fs_ops *fs);
int get_mmc_devpath(char devpath[]);
bool mmc_check_mounted(const char *mount_point);

int get_block_number(void);

void mmc_mount_done(void);
#endif /* __MMC_HANDLER_H__ */
