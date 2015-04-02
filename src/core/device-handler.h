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


#ifndef __DEVICE_HANDLER_H__
#define __DEVICE_HANDLER_H__

#include "common.h"

enum dock_type {
	DOCK_NONE	= 0,
	DOCK_SOUND	= 7,
};

int get_usb_state_direct(void);

void sync_cradle_status(void);

void internal_pm_change_state(unsigned int s_bits);
#endif /* __DEVICE_HANDLER_H__ */
