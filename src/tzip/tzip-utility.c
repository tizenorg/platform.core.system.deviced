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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unzip.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <stdarg.h>
#include "core/log.h"

#include "tzip-utility.h"

static sem_t tzip_sem;

static GHashTable *hashmap;

struct tzip_mount_entry *get_mount_entry(const char *mount_path)
{
	struct tzip_mount_entry *entry;
	entry = (struct tzip_mount_entry *)g_hash_table_lookup(
			hashmap, mount_path);
	return entry;
}

struct tzip_mount_entry *find_mount_entry(
		const char *mount_path, int *dir_status)
{
	int len, mlen;
	GHashTableIter iter;
	gpointer key, value;
	char *path;

	*dir_status = -1;
	len = strlen(mount_path);

	g_hash_table_iter_init(&iter, hashmap);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		path = (char *)key;
		/*
		* FUSE checks directory status in recursive passion.
		* For example if mount path = "/tmp/d1",
		* FUSE first checks status of "/tmp" and proceeds to check
		* "/tmp/d1" only if status "/tmp" is success.
		*
		* This function takes care of this scenario by setting dir_status = 1
		* to indicate parent directory of mount path is found
		*/
		mlen =  strlen(path);
		if (len < mlen) {
			if (strncmp(mount_path, path, len) == 0) {
				/*
				* Dont break, till exact match is found for node->path.
				* Just mark *dir_status = 1  to indicate
				* parent directory match is found
				*/
				*dir_status = 1;
			}
		} else if (mlen == len &&
				strncmp(mount_path, path, mlen) == 0) {
			/* Break, exact match is found for node->path */
			*dir_status = 1;
			return get_mount_entry(path);
		} else if (strncmp(mount_path, path, mlen) == 0 &&
				mount_path[mlen] == DIR_DELIMETER) {
			/* Break, mount directory found for a given file or directory */
			*dir_status = 1;
			return get_mount_entry(path);
		}
	}

	return NULL;
}

struct tzip_dir_info *get_dir_list(
		struct tzip_mount_entry *entry, const char *dir_name)
{
	struct tzip_dir_info *dinfo;

	dinfo = (struct tzip_dir_info *)g_hash_table_lookup(
			entry->dir_hash, dir_name);

	if (!dinfo)
		_D("Empty Folder  %s", dir_name);

	return dinfo;
}

void fileinfo_to_stat(unz_file_info *file_info, struct stat *file_stat, mode_t mode)
{
	struct tm newdate;
	time_t file_time;
	struct timeval tv;

	if (file_info) {
		newdate.tm_sec = file_info->tmu_date.tm_sec;
		newdate.tm_min = file_info->tmu_date.tm_min;
		newdate.tm_hour = file_info->tmu_date.tm_hour;
		newdate.tm_mday = file_info->tmu_date.tm_mday;
		newdate.tm_mon = file_info->tmu_date.tm_mon;
		if (file_info->tmu_date.tm_year > 1900)
			newdate.tm_year = file_info->tmu_date.tm_year - 1900;
		else
			newdate.tm_year = file_info->tmu_date.tm_year ;
		newdate.tm_isdst = -1;

		file_time = mktime(&newdate);
	} else {
		/* add current time for mount directory */
		gettimeofday(&tv, NULL);
		file_time = tv.tv_sec;
	}

	file_stat->st_mode = mode;
	file_stat->st_atime = file_time;
	file_stat->st_mtime = file_time;
	file_stat->st_ctime = file_time;

	if (mode & S_IFDIR || !file_info)
		file_stat->st_size = 4096;
	else
		file_stat->st_size = file_info->uncompressed_size;
}

