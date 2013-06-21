/*
 * deviced
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


#ifndef __DD_LED_H__
#define __DD_LED_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-led.h
 * @ingroup     DEVICED_LIBRARY
 * @brief       This file provides for control of led
 */

int led_get_brightness(void);
int led_get_max_brightness(void);
int led_set_brightness(int val);

#ifdef __cplusplus
}
#endif
#endif
