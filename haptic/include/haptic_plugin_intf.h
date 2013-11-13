/*
 *  haptic
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jiyoung Yun <jy910.yun@samsung.com>
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
 *
*/


#ifndef __HAPTIC_PLUGIN_INTF_H__
#define __HAPTIC_PLUGIN_INTF_H__

#include "haptic_module.h"

typedef struct {
	int (*haptic_internal_get_device_count) (int*);
	int (*haptic_internal_open_device) (int, int*);
	int (*haptic_internal_close_device) (int);
	int (*haptic_internal_vibrate_monotone) (int, int, int, int, int*);
	int (*haptic_internal_vibrate_file) (int, const char*, int, int, int, int*);
	int (*haptic_internal_vibrate_buffer) (int, const unsigned char*, int, int, int, int*);
	int (*haptic_internal_stop_effect) (int, int);
	int (*haptic_internal_stop_all_effects) (int);
	int (*haptic_internal_pause_effect) (int, int);
	int (*haptic_internal_resume_effect) (int, int);
	int (*haptic_internal_get_effect_state) (int, int, int*);
	int (*haptic_internal_create_effect) (unsigned char*, int, haptic_module_effect_element*, int);
	int (*haptic_internal_save_effect) (const unsigned char*, int, const char*);
	int (*haptic_internal_get_file_duration) (int, const char*, int*);
	int (*haptic_internal_get_buffer_duration) (int, const unsigned char*, int*);
	int (*haptic_internal_convert_binary) (const unsigned char*, int, const char*);
} haptic_plugin_interface;

const haptic_plugin_interface *get_haptic_plugin_interface();

#endif	/* __HAPTIC_PLUGIN_INTF_H__ */
