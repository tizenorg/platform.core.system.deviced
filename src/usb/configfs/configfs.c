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

#include "usbg/usbg.h"

#define CFS_GADGET_SETTING     "/etc/deviced/cfs-gadget-setting.conf"
#define CFS_GADGET_OPERATION   "/etc/deviced/cfs-gadget-operations.conf"
#define CFS_GADGET_SCHEME      "/etc/deviced/cfs-gadget.gs"

#define SECTION_BASE    "BASE"
#define KEY_CFSROOT     "cfsroot"
#define KEY_UDC         "udc"
#define KEY_DEFAULT     "default"
#define KEY_PREBIND     "prebind"
#define KEY_POSTBIND    "postbind"
#define KEY_PREUNBIND   "preunbind"
#define KEY_POSTUNBIND  "postunbind"

#define BUF_MAX 128

struct oper_data {
	char *type;
	char *name;
};

static char cfs_root[BUF_MAX];
static char udc_name[BUF_MAX];
static char config_default[BUF_MAX];
static bool base_config_initialized = false;

static usbg_state *usbg_ctx;
static usbg_udc *udc;
static usbg_gadget *slp_gadget;

static inline bool udc_name_is_valid()
{
	return udc_name[0] != '\0' && strcmp(udc_name, "any");
}

static int load_operation_config(struct parse_result *result, void *user_data)
{
	struct oper_data *data = user_data;
	int ret;

	if (!data || !result)
		return -EINVAL;

	if (strcmp(result->section, data->type) ||
	    strcmp(result->name, data->name))
		return 0;

	ret = launch_app_cmd(result->value);

	/*
	 * This is required because when we start sdb using ffs
	 * we have to wait untill it writes descriptors and strings
	 * otherwise bind operation will fail.
	 *
	 * This will be removed when sdb will be fully ported to systemd
	 * socket activation.
	 */
	system("sleep 1");

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

	ret = config_parse(CFS_GADGET_OPERATION,
			load_operation_config, &data);
	if (ret < 0)
		_E("Failed to load usb operation (%d)", ret);

	return ret;
}

static int load_base_config(struct parse_result *result, void *user_data)
{
	if (!result || result->section == NULL || result->name == NULL)
		return -EINVAL;

	if (strcmp(result->section, SECTION_BASE))
		return 0;

	if (!strcmp(result->name, KEY_CFSROOT)) {
		snprintf(cfs_root, sizeof(cfs_root), "%s", result->value);
		_I("USB config cfsroot(%s)", cfs_root);
	} else if (!strcmp(result->name, KEY_UDC)) {
		snprintf(udc_name, sizeof(udc_name), "%s", result->value);
		_I("USB config udc(%s)", udc_name);
	} else if (!strcmp(result->name, KEY_DEFAULT)) {
		snprintf(config_default, sizeof(config_default),
			 "%s", result->value);
		_I("USB config default(%s)", config_default);
	}

	return 0;
}

static inline int parse_base_config()
{
	int ret = 0;

	if (!base_config_initialized) {
		ret = config_parse(CFS_GADGET_SETTING,
				   load_base_config, NULL);
		base_config_initialized = !(ret < 0);
	}

	return ret;
}

static int remove_all_bindings(usbg_config *c)
{
	usbg_binding *b;
	int ret = 0;

	while (b = usbg_get_first_binding(c)) {
		ret = usbg_rm_binding(b);
		if (ret != USBG_SUCCESS) {
			_E("Unable to remove function from config (%d)", ret);
			goto out;
		}
	}
out:
	return ret;
}

