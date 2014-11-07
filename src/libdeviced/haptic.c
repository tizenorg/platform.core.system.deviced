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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <vconf.h>

#include "log.h"
#include "dbus.h"
#include "haptic-plugin-intf.h"
#include "dd-haptic.h"
#include "common.h"

#define METHOD_OPEN_DEVICE			"OpenDevice"
#define METHOD_CLOSE_DEVICE			"CloseDevice"
#define METHOD_STOP_DEVICE			"StopDevice"
#define METHOD_VIBRATE_MONOTONE		"VibrateMonotone"
#define METHOD_VIBRATE_BUFFER		"VibrateBuffer"
#define METHOD_GET_COUNT			"GetCount"
#define METHOD_GET_STATE			"GetState"
#define METHOD_GET_DURATION			"GetDuration"
#define METHOD_CREATE_EFFECT		"CreateEffect"
#define METHOD_SAVE_BINARY			"SaveBinary"

#define TEMP_BUFFER_SIZE			(64*1024)

/* START of Static Function Section */
static unsigned char* convert_file_to_buffer(const char *file_name, int *size)
{
	FILE *pf;
	long file_size;
	unsigned char *pdata = NULL;

	if (!file_name)
		return NULL;

	/* Get File Stream Pointer */
	pf = fopen(file_name, "rb");
	if (!pf) {
		_E("fopen failed : %s", strerror(errno));
		return NULL;
	}

	if (fseek(pf, 0, SEEK_END))
		goto error;

	file_size = ftell(pf);
	if (fseek(pf, 0, SEEK_SET))
		goto error;

	if (file_size < 0)
		goto error;

	pdata = (unsigned char*)malloc(file_size);
	if (!pdata)
		goto error;

	if (fread(pdata, 1, file_size, pf) != file_size)
		goto err_free;

	fclose(pf);
	*size = file_size;
	return pdata;

err_free:
	free(pdata);

error:
	fclose(pf);

	_E("failed to convert file to buffer (%s)", strerror(errno));
	return NULL;
}

static int save_data(const unsigned char *data, int size, const char *file_path)
{
	FILE *file;
	int fd;

	file = fopen(file_path, "wb+");
	if (file == NULL) {
		_E("To open file is failed : %s", strerror(errno));
		return -1;
	}

	if (fwrite(data, 1, size, file) != size) {
		_E("To write file is failed : %s", strerror(errno));
		fclose(file);
		return -1;
	}

	fd = fileno(file);
	if (fd < 0) {
		_E("To get file descriptor is failed : %s", strerror(errno));
		fclose(file);
		return -1;
	}

	if (fsync(fd) < 0) {
		_E("To be synchronized with the disk is failed : %s", strerror(errno));
		fclose(file);
		return -1;
	}

	fclose(file);
	return 0;
}

