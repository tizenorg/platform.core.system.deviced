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
#include "core/edbus-handler.h"
#include "core/udev.h"
#include "extcon.h"

#define EXTCON_PATH "/sys/class/extcon"
#define STATE_NAME  "STATE"

#define BUF_MAX 256

static dd_list *extcon_list;

void add_extcon(struct extcon_ops *dev)
{
	DD_LIST_APPEND(extcon_list, dev);
}

void remove_extcon(struct extcon_ops *dev)
{
	DD_LIST_REMOVE(extcon_list, dev);
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

static int extcon_update(const char *name, const char *value)
{
	struct extcon_ops *dev;
	int status;

	if (!name || !value)
		return -EINVAL;

	dev = find_extcon(name);
	if (!dev) {
		_E("fail to find matched extcon device : name(%s)", name);
		return -EINVAL;
	}

	status = atoi(value);
	_I("Changed %s device : %d -> %d", name, dev->status, status);

	dev->status = status;
	if (dev->update)
		dev->update(status);

	return 0;
}

static int extcon_parsing_value(const char *value)
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
		/* name is env_name and p+1 is env_value */
		extcon_update(name, p+1);
		s = strchr(p, '\n');
		if (!s)
			break;
		s += 1;
	}

	return 0;
}

static void uevent_extcon_handler(struct udev_device *dev)
{
	const char *env_value;
	int ret;

	env_value = udev_device_get_property_value(dev, STATE_NAME);
	if (!env_value)
		return;

	ret = extcon_parsing_value(env_value);
	if (ret < 0)
		_E("fail to parse extcon value : %d", ret);
}

static int extcon_load_uevent(struct parse_result *result, void *user_data)
{
	if (!result)
		return 0;

	if (!result->name || !result->value)
		return 0;

	extcon_update(result->name, result->value);

	return 0;
}

static int get_extcon_state_node(char *state, unsigned int len)
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

static DBusMessage *dbus_get_extcon_status(E_DBus_Object *obj,
		DBusMessage *msg)
{
	DBusError err;
	struct extcon_ops *dev;
	char *str;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
				DBUS_TYPE_STRING, &str,
				DBUS_TYPE_INVALID)) {
		_E("fail to get message : %s - %s", err.name, err.message);
		dbus_error_free(&err);
		ret = -EINVAL;
		goto error;
	}

	dev = find_extcon(str);
	if (!dev) {
		_E("fail to matched extcon device : %s", str);
		ret = -ENOENT;
		goto error;
	}

	ret = dev->status;
	_D("Extcon device : %s, status : %d", dev->name, dev->status);

error:
	return make_reply_message(msg, ret);
}

static DBusMessage *dbus_enable_device(E_DBus_Object *obj, DBusMessage *msg)
{
	char *device;
	int ret;

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_STRING, &device, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	ret = extcon_update(device, "1");

out:
	return make_reply_message(msg, ret);
}

static DBusMessage *dbus_disable_device(E_DBus_Object *obj, DBusMessage *msg)
{
	char *device;
	int ret;

	if (!dbus_message_get_args(msg, NULL,
		    DBUS_TYPE_STRING, &device, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	ret = extcon_update(device, "0");

out:
	return make_reply_message(msg, ret);
}

static int extcon_probe(void *data)
{
	/**
	 * find extcon class.
	 * if there is no extcon class,
	 * deviced does not control extcon devices.
	 */
	if (access(EXTCON_PATH, R_OK) != 0) {
		_E("there is no extcon class");
		return -ENODEV;
	}

	return 0;
}

static struct uevent_handler uh = {
	.subsystem = EXTCON_SUBSYSTEM,
	.uevent_func = uevent_extcon_handler,
};

static const struct edbus_method edbus_methods[] = {
	{ "GetStatus", "s",  "i", dbus_get_extcon_status },
	{ "enable",    "s", NULL, dbus_enable_device },  /* for devicectl */
	{ "disable",   "s", NULL, dbus_disable_device }, /* for devicectl */
};

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

	/* register extcon uevent */
	ret = register_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to register extcon uevent : %d", ret);

	/* set initialize extcon device state */
	ret = get_extcon_state_node(state, sizeof(state));
	if (ret == 0) {
		ret = config_parse(state, extcon_load_uevent, NULL);
		if (ret < 0)
			_E("Failed to load %s file : %d", state, ret);
	} else {
		_E("Failed to get extcon uevent state node");
	}

	ret = register_edbus_interface_and_method(DEVICED_PATH_EXTCON,
			DEVICED_INTERFACE_EXTCON,
			edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus interface and method(%d)", ret);
}

static void extcon_exit(void *data)
{
	dd_list *l;
	struct extcon_ops *dev;
	int ret;

	/* unreigster extcon uevent */
	ret = unregister_kernel_uevent_control(&uh);
	if (ret < 0)
		_E("fail to unregister extcon uevent : %d", ret);

	DD_LIST_FOREACH(extcon_list, l, dev) {
		_I("[extcon] deinit (%s)", dev->name);
		if (dev->exit)
			dev->exit(data);
	}
}

static const struct device_ops extcon_device_ops = {
	.name   = "extcon",
	.probe  = extcon_probe,
	.init   = extcon_init,
	.exit   = extcon_exit,
};

DEVICE_OPS_REGISTER(&extcon_device_ops)