int add_dir_info(struct tzip_mount_entry *entry, const char *parent_dir,
		const char *filename, unz_file_info *file_info, mode_t mode)
{
	struct tzip_file_info *finfo = NULL;
	struct tzip_dir_info *dinfo = NULL;
	int len;

	/* create parent directory if does not exist */
	if (!entry->dir_hash) {
		/* first entry, initialize dir_hash table */
		entry->dir_hash = g_hash_table_new_full(
				g_str_hash, g_str_equal, NULL, NULL);
	}

	dinfo = (struct tzip_dir_info *)g_hash_table_lookup(
			entry->dir_hash, parent_dir);
	if (!dinfo) {
		/* new parent directory node, add */
		dinfo = (struct tzip_dir_info *)malloc(
				sizeof(struct tzip_dir_info));
		if (!dinfo) {
			_E("Malloc failed");
			return -ENOMEM;
		}

		dinfo->file_hash =  g_hash_table_new_full(
				g_str_hash, g_str_equal, NULL, NULL);
		if (!dinfo->file_hash) {
			_E("Malloc failed");
			free(dinfo);
			return -ENOMEM;
		}

		len = strlen(parent_dir) + 1;
		dinfo->name = (char *)malloc(len);
		if (!dinfo->name) {
			_E("Malloc failed");
			free(dinfo);
			return -ENOMEM;
		}
		snprintf(dinfo->name, len, "%s", parent_dir);
		g_hash_table_insert(entry->dir_hash, dinfo->name, dinfo);
		fileinfo_to_stat(NULL, &dinfo->stat, S_IFDIR);
	}

	/* add dir info in parent dir node */
	finfo = (struct tzip_file_info *)malloc(sizeof(struct tzip_file_info));
	if (!finfo) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	len = strlen(filename) + 1;
	finfo->name = (char *)malloc(len);
	if (!finfo->name) {
		free(finfo);
		_E("Malloc failed");
		return -ENOMEM;
	}
	strncpy(finfo->name, filename, len);

	fileinfo_to_stat(file_info, &finfo->stat, mode);

	g_hash_table_insert(dinfo->file_hash, finfo->name, finfo);

	return 0;
}

int extract_zipfile_details(
		struct tzip_mount_entry *entry, const char *zip_path)
{
	char *dir_name;
	uLong i;
	int ret = 0;
	mode_t mode;

	_D("Adding mount (%s, %s) to list", entry->path, zip_path);

	/* Open the zip file */
	unzFile *zipfile = unzOpen(zip_path);
	if (!zipfile) {
		_E("unzOpen Failed %s ", zip_path);
		return -EINVAL ;
	}

	/* Get info about the zip file */
	unz_global_info global_info;
	if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK) {
		_E("unzGetGlobalInfo Failed");
		ret = -EINVAL;
		goto out;
	}

	/* Loop to extract all files */
	for (i = 0; i < global_info.number_entry; ++i) {
		/* Get info about current file. */
		unz_file_info file_info = {0,};
		char filename[MAX_FILENAME_LEN] = {0};

		ret = unzGetCurrentFileInfo(zipfile, &file_info, filename,
				MAX_FILENAME_LEN, NULL, 0, NULL, 0);
		if (ret != UNZ_OK) {
			_E("unzGetCurrentFileInfo Failed");
			ret = -EINVAL;
			goto out;
		}

		_D("unzGetCurrentFileInfo file name - %s", filename);

		/* Check if this entry is a directory or file. */
		const size_t filename_length = strlen(filename);

		if (filename[filename_length-1] == DIR_DELIMETER) {
			_D("Entry is a directory");
			filename[filename_length-1] = 0;
			dir_name = strrchr(filename, '/');
			mode = S_IFDIR;
		} else {
			_D("Entry is a file");
			dir_name = strrchr(filename, '/');
			mode = S_IFREG;
		}

		if (dir_name == NULL) {
			ret = add_dir_info(entry, ".", filename, &file_info, mode);
		} else {
			*dir_name = 0;
			ret = add_dir_info(entry, filename,
					dir_name+1, &file_info, mode);
		}
		if (ret)
			break;

		/* Go the the next entry listed in the zip file. */
		if ((i+1) < global_info.number_entry) {
			ret = unzGoToNextFile(zipfile);
			if (ret != UNZ_OK) {
				_E("unzGoToNextFile Failed");
				ret = -EINVAL ;
				break;
			}
		}
	}

