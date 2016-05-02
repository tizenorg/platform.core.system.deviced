/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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

#define SMACKFS_MOUNT_OPT   "smackfsroot=*,smackfsdef=*"

#define RETRY_COUNT         10

enum block_fs_type {
	FS_TYPE_VFAT = 0,
	FS_TYPE_EXT4,
};

struct block_fs_ops {
	enum block_fs_type type;
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

void add_fs(const struct block_fs_ops *fs);
void remove_fs(const struct block_fs_ops *fs);

enum block_device_type {
	BLOCK_SCSI_DEV,
	BLOCK_MMC_DEV,
};

enum mount_state {
	BLOCK_UNMOUNT,
	BLOCK_MOUNT,
};

enum unmount_operation {
	UNMOUNT_NORMAL,
	UNMOUNT_FORCE,
};

struct block_data {
	enum block_device_type block_type;
	char *devnode;
	char *syspath;
	char *fs_usage;
	char *fs_type;
	char *fs_version;
	char *fs_uuid_enc;
	bool readonly;
	char *mount_point;
	enum mount_state state;
	bool primary;   /* the first partition */
	int flags;
	int id;  /* libstorage uses the id as the storage_id */
};

struct block_dev_ops {
	const char *name;
	enum block_device_type block_type;
	void (*mounted) (struct block_data *data, int result);
	void (*unmounted) (struct block_data *data, int result);
	void (*formatted) (struct block_data *data, int result);
	void (*inserted) (struct block_data *data);
	void (*removed) (struct block_data *data);
};

void add_block_dev(const struct block_dev_ops *ops);
void remove_block_dev(const struct block_dev_ops *ops);

#define BLOCK_DEVICE_OPS_REGISTER(dev) \
static void __CONSTRUCTOR__ block_dev_init(void) \
{ \
	add_block_dev(dev); \
} \
static void __DESTRUCTOR__ block_dev_exit(void) \
{ \
	remove_block_dev(dev); \
}

enum block_flags {
	FLAG_NONE        = 0,
	UNMOUNT_UNSAFE   = 1 << 0,
	FS_BROKEN        = 1 << 1,
	FS_EMPTY         = 1 << 2,
	FS_NOT_SUPPORTED = 1 << 3,
	MOUNT_READONLY   = 1 << 4,
};

/* a: struct block_data
 * b: enum block_flags  */
#define BLOCK_FLAG_SET(a, b) \
	do { \
		if (a) { \
			(((a)->flags) |= (b)); \
		} \
	} while (0)

#define BLOCK_FLAG_UNSET(a, b) \
	do { \
		if (a) { \
			(((a)->flags) &= ~(b)); \
		} \
	} while (0)

#define BLOCK_IS_FLAG_SET(a, b) \
	((a) ? ((((a)->flags) & (b)) ? true : false) : false)

#define BLOCK_FLAG_CLEAR_ALL(a) \
	do { \
		if (a) { \
			((a)->flags) = FLAG_NONE; \
		} \
	} while (0)

#define BLOCK_FLAG_MOUNT_CLEAR(a) \
	do { \
		BLOCK_FLAG_UNSET((a), FS_BROKEN); \
		BLOCK_FLAG_UNSET((a), FS_EMPTY); \
		BLOCK_FLAG_UNSET((a), FS_NOT_SUPPORTED); \
		BLOCK_FLAG_UNSET((a), MOUNT_READONLY); \
	} while (0)

#define BLOCK_FLAG_UNMOUNT_CLEAR(a) \
	do { \
		BLOCK_FLAG_UNSET((a), UNMOUNT_UNSAFE); \
	} while (0)

#define BLOCK_GET_MOUNT_FLAGS(a, c) \
	do { \
		(c) = (a)->flags; \
		(c) &= ~UNMOUNT_UNSAFE; \
	} while (0)

#define BLOCK_GET_UNMOUNT_FLAGS(a, c) \
	do { \
		(c) = 0; \
		if (BLOCK_IS_FLAG_SET((a), UNMOUNT_UNSAFE)) \
			(c) |= UNMOUNT_UNSAFE; \
	} while (0)

#define BLOCK_GET_FORMAT_FLAGS(a, c) \
	do { \
		(c) = 0; \
		if (BLOCK_IS_FLAG_SET((a), FS_NOT_SUPPORTED)) \
			(c) |= FS_NOT_SUPPORTED; \
	} while (0)

#endif /* __BLOCK_H__ */
