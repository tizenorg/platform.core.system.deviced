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
#include <stdbool.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "core/log.h"
#include "haptic.h"

#define HAPTIC_MODULE_PATH			"/usr/lib/libhaptic-module.so"

/* Haptic Plugin Interface */
static void *dlopen_handle;
static const struct haptic_plugin_ops *plugin_intf;

static bool is_valid(void)
{
	struct stat buf;
	const struct haptic_plugin_ops *(*get_haptic_plugin_interface) () = NULL;

	if (stat(HAPTIC_MODULE_PATH, &buf)) {
		_E("file(%s) is not presents", HAPTIC_MODULE_PATH);
		goto error;
	}

	dlopen_handle = dlopen(HAPTIC_MODULE_PATH, RTLD_NOW);
	if (!dlopen_handle) {
		_E("dlopen failed: %s", dlerror());
		goto error;
	}

	get_haptic_plugin_interface = dlsym(dlopen_handle, "get_haptic_plugin_interface");
	if (!get_haptic_plugin_interface) {
		_E("dlsym failed : %s", dlerror());
		goto error;
	}

	plugin_intf = get_haptic_plugin_interface();
	if (!plugin_intf) {
		_E("get_haptic_plugin_interface() failed");
		goto error;
	}

	_I("Support external haptic device");
	return true;

error:
	if (dlopen_handle) {
		dlclose(dlopen_handle);
		dlopen_handle = NULL;
	}

	_E("Do not support external haptic device");
	return false;
}

static const struct haptic_plugin_ops *load(void)
{
	return plugin_intf;
}

static void release(void)
{
	if (dlopen_handle) {
		dlclose(dlopen_handle);
		dlopen_handle = NULL;
	}

	plugin_intf = NULL;
}

static const struct haptic_ops ext_ops = {
	.type     = HAPTIC_EXTERNAL,
	.is_valid = is_valid,
	.load     = load,
	.release  = release,
};

HAPTIC_OPS_REGISTER(&ext_ops)