out:
	unzClose(zipfile);
	return ret;
}

int add_mount_entry(const char *zip_path, const char *mount_path)
{
	struct tzip_mount_entry *entry = NULL;
	int ret = 0;
	int len;

	/* check if this mount path is already there, return error if yes */
	entry = get_mount_entry(mount_path);
	if (entry) {
		_E("mount_path : %s already exists ", mount_path);
		return -EEXIST;
	}

	entry = (struct tzip_mount_entry *)malloc(
			sizeof(struct tzip_mount_entry));
	if (!entry) {
		_E("Malloc failed");
		return -ENOMEM;
	}
	entry->dir_hash = NULL;

	len = strlen(mount_path) + 1;
	entry->path = (char *)malloc(len);
	if (!entry->path) {
		_E("Malloc failed");
		ret = -ENOMEM;
		goto out;
	}
	snprintf(entry->path, len, "%s", mount_path);

	len = strlen(zip_path) + 1;
	entry->zip_path = (char *)malloc(len);
	if (!entry->zip_path) {
		_E("Malloc failed");
		ret = -ENOMEM;
		goto out;
	}

	strncpy(entry->zip_path, zip_path, len);

	g_hash_table_insert(hashmap, entry->path, entry);

	/* pasrse zip file and update list */
	ret = extract_zipfile_details(entry, zip_path);
	if (ret) {
		free_mount_node(entry);
		return ret;
	}

out:
	if (ret < 0 && entry) {
		free(entry->path);
		free(entry);
	}
	return ret;
}

void copy_file_stat(struct stat  *src, struct stat  *dest)
{
	dest->st_mode = src->st_mode;
	dest->st_nlink = src->st_nlink;
	dest->st_atime = src->st_atime;
	dest->st_mtime = src->st_mtime;
	dest->st_ctime = src->st_ctime;
	dest->st_size = src->st_size;
}

int get_file_info(const char *file_path, struct tzip_file_info *flist)
{
	struct tzip_mount_entry *entry;
	struct tzip_dir_info *dinfo;
	struct tzip_file_info *finfo;
	char *file_name;
	int file_index = 0;
	int dir_status;

	_D("File path : %s ", file_path);

	/* return file_info for a given file and mount path */
	entry = find_mount_entry(file_path, &dir_status);
	if (!entry) {
		_E("mount_path : %s is not mounted, dir_status %d", file_path, dir_status);
		return dir_status;
	}

	_D("Got mount path : %s", entry->path);
	file_index = strlen(entry->path);

	file_name = strrchr(&file_path[file_index], '/');
	if (!file_name) {
		_D("Parent Directory");
		return 1;
	} else if (strlen(file_name) == strlen(&file_path[file_index])) {
		_D("Checking dir : %s", ".");
		dinfo = get_dir_list(entry, ".");
	} else {
		*file_name = 0;
		_D("Checking dir : %s",  &file_path[file_index+1]);
		dinfo = get_dir_list(entry,  &file_path[file_index+1]);
	}
	if (!dinfo) {
		_D(" Empty Folder %s", file_path);
		return -ENOENT;
	}

	file_name = file_name+1;
	_D("File name : %s ", file_name);

	finfo = (struct tzip_file_info *)g_hash_table_lookup(
			dinfo->file_hash, file_name);
	if (!finfo) {
		_E("File %s not found", file_path);
		return -ENOENT;
	}

	flist->name = finfo->name;
	copy_file_stat(&finfo->stat, &flist->stat);

	return 0;
}

int get_path_prop(const char *path, struct stat *stbuf)
{
	int ret = 0;
	struct tzip_file_info file;

	ret = get_file_info(path, &file);
	if (ret < 0) {
		_E("File not found : %s ", path);
		return -ENOENT;
	}

	if (ret == 1) {
		stbuf->st_mode = S_IFDIR | DEFAULT_FILE_MODE;
		stbuf->st_nlink = 1;
		stbuf->st_size = 4096;
	} else {
		stbuf->st_mode = file.stat.st_mode | DEFAULT_FILE_MODE;
		stbuf->st_nlink = 1;
		stbuf->st_atime  = file.stat.st_atime;
		stbuf->st_mtime  = file.stat.st_mtime;
		stbuf->st_ctime  = file.stat.st_ctime;
		stbuf->st_size  = file.stat.st_size;
	}

	return 0;
}

