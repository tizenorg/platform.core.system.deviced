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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/log.h"
#include "core/common.h"
#include "core/config-parser.h"
#include "core/launch.h"
#include "usb/usb.h"

#define SLP_GADGET_SETTING     "/etc/deviced/slp-gadget-setting.conf"
#define SLP_GADGET_OPERATION   "/etc/deviced/slp-gadget-operation.conf"

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
static bool base_config_initialized = false;

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

static int load_operation_config(struct parse_result *result, void *user_data)
{
	struct oper_data *data = user_data;
	int ret;

	if (!data || !result)
		return -EINVAL;

	if (strncmp(result->section, data->type, strlen(result->section)))
		return 0;

	if (strncmp(result->name, data->name, strlen(result->name)))
		return 0;

	ret = launch_app_cmd(result->value);

	_I("Execute(%s: %d)", result->value, ret);

	return ret;
}

static int execute_operation(char *type, char *name)
{
	int ret;
	struct oper_data data;

	if (!name)
		return -EINVAL;

	if (!type)
		type = config_default;

	data.name = name;
	data.type = type;

	ret = config_parse(SLP_GADGET_OPERATION,
			load_operation_config, &data);
	if (ret < 0)
		_E("Failed to load usb operation (%d)", ret);

	return ret;
}

static int change_config_state(char *enable)
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

static int update_configuration(char *name)
{
	if (!name)
		name = config_default;

	return config_parse(SLP_GADGET_SETTING,
			load_setting_config, name);
}

static inline int parse_base_config(char *name)
{
	int ret = 0;

	if (!base_config_initialized) {
		ret = config_parse(SLP_GADGET_SETTING,
				   load_base_config, name);

		base_config_initialized = !(ret < 0);
	}

	return ret;
}

static int slp_gadget_init(char *name)
{
	int ret;

	ret = parse_base_config(name);
	if (ret < 0) {
		_E("Failed to get base information(%d)", ret);
		return ret;
	}

	ret = update_configuration(name);
	if (ret < 0)
		_E("Failed to update usb configuration(%d)", ret);

	return ret;
}

static int slp_gadget_enable(char *name)
{
	int ret;

	ret = change_config_state(CONFIG_DISABLE);
	if (ret < 0) {
		_E("Failed to disable usb config");
		return ret;
	}

	ret = change_config_state(CONFIG_ENABLE);
	if (ret < 0) {
		_E("Failed to enable usb config");
		return ret;
	}

	return execute_operation(name, KEY_START);
}

static int slp_gadget_disable(char *name)
{
	return execute_operation(name, KEY_STOP);
}

static const struct usb_config_plugin_ops slp_gadget_plugin = {
	.init      = slp_gadget_init,
	.enable    = slp_gadget_enable,
	.disable   = slp_gadget_disable,
};

static bool slp_gadget_is_valid(void)
{
	struct stat st;
	bool is_valid = false;
	int ret;

	ret = parse_base_config(NULL);
	if (ret < 0) {
		_E("Failed to get base information(%d)", ret);
		return is_valid;
	}

	/* Check if slp-gadget is realy provided by kernel */
	ret = stat(config_rootpath, &st);
	if (!ret && S_ISDIR(st.st_mode))
		is_valid = true;

	/* TODO
	 * add checking default config valid condition */
	return is_valid;
}

static const struct usb_config_plugin_ops *slp_gadget_load(void)
{
	return &slp_gadget_plugin;
}

static const struct usb_config_ops usb_config_slp_gadget_ops = {
	.is_valid  = slp_gadget_is_valid,
	.load      = slp_gadget_load,
};

USB_CONFIG_OPS_REGISTER(&usb_config_slp_gadget_ops)
