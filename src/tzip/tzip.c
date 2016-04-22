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

#define FUSE_USE_VERSION 26

#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "core/device-notifier.h"

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unzip.h>
#include <pthread.h>
#include <signal.h>
#include <gio/gio.h>
#include <glib.h>
#include <sys/stat.h>
#include <assert.h>
#include <attr/xattr.h>

#include "tzip-utility.h"

static pthread_t thread;
static pthread_t mount_thread;
static pthread_attr_t attr;
static struct fuse *fuse_handle;
static struct fuse_chan *channel;
static GAsyncQueue *async_queue;

static int tzip_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	if (!path || !stbuf) {
		_E("Invalid Arguments path : %p stbuf %p ", path, stbuf);
		return -EINVAL;
	}

	_D("Get ATTR path [%s]", path);

	tzip_lock();
	res = get_path_prop(path, stbuf);
	tzip_unlock();

	_D("Exit : %d", res);

	return res;
}

static int tzip_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	struct tzip_dir_info *flist;
	GHashTableIter iter;
	gpointer key, value;
	char *name;

	if (!path || !buf) {
		_E("Invalid Arguments path : %p buf %p ", path, buf);
		return -EINVAL;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	tzip_lock();
	flist = get_dir_files(path);
	if (flist == NULL) {
		tzip_unlock();
		_E("No Files in Dir : %s ", path);
		return 0;
	}

	g_hash_table_iter_init(&iter, flist->file_hash);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		name = (char *)key;
		filler(buf, name, NULL, 0);
	}
	tzip_unlock();

	return 0;
}

static int tzip_open(const char *path, struct fuse_file_info *fi)
{
	int ret;
	char *file;
	char *zippath;
	int dir_status;
	struct tzip_mount_entry *entry;
	struct tzip_handle *handle;
	unz_file_info *file_info;
	unz_global_info global_info;
	unzFile *zipfile;
	int len;

	if (!path || !fi) {
		_E("Invalid Arguments  path : %p fi : %p", path, fi);
		return -EINVAL;
	}
	_D("Open - Path : %s", path);

	tzip_lock();
	entry = find_mount_entry(path, &dir_status);
	if (!entry) {
		_E("Mount Entry Not Found ");
		tzip_unlock();
		return -ENOENT;
	}

	zippath = (char *)malloc(strlen(entry->zip_path)+1);
	if (!zippath) {
		tzip_unlock();
		_E("Malloc failed");
		return -ENOMEM;
	}
	strcpy(zippath, entry->zip_path);

	len = strlen(entry->path);
	file = (char *)malloc(strlen(path) - len + 1);
	if (!file) {
		free(zippath);
		tzip_unlock();
		_E("Malloc failed");
		return -ENOMEM;
	}
	strcpy(file, path + len + 1);
	tzip_unlock();

	zipfile = unzOpen(zippath);
	if (!zipfile) {
		_E("unzOpen Failed");
		free(zippath);
		free(file);
		return -ENOENT;
	}

	/* Get info about the zip file */
	if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK) {
		unzClose(zipfile);
		free(zippath);
		free(file);
		_E("unzGetGlobalInfo Failed");
		return -EINVAL;
	}

	if (unzLocateFile(zipfile, file, CASE_SENSITIVE) != UNZ_OK) {
		unzClose(zipfile);
		free(zippath);
		_E("File :[%s] Not Found : unzLocateFile failed", file);
		free(file);
		return -ENOENT;
	}

	file_info = (unz_file_info *)malloc(sizeof(unz_file_info));
	if (!file_info) {
		unzClose(zipfile);
		free(zippath);
		free(file);
		_E("Malloc failed");
		return -ENOMEM;
	}

	ret = unzGetCurrentFileInfo(zipfile, file_info, (char *)file,
			strlen(file), NULL, 0, NULL, 0);
	if (ret != UNZ_OK) {
		unzClose(zipfile);
		free(zippath);
		free(file);
		free(file_info);
		_E("unzGetCurrentFileInfo Failed");
		return -EINVAL;
	}

	ret = unzOpenCurrentFile(zipfile);
	if (ret != UNZ_OK) {
		unzClose(zipfile);
		free(zippath);
		free(file);
		free(file_info);
		_E("unzOpenCurrentFile Failed");
		return -EINVAL;
	}
	handle = (struct tzip_handle *)malloc(sizeof(struct tzip_handle));
	if (!handle) {
		unzCloseCurrentFile(zipfile);
		unzClose(zipfile);
		free(zippath);
		free(file);
		free(file_info);
		_E("Malloc failed");
		return -ENOMEM;
	}
	handle->file_info = file_info;
	handle->zipfile = zipfile;
	handle->offset = 0;
	handle->path = zippath;
	handle->file = file;
	handle->pbuf = NULL;
	handle->from = 0;
	handle->to = 0;
	 if (sem_init(&handle->lock, 0, 1) == -1)
		_E("sem_init failed");

	ret = (int)handle;
	fi->fh = (uint64_t)ret;
	return 0;
}