static haptic_feedback_e convert_setting_to_module_level(void)
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

	/* check if pointer is valid */
	if (device_number == NULL) {
		_E("Invalid parameter : device_number(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* request to deviced to get haptic count */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_GET_COUNT, NULL, NULL);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	*device_number = ret;
	return HAPTIC_ERROR_NONE;
}

API int haptic_open(haptic_device_e device_index, haptic_device_h *device_handle)
{
	char str_index[32];
	char *arr[1];
	int ret;

	/* check if index is valid */
	if (!(device_index == HAPTIC_DEVICE_0 || device_index == HAPTIC_DEVICE_1 ||
				device_index == HAPTIC_DEVICE_ALL)) {
		_E("Invalid parameter : device_index(%d)", device_index);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if pointer is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	snprintf(str_index, sizeof(str_index), "%d", device_index);
	arr[0] = str_index;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_OPEN_DEVICE, "i", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	*device_handle = (haptic_device_h)ret;
	return HAPTIC_ERROR_NONE;
}

API int haptic_close(haptic_device_h device_handle)
{
	char str_handle[32];
	char *arr[1];
	int ret;

	/* check if handle is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	snprintf(str_handle, sizeof(str_handle), "%u", device_handle);
	arr[0] = str_handle;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_CLOSE_DEVICE, "u", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

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
	char str_handle[32];
	char str_duration[32];
	char str_feedback[32];
	char str_priority[32];
	char *arr[4];
	int ret;

	/* check if handle is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if passed arguments are valid */
	if (duration < 0 && duration != HAPTIC_DURATION_UNLIMITED) {
		_E("Invalid parameter : duration(%d)", duration);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (feedback < HAPTIC_FEEDBACK_0 || feedback > HAPTIC_FEEDBACK_AUTO) {
		_E("Invalid parameter : feedback(%d)", feedback);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (priority < HAPTIC_PRIORITY_MIN || priority > HAPTIC_PRIORITY_HIGH) {
		_E("Invalid parameter : priority(%d)", priority);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* in case of FEEDBACK_AUTO, should be converted */
	if (feedback == HAPTIC_FEEDBACK_AUTO)
		feedback = convert_setting_to_module_level();

	snprintf(str_handle, sizeof(str_handle), "%u", device_handle);
	arr[0] = str_handle;
	snprintf(str_duration, sizeof(str_duration), "%d", duration);
	arr[1] = str_duration;
	snprintf(str_feedback, sizeof(str_feedback), "%d", feedback);
	arr[2] = str_feedback;
	snprintf(str_priority, sizeof(str_priority), "%d", priority);
	arr[3] = str_priority;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_VIBRATE_MONOTONE, "uiii", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	if (effect_handle != NULL)
		*effect_handle = (haptic_effect_h)ret;

	return HAPTIC_ERROR_NONE;
}

API int haptic_vibrate_file(haptic_device_h device_handle, const char *file_path, haptic_effect_h *effect_handle)
{
	char *vibe_buffer;
	int size, ret;

	vibe_buffer = convert_file_to_buffer(file_path, &size);
	if (!vibe_buffer) {
		_E("Convert file to buffer error");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = haptic_vibrate_buffers_with_detail(device_handle,
											vibe_buffer,
											size,
											HAPTIC_ITERATION_ONCE,
											HAPTIC_FEEDBACK_AUTO,
											HAPTIC_PRIORITY_MIN,
											effect_handle);
	free(vibe_buffer);
	return ret;
}

API int haptic_vibrate_file_with_detail(haptic_device_h device_handle,
                                    const char *file_path,
                                    haptic_iteration_e iteration,
                                    haptic_feedback_e feedback,
                                    haptic_priority_e priority,
                                    haptic_effect_h *effect_handle)
{
	char *vibe_buffer;
	int size, ret;

	vibe_buffer = convert_file_to_buffer(file_path, &size);
	if (!vibe_buffer) {
		_E("Convert file to buffer error");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = haptic_vibrate_buffers_with_detail(device_handle,
											vibe_buffer,
											size,
											iteration,
											feedback,
											priority,
											effect_handle);
	free(vibe_buffer);
	return ret;
}

API int haptic_vibrate_buffer(haptic_device_h device_handle, const unsigned char *vibe_buffer, haptic_effect_h *effect_handle)
{
	return haptic_vibrate_buffers_with_detail(device_handle,
											 vibe_buffer,
											 TEMP_BUFFER_SIZE,
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
											 TEMP_BUFFER_SIZE,
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
	char str_handle[32];
	char str_iteration[32];
	char str_feedback[32];
	char str_priority[32];
	char *arr[6];
	struct dbus_byte bytes;
	int ret;

	/* check if handle is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if passed arguments are valid */
	if (vibe_buffer == NULL) {
		_E("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (iteration < HAPTIC_ITERATION_ONCE || iteration > HAPTIC_ITERATION_INFINITE) {
		_E("Invalid parameter : iteration(%d)", iteration);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (feedback < HAPTIC_FEEDBACK_0 || feedback > HAPTIC_FEEDBACK_AUTO) {
		_E("Invalid parameter : feedback(%d)", feedback);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (priority < HAPTIC_PRIORITY_MIN || priority > HAPTIC_PRIORITY_HIGH) {
		_E("Invalid parameter : priority(%d)", priority);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* in case of FEEDBACK_AUTO, should be converted */
	if (feedback == HAPTIC_FEEDBACK_AUTO)
		feedback = convert_setting_to_module_level();

	snprintf(str_handle, sizeof(str_handle), "%u", device_handle);
	arr[0] = str_handle;
	bytes.size = size;
	bytes.data = vibe_buffer;
	arr[2] = (char*)&bytes;
	snprintf(str_iteration, sizeof(str_iteration), "%d", iteration);
	arr[3] = str_iteration;
	snprintf(str_feedback, sizeof(str_feedback), "%d", feedback);
	arr[4] = str_feedback;
	snprintf(str_priority, sizeof(str_priority), "%d", priority);
	arr[5] = str_priority;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_VIBRATE_BUFFER, "uayiii", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	if (effect_handle != NULL)
		*effect_handle = (haptic_effect_h)ret;

	return HAPTIC_ERROR_NONE;
}

API int haptic_stop_effect(haptic_device_h device_handle, haptic_effect_h effect_handle)
{
	return haptic_stop_all_effects(device_handle);
}

API int haptic_stop_all_effects(haptic_device_h device_handle)
{
	char str_handle[32];
	char *arr[1];
	int ret;

	/* check if handle is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	snprintf(str_handle, sizeof(str_handle), "%u", device_handle);
	arr[0] = str_handle;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_STOP_DEVICE, "u", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	return HAPTIC_ERROR_NONE;
}

API int haptic_get_effect_state(haptic_device_h device_handle, haptic_effect_h effect_handle, haptic_state_e *effect_state)
{
	char str_index[32];
	char *arr[1];
	int ret;

	/* check if handle is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if pointer is valid */
	if (effect_state == NULL) {
		_E("Invalid parameter : effect_state(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	snprintf(str_index, sizeof(str_index), "%d", HAPTIC_DEVICE_0);
	arr[0] = str_index;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_GET_STATE, "i", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	*effect_state = (haptic_state_e)ret;
	return HAPTIC_ERROR_NONE;
}

API int haptic_create_effect(unsigned char *vibe_buffer,
                         int max_bufsize,
                         haptic_effect_element_s *elem_arr,
                         int max_elemcnt)
{
	DBusError err;
	DBusMessage *msg;
	dbus_bool_t ret;
	char str_bufsize[32];
	char str_elemcnt[32];
	char *arr[4];
	char *data;
	int i, temp, size, ret_val;
	struct dbus_byte bytes;

	/* check if passed arguments are valid */
	if (vibe_buffer == NULL) {
		_E("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_bufsize <= 0) {
		_E("Invalid parameter : max_bufsize(%d)", max_bufsize);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (elem_arr == NULL) {
		_E("Invalid parameter : elem_arr(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_elemcnt <= 0) {
		_E("Invalid parameter : max_elemcnt(%d)", max_elemcnt);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* convert to proper feedback level in case of auto */
	for (i = 0; i < max_elemcnt; i++) {
		if (elem_arr[i].haptic_level == HAPTIC_FEEDBACK_AUTO) {
			vconf_get_int(VCONFKEY_SETAPPL_TOUCH_FEEDBACK_VIBRATION_LEVEL_INT, &temp);
			elem_arr[i].haptic_level = temp*20;
		}
	}

	snprintf(str_bufsize, sizeof(str_bufsize), "%d", max_bufsize);
	arr[0] = str_bufsize;
	bytes.size = sizeof(haptic_effect_element_s)*max_elemcnt;
	bytes.data = (unsigned char*)elem_arr;
	arr[2] = (char*)&bytes;
	snprintf(str_elemcnt, sizeof(str_elemcnt), "%d", max_elemcnt);
	arr[3] = str_elemcnt;

	/* request to deviced to open haptic device */
	msg = dbus_method_sync_with_reply(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_CREATE_EFFECT, "iayi", arr);
	if (!msg)
		return HAPTIC_ERROR_OPERATION_FAILED;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &data, &size,
			DBUS_TYPE_INT32, &ret_val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		goto err;
	}

	if (ret_val < 0) {
		_E("%s-%s failed : %d", DEVICED_INTERFACE_HAPTIC, METHOD_CREATE_EFFECT, ret_val);
		goto err;
	}

	if (max_bufsize < size) {
		_E("max_bufsize(%d) is smaller than effect buffer size(%d)", max_bufsize, size);
		goto err;
	}

	memcpy(vibe_buffer, data, max_bufsize);
	dbus_message_unref(msg);
	return HAPTIC_ERROR_NONE;
err:
	dbus_message_unref(msg);
	return HAPTIC_ERROR_OPERATION_FAILED;
}

API int haptic_save_effect(const unsigned char *vibe_buffer,
                       int max_bufsize,
                       const char *file_path)
{
	struct stat buf;
	int size, ret;

	/* check if passed arguments are valid */
	if (vibe_buffer == NULL) {
		_E("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_bufsize <= 0) {
		_E("Invalid parameter : max_bufsize(%d)", max_bufsize);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_path == NULL) {
		_E("Invalid parameter : file_path(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if the file already exists */
	if (!stat(file_path, &buf)) {
		_E("Already exist : file_path(%s)", file_path);
		return HAPTIC_ERROR_FILE_EXISTS;
	}

	_D("file path : %s", file_path);
	ret = save_data(vibe_buffer, max_bufsize, file_path);
	if (ret < 0) {
		_E("fail to save data");
		return HAPTIC_MODULE_OPERATION_FAILED;
	}

	return HAPTIC_ERROR_NONE;
}

API int haptic_get_file_duration(haptic_device_h device_handle, const char *file_path, int *file_duration)
{
	char *vibe_buffer;
	int size, ret;

	vibe_buffer = convert_file_to_buffer(file_path, &size);
	if (!vibe_buffer) {
		_E("Convert file to buffer error");
		return HAPTIC_ERROR_OPERATION_FAILED;
	}

	ret = haptic_get_buffers_duration(device_handle,
									 vibe_buffer,
									 size,
									 file_duration);
	free(vibe_buffer);
	return ret;
}

API int haptic_get_buffer_duration(haptic_device_h device_handle, const unsigned char *vibe_buffer, int *buffer_duration)
{
	return haptic_get_buffers_duration(device_handle,
									vibe_buffer,
									TEMP_BUFFER_SIZE,
									buffer_duration);
}

API int haptic_get_buffers_duration(haptic_device_h device_handle, const unsigned char *vibe_buffer, int size, int *buffer_duration)
{
	char str_handle[32];
	char *arr[3];
	struct dbus_byte bytes;
	int ret;

	/* check if handle is valid */
	if (device_handle == NULL) {
		_E("Invalid parameter : device_handle(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (vibe_buffer == NULL) {
		_E("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if pointer is valid */
	if (buffer_duration == NULL) {
		_E("Invalid parameter : buffer_duration(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	snprintf(str_handle, sizeof(str_handle), "%u", device_handle);
	arr[0] = str_handle;
	bytes.size = size;
	bytes.data = vibe_buffer;
	arr[2] = (char*)&bytes;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_GET_DURATION, "uay", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	*buffer_duration = ret;
	return HAPTIC_ERROR_NONE;
}

API int haptic_save_led(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path)
{
	struct stat buf;
	char str_bufsize[32];
	char *arr[3];
	struct dbus_byte bytes;
	int ret;

	/* check if passed arguments are valid */
	if (vibe_buffer == NULL) {
		_E("Invalid parameter : vibe_buffer(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (max_bufsize <= 0) {
		_E("Invalid parameter : max_bufsize(%d)", max_bufsize);
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	if (file_path == NULL) {
		_E("Invalid parameter : file_path(NULL)");
		return HAPTIC_ERROR_INVALID_PARAMETER;
	}

	/* check if the file already exists */
	if (!stat(file_path, &buf)) {
		_E("Already exist : file_path(%s)", file_path);
		return HAPTIC_ERROR_FILE_EXISTS;
	}

	bytes.size = max_bufsize;
	bytes.data = vibe_buffer;
	arr[1] = (char*)&bytes;
	arr[2] = (char*)file_path;

	/* request to deviced to open haptic device */
	ret = dbus_method_sync(DEVICED_BUS_NAME,
			DEVICED_PATH_HAPTIC, DEVICED_INTERFACE_HAPTIC,
			METHOD_SAVE_BINARY, "ays", arr);
	if (ret < 0)
		return HAPTIC_ERROR_OPERATION_FAILED;

	return HAPTIC_ERROR_NONE;
}
