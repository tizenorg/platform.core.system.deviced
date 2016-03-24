/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <vconf.h>
#include <errno.h>
#include <storage.h>

#include "log.h"
#include "common.h"
#include "dd-deviced.h"
#include "dd-storage.h"

struct storage_info {
	storage_type_e type;
	char *path;
	size_t len;
};

static bool storage_get_info(int storage_id, storage_type_e type,
		storage_state_e state, const char *path, void *user_data)
{
	struct storage_info *info;

	if (storage_id < 0 || !path || !user_data)
		return true;

	info = (struct storage_info *)user_data;
	if (type != info->type)
		return true;

	if (state != STORAGE_STATE_MOUNTED &&
		state != STORAGE_STATE_MOUNTED_READ_ONLY)
		return true;

	snprintf(info->path, info->len, "%s", path);
	return false;
}

API int storage_get_path(int type, char *path, int size)
{
	static char int_path[256] = { 0, };
	static char ext_path[256] = { 0, };
	int ret;
	struct storage_info info;

	if (!path || size <= 0)
		return -1;

	switch (type) {
	case STORAGE_DEFAULT:
	case STORAGE_INTERNAL:
		if (strlen(int_path) > 0) {
			snprintf(path, size, "%s", int_path);
			return 0;
		}
		info.path = int_path;
		info.len = sizeof(int_path);
		info.type = type;
		break;
	case STORAGE_EXTERNAL:
		if (strlen(ext_path) > 0) {
			snprintf(path, size, "%s", ext_path);
			return 0;
		}
		info.path = ext_path;
		info.len = sizeof(ext_path);
		info.type = type;
		break;
	default:
		_E("Invalid type (%d)", type);
		return -1;
	}

	ret = storage_foreach_device_supported(storage_get_info, &info);
	if (ret != STORAGE_ERROR_NONE) {
		_E("Failed to get storage information (%d)", ret);
		return -1;
	}

	if (strlen(info.path) == 0)
		return -1;

	snprintf(path, size, "%s", info.path);
	return 0;
}
