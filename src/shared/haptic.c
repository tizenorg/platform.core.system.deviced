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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <vconf.h>

#include "log.h"
#include "haptic-plugin-intf.h"
#include "dd-haptic.h"

#ifndef API
#define API __attribute__ ((visibility("default")))
#endif

#define HAPTIC_MODULE_PATH			"/usr/lib/libhaptic-module.so"

/* Haptic Handle Control */
static unsigned int __handle_cnt;

/* Haptic Plugin Interface */
static void *dlopen_handle;
static const struct haptic_ops *plugin_intf;

/* START of Static Function Section */
static int __module_init(void)
{
	struct stat buf;
	const struct haptic_ops *(*get_haptic_plugin_interface) () = NULL;

	if (stat(HAPTIC_MODULE_PATH, &buf)) {
		PRT_ERR("file(%s) is not presents", HAPTIC_MODULE_PATH);
		goto EXIT;
	}

	dlopen_handle = dlopen(HAPTIC_MODULE_PATH, RTLD_NOW);
	if (!dlopen_handle) {
		PRT_ERR("dlopen failed: %s", dlerror());
		goto EXIT;
	}


	get_haptic_plugin_interface = dlsym(dlopen_handle, "get_haptic_plugin_interface");
	if (!get_haptic_plugin_interface) {
		PRT_ERR("dlsym failed : %s", dlerror());
		goto EXIT;
	}

	plugin_intf = get_haptic_plugin_interface();
	if (!plugin_intf) {
		PRT_ERR("get_haptic_plugin_interface() failed");
		goto EXIT;
	}

	PRT_DBG("This device can vibe");
	return 0;

EXIT:
	if (dlopen_handle) {
		dlclose(dlopen_handle);
		dlopen_handle = NULL;
	}

	PRT_DBG("This device can not vibe");
	return -1;
}

static int __module_fini(void)
{
	if (dlopen_handle) {
		dlclose(dlopen_handle);
		dlopen_handle = NULL;
	}

	PRT_DBG("haptic module is released");
	return 0;
}

static haptic_feedback_e __get_setting_feedback_level(void)
{
	int setting_fb_level;

	if (vconf_get_int(VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT, &setting_fb_level) < 0) {
		setting_fb_level = SETTING_VIB_FEEDBACK_LEVEL3;
	}

	if (setting_fb_level < HAPTIC_FEEDBACK_0 || setting_fb_level > HAPTIC_FEEDBACK_5)
		return -1;

	switch (setting_fb_level) {
	case SETTING_VIB_FEEDBACK_LEVEL0 : return HAPTIC_FEEDBACK_0;
	case SETTING_VIB_FEEDBACK_LEVEL1 : return HAPTIC_FEEDBACK_1;
	case SETTING_VIB_FEEDBACK_LEVEL2 : return HAPTIC_FEEDBACK_2;
	case SETTING_VIB_FEEDBACK_LEVEL3 : return HAPTIC_FEEDBACK_3;
	case SETTING_VIB_FEEDBACK_LEVEL4 : return HAPTIC_FEEDBACK_4;
	case SETTING_VIB_FEEDBACK_LEVEL5 : return HAPTIC_FEEDBACK_5;
	default:
		break;
	}
	return -1;
}
/* END of Static Function Section */

