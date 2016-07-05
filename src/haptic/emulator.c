/*
 * deviced-vibrator
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
#include <errno.h>

#include "core/log.h"
#include "haptic.h"

#define DEFAULT_HAPTIC_HANDLE	0xFFFF
#define DEFAULT_EFFECT_HANDLE	0xFFFA

/* START: Haptic Module APIs */
static int get_device_count(int *count)
{
	if (count)
		*count = 1;

	return 0;
}

static int open_device(int device_index, int *device_handle)
{
	if (device_handle)
		*device_handle = DEFAULT_HAPTIC_HANDLE;

	return 0;
}

static int close_device(int device_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	return 0;
}

static int vibrate_monotone(int device_handle, int duration, int feedback, int priority, int *effect_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	if (effect_handle)
		*effect_handle = DEFAULT_EFFECT_HANDLE;

	return 0;
}

static int vibrate_buffer(int device_handle, const unsigned char *vibe_buffer, int iteration, int feedback, int priority, int *effect_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	if (effect_handle)
		*effect_handle = DEFAULT_EFFECT_HANDLE;

	return 0;
}

static int vibrate_effect(int device_handle, const char *pattern, int feedback, int priority)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	return 0;
}

static int stop_device(int device_handle)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	return 0;
}

static int is_supported(const char *pattern)
{
	return 0;
}

static int get_device_state(int device_index, int *effect_state)
{
	if (effect_state)
		*effect_state = 0;

	return 0;
}

static int create_effect(unsigned char *vibe_buffer, int max_bufsize, haptic_module_effect_element *elem_arr, int max_elemcnt)
{
	_E("Not support feature");
	return -EACCES;
}

static int get_buffer_duration(int device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	if (device_handle != DEFAULT_HAPTIC_HANDLE)
		return -EINVAL;

	_E("Not support feature");
	return -EACCES;
}

static int convert_binary(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	_E("Not support feature");
	return -EACCES;
}
/* END: Haptic Module APIs */

static const struct haptic_plugin_ops default_plugin = {
	.get_device_count    = get_device_count,
	.open_device         = open_device,
	.close_device        = close_device,
	.vibrate_monotone    = vibrate_monotone,
	.vibrate_buffer      = vibrate_buffer,
	.vibrate_effect      = vibrate_effect,
	.is_supported        = is_supported,
	.stop_device         = stop_device,
	.get_device_state    = get_device_state,
	.create_effect       = create_effect,
	.get_buffer_duration = get_buffer_duration,
	.convert_binary      = convert_binary,
};

static bool is_valid(void)
{
#ifdef EMULATOR
	_I("Support emulator haptic device");
	return true;
#else
	_E("Do not support emulator haptic device");
	return false;
#endif
}

static const struct haptic_plugin_ops *load(void)
{
	return &default_plugin;
}

static const struct haptic_ops emul_ops = {
	.is_valid = is_valid,
	.load     = load,
};

HAPTIC_OPS_REGISTER(&emul_ops)
