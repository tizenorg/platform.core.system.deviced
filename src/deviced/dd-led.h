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


#ifndef __DD_LED_H__
#define __DD_LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @file        dd-led.h
 * @defgroup    CAPI_SYSTEM_DEVICED_LED_MODULE Led
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of led
 * @section CAPI_SYSTEM_DEVICED_LED_MODULE_HEADER Required Header
 *   \#include <dd-led.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_LED_MODULE
 * @{
 */

#define led_set_brightness(val)	\
		led_set_brightness_with_noti(val, false)

/**
 * @par Description:
 *  This API is used to get the current brightness of the led.\n
 *  It gets the current brightness of the led by calling device_get_property() function.\n
 *  It returns integer value which is the current brightness on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return current brightness value on success, -1 if failed
 * @see led_set_brightness_with_noti()
 * @par Example
 * @code
 *  ...
 *  int cur_brt;
 *  cur_brt = led_get_brightness();
 *  if( cur_brt < 0 )
 *      printf("Fail to get the current brightness of the led.\n");
 *  else
 *      printf("Current brightness of the led is %d\n", cur_brt);
 *  ...
 * @endcode
 */
int led_get_brightness(void);

/**
 * @par Description:
 *  This API is used to get the max brightness of the led.\n
 *  It gets the max brightness of the led by calling device_get_property() function.\n
 *  It returns integer value which is the max brightness on success.\n
 *  Or a negative value(-1) is returned on failure
 * @return max brightness value on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  int max_brt;
 *  max_brt = led_get_max_brightness();
 *  if( max_brt < 0 )
 *      printf("Fail to get the max brightness of the led.\n");
 *  ...
 * @endcode
 */
int led_get_max_brightness(void);

/**
 * @par Description:
 *  This API is used to set the current brightness of the led.\n
 *      It sets the current brightness of the led by calling device_set_property() function.\n
 * @param[in] val brightness value that you want to set
 * @param[in] enable noti
 * @return 0 on success, -1 if failed
 * @see led_get_brightness()
 * @par Example
 * @code
 *  ...
 *  if( led_set_brightness_with_noti(1, 1) < 0 )
 *     printf("Fail to set the brightness of the led\n");
 *  ...
 * @endcode
 */
int led_set_brightness_with_noti(int val, bool enable);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_LED_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
