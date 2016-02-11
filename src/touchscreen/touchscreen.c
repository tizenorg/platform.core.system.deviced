/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stdbool.h>
#include <hw/touchscreen.h>
#include "core/devices.h"
#include "core/common.h"
#include "core/log.h"

static struct touchscreen_device *touchscreen_dev;

static void touchscreen_init(void *data)
{
	struct hw_info *info;
	int ret;

	if (touchscreen_dev)
		return;

	ret = hw_get_info(TOUCHSCREEN_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);
	if (ret < 0) {
		_E("Fail to load touchscreen shared library (%d)", ret);
		return;
	}

	if (!info->open) {
		_E("fail to open touchscreen device : open(NULL)");
		return;
	}

	ret = info->open(info, NULL, (struct hw_common **)&touchscreen_dev);
	if (ret < 0) {
		_E("fail to get touchscreen device structure : (%d)", ret);
		return;
	}

	_I("touchscreen device structure load success");
}

static void touchscreen_exit(void *data)
{
	struct hw_info *info;

	if (!touchscreen_dev)
		return;

	info = touchscreen_dev->common.info;
	assert(info);

	info->close((struct hw_common *)touchscreen_dev);
}

static int touchscreen_set_state(enum touchscreen_state state)
{
	int ret;
	char *act;

	if (!touchscreen_dev) {
		_E("touchscreen device structure is not loaded");
		return -ENOENT;
	}

	if (state != TOUCHSCREEN_ON && state != TOUCHSCREEN_OFF) {
		_E("Invalid parameter");
		return -EINVAL;
	}

	if (!touchscreen_dev->set_state) {
		_E("touchscreen state change is not supported");
		return -ENOTSUP;
	}

	act = (state == TOUCHSCREEN_ON) ? "enable" : "disable";

	ret = touchscreen_dev->set_state(state);
	if (ret == 0)
		_I("Success to %s touchscreen", act);
	else
		_E("Failed to %s touchscreen (%d)", act, ret);

	return ret;
}

static int touchscreen_start(enum device_flags flags)
{
	return touchscreen_set_state(TOUCHSCREEN_ON);
}

static int touchscreen_stop(enum device_flags flags)
{
	return touchscreen_set_state(TOUCHSCREEN_OFF);
}

static const struct device_ops touchscreen_device_ops = {
	.name     = "touchscreen",
	.init     = touchscreen_init,
	.exit     = touchscreen_exit,
	.start    = touchscreen_start,
	.stop     = touchscreen_stop,
};

DEVICE_OPS_REGISTER(&touchscreen_device_ops)