API int haptic_get_count(int *device_number)
{
	int ret;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_number == NULL) {
		PRT_ERR("Invalid parameter : device_number(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->get_device_count) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->get_device_count == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->get_device_count(device_number);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_get_device_count is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_open(haptic_device_e device_index, haptic_device_h *device_handle)
{
	int ret;
	int handle;

	if (!(device_index == HAPTIC_DEVICE_0 || device_index == HAPTIC_DEVICE_1 || device_index == HAPTIC_DEVICE_ALL)) {
		PRT_ERR("Invalid parameter : device_index(%d)", device_index);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (device_handle == NULL) {
		PRT_ERR("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (__handle_cnt == 0) {
		ret = __module_init();
		if (ret < 0) {
			PRT_ERR("__module_init failed");
			return HAPTIC_ERROR_OPERATION_FAILED;
		}
	}

	if (!plugin_intf || !plugin_intf->open_device) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->open_device == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->open_device((int)device_index, &handle);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_open_device is failed : %d", ret);
		__module_fini();
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	__handle_cnt++;
	*device_handle = (haptic_device_h)handle;
	return HAPTIC_ERROR_NONE;
}

API int haptic_close(haptic_device_h device_handle)
{
	int ret;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->close_device) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->close_device == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->close_device((int)device_handle);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_close_device is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	__handle_cnt--;
	if (__handle_cnt == 0) {
		__module_fini();
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_vibrate_monotone(haptic_device_h device_handle, int duration, haptic_effect_h *effect_handle)
{
	return haptic_vibrate_monotone_with_detail(device_handle,
											   duration,
											   HAPTIC_FEEDBACK_AUTO,
											   HAPTIC_PRIORITY_MIN,
											   effect_handle);
}

API int haptic_vibrate_monotone_with_detail(haptic_device_h device_handle,
                                        int duration,
                                        haptic_feedback_e feedback,
                                        haptic_priority_e priority,
                                        haptic_effect_h *effect_handle)
{
	int ret;
	int handle;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (duration < 0) {
		PRT_ERR("Invalid parameter : duration(%d)", duration);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (feedback < HAPTIC_FEEDBACK_0 || feedback > HAPTIC_FEEDBACK_AUTO) {
		PRT_ERR("Invalid parameter : feedback(%d)", feedback);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (priority < HAPTIC_PRIORITY_MIN || priority > HAPTIC_PRIORITY_HIGH) {
		PRT_ERR("Invalid parameter : priority(%d)", priority);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->vibrate_monotone) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->vibrate_monotone == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	if (feedback == HAPTIC_FEEDBACK_AUTO) {
		PRT_DBG("Auto feedback level, feedback value will be changed");
		feedback = __get_setting_feedback_level();
	}

	PRT_DBG("duration : %d, feedback : %d, priority : %d", duration, feedback, priority);
	ret = plugin_intf->vibrate_monotone((int)device_handle, duration, feedback, priority, &handle);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_vibrate_monotone is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	if (effect_handle != NULL) {
		*effect_handle = (haptic_effect_h)handle;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_vibrate_file(haptic_device_h device_handle, const char *file_path, haptic_effect_h *effect_handle)
{
	return haptic_vibrate_file_with_detail(device_handle,
										   file_path,
										   HAPTIC_ITERATION_ONCE,
										   HAPTIC_FEEDBACK_AUTO,
										   HAPTIC_PRIORITY_MIN,
										   effect_handle);
}

API int haptic_vibrate_file_with_detail(haptic_device_h device_handle,
                                    const char *file_path,
                                    haptic_iteration_e iteration,
                                    haptic_feedback_e feedback,
                                    haptic_priority_e priority,
                                    haptic_effect_h *effect_handle)
{
	int ret;
	int handle;
	struct stat buf;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_path == NULL) {
		PRT_ERR("Invalid parameter : file_path(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (stat(file_path, &buf)) {
		PRT_ERR("Invalid parameter : (%s) is not presents", file_path);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (iteration < HAPTIC_ITERATION_ONCE || iteration > HAPTIC_ITERATION_INFINITE) {
		PRT_ERR("Invalid parameter : iteration(%d)", iteration);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (feedback < HAPTIC_FEEDBACK_0 || feedback > HAPTIC_FEEDBACK_AUTO) {
		PRT_ERR("Invalid parameter : feedback(%d)", feedback);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (priority < HAPTIC_PRIORITY_MIN || priority > HAPTIC_PRIORITY_HIGH) {
		PRT_ERR("Invalid parameter : priority(%d)", priority);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->vibrate_file) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->vibrate_file == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	if (feedback == HAPTIC_FEEDBACK_AUTO) {
		PRT_DBG("Auto feedback level, feedback value will be changed");
		feedback = __get_setting_feedback_level();
	}

	PRT_DBG("file_path : %s, iteration : %d, feedback : %d, priority : %d", file_path, iteration, feedback, priority);
	ret = plugin_intf->vibrate_file((int)device_handle, file_path, iteration, feedback, priority, &handle);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_vibrate_file is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	if (effect_handle != NULL) {
		*effect_handle = (haptic_effect_h)handle;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_vibrate_buffer(haptic_device_h device_handle, const unsigned char *vibe_buffer, haptic_effect_h *effect_handle)
{
	return haptic_vibrate_buffers_with_detail(device_handle,
											 vibe_buffer,
											 0,
											 HAPTIC_ITERATION_ONCE,
											 HAPTIC_FEEDBACK_AUTO,
											 HAPTIC_PRIORITY_MIN,
											 effect_handle);
}

API int haptic_vibrate_buffer_with_detail(haptic_device_h device_handle,
                                      const unsigned char *vibe_buffer,
                                      haptic_iteration_e iteration,
                                      haptic_feedback_e feedback,
                                      haptic_priority_e priority,
                                      haptic_effect_h *effect_handle)
{
	return haptic_vibrate_buffers_with_detail(device_handle,
											 vibe_buffer,
											 0,
											 iteration,
											 feedback,
											 priority,
											 effect_handle);
}

API int haptic_vibrate_buffers(haptic_device_h device_handle,
							const unsigned char *vibe_buffer,
							int size,
							haptic_effect_h *effect_handle)
{
	return haptic_vibrate_buffers_with_detail(device_handle,
											 vibe_buffer,
											 size,
											 HAPTIC_ITERATION_ONCE,
											 HAPTIC_FEEDBACK_AUTO,
											 HAPTIC_PRIORITY_MIN,
											 effect_handle);
}

API int haptic_vibrate_buffers_with_detail(haptic_device_h device_handle,
                                      const unsigned char *vibe_buffer,
									  int size,
                                      haptic_iteration_e iteration,
                                      haptic_feedback_e feedback,
                                      haptic_priority_e priority,
                                      haptic_effect_h *effect_handle)
{
	int ret;
	int handle;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (vibe_buffer == NULL) {
		PRT_ERR("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (iteration < HAPTIC_ITERATION_ONCE || iteration > HAPTIC_ITERATION_INFINITE) {
		PRT_ERR("Invalid parameter : iteration(%d)", iteration);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (feedback < HAPTIC_FEEDBACK_0 || feedback > HAPTIC_FEEDBACK_AUTO) {
		PRT_ERR("Invalid parameter : feedback(%d)", feedback);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (priority < HAPTIC_PRIORITY_MIN || priority > HAPTIC_PRIORITY_HIGH) {
		PRT_ERR("Invalid parameter : priority(%d)", priority);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->vibrate_buffer) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->vibrate_buffer == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	if (feedback == HAPTIC_FEEDBACK_AUTO) {
		PRT_DBG("Auto feedback level, feedback value will be changed");
		feedback = __get_setting_feedback_level();
	}

	PRT_DBG("iteration : %d, feedback : %d, priority : %d", iteration, feedback, priority);
	ret = plugin_intf->vibrate_buffer((int)device_handle, vibe_buffer, iteration, feedback, priority, &handle);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_vibrate_buffer is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	if (effect_handle != NULL) {
		*effect_handle = (haptic_effect_h)handle;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_stop_effect(haptic_device_h device_handle, haptic_effect_h effect_handle)
{
	return haptic_stop_all_effects(device_handle);
}

API int haptic_stop_all_effects(haptic_device_h device_handle)
{
	int ret;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->stop_all_effects) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->stop_all_effects == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->stop_all_effects((int)device_handle);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_stop_all_effects is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_get_effect_state(haptic_device_h device_handle, haptic_effect_h effect_handle, haptic_state_e *effect_state)
{
	int ret;
	int state;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (effect_handle < 0) {
		PRT_ERR("Invalid parameter : effect_handle(%d)", effect_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (effect_state == NULL) {
		PRT_ERR("Invalid parameter : effect_state(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->get_effect_state) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->get_effect_state == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->get_effect_state((int)device_handle, (int)effect_handle, &state);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_get_effect_state is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	*effect_state = (haptic_state_e)state;
	return HAPTIC_ERROR_NONE;
}

API int haptic_create_effect(unsigned char *vibe_buffer,
                         int max_bufsize,
                         haptic_effect_element_s *elem_arr,
                         int max_elemcnt)
{
	int ret;
	int i;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (vibe_buffer == NULL) {
		PRT_ERR("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_bufsize <= 0) {
		PRT_ERR("Invalid parameter : max_bufsize(%d)", max_bufsize);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (elem_arr == NULL) {
		PRT_ERR("Invalid parameter : elem_arr(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_elemcnt <= 0) {
		PRT_ERR("Invalid parameter : max_elemcnt(%d)", max_elemcnt);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->create_effect) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->create_effect == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	for (i = 0; i < max_elemcnt; i++) {
		if (elem_arr[i].haptic_level == HAPTIC_FEEDBACK_AUTO) {
			vconf_get_int(VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT, &elem_arr[i].haptic_level);
            elem_arr[i].haptic_level = elem_arr[i].haptic_level*20;
		}
	}

	ret = plugin_intf->create_effect(vibe_buffer, max_bufsize, (haptic_module_effect_element*)elem_arr, max_elemcnt);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_create_effect is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_save_effect(const unsigned char *vibe_buffer,
                       int max_bufsize,
                       const char *file_path)
{
	int ret;
	struct stat buf;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (vibe_buffer == NULL) {
		PRT_ERR("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_bufsize <= 0) {
		PRT_ERR("Invalid parameter : max_bufsize(%d)", max_bufsize);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_path == NULL) {
		PRT_ERR("Invalid parameter : file_path(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!stat(file_path, &buf)) {
		PRT_ERR("Already exist : file_path(%s)", file_path);
		return HAPTIC_ERROR_FILE_EXISTS;
	}

	if (!plugin_intf || !plugin_intf->save_effect) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->save_effect == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	PRT_DBG("file path : %s", file_path);
	ret = plugin_intf->save_effect(vibe_buffer, max_bufsize, file_path);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_save_effect is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_get_file_duration(haptic_device_h device_handle, const char *file_path, int *file_duration)
{
	int ret;
	struct stat buf;
	int duration;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_path == NULL) {
		PRT_ERR("Invalid parameter : file_path(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (stat(file_path, &buf)) {
		PRT_ERR("Invalid parameter : (%s) is not presents", file_path);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_duration == NULL) {
		PRT_ERR("Invalid parameter : file_duration(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->get_file_duration) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->get_file_duration == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->get_file_duration((int)device_handle, file_path, &duration);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_stop_get_file_duration is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	*file_duration = duration;
	return HAPTIC_ERROR_NONE;
}

API int haptic_get_buffer_duration(haptic_device_h device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	return haptic_get_buffers_duration(device_handle,
									vibe_buffer,
									0,
									buffer_duration);
}

API int haptic_get_buffers_duration(haptic_device_h device_handle, const unsigned char *vibe_buffer, int size, int *buffer_duration)
{
	int ret;
	int duration;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (device_handle < 0) {
		PRT_ERR("Invalid parameter : device_handle(%d)", device_handle);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (vibe_buffer == NULL) {
		PRT_ERR("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (buffer_duration == NULL) {
		PRT_ERR("Invalid parameter : buffer_duration(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!plugin_intf || !plugin_intf->get_buffer_duration) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->get_buffer_duration == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = plugin_intf->get_buffer_duration((int)device_handle, vibe_buffer, &duration);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_stop_get_buffer_duration is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	*buffer_duration = duration;
	return HAPTIC_ERROR_NONE;
}

API int haptic_save_led(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	int ret;
	struct stat buf;

	if (__handle_cnt == 0) {
		PRT_ERR("Not initialized");
		return HAPTIC_ERROR_NOT_INITIALIZED;
	}

	if (vibe_buffer == NULL) {
		PRT_ERR("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_bufsize <= 0) {
		PRT_ERR("Invalid parameter : max_bufsize(%d)", max_bufsize);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_path == NULL) {
		PRT_ERR("Invalid parameter : file_path(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (!stat(file_path, &buf)) {
		PRT_ERR("Already exist : file_path(%s)", file_path);
		return HAPTIC_ERROR_FILE_EXISTS;
	}

	if (!plugin_intf || !plugin_intf->convert_binary) {
		PRT_ERR("plugin_intf == NULL || plugin_intf->convert_binary == NULL");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	PRT_DBG("file path : %s", file_path);
	ret = plugin_intf->convert_binary(vibe_buffer, max_bufsize, file_path);
	if (ret != HAPTIC_MODULE_ERROR_NONE) {
		PRT_ERR("haptic_internal_save_effect is failed : %d", ret);
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	return HAPTIC_ERROR_NONE;
}
