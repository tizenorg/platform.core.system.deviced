/*
 * devman
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
#include <vconf.h>

#include "devlog.h"
#include "devman_haptic.h"
#include "devman_haptic_ext_core.h"

#ifndef EXTAPI
#define EXTAPI __attribute__ ((visibility("default")))
#endif /* EXTAPI */

/* START: devman_haptic APIs */
EXTAPI
int device_haptic_open(haptic_dev_idx dev_idx, unsigned int mode)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_close(int device_handle)
{

	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_buffer(int device_handle, const unsigned char *vibe_buffer, int iteration, int feedback_level)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_file(int device_handle, const char *file_name, int iteration, int feedback_level)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_file_with_priority(int device_handle, const char *file_name, int priority_level, int iteration, int feedback_level)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_pattern(int device_handle, int pattern, int iteration, int feedback_level)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_stop_play(int device_handle)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_monotone(int device_handle, int duration)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_monotone_with_feedback_level(int device_handle, int duration, int feedback_level)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_play_monotone_with_detail_feedback_level(int device_handle, int duration, int detail_feedback_level)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_get_buffer_duration(int device_handle, const unsigned char *vibe_buffer, int *duration)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_get_file_duration(int device_handle, const char *file_name, int *duration)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_get__duration(int device_handle, int pattern, int *duration)
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}

EXTAPI
int device_haptic_get_device_count()
{
	DEVERR("This api was deprecated, you have to use libhaptic.");
	return HAPTIC_FAIL;
}
/* END: devman_haptic APIs */


/* START: devman_haptic_ext APIs */
EXTAPI
int device_haptic_get_device_state(int device_index, int *state)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_device_capability_bool(int device_index, int device_cap_type, unsigned char *device_cap_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_device_capability_int32(int device_index, int device_cap_type, int *device_cap_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_device_capability_string(int device_index, int device_cap_type, int size, char *device_cap_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_device_property_bool(int device_handle, int device_prop_type, unsigned char *device_prop_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_set_device_property_bool(int device_handle, int device_prop_type, unsigned char device_prop_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_device_property_int32(int device_handle, int device_prop_type, int *device_prop_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_set_device_property_int32(int device_handle, int device_prop_type, int device_prop_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_device_property_string(int device_handle, int device_prop_type, int size, char *device_prop_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_set_device_property_string(int device_handle, int device_prop_type, const char *device_prop_value)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_count(const unsigned char *haptic_buffer)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_name(const unsigned char *haptic_buffer, int effect_index, int size, char *effect_name)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_name_u(const unsigned char *haptic_buffer, int effect_index, int size, unsigned short *effect_name)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_index_from_name(const unsigned char *haptic_buffer, char const *effect_name, int *effect_index)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_index_from_name_u(const unsigned char *haptic_buffer, const unsigned short *effect_name, int *effect_index)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_play_effect(int device_handle, const unsigned char *haptic_buffer, int effect_index, int *effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_play_effect_repeat(int device_handle, const unsigned char *haptic_buffer, int effect_index, unsigned char repeat, int *effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_stop_playing_effect(int device_handle, int effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_type(const unsigned char *haptic_buffer, int effect_index, int *effect_type)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_magsweep_effect_definition(const unsigned char *haptic_buffer, int effect_index,
	int *duration, int *magnitude, int *style, int *attacktime, int *attacklevel,
	int *fadetime, int *fadelevel)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_periodic_effect_definition(const unsigned char *haptic_buffer, int effect_index,
	int *duration, int *magnitude, int *period, int *style_and_wave_type, int *attacktime, int *attacklevel,
	int *fadetime, int *fadelevel)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_duration(const unsigned char *haptic_buffer, int effect_index, int *effect_duration)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_play_magsweep_effect(int device_handle,
	int duration, int magnitude, int style, int attacktime, int attacklevel,
	int fadetime, int fadelevel, int *effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_play_periodic_effect(int device_handle,
	int duration, int magnitude, int period, int style_and_wave_type, int attacktime, int attacklevel,
	int fadetime, int fadelevel, int *effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_modify_playing_magsweep_effect(int device_handle, int effect_handle,
        int duration, int magnitude, int style, int attacktime, int attacklevel, int fadetime, int fadelevel)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_modify_playing_periodic_effect(int device_handle, int effect_handle,
        int duration, int magnitude, int period, int style_and_wave_type, int attacktime, int attacklevel, int fadetime, int fadelevel)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_create_streaming_effect(int device_handle, int *effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_destroy_streaming_effect(int device_handle, int effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_play_streaming_sample(int device_handle, int effect_handle, const unsigned char *streaming_sample, int size)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_play_streaming_sample_with_offset(int device_handle, int effect_handle, const unsigned char *streaming_sample, int size, int offset_time)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_stop_all_playing_effects(int device_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_save_file(const unsigned char *ivt_buffer, int max_bufsize, const char *path_name)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_delete_file(const char *path_name)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_pause_playing_effect(int device_handle, int effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_resume_paused_effect(int device_handle, int effect_handle)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_effect_state(int device_handle, int effect_handle, int *effect_state)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_get_size(const unsigned char *haptic_buffer, int size)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_initialize_buffer(unsigned char *haptic_buffer, int size)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_insert_element(unsigned char *haptic_buffer, int size, int timeline_index, const HapticElement *element)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_insert_element2(unsigned char *haptic_buffer, int size, int timeline_index, const HapticElement2 *element)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_insert_element3(unsigned char *haptic_buffer, int size, int timeline_index, const HapticElement3 *element)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_read_element(const unsigned char *haptic_buffer, int size, int timeline_index, int element_index, HapticElement *element)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_read_element2(const unsigned char *haptic_buffer, int size, int timeline_index, int element_index, HapticElement2 *element)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_read_element3(const unsigned char *haptic_buffer, int size, int timeline_index, int element_index, HapticElement3 *element)
{
	return HAPTIC_SUCCESS;
}

EXTAPI
int device_haptic_remove_element(unsigned char *haptic_buffer, int size, int timeline_index, int element_index)
{
	return HAPTIC_SUCCESS;
}
/* END: devman_haptic_ext APIs */