static int update_configuration(int id, const char *label,
				const char *functions)
{
	usbg_config *c;
	char *funcs = NULL;
	char *func, *funcs_begin;
	int ret = 0;

	c = usbg_get_config(slp_gadget, id, label);
	if (!c) {
		if (functions[0] =='\0')
			return ret;

		usbg_config_attrs attrs = {
			/* USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER */
			.bmAttributes = 0xc0,
			.bMaxPower = 250,
		};

		ret = usbg_create_config(slp_gadget, id, label,
					 &attrs, NULL, &c);
		if (ret != USBG_SUCCESS) {
			_E("Unable to create config #%d (%d)", id, ret);
			return ret;
		}
	} else if (functions[0] =='\0') {
		return usbg_rm_config(c, USBG_RM_RECURSE);
	}

	ret = remove_all_bindings(c);
	if (ret)
		return ret;

	funcs_begin = strdup(functions);
	if (!funcs_begin)
		return -ENOMEM;

	ret = 0;
	funcs = funcs_begin;
	do {
		char *type_str, *instance;
		int type;
		usbg_function *f;

		func = strsep(&funcs, ",");
		type_str = strsep(&func, ".");
		instance = func;

		type = usbg_lookup_function_type(type_str);
		if (type < 0) {
			/*
			 * Yes, that's true, in case of unknown function
			 * we skip this one. This is the default behaviour
			 * of old slp-gadget and we kept it.
			 */
			_I("Unknown function type: %s", type_str);
			continue;
		}

		f = usbg_get_function(slp_gadget, type, instance);
		if (!f) {
			_I("Unable to find function: %s %s",
			   type_str, instance);
			continue;
		}

		ret = usbg_add_config_function(c, NULL, f);
		if (ret != USBG_SUCCESS) {
			_I("Unable to add function: %s %s to config %d (%d)",
			   type_str, instance, id, ret);
			goto free_funcs;
		}
	} while (funcs);

	ret = 0;

free_funcs:
	free(funcs_begin);
	return ret;
}

static int update_gadget_config(struct parse_result *result, void *user_data)
{
	char *section = user_data;
	int code;
	int ret = -EINVAL;

	if (!result || !section || !result->section || !result->name)
		return ret;

	if (strcmp(section, result->section))
		return 0;

	if (!strcmp(result->name, "funcs_fconf"))
		return update_configuration(1, "cfs_first_config",
					    result->value);
	else if (!strcmp(result->name, "funcs_sconf"))
		return update_configuration(2, "cfs_second_config",
					    result->value);

	code = usbg_lookup_gadget_attr(result->name);
	if (code >= 0) {
		long val;

		val = strtol(result->value, NULL, 0);
		return usbg_set_gadget_attr(slp_gadget, code, (int) val);
	}

	code = usbg_lookup_gadget_str(result->name);
	if (code < 0)
		return ret;

	ret = usbg_set_gadget_str(slp_gadget, code,
				   LANG_US_ENG, result->value);

	return ret;
}

static int update_gadget(const char *config)
{
	return config_parse(CFS_GADGET_SETTING, update_gadget_config,
			    (void *)config);
}

static int cfs_gadget_init(char *name)
{
	FILE *gadget_scheme;
	int ret;

	ret = parse_base_config();
	if (ret < 0) {
		_E("Failed to get base information(%d)", ret);
		return ret;
	}

	ret = usbg_init(cfs_root, &usbg_ctx);
	if (ret != USBG_SUCCESS) {
		_E("Failed to init libusbg(%d)", ret);
		goto out;
	}

	udc = udc_name_is_valid() ? usbg_get_udc(usbg_ctx, udc_name)
		: usbg_get_first_udc(usbg_ctx);
	if (!udc) {
		_E("Requested UDC not found");
		goto cleanup_ctx;
	}

	slp_gadget = usbg_get_gadget(usbg_ctx, "slp-gadget");
	if (!slp_gadget) {
		_I("Gadget not existing. Creating a new one");
		gadget_scheme = fopen(CFS_GADGET_SCHEME, "r");
		if (!gadget_scheme) {
			ret = errno;
			_E("Failed to open gadget scheme(%s, )",
			   CFS_GADGET_SCHEME, ret);
			goto cleanup_ctx;
		}

		ret = usbg_import_gadget(usbg_ctx, gadget_scheme,
					 "slp-gadget", &slp_gadget);
		fclose(gadget_scheme);
		if (ret != USBG_SUCCESS) {
			_E("Failed to import gadget scheme(%d)", ret);
			goto cleanup_ctx;
		}
	}

	ret = update_gadget(name ? name : config_default);
	if (ret < 0) {
		_E("Failed to update usb configuration(%d)", ret);
		goto remove_gadget;
	}

	return 0;

remove_gadget:
	usbg_rm_gadget(slp_gadget, USBG_RM_RECURSE);
	slp_gadget = NULL;
cleanup_ctx:
	usbg_cleanup(usbg_ctx);
	usbg_ctx = NULL;
out:
	return ret;
}