static int tzip_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	struct tzip_handle *handle;
	int ret;

	if (!path || !buf) {
		_E("Invalid Arguments path : %p buf %p ", path, buf);
		return -EINVAL;
	}
	if (!fi || fi->fh == 0) {
		_E("Invalid Zip Handle ");
		return -EINVAL;
	}
	ret = (int)fi->fh;
	handle = (struct tzip_handle *)ret;

	_D("Read - Path : %s  size : %zd offset : %jd ", path, size, offset);
	sem_wait(&handle->lock);
	ret = read_zipfile(handle, buf, size, offset);
	sem_post(&handle->lock);
	_D("Read ret = %d", ret);
	return ret;
}

static int tzip_release(const char *path, struct fuse_file_info *fi)
{
	struct tzip_handle *handle;
	int ret;

	if (!path) {
		_E("Invalid Arguments path : %p", path);
		return -EINVAL;
	}
	if (!fi || fi->fh == 0) {
		_E("Invalid Zip Handle ");
		return -EINVAL;
	}
	ret = (int)fi->fh;
	handle = (struct tzip_handle *)ret;

	if (sem_destroy(&handle->lock) == -1)
		_E("sem_destroy failed");

	unzCloseCurrentFile(handle->zipfile);
	unzClose(handle->zipfile);
	free(handle->file);
	free(handle->path);
	free(handle->file_info);
	if (handle->pbuf)
		free(handle->pbuf);
	free(handle);

	return 0;
}

static struct fuse_operations tzip_oper = {
	.getattr = tzip_getattr,
	.open = tzip_open,
	.read = tzip_read,
	.release = tzip_release,
	.readdir = tzip_readdir,
};

static void *tzip_thread(void *arg)
{
	int ret;
	GHashTable *hash;
	FILE *fp;
	char *file_path;
	char *mount_dir;
	char *mount_info = NULL;
	size_t len;
	ssize_t read;
	char *saveptr;
	char *argv[4] = {"deviced", "-o", "allow_other", NULL};
	struct fuse_args args = FUSE_ARGS_INIT(3, argv);

	ret = mkdir(TZIP_ROOT_PATH, 0755);
	if (ret < 0 && errno != EEXIST) {
		_E("fail to make dir %s", TZIP_ROOT_PATH);
		return NULL;
	}

	tzip_lock();
	channel = fuse_mount(TZIP_ROOT_PATH, &args);
	if (!channel) {
		_E("Trying to mount after cleaning up previous instance %p ", TZIP_ROOT_PATH);
		fuse_unmount(TZIP_ROOT_PATH, NULL);

		channel = fuse_mount(TZIP_ROOT_PATH, NULL);
		if (!channel) {
			_E("Failed at mount_point %p ", TZIP_ROOT_PATH);
			tzip_unlock();
			return NULL;
		}
	}

	fuse_handle = fuse_new(channel, NULL,
					&tzip_oper, sizeof(tzip_oper), NULL);
	if (!fuse_handle) {
		_E("Failed at  fuse_new");
		tzip_unlock();
		return NULL;
	}

	/* initialize string hash table */
	hash = hashmap_init();
	if (!hash) {
		_E("Failed at  hashmap_init");
		tzip_unlock();
		return NULL;
	}
	fp = fopen(TZIP_INFO_FILE, "r");
	if (fp) {
		while ((read = getline(&mount_info, &len, fp)) != -1) {
			if (mount_info) {
				mount_info[strcspn(mount_info, "\n")] = '\0';
				file_path = (char *)strtok_r(mount_info, ":", &saveptr);
				mount_dir = (char *)strtok_r(NULL, ":", &saveptr);

				if (file_path != NULL && mount_dir != NULL)
					tzip_remount_zipfs(file_path, mount_dir);
				free(mount_info);
				mount_info = NULL;
			}
		}
		fclose(fp);
	}
	tzip_unlock();
	if (fuse_loop_mt(fuse_handle)) {
		_E("Failed at  fuse_loop_mt");
		return NULL;
	}

	return NULL;
}

