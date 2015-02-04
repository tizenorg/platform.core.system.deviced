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


#include <stdio.h>

#include "core/log.h"
#include "core/list.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/config-parser.h"
#include "core/device-notifier.h"
#include "extcon.h"

#define EXTCON_PATH	"/sys/class/extcon"

#define BUF_MAX 256

static dd_list *extcon_list;

void add_extcon(const struct extcon_ops *dev)
{
	DD_LIST_APPEND(extcon_list, dev);
}

void remove_extcon(const struct extcon_ops *dev)
{
	DD_LIST_REMOVE(extcon_list, dev);
}

static int extcon_changed(struct extcon_ops *dev, int status)
{
	if (!dev)
		return -EINVAL;

	if (dev->status == status)
		return 0;

	_I("Changed %s device : %d -> %d", dev->name, dev->status, status);

	dev->status = status;
	device_notify(dev->noti, &status);

	return 0;
}

static struct extcon_ops *find_extcon(const char *name)
{
	dd_list *l;
	struct extcon_ops *dev;

	if (!name)
		return NULL;

	DD_LIST_FOREACH(extcon_list, l, dev) {
		if (!strcmp(dev->name, name))
			return dev;
	}

	return NULL;
}

int extcon_get_status(const char *name)
{
	struct extcon_ops *dev;

	if (!name)
		return -EINVAL;

	dev = find_extcon(name);
	if (!dev)
		return -ENOENT;

	return dev->status;
}

int extcon_update(const char *value)
{
	char *s, *p;
	char name[NAME_MAX];
	struct extcon_ops *dev;

	if (!value)
		return -EINVAL;

	s = (char*)value;
	while (s && *s != '\0') {
		p = strchr(s, '=');
		if (!p)
			break;
		memset(name, 0, sizeof(name));
		memcpy(name, s, p-s);
		dev = find_extcon(name);
		if (dev)
			extcon_changed(dev, atoi(p+1));
		s = strchr(p, '\n');
		if (!s)
			break;
		s += 1;
	}

	return 0;
}

static int extcon_load_uevent(struct parse_result *result, void *user_data)
{
	struct extcon_ops *dev;
	int val;

	if (!result)
		return 0;

	if (!result->name || !result->value)
		return 0;

	val = atoi(result->value);
	dev = find_extcon(result->name);
	if (dev)
		extcon_changed(dev, val);

	return 0;
}

static int get_extcon_uevent_state(char *state, unsigned int len)
{
	DIR *dir;
	struct dirent *entry;
	char node[BUF_MAX];
	int ret;

	if (!state)
		return -EINVAL;

	dir = opendir(EXTCON_PATH);
	if (!dir) {
		ret = -errno;
		_E("Cannot open dir (%s, errno:%d)", EXTCON_PATH, ret);
		return ret;
	}

	ret = -ENOENT;
	while ((entry = readdir(dir))) {
		if (entry->d_name[0] == '.')
			continue;
		snprintf(node, sizeof(node), "%s/%s/state",
				EXTCON_PATH, entry->d_name);
		_I("checking node (%s)", node);
		if (access(node, F_OK) != 0)
			continue;

		ret = 0;
		break;
	}

	if (dir)
		closedir(dir);

	if (ret == 0) {
		strncpy(state, node, len - 1);
		state[len -1] = '\0';
	}

	return ret;
}

static void extcon_init(void *data)
{
	int ret;
	char state[256];
	dd_list *l;
	struct extcon_ops *dev;

	if (!extcon_list)
		return;

	DD_LIST_FOREACH(extcon_list, l, dev) {
		_I("[extcon] init (%s)", dev->name);
		if (dev->init)
			dev->init(data);
	}

	/* load extcon uevent */
	ret = get_extcon_uevent_state(state, sizeof(state));
	if (ret == 0) {
		ret = config_parse(state, extcon_load_uevent, NULL);
		if (ret < 0)
			_E("Failed to load %s file : %d", EXTCON_PATH, ret);
	} else {
		_E("Failed to get extcon uevent state node");
	}
}

static void extcon_exit(void *data)
{
	dd_list *l;
	struct extcon_ops *dev;

	DD_LIST_FOREACH(extcon_list, l, dev) {
		_I("[extcon] deinit (%s)", dev->name);
		if (dev->exit)
			dev->exit(data);
	}
}

const struct device_ops extcon_device_ops = {
	.name	= "extcon",
	.init	= extcon_init,
	.exit	= extcon_exit,
};

DEVICE_OPS_REGISTER(&extcon_device_ops)
