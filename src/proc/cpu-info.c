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


#include <fcntl.h>

#include "core/log.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/common.h"

#define METHOD_GET_REVISION	"GetRevision"
#define PATH_NAME		"/proc/cpuinfo"
#define REVISION_NAME	"Revision"
#define TOK_DELIMITER	":"
#define END_DELIMITER	" \n"

#define CONVERT_TYPE	10
#define REVISION_SIZE	4
#define FILE_BUFF_MAX	1024
#define NOT_INITIALIZED	(-1)

static int read_from_file(const char *path, char *buf, size_t size)
{
	int fd;
	size_t count;

	if (!path)
		return -1;

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		_E("Could not open '%s'", path);
		return -1;
	}

	count = read(fd, buf, size);

	if ((int)count != -1 && count > 0) {
		count = (count < size) ? count : size - 1;
		while (count > 0 && buf[count - 1] == '\n')
			count--;
		buf[count] = '\0';
	} else {
		buf[0] = '\0';
	}

	close(fd);

	return 0;
}

static int get_revision(char *rev, int len)
{
	char buf[FILE_BUFF_MAX];
	char *tag;
	char *start, *ptr;
	char *saveptr;
	long rev_num;
	const int radix = 16;

	if (rev == NULL || len <= 0) {
		_E("Invalid argument !\n");
		return -1;
	}

	if (read_from_file(PATH_NAME, buf, FILE_BUFF_MAX) < 0) {
		_E("fail to read %s\n", PATH_NAME);
		return -1;
	}

	tag = strstr(buf, REVISION_NAME);
	if (tag == NULL) {
		_E("cannot find Hardware in %s\n", PATH_NAME);
		return -1;
	}

	start = strstr(tag, TOK_DELIMITER);
	if (start == NULL) {
		_E("cannot find Hardware in %s\n", PATH_NAME);
		return -1;
	}

	start++;
	ptr = strtok_r(start, END_DELIMITER, &saveptr);
	if (!ptr) {
		_E("fail to extract tokens");
		return -1;
	}
	ptr += strlen(ptr);
	ptr -= 2;

	memset(rev, 0x00, REVISION_SIZE);
	rev_num = strtol(ptr, NULL, radix);
	snprintf(rev, len, "%ld", rev_num);

	return 0;
}

static DBusMessage *dbus_revision_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	char rev[FILE_BUFF_MAX];
	char *ptr;
	static int ret = NOT_INITIALIZED;

	if (ret != NOT_INITIALIZED)
		goto out;
	ret = get_revision(rev, sizeof(rev));
	if (ret == 0)
		ret = strtol(rev, &ptr, CONVERT_TYPE);
out:
	_D("rev : %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ METHOD_GET_REVISION, NULL, "i", dbus_revision_handler },
};

static void cpu_info_init(void *data)
{
	int ret;

	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

static const struct device_ops cpu_info_device_ops = {
	.name     = "cpu_info",
	.init     = cpu_info_init,
};

DEVICE_OPS_REGISTER(&cpu_info_device_ops)
