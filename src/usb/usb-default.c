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

#define USB_SETTING     "/etc/deviced/usb-setting.conf"
#define USB_OPERATION   "/etc/deviced/usb-operation.conf"

#define SECTION_BASE    "BASE"
#define KEY_ROOTPATH    "rootpath"
#define KEY_LOAD        "load"
#define KEY_DEFAULT     "default"
#define KEY_START       "start"
#define KEY_STOP        "stop"

#define CONFIG_ENABLE   "1"
#define CONFIG_DISABLE  "0"

#define BUF_MAX 128

struct oper_data {
	char *type;
	char *name;
};

static char config_rootpath[BUF_MAX];
static char config_load[BUF_MAX];
static char config_default[BUF_MAX];

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

static int load_setting_config(struct parse_result *result, void *user_data)
{
	char *section = user_data;
	char path[BUF_MAX];
	int ret;

	if (!result || !section)
		return -EINVAL;

	if (result->section == NULL || result->name == NULL)
		return -EINVAL;

	if (strncmp(section, result->section, strlen(section)))
		return 0;

	snprintf(path, sizeof(path), "%s/%s", config_rootpath, result->name);
	ret = write_file(path, result->value);
	if (ret < 0)
		_E("Failed to write (%s, %s)", path, result->value);

	return ret;
}

static int load_base_config(struct parse_result *result, void *user_data)
{
	if (!result)
		return -EINVAL;

	if (result->section == NULL || result->name == NULL)
		return -EINVAL;

	if (strncmp(result->section, SECTION_BASE, sizeof(SECTION_BASE)))
		return 0;

	if (!strncmp(result->name, KEY_ROOTPATH, sizeof(KEY_ROOTPATH))) {
		snprintf(config_rootpath, sizeof(config_rootpath),
				"%s", result->value);
		_I("USB config rootpath(%s)", config_rootpath);
	}

	else if (!strncmp(result->name, KEY_LOAD, sizeof(KEY_LOAD))) {
		snprintf(config_load, sizeof(config_load),
				"%s", result->value);
		_I("USB config load(%s)", config_load);
	}

	else if (!strncmp(result->name, KEY_DEFAULT, sizeof(KEY_DEFAULT))) {
		snprintf(config_default, sizeof(config_default),
				"%s", result->value);
		_I("USB config default(%s)", config_default);
	}

	return 0;
}

static int get_operation_ops(char *cmd, const char **buf, size_t len)
{
	char ops[256];
	char *f, *t;
	int i;
	bool fin;

	if (!cmd)
		return -EINVAL;

	snprintf(ops, sizeof(ops), "%s", cmd);

	if (strlen(cmd) == 0)
		return 0;

	i = 0;
	t = f = ops;
	fin = false;
	while (!fin) {
		if (*t != '\0' && *t != '\n' &&
			*t != '\t' && *t != ' ') {
			t++;
			continue;
		}

		if (*t == '\0' || i >= len - 2)
			fin = true;

		if (f == t) {
			f = ++t;
			continue;
		}

		*t = '\0';
		buf[i++] = f;
		f = ++t;
	}

	if (i > 0)
		buf[i++] = NULL;

	return i;
}

static int load_operation_config(struct parse_result *result, void *user_data)
{
	struct oper_data *data = user_data;
	int ret, num;
	const char *buf[64];

	if (!data || !result)
		return -EINVAL;

	if (strncmp(result->section, data->type, strlen(result->section)))
		return 0;

	if (strncmp(result->name, data->name, strlen(result->name)))
		return 0;

	num = get_operation_ops(result->value, buf, sizeof(buf));
	if (num <= 0)
		return num;
	ret = run_child(num, buf);

	_I("Execute(%s: %d)", result->value, ret);

	return ret;
}

static int usb_execute_operation(char *type, char *name)
{
	int ret;
	struct oper_data data;

	if (!name)
		return -EINVAL;

	if (!type)
		type = config_default;

	data.name = name;
	data.type = type;

	ret = config_parse(USB_OPERATION,
			load_operation_config, &data);
	if (ret < 0)
		_E("Failed to load usb operation (%d)", ret);

	return ret;
}

static int usb_load_configuration(char *enable)
{
	int ret;
	static char buf[BUF_MAX];
	static bool node = false;

	if (!node) {
		snprintf(buf, sizeof(buf), "%s/%s",
				config_rootpath, config_load);
		node = true;
	}

	ret = write_file(buf, enable);
	if (ret < 0)
		_E("Failed to write (%s, %s)", buf, enable);

	return ret;
}

static int usb_update_configuration(char *name)
{
	if (!name)
		name = config_default;

	return config_parse(USB_SETTING,
			load_setting_config, name);
}

static int usb_init(char *name)
{
	int ret;

	ret = config_parse(USB_SETTING,
			load_base_config, name);
	if (ret < 0) {
		_E("Failed to get base information(%d)", ret);
		return ret;
	}

	ret = usb_update_configuration(name);
	if (ret < 0)
		_E("Failed to update usb configuration(%d)", ret);

	return ret;
}

static int usb_enable(char *name)
{
	int ret;

	ret = usb_load_configuration(CONFIG_DISABLE);
	if (ret < 0) {
		_E("Failed to disable usb config");
		return ret;
	}

	ret = usb_load_configuration(CONFIG_ENABLE);
	if (ret < 0) {
		_E("Failed to enable usb config");
		return ret;
	}

	return usb_execute_operation(name, KEY_START);
}

static int usb_disable(char *name)
{
	return usb_execute_operation(name, KEY_STOP);
}

static const struct usb_config_plugin_ops default_plugin = {
	.init      = usb_init,
	.enable    = usb_enable,
	.disable   = usb_disable,
};

static bool usb_valid(void)
{
	/* TODO
	 * add checking default config valid condition */
	return true;
}

static const struct usb_config_plugin_ops *usb_load(void)
{
	return &default_plugin;
}

static const struct usb_config_ops usb_config_default_ops = {
	.is_valid  = usb_valid,
	.load      = usb_load,
};

USB_CONFIG_OPS_REGISTER(&usb_config_default_ops)
