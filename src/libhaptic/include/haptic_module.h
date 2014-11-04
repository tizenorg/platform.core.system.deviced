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


#ifndef __HAPTIC_MODULE_H__
#define __HAPTIC_MODULE_H__


/**
 * @brief Enumerations of device id for the Haptic Module API.
 * @details We support two motors now.
 */
typedef enum {
    HAPTIC_MODULE_DEVICE_0 = 0x0,             /**< 1st motor */
    HAPTIC_MODULE_DEVICE_1 = 0x1,             /**< 2nd motor */
    HAPTIC_MODULE_DEVICE_ALL = 0x4,           /**< both of them */
} haptic_module_device;

/**
 * @brief Enumerations of priority level for the Haptic Module API.
 */
typedef enum
{
    HAPTIC_MODULE_PRIORITY_MIN = 0,        /**< Minimum effect priority for developers (default) */
    HAPTIC_MODULE_PRIORITY_MIDDLE,         /**< Maximum effect priority for developers */
    HAPTIC_MODULE_PRIORITY_HIGH,           /**< Maximum effect priority for OEMs */
} haptic_module_priority;

/**
 * @brief Enumerations of feedback level for the Haptic Module API.
 * @details Haptic level means vibration power (intensity).
 */
typedef enum
{
    HAPTIC_MODULE_FEEDBACK_MIN = 0,
    HAPTIC_MODULE_FEEDBACK_MAX = 100,
} haptic_module_feedback;

/**
 * @brief Enumerations of iteration count for the Haptic Module API.
 */
typedef enum
{
    HAPTIC_MODULE_ITERATION_ONCE = 1,
    HAPTIC_MODULE_ITERATION_INFINITE = 256,
} haptic_module_iteration;

/**
 * @brief Enumerations of effect or device state for the Haptic Module API.
 */
typedef enum
{
    HAPTIC_MODULE_STATE_PLAYING = 0,
    HAPTIC_MODULE_STATE_STOP,
} haptic_module_state;

/* Error and Return value codes */
#define HAPTIC_MODULE_ERROR_NONE         						 0
#define HAPTIC_MODULE_NOT_INITIALIZED    						-1
#define HAPTIC_MODULE_ALREADY_INITIALIZED						-2
#define HAPTIC_MODULE_INVALID_ARGUMENT   						-3
#define HAPTIC_MODULE_OPERATION_FAILED   						-4
#define HAPTIC_MODULE_NOT_SUPPORTED								-5

/**
 * @par Description:
 *      effect element for haptic module.
 */
typedef struct {
    int haptic_duration; /**< Duration of the effect element in millisecond */
	int haptic_level;	 /**< Level of the effect element (0 ~ 100) */
} haptic_module_effect_element;

#endif  /* __HAPTIC_MODULE_H__ */
