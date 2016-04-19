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

#ifndef __TZIP_UTILITY_H__
#define __TZIP_UTILITY_H__

#include <stdio.h>
#include <glib.h>

#define TZIP_ROOT_PATH "/run/tzip"

#define MAX_FILENAME_LEN 256

#define DIR_DELIMETER '/'

#define DEFAULT_FILE_MODE 0755

#define TZIP_INFO_FILE "/run/.deviced"

#define TZIP_MSGQ_NAME "/tzipmsgq"

#define CASE_SENSITIVE	1

#define MAX_CHUNK_SIZE	4096

/* structure for storing a file info */
struct tzip_file_info {
	char *name;
	struct stat  stat;
};

/* structure for storing a dir info */
struct tzip_dir_info {
	char *name;
	struct stat  stat;
	GHashTable *file_hash; /* hash table for storing all files present in a given directory */
};

/* table containing mount path and corresponding files */
struct tzip_mount_entry {
	char *path;
	char *zip_path;
	GHashTable *dir_hash; /* hash table for storing all directory info of a given zip file */
};

/* Structure containing Zip file handle */
struct tzip_handle {
	unzFile *zipfile;
	unz_file_info *file_info;
	char *path;
	char *file;
	off_t offset;
};

/* structure containing message queue data */
struct tzip_msg_data {
	char type; /* 'm' for mount , 'u' for unmount */
	char *zippath;
	char *mountpath;
};

struct tzip_dir_info *get_dir_files(const char *dir);
int add_mount_entry(const char *zip_path, const char *mount_path);
int remove_mount_entry(const char *mount_path);
int get_path_prop(const char *path, struct stat *stbuf);
struct tzip_mount_entry *get_mount_entry(const char *mount_path);
struct tzip_mount_entry *find_mount_entry(const char *mount_path, int *dir_status);
void tzip_lock_init(void);
void tzip_lock_deinit(void);
void tzip_lock(void);
void tzip_unlock(void);

/* local function declrations */
int get_file_info(const char *file_path, struct tzip_file_info *file_info);
void copy_file_stat(struct stat  *src, struct stat  *dest);
int extract_zipfile_details(struct tzip_mount_entry *node, const char *zip_path);
int add_dir_info(struct tzip_mount_entry *mnode, const char *parent_dir, const char *dir, unz_file_info *file_info, mode_t mode);
void fileinfo_to_stat(unz_file_info *file_info, struct stat *file_stat, mode_t mode);
struct tzip_dir_info *get_dir_list(struct tzip_mount_entry *dir_node, const char *dir_name);
void free_mount_node(struct tzip_mount_entry *mount_node);
int read_zipfile(struct tzip_handle *handle, char *buf, size_t size, off_t offset);
GHashTable *hashmap_init(void);
GHashTable *get_hashmap(void);
int tzip_store_mount_info(const char* zip_path, const char* mount_path);
int tzip_remount_zipfs(const char *src_file, const char *mount_point);
#endif /* __TZIP_UTILITY_H__ */