int tzip_mount_zipfs(const char *src_file, const char *mount_point, const char *smack)
{
	int ret = 0;
	char *tzip_path;

	tzip_lock();
	ret = add_mount_entry(src_file, mount_point);
	if (ret) {
		_E("Failed to add_mount_entry %s", mount_point);
		tzip_unlock();
		return ret;
	}

	tzip_path = (char *)malloc(strlen(TZIP_ROOT_PATH) + strlen(mount_point) + 1);
	if (!tzip_path) {
		_E("Malloc failed");
		remove_mount_entry(mount_point);
		tzip_unlock();
		return -ENOMEM;
	}
	strcpy(tzip_path, TZIP_ROOT_PATH);
	strcat(tzip_path, mount_point);

	_D("creating sym link : %s and %s", tzip_path, mount_point);
	ret = symlink(tzip_path, mount_point);
	if (ret) {
		_E("symlink failed");
		remove_mount_entry(mount_point);
		ret = -errno;
	} else {
		ret = tzip_store_mount_info(src_file, mount_point);
		if (ret != 0) {
			_E("Failed to store_mount_info %s", mount_point);
		}
		if (smack) {
			ret = lsetxattr(mount_point, "security.SMACK64", smack, strlen(smack), 0);
			if (ret < 0) {
				_E("setting smack label using lsetxattr() failed");
				remove_mount_entry(mount_point);
				ret = -errno;
			}
		}
	}

	tzip_unlock();
	free(tzip_path);
	_D("Exit : %d", ret);
	return ret;
}

int tzip_unmount_zipfs(const char *mount_point)
{
	int ret = 0;
	char *tzip_path;

	tzip_lock();
	ret = remove_mount_entry(mount_point);
	if (ret) {
		_E("Failed to remove_mount_entry %s", mount_point);
		tzip_unlock();
		return ret;
	}

	tzip_path = (char *)malloc(strlen(TZIP_ROOT_PATH) + strlen(mount_point) + 1);
	if (!tzip_path) {
		_E("Malloc failed");
		tzip_unlock();
		return -ENOMEM;
	}
	strcpy(tzip_path, TZIP_ROOT_PATH);
	strcat(tzip_path, mount_point);

	_D("deleting sym link : %s and %s", tzip_path, mount_point);
	ret = unlink(mount_point);
	if (ret) {
		_E("unlink failed");
		ret = -errno;
	}
	tzip_unlock();
	free(tzip_path);
	_D("Exit : %d", ret);
	return ret;
}

int tzip_is_mounted(const char *mount_point)
{
	struct tzip_mount_entry *entry = NULL;
	int ret = 0;
	char *tzip_path;
	struct stat sb;

	if (!mount_point) {
		_E("Invalid Arguments mount_point %p ", mount_point);
		return -EINVAL;
	}

	if (!get_hashmap()) {
		_E("tzip init is not done yet");
		return 0;
	}

	entry = get_mount_entry(mount_point);
	if (!entry) {
		_E("mount_path : %s does not exists ", mount_point);
		return 0;
	}

	tzip_path = (char *)malloc(strlen(TZIP_ROOT_PATH) + strlen(mount_point) + 1);
	if (!tzip_path) {
		_E("Malloc failed");
		return -ENOMEM;
	}
	strcpy(tzip_path, TZIP_ROOT_PATH);
	strcat(tzip_path, mount_point);

	if (stat(tzip_path, &sb) == -1 || stat(mount_point, &sb) == -1)
		ret = 0;
	else
		ret = 1;

	free(tzip_path);
	return ret;
}


static void *tzip_mount_thread(void *arg)
{
	struct tzip_msg_data *msgdata;
	int ret = 0;

	/* if g_async_queue_new() fails, tzip mount/unmount requests can not be handled */
	async_queue = g_async_queue_new();
	assert(async_queue);

	while (1) {
		/* g_async_queue_pop() is a blocking call, it should never return NULL */
		msgdata = (struct tzip_msg_data *)g_async_queue_pop(async_queue);
		assert(msgdata);

		if (msgdata->type == 'm') {
			ret = tzip_mount_zipfs(msgdata->zippath, msgdata->mountpath,
				msgdata->smack);
			free(msgdata->zippath);
			free(msgdata->mountpath);
			if (msgdata->smack)
				free(msgdata->smack);
		} else if (msgdata->type == 'u') {
			ret = tzip_unmount_zipfs(msgdata->mountpath);
			free(msgdata->mountpath);
		}
		free(msgdata);

		if (ret)
			_E("Failed to process mount/unmount request");
	}

	/* dead code, added just to satisfy compiler */
	return NULL;
}

