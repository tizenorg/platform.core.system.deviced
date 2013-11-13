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


#ifndef __DD_CONTROL_H__
#define __DD_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @file        dd-control.h
 * @ingroup     DEVICED_LIBRARY
 * @brief       This file provides to enable/disable devices
 */

#define CONTROL_HANDLER_NAME		"control"

enum control_device_type {
	/* Add device define here  */
	DEVICE_CONTROL_MMC,
	DEVICE_CONTROL_MAX,
};

/*
 * Add new function to control in library.
 */
int deviced_mmc_control(bool enable);

#ifdef __cplusplus
}
#endif
#endif