static void cfs_gadget_cleanup(char *name)
{
	int ret;

	if (slp_gadget) {
		if (usbg_get_gadget_udc(slp_gadget)) {
			_E("USB gadget still enabled. Disabling...");
			usbg_disable_gadget(slp_gadget);
		}

		ret = usbg_rm_gadget(slp_gadget, USBG_RM_RECURSE);
		if (ret != USBG_SUCCESS)
			_E("Failed to remove usb gadget from configfs(%d)",
			   ret);
		/* there is no good way for error recovery here */
		slp_gadget = NULL;
	}

	if (usbg_ctx) {
		usbg_cleanup(usbg_ctx);
		usbg_ctx = NULL;
	}
}

static int cfs_gadget_enable(char *name)
{
	int ret;

	/* don't do anything if gadget is already enabled */
	if (usbg_get_gadget_udc(slp_gadget))
		return 0;

	ret = execute_operation(name, KEY_PREBIND);
	if (ret < 0) {
		_E("Unable to execute pre bind operations (%d)", ret);
		goto out;
	}

	ret = usbg_enable_gadget(slp_gadget, udc);
	if (ret != USBG_SUCCESS) {
		_E("Unable to bind gadget (%d)", ret);
		/* TODO add some kind of error recovery for prebind commands */
		goto out;
	}

	ret = execute_operation(name, KEY_POSTBIND);
	if (ret < 0) {
		_E("Unable to execute post bind operations (%d)", ret);
		/* TODO add some kind of error recovery for prebind commands */
		goto out;
	}
out:
	return ret;
}

static int cfs_gadget_disable(char *name)
{
	int ret;

	/* don't do anything if gadget is not really enabled */
	if (!usbg_get_gadget_udc(slp_gadget))
		return 0;

	ret = execute_operation(name, KEY_PREUNBIND);
	if (ret < 0) {
		_E("Unable to execute pre unbind operations (%d)", ret);
		goto out;
	}

	ret = usbg_disable_gadget(slp_gadget);
	if (ret != USBG_SUCCESS) {
		_E("Unable to unbind gadget (%d)", ret);
		/*
		 * TODO add some kind of error recovery
		 * for preunbind commands
		 */
		goto out;
	}

	ret = execute_operation(name, KEY_POSTUNBIND);
	if (ret < 0) {
		_E("Unable to execute post bind operations (%d)", ret);
		/*
		 * TODO add some kind of error recovery
		 * for preunbind commands
		 */
	}
out:
	return ret;
}

static const struct usb_config_plugin_ops cfs_gadget_plugin = {
	.init      = cfs_gadget_init,
	.deinit    = cfs_gadget_cleanup,
	.enable    = cfs_gadget_enable,
	.disable   = cfs_gadget_disable,
};

static bool cfs_gadget_is_valid(void)
{
	usbg_state *ctx;
	usbg_udc *u = NULL;
	bool is_valid = false;
	int ret;

	ret = parse_base_config();
	if (ret < 0) {
		_E("Failed to get base information(%d)", ret);
		goto out;
	}

	/* Check if we will be able to init libusbg */
	ret = usbg_init(cfs_root, &ctx);
	if (ret != USBG_SUCCESS) {
		_E("Failed to init libusbg(%d)", ret);
		goto out;
	}

	/*
	 * Here we are sure that configfs and libcomposite are
	 * available in our kernel as libusbg checks this.
	 * Now it's time to check if we have suitable UDC.
	 */
	if (!udc_name_is_valid()) {
		/* We are looking for any available UDC */
		u = usbg_get_first_udc(ctx);
	} else {
		/* We are looking for udc with specified name */
		u = usbg_get_udc(ctx, udc_name);
	}

	if (!u) {
		_E("Requested UDC not found");
		goto cleanup_ctx;
	}

	/* Add here some additional checks if needed */

	is_valid = true;

cleanup_ctx:
	usbg_cleanup(ctx);
out:
	return is_valid;
}

static const struct usb_config_plugin_ops *cfs_gadget_load(void)
{
	return &cfs_gadget_plugin;
}

static const struct usb_config_ops usb_config_cfs_gadget_ops = {
	.is_valid  = cfs_gadget_is_valid,
	.load      = cfs_gadget_load,
};

USB_CONFIG_OPS_REGISTER(&usb_config_cfs_gadget_ops)
