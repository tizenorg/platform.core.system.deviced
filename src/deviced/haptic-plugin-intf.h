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


#ifndef __HAPTIC_PLUGIN_INTF_H__
#define __HAPTIC_PLUGIN_INTF_H__

#include "haptic-module.h"

struct haptic_ops {
	int (*get_device_count) (int*);
	int (*open_device) (int, int*);
	int (*close_device) (int);
	int (*vibrate_monotone) (int, int, int, int, int*);
	int (*vibrate_file) (int, const char*, int, int, int, int*);
	int (*vibrate_buffer) (int, const unsigned char*, int, int, int, int*);
	int (*stop_effect) (int, int);
	int (*stop_all_effects) (int);
	int (*pause_effect) (int, int);
	int (*resume_effect) (int, int);
	int (*get_effect_state) (int, int, int*);
	int (*create_effect) (unsigned char*, int, haptic_module_effect_element*, int);
	int (*save_effect) (const unsigned char*, int, const char*);
	int (*get_file_duration) (int, const char*, int*);
	int (*get_buffer_duration) (int, const unsigned char*, int*);
	int (*convert_binary) (const unsigned char*, int, const char*);
};

const struct haptic_ops *get_haptic_plugin_interface();

#endif	/* __HAPTIC_PLUGIN_INTF_H__ */
