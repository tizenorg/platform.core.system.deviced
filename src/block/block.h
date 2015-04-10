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


#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <stdbool.h>
#include "core/common.h"

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

enum block_device_type {
	BLOCK_SCSI_DEV,
	BLOCK_MMC_DEV,
};

enum unmount_operation {
	UNMOUNT_NORMAL,
	UNMOUNT_FORCE,
};

struct block_data {
	enum block_device_type type;
	char *devpath;
	char *mount_point;
	bool deleted;
};

struct block_dev_ops {
	const char *name;
	enum block_device_type type;
	void (*init) (void *data);
	void (*exit) (void *data);
	int (*mount) (struct block_data *data, int result);
	int (*unmount) (struct block_data *data, int result);
	int (*format) (struct block_data *data, int result);
	int (*remove) (struct block_data *data, int result);
};

void add_block_dev(const struct block_dev_ops *dev);
void remove_block_dev(const struct block_dev_ops *dev);

#define BLOCK_DEVICE_OPS_REGISTER(dev) \
static void __CONSTRUCTOR__ module_init(void) \
{ \
	add_block_dev(dev); \
} \
static void __DESTRUCTOR__ module_exit(void) \
{ \
	remove_block_dev(dev); \
}

#define SMACKFS_MOUNT_OPT   "smackfsroot=*,smackfsdef=*"
#define RETRY_COUNT         10

int mount_block_device(const char *devpath);
int unmount_block_device(const char *devpath, enum unmount_operation option);
int format_block_device(const char *devpath, enum unmount_operation option);
int add_block_device(const char *devpath);
int remove_block_device(const char *devpath);

#endif /* __BLOCK_H__ */
