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
#include <errno.h>
#include <hw/backlight.h>

#include "util.h"
#include "backlight-service.h"

static struct backlight_device_t *backlight_dev;
static struct backlight_state_t state;

int backlight_get_brightness(int *brightness)
{
	if (!brightness)
		return -EINVAL;

	*brightness = state.brightness;
	return 0;
}

int backlight_set_brightness(int brightness)
{
	if (brightness < 0 || brightness > 100)
		return -EINVAL;

	state.brightness = brightness;
	state.mode = BACKLIGHT_MANUAL;

	return backlight_dev->set_state(&state);
}

int backlight_set_mode(enum backlight_mode mode)
{
	if (mode == BACKLIGHT_MODE_MANUAL) {
		state.brightness = -1;
		state.mode = BACKLIGHT_MANUAL;
	} else if (mode == BACKLIGHT_MODE_SENSOR)
		state.mode = BACKLIGHT_SENSOR;
	else
		return -EINVAL;

	return backlight_dev->set_state(&state);
}
			
int backlight_service_load(void)
{
	struct hw_info_t *info;
	int r;

	r = hw_get_info(BACKLIGHT_HARDWARE_DEVICE_ID, &info);
	if (r < 0) {
		_E("fail to load backlight shared library : %d", r);
		return -ENOENT;
	}

	r = info->open(info, NULL, (struct hw_common_t*)&backlight_dev);
	if (r < 0) {
		_E("fail to get backlight device structure : %d", r);
		return -EPERM;
	}

	_D("backlight device structure load success");
	return 0;
}

int backlight_service_free(void)
{
	struct hw_info_t *info;

	info = backlight_dev->common.info;
	info->close(backlight_dev);
}