void tzip_server_init(void)
{
	if (pthread_attr_init(&attr) != 0)
		_E("pthread_attr_init Failed");

	if (pthread_create(&thread, &attr, &tzip_thread, NULL)) {
		_E("pthread_create Failed");
	}

	if (pthread_create(&mount_thread, &attr, &tzip_mount_thread, NULL)) {
		_E("pthread_create Failed");
	}
}

void tzip_server_exit(void)
{
	if (!fuse_handle || !channel) {
		_E("Invalid Argument ");
		return;
	}

	fuse_exit(fuse_handle);
	fuse_unmount(TZIP_ROOT_PATH, channel);
	fuse_destroy(fuse_handle);
	fuse_handle = NULL;
	channel = NULL;
}

static DBusMessage *edbus_request_mount_tzip(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *zippath;
	char *mountpath;
	char *smack;
	int ret;
	struct tzip_msg_data *msgdata;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &mountpath, DBUS_TYPE_STRING, &zippath,
		    DBUS_TYPE_STRING, &smack, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (!mountpath || !zippath) {
		_E("invalid input");
		ret = -EINVAL;
		goto error;
	}

	if (!fuse_handle)
		tzip_server_init();

	msgdata = malloc(sizeof(struct tzip_msg_data));
	if (!msgdata) {
		_E("Malloc failed");
		ret = -ENOMEM;
		goto error;
	}

	msgdata->type = 'm';
	msgdata->zippath = strdup(zippath);
	if (!msgdata->zippath) {
		_E("Malloc failed");
		ret = -ENOMEM;
		free(msgdata);
		goto error;
	}

	msgdata->mountpath = strdup(mountpath);
	if (!msgdata->mountpath) {
		_E("Malloc failed");
		ret = -ENOMEM;
		free(msgdata->zippath);
		free(msgdata);
		goto error;
	}

	if (smack) {
		msgdata->smack = strdup(smack);
		if (!msgdata->smack) {
			_E("Malloc failed");
			ret = -ENOMEM;
			free(msgdata->zippath);
			free(msgdata->mountpath);
			free(msgdata);
			goto error;
		}
	} else {
		/* Use default smack */
		msgdata->smack = NULL;
	}

	if (async_queue) {
		g_async_queue_push(async_queue, (gpointer)msgdata);
		ret = 0;
	} else {
		ret = -EAGAIN;
		free(msgdata->zippath);
		free(msgdata->mountpath);
		if (msgdata->smack)
			free(msgdata->smack);
		free(msgdata);
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_unmount_tzip(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *mountpath;
	int ret;
	struct tzip_msg_data *msgdata;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &mountpath, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	if (!mountpath) {
		_E("invalid input");
		ret = -EINVAL;
		goto error;
	}

	if (!fuse_handle)
		tzip_server_init();

	msgdata = malloc(sizeof(struct tzip_msg_data));
	if (!msgdata) {
		_E("Malloc failed");
		ret = -ENOMEM;
		goto error;
	}

	msgdata->type = 'u';
	msgdata->mountpath = strdup(mountpath);
	if (!msgdata->mountpath) {
		_E("Malloc failed");
		ret = -ENOMEM;
		free(msgdata);
		goto error;
	}

	if (async_queue) {
		g_async_queue_push(async_queue, (gpointer)msgdata);
		ret = 0;
	} else {
		ret = -ENOMEM;
		free(msgdata->mountpath);
		free(msgdata);
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *edbus_request_ismounted_tzip(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *mountpath;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &mountpath, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto error;
	}

	ret = tzip_is_mounted(mountpath);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "Mount", "sss", "i", edbus_request_mount_tzip },
	{ "Unmount", "s", "i", edbus_request_unmount_tzip },
	{ "IsMounted", "s", "i", edbus_request_ismounted_tzip },
	/* Add methods here */
};

static int booting_done(void *data)
{
	if (!fuse_handle)
		tzip_server_init();

	return 0;
}

static void tzip_init(void *data)
{
	int ret;
	_D("tzip_init ");

	tzip_lock_init();

	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	ret = register_edbus_interface_and_method(DEVICED_PATH_TZIP,
			DEVICED_INTERFACE_TZIP,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static void tzip_exit(void *data)
{
	_D("tzip_exit ");
	tzip_server_exit();
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
}

static const struct device_ops tzip_device_ops = {
	.name     = "tzip",
	.init     = tzip_init,
	.exit  = tzip_exit,
};

DEVICE_OPS_REGISTER(&tzip_device_ops)