struct tzip_dir_info *get_dir_files(const char *dir)
{
	struct tzip_mount_entry *entry;
	struct tzip_dir_info *dinfo;
	int file_index = 0;
	int dir_status;
	int dir_len = 0;

	_D("Dir  path : %s ", dir);

	entry = find_mount_entry(dir, &dir_status);
	if (!entry) {
		_E("mount_path : %s is not mounted, dir_status %d", dir, dir_status);
		return NULL;
	}

	_D("Mount path : %s", entry->path);
	file_index = strlen(entry->path);

	dir_len = strlen(dir);
	if (file_index == dir_len) {
		_D("Checking dir : %s", ".");
		dinfo = get_dir_list(entry, ".");
	} else {
		_D("Checking dir : %s", &dir[file_index+1]);
		dinfo = get_dir_list(entry, &dir[file_index+1]);
	}
	if (!dinfo) {
		_D(" Empty Folder  %s ", dir);
		return NULL;
	}

	return dinfo;
}

int remove_mount_entry(const char *mount_path)
{
	struct tzip_mount_entry *entry;

	if (!mount_path) {
		_E("Invalid mount path ");
		return -ENOENT;
	}

	entry = (struct tzip_mount_entry *)get_mount_entry(mount_path);
	if (!entry) {
		_D("Mount path : %s not found", mount_path);
		return -ENOENT;
	}

	free_mount_node(entry);
	return 0;
}

void free_mount_node(struct tzip_mount_entry *entry)
{
	struct tzip_file_info *finfo;
	struct tzip_dir_info *dinfo;
	GHashTableIter f_iter;
	GHashTableIter d_iter;
	gpointer fkey, fval;
	gpointer dkey, dval;

	if (!entry->dir_hash)
		goto out;

	g_hash_table_iter_init(&d_iter, entry->dir_hash);
	while (g_hash_table_iter_next(&d_iter, &dkey, &dval)) {
		dinfo = (struct tzip_dir_info *)dval;

		g_hash_table_iter_init(&f_iter, dinfo->file_hash);
		while (g_hash_table_iter_next(&f_iter, &fkey, &fval)) {
			finfo = (struct tzip_file_info *)fval;
			g_hash_table_remove(dinfo->file_hash, fkey);
			free(finfo->name);
			free(finfo);
		}

		g_hash_table_remove(entry->dir_hash, dkey);
		free(dinfo->name);
		if (dinfo->file_hash)
			g_hash_table_destroy(dinfo->file_hash);
		free(dinfo);
	}

out:
	g_hash_table_remove(hashmap, entry->path);
	free(entry->path);
	free(entry->zip_path);
	if (entry->dir_hash)
		g_hash_table_destroy(entry->dir_hash);
	free(entry);
}

void tzip_lock_init(void)
{
	 if (sem_init(&tzip_sem, 0, 1) == -1)
		_E("sem_init failed");
}

void tzip_lock_deinit(void)
{
	 if (sem_destroy(&tzip_sem) == -1)
		_E("sem_destroy failed");
}

void tzip_lock(void)
{
	sem_wait(&tzip_sem);
}

void tzip_unlock(void)
{
	sem_post(&tzip_sem);
}

int reset_zipfile(struct tzip_handle *handle)
{
	int ret;

	if (!handle)
		return -EINVAL;

	ret = unzCloseCurrentFile(handle->zipfile);
	if (ret != UNZ_OK) {
		_E("unzOpenCurrentFile Failed");
		return -EINVAL;
	}

	if (unzLocateFile(handle->zipfile, handle->file, CASE_SENSITIVE) != UNZ_OK) {
		_E("File :[%s] Not Found : unzLocateFile failed", handle->file);
		return -ENOENT;
	}

	ret = unzOpenCurrentFile(handle->zipfile);
	if (ret != UNZ_OK) {
		_E("unzOpenCurrentFile Failed");
		return -EINVAL;
	}

	return 0;
}

