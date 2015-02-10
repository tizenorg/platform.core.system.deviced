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
#include <string.h>

#include "core/log.h"
#include "core/common.h"
#include "core/config-parser.h"
#include "core/launch.h"
#include "usb.h"

#define USB_CONFIGURATION	"/etc/deviced/usb-config.conf"
#define USB_OPERATION		"/etc/deviced/usb-operation.conf"

#define CONF_ROOTPATH	"ROOTPATH"
#define CONF_PATH		"path"
#define CONF_ENABLE		"ENABLE"
#define CONF_DISABLE	"DISABLE"
#define OPER_START		"start"
#define OPER_STOP		"stop"

struct oper_data {
	char *type;
	char *name;
};

static int write_file(char *path, char *val)
{
	FILE *fp;
	int ret;
	unsigned int len;

	if (!path || !val)
		return -EINVAL;

	fp = fopen(path, "w");
	if (!fp) {
		ret = -errno;
		_E("Failed to open file (%s, errno:%d)", path, ret);
		return ret;
	}

	len = strlen(val);
	ret = fwrite(val, sizeof(char), len, fp);
	fclose(fp);
	if (ret < len) {
		_E("Failed to write (%s, %s)", path, val);
		return -ENOMEM;
	}

	return 0;
}

static int load_configuration_config(struct parse_result *result, void *user_data)
{
	static char rootpath[128] = {0,};
	char *section = user_data;
	char path[128];
	int ret;

	if (!section || !result)
		return -EINVAL;

	if (strlen(rootpath) == 0) {
		if (strcmp(result->section, CONF_ROOTPATH))
			return 0;
		if (strcmp(result->name, CONF_PATH))
			return 0;
		snprintf(rootpath, sizeof(rootpath), "%s", result->value);
		return 0;
	}

	if (strcmp(result->section, section))
		return 0;

	snprintf(path, sizeof(path), "%s/%s", rootpath, result->name);
	ret = write_file(path, result->value);
	if (ret < 0) {
		_E("Failed to write (%s, %s)", path, result->value);
		return ret;
	}

	return 0;
}

static int usb_set_configuration(char *name)
{
	int ret;

	if (!name)
		return -EINVAL;

	ret = config_parse(USB_CONFIGURATION,
			load_configuration_config, name);
	if (ret < 0)
		_E("Failed to load usb configuration(%d)", ret);

	return ret;
}

static int load_operation_config(struct parse_result *result, void *user_data)
{
	struct oper_data *data = user_data;
	int ret;

	if (!data || !result)
		return -EINVAL;

	if (strcmp(result->section, data->type))
		return 0;

	if (strcmp(result->name, data->name))
		return 0;

	ret = launch_app_cmd(result->value);

	_I("Execute(%s: %d)", result->value, ret);

	return ret;
}

static int usb_execute_operation(char *type, char *name)
{
	int ret;
	struct oper_data data;

	if (!name || !type)
		return -EINVAL;

	data.name = name;
	data.type = type;

	ret = config_parse(USB_OPERATION,
			load_operation_config, &data);
	if (ret < 0)
		_E("Failed to load usb operation (%d)", ret);

	return ret;
}

int config_init(char *name)
{
	return usb_set_configuration(name);
}

void config_deinit(char *name)
{
}

int config_enable(char *name)
{
	int ret;

	ret = usb_set_configuration(CONF_DISABLE);
	if (ret < 0) {
		_E("Failed to set configuration(%d)", ret);
		return ret;
	}

	ret = usb_set_configuration(CONF_ENABLE);
	if (ret < 0) {
		_E("Failed to set configuration(%d)", ret);
		return ret;
	}

	return usb_execute_operation(name, OPER_START);
}

int config_disable(char *name)
{
	return usb_execute_operation(name, OPER_STOP);
}

static const struct usb_config_plugin_ops default_plugin = {
	.init		= config_init,
	.deinit		= config_deinit,
	.enable		= config_enable,
	.disable	= config_disable,
};

static bool is_valid(void)
{
	/* TODO
	 * add checking default config valid condition */
	return true;
}

static const struct usb_config_plugin_ops *load(void)
{
	return &default_plugin;
}

static void release(void)
{
}

static const struct usb_config_ops usb_config_default_ops = {
	.is_valid	= is_valid,
	.load		= load,
	.release	= release,
};

USB_CONFIG_OPS_REGISTER(&usb_config_default_ops)
