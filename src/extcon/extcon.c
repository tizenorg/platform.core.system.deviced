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


#include <stdio.h>
#include <vconf.h>
#include <string.h>

#include "core/log.h"
#include "core/list.h"
#include "core/config-parser.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/device-notifier.h"

#define EXTCON_PATH			"/sys/class/extcon"

#define BUF_MAX 256

struct extcon_dev {
	char *name;
	int status;
	enum device_notifier_type noti;
	int (*changed)(struct extcon_dev *dev, int status);
};

static int extcon_changed(struct extcon_dev *dev, int status)
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

static struct extcon_dev extcon_info[] = {
	{ "TA"		, 0	, DEVICE_NOTIFIER_TA		, extcon_changed	},
	{ "USB"		, 0	, DEVICE_NOTIFIER_USB		, extcon_changed	},
	{ "USB-HOST", 0	, DEVICE_NOTIFIER_USBHOST	, extcon_changed	},
	/* Add other extcon devices here */
};

static struct extcon_dev *find_extcon(const char *name)
{
	int i, len;

	if (!name)
		return NULL;

	len = strlen(name);
	for (i = 0 ; i < ARRAY_SIZE(extcon_info) ; i++) {
		if (!strncmp(name, extcon_info[i].name, len))
			return &(extcon_info[i]);
	}

	return NULL;
}

int get_extcon_status(const char *name)
{
	struct extcon_dev *dev;

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
	struct extcon_dev *dev;

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
			dev->changed(dev, atoi(p+1));
		s = strchr(p, '\n');
		if (!s)
			break;
		s += 1;
	}

	return 0;
}

static int extcon_load_uevent(struct parse_result *result, void *user_data)
{
	struct extcon_dev *dev;
	int val;

	if (!result)
		return 0;

	if (!result->name || !result->value)
		return 0;

	val = atoi(result->value);
	dev = find_extcon(result->name);
	if (dev && dev->changed)
		dev->changed(dev, val);

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
		if (!entry)
			break;
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

	if (ret == 0)
		snprintf(state, len, "%s", node);

	return ret;
}

static int extcon_booting_done(void *data)
{
	int ret;
	char state[256];

	/* load extcon uevent */
	ret = get_extcon_uevent_state(state, sizeof(state));
	if (ret == 0) {
		ret = config_parse(state, extcon_load_uevent, NULL);
		if (ret < 0)
			_E("Failed to load %s file : %d", EXTCON_PATH, ret);
	} else {
		_E("Failed to get extcon uevent state node");
	}

	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, extcon_booting_done);
	device_notify(DEVICE_NOTIFIER_EXTCON_READY, NULL);

	return 0;
}

static void extcon_init(void *data)
{
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, extcon_booting_done);
}

static void extcon_exit(void *data)
{
}

const struct device_ops extcon_device_ops = {
	.name     = "extcon",
	.init     = extcon_init,
	.exit     = extcon_exit,
};

DEVICE_OPS_REGISTER(&extcon_device_ops)