static int seek_from_current_offset(struct tzip_handle *handle,
		char *buf, size_t size, off_t offset)
{
	 int diff_size;
	 int bytes_read;
	 int ret;

	if (!handle)
		return -EINVAL;

	if (offset <= handle->offset)
		return -EINVAL;

	/* read from current position */
	diff_size = offset - handle->offset;
	_I("Read from current position (%jd) till offset (%jd)", handle->offset, offset);

	free(handle->pbuf);
	handle->pbuf = (char *)malloc(diff_size);
	if (!handle->pbuf) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	bytes_read = 0;
	do {
		ret = unzReadCurrentFile(handle->zipfile,
				handle->pbuf+bytes_read, diff_size - bytes_read);
		if (ret < 0) {
			_E("unzReadCurrentFile Failed");
			ret = -EINVAL;
			goto out;
		}
		bytes_read += ret;

	} while (bytes_read < diff_size && ret != 0);

	if (bytes_read == diff_size) {
		handle->from = handle->offset;
		handle->to = offset;
		return 0;
	}

	ret = 0;

out:
	free(handle->pbuf);
	handle->pbuf = NULL;

	return ret;;
}

static int seek_from_begining(struct tzip_handle *handle,
		char *buf, size_t size, off_t offset)
{
	char *tmp_buf = NULL;
	size_t buf_size;
	size_t chunk_count;
	int diff_size;
	int i;
	int bytes_read;
	int ret;

	/* read from the begining*/
	_I("Read from the begining till offset (%jd)", offset);
	if (offset < 0)
		diff_size = handle->offset + offset;
	else
		diff_size = offset;

	if (reset_zipfile(handle)) {
		_E("reset_zipfile Failed");
		return -EINVAL;
	}

	/* dont read more than MAX_CHUNK_SIZE at once */
	if (diff_size < MAX_CHUNK_SIZE)
		buf_size = diff_size;
	else
		buf_size = MAX_CHUNK_SIZE;

	tmp_buf = (char *)malloc(buf_size);
	if (!tmp_buf) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	/* chunk_count will have total number of chunks to read to reach offset position */
	chunk_count = diff_size/buf_size;
	if (diff_size % buf_size)
		chunk_count += 1;

	for (i = 0; i < chunk_count; i++) {
		/* adjust last chunk size according to offset */
		if (chunk_count > 1 && chunk_count == i + 1)
			buf_size = diff_size - (buf_size * i);

		bytes_read = 0;
		do {
			ret = unzReadCurrentFile(handle->zipfile,
					tmp_buf+bytes_read, buf_size - bytes_read);
			if (ret < 0) {
				_E("unzReadCurrentFile Failed(%d)", ret);
				ret = -EINVAL;
				goto out;
			}
			bytes_read += ret;
		} while (bytes_read < buf_size && ret != 0);

		if (!ret) {
			_E("EOF reached");
			break;
		}
	}

	ret = 0;

out:
	free(tmp_buf);
	return ret;
}

static int seek_offset(struct tzip_handle *handle,
		char *buf, size_t size, off_t offset)
{
	/* seek and read buffer */
	if (!handle || !handle->file_info)
		return -EINVAL;

	if (offset == handle->offset)
		return 0;

	/* seek and read buffer */
	if (offset >= handle->from &&
		(offset + size) <= handle->to &&
		handle->pbuf) {
		_I("Have already read this chunk");
		memcpy(buf, handle->pbuf + (offset - handle->from), size);
		return size;
	}

	if (offset > handle->offset)
		return seek_from_current_offset(handle, buf, size, offset);

	if (offset != 0)
		return seek_from_begining(handle, buf, size, offset);

	return 0;
}

