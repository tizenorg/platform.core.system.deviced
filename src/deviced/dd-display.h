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


#ifndef __DD_DISPLAY_H__
#define __DD_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-display.h
 * @defgroup    CAPI_SYSTEM_DEVICED_DISPLAY_MODULE Display
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of display
 * @section CAPI_SYSTEM_DEVICED_DISPLAY_MODULE_HEADER Required Header
 *   \#include <dd-display.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_DISPLAY_MODULE
 * @{
 */

#include "dd-common.h"

/**
 * LCD state
 */
#define LCD_NORMAL  0x01 /**< NORMAL state */
#define LCD_DIM     0x02 /**< LCD dimming state */
#define LCD_OFF     0x04 /**< LCD off state */
#define SUSPEND     0x08 /**< Sleep state */
#define POWER_OFF   0x10 /**< Sleep state */
#define STANDBY     0x20 /**< Standby state */
#define SETALL (LCD_DIM | LCD_OFF | LCD_NORMAL)	/*< select all state - not supported yet */

/**
 * Parameters for display_lock_state()
 */
#define STAY_CUR_STATE	0x1
#define GOTO_STATE_NOW	0x2
#define HOLD_KEY_BLOCK	0x4
#define STANDBY_MODE	0x8

/**
 * Parameters for display_unlock_state()
 */
#define PM_SLEEP_MARGIN	0x0	/**< keep guard time for unlock */
#define PM_RESET_TIMER	0x1	/**< reset timer for unlock */
#define PM_KEEP_TIMER	0x2	/**< keep timer for unlock */

/**
 * @brief This API is used to get the max brightness of the display.\n
 * @details It gets the current brightness of the display
 *	by calling device_get_property() function.\n
 *	It returns integer value which is the max brightness on success.\n
 *	Or a negative value(-1) is returned on failure
 * @return max brightness value on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  int max_brt;
 *  max_brt = display_get_max_brightness();
 *  if( max_brt < 0 )
 *      printf("Fail to get the max brightness of the display.\n");
 *  else
 *      printf("Max brightness of the display is %d\n", max_brt);
 *  ...
 * @endcode
 */
int display_get_max_brightness(void);

/**
 * @brief This API is used to set the current brightness of the display
 *	and system brightness value in settings.\n
 * @details It sets the current brightness of the display
 *	by calling device_set_property() function.\n
 *	MUST use this API very carefully. \n
 *	This api is different from display_set_brightness api.\n
 *	display_set_brightness api will change only device brightness value.\n
 *	but this api will change device brightness
 *	as well as system brightness value.\n
 * @param[in] val brightness value that you want to set
 * @return 0 on success, negative if failed
 * @see display_set_brightness()
 * @par Example
 * @code
 *  ...
 *  if( display_set_brightness_with_setting(6) < 0 )
 *      printf("Fail to set the current brightness of the display\n");
 *  else
 *      printf("The current brightness of the display is set 6\n");
 *  ...
 * @endcode
 */
int display_set_brightness_with_setting(int val);

/**
 * @brief This API is used to lock a particular display state
 *	as the current display-state.\n
 * @details The parameter state specifies the display state which
 *	you want to lock LCD_NORMAL, LCD_DIM, LCD_OFF. \n
 *	The second parameter Flag is set if you want to go
 *	the requested lock state directly.\n
 *	The third parameter timeout specifies lock-timeout in milliseconds.
 *	If the value 0 is selected, the display state remains locked
 *	until display_unlock_state is called.
 * @param[in] state target power state which you want to lock
 *	- LCD_NORMAL, LCD_DIM, LCD_OFF
 * @param[in] flag set if you want to go the lock state directly \n
 *	GOTO_STATE_NOW - State is changed directly you want to lock.\n
 *	STAY_CUR_STATE - State is not changed directly and
 *	phone stay current state until timeout expired.\n
 *	HOLD_KEY_BLOCK - Hold key is blocked during locking
 *	LCD_NORMAL or LCD_DIM. \n
 *	Then LCD state transition to LCD_OFF is blocked. \n
 *	If this flag is not set, phone state is lcd off
 *	after pressing hold key. \n
 *	GOTO_STATE_NOW and STAY_CUR_STATE can't be applied at the same time.
 * @param[in] timeout lock-timeout in miliseconds. \n
 *	0 is always lock until calling display_unlock_state
 * @return 0 on success, negative if failed
 * @see display_unlock_state(), display_change_state()
 * @par Example
 * @code
 *  ...
 *  result = pm_lock_state(LCD_NORMAL, GOTO_STATE_NOW, SET_TIMEOUT);
 *  if( result < 0 )
 *      printf("[ERROR] return value result =%d, \n",result);
 *  else
 *      printf("[SUCCESS]return value result =%d \n",result);
 *  ...
 * @endcode
 */
int display_lock_state(unsigned int state, unsigned int flag, unsigned int timeout);

/**
 * @brief This API is used to unlock the display state. \n
 * @details The parameter state specifies the display
 *	state which you want to unlock. \n
 *	Some examples are LCD_NORMAL, LCD_DIM, LCD_OFF.
 * @param[in] state target display state which you want to unlock
 * @param[in] flag set timer which is going to the next state after unlocking\n
 *	PM_SLEEP_MARGIN - If the current status is lcd off,
 *	deviced reset timer to 1 second.
 *	If the current status is not lcd off, deviced uses the existing timer.\n
 *	PM_RESET_TIMER - deviced resets timer.
 *	(lcd normal : reset timer to predfined value
 *	which is set in setting module,
 *	lcd dim : reset timer to 5 seconds, lcd off : reset timer to 1 seconds)\n
 *	PM_KEEP_TIMER - deviced uses the existing timer
 *	(if timer is already expired, deviced changes the status) \n
 * @return 0 on success, negative if failed
 * @see display_lock_state(), display_change_state()
 * @par Example
 * @code
 *  ...
 *  result = display_unlock_state(LCD_NORMAL,PM_RESET_TIMER);
 *  if( result < 0 )
 *      printf("[ERROR] return value result =%d, \n",result);
 *  else
 *      printf("[SUCCESS]return value result =%d \n",result);
 *  ...
 * @endcode
 */
int display_unlock_state(unsigned int state, unsigned int flag);

/**
 * @brief This API is used to change display state by force.
 * @param[in] state display state - LCD_NORMAL, LCD_DIM, LCD_OFF
 * @return 0 on success, negative if failed.
 * @see display_lock_state(), display_unlock_state()
 * @par Example
 * @code
 *  ...
 *  result = display_change_state(LCD_OFF);
 *  if( result < 0 )
 *      printf("[ERROR] return value result =%d, \n",result);
 *  else
 *      printf("[SUCCESS]return value result =%d \n",result);
 *  ...
 * @endcode
 */
int display_change_state(unsigned int state);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_DISPLAY_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