int read_zipfile(struct tzip_handle *handle, char *buf,
			size_t size, off_t offset)
{
	int bytes_read;
	int ret;

	if (!handle || !handle->file_info)
		return -EINVAL;

	if (offset > handle->file_info->uncompressed_size) {
		_E("Invalid Offset (%jd) for file size (%d)", offset, handle->file_info->uncompressed_size);
		return -EINVAL;
	}

	/* seek and read buffer */
	_I("offset (%jd) handle->from(%jd) (offset + size) (%jd) handle->to (%jd) handle->offset (%jd)",
			offset, handle->from, (offset + size), handle->to, handle->offset);

	ret = seek_offset(handle, buf, size, offset);
	if (ret < 0) {
		_E("Failed to seek offset (%d)", ret);
		return ret;
	}

	bytes_read = 0;
	do {
		ret = unzReadCurrentFile(handle->zipfile,
				buf + bytes_read, size - bytes_read);
		if (ret < 0) {
			_E("unzReadCurrentFile Failed");
			return -EINVAL;
		}
		bytes_read += ret;
	} while (bytes_read < size && ret != 0);

	/* store current zip position for subsequent read */
	handle->offset = offset + bytes_read;

	_D("bytes_read : %d [offset : %jd, total size : %d]", bytes_read, handle->offset, handle->file_info->uncompressed_size);
	return bytes_read;
}

GHashTable *hashmap_init(void)
{
	hashmap = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	return hashmap;
}

GHashTable *get_hashmap(void)
{
	return hashmap;
}

int tzip_store_mount_info(const char *zip_path, const char *mount_path)
{
	FILE *fp;
	int len;
	int ret = 0;
	char *file_entry;

	if (!zip_path || !mount_path) {
		_E("Invalid Arguments path : %p buf %p ", zip_path, mount_path);
		return -ENOENT;
	}
	_D("zip_path - : [%s]  mount_path : [%s] ", zip_path, mount_path);

	fp = fopen(TZIP_INFO_FILE, "a+");
	if (fp == NULL) {
		_E("fopen() Failed!!!");
		return -EIO;
	}

	len = sizeof(char) * (strlen(zip_path) + strlen(mount_path) + 3);
	file_entry = (char *)malloc(len);
	if (!file_entry) {
		_E("Malloc failed");
		ret = -ENOMEM;
		goto out;
	}
	snprintf(file_entry, len, "%s:%s\n", zip_path, mount_path);

	len = strlen(file_entry);
	if (fwrite(file_entry, sizeof(char), len, fp) != len) {
		_E(" fwrite Failed !!!! ");
		ret = -EIO;
		goto out;
	}

out:
	free(file_entry);
	fclose(fp);
	return ret;
}

int tzip_remount_zipfs(const char *src_file, const char *mount_point)
{
	int ret = 0;
	char *tzip_path;
	int path_len, mp_len;

	if (!src_file || !mount_point) {
		_E("Invalid Arguments src_file : %p mount_point %p ", src_file, mount_point);
		return -EINVAL;
	}

	ret = add_mount_entry(src_file, mount_point);
	if (ret) {
		_E("Failed to add_mount_entry %s", mount_point);
		return ret;
	}

	path_len = sizeof(TZIP_ROOT_PATH); /* strlen(TZIP_ROOT_PATH) + 1 */
	mp_len = strlen(mount_point);
	tzip_path = (char *)malloc(path_len + mp_len);
	if (!tzip_path) {
		_E("Malloc failed");
		ret = -ENOMEM;
		goto out;
	}
	strncpy(tzip_path, TZIP_ROOT_PATH, path_len);
	strncat(tzip_path, mount_point, mp_len);

	_D("creating sym link : %s and %s", tzip_path, mount_point);
	ret = unlink(mount_point);
	if (ret) {
		_E("unlink failed");
		ret = -errno;
		goto out;
	}

	ret = symlink(tzip_path, mount_point);
	if (ret) {
		_E("symlink failed");
		ret = -errno;
		goto out;
	}

out:
	if (ret < 0)
		remove_mount_entry(mount_point);
	free(tzip_path);
	_D("Exit : %d", ret);
	return ret;
}
