/*
 *  libslp-pm
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: DongGi Jang <dg0402.jang@samsung.com>
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


#ifndef __POWER_MANAGER_LIBRARY_H__
#define __POWER_MANAGER_LIBRARY_H__

#include "pmapi_managed.h"

/**
 * @defgroup POWER_MANAGER Power manager library
 * @ingroup SYSTEM_FRAMEWORK
 * @brief Power manager control API library
 *
 * @open
 * @addtogroup POWER_MANAGER
 * @{
 * This library provides APIs to lock/unlock the power state of the system.
 * Power Manager controls the power state as following.
 * @image html power-manager-fsm.bmp "Fig. 1 State Diagram of Power Manager
 * <br> If there is no user input for a certain time, PM changes the power state
 *  that has lower power consumption. <br> When the user input or other system interrupt occurs,
 *  PM rolls the state back immediately.<br> If applications or other frameworks want to stop
 *  enter the specific state of power manager, use pm_lock_state() and pm_unlock_state()
 *  <br> Be careful! A caller process should unlock the power state after locking without timeout.
 *  <br> If you want to stay the LCD normal state, you can use
 *  @li @c pm_lock_state(LCD_NORMAL, GOTO_STATE_NOW, 0)
 *  <p><br> After finishing your job with locking, call
 *  @li @c pm_unlock_state(LCD_NORMAL, PM_RESET_TIMER)
 *  <p><br> Additionally, you can use the timeout for lock the state.
 *  If timeout is set, caller process doesn't need to call the unlock API.
 *  But the process should be alive.
 *  If caller process is dead, lock condition would be drop by the power manager.
 *  <p><br> Here is sample codes
 * @code

#include "pmapi.h"
int main(int argc, char** argv)
{
	int result;

	printf("=========================================\n");
	printf("=  Lock / Unlock to transit a power manager state   =\n");
	printf("=========================================\n");

	result = pm_lock_state(LCD_NORMAL, GOTO_STATE_NOW,0); //Lock on lcd-off until explicit unlock.
	if (!result)
		printf("SUCCESS");
	else
		printf("FAILED");

	// DO something.

	result = pm_unlock_state(LCD_NORMAL,PM_RESET_TIMER); //Unlock
	if (!result)
		printf("SUCCESS");
	else
		printf("FAILED");

	result = pm_lock_state(LCD_OFF,STAY_CUR_STATE, 5000); // Lock on lcd-off during 5 seconds.
	if (!result)
		printf("SUCCESS");
	else
		printf("FAILED");

	// DO something.
	sleep(10);

	return 0;
}
 * @endcode
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_NORMAL	0x1   /**< NORMAL state */
#define LCD_DIM		0x2  /**< LCD dimming state */
#define LCD_OFF		0x4  /**< LCD off state */
#define SUSPEND		0x8  /**< Sleep state */
#define POWER_OFF	0x16  /**< Sleep state */
#define SETALL (LCD_DIM | LCD_OFF | LCD_NORMAL)	/*< select all state - not supported yet */

/* parameters for pm_lock_state() */
#define STAY_CUR_STATE	0x0
#define GOTO_STATE_NOW	0x1
#define HOLD_KEY_BLOCK	0x2

/* paramters for pm_unlcok_state() - details are described at 162 line */
#define PM_SLEEP_MARGIN	0x0	/**< keep guard time for unlock */
#define PM_RESET_TIMER	0x1	/**< reset timer for unlock */
#define PM_KEEP_TIMER	0x2	/**< keep timer for unlock */

/**
 * @fn int pm_lock_state	(unsigned int state,
 * 							 unsigned int flag,
 * 							 unsigned int timeout);
 * @brief This API is used to lock a particular power-state as the current power-state.\n
 * 		The parameter state specifies the power state which you want to lock LCD_NORMAL, LCD_DIM, LCD_OFF. \n
 * 		The second parameter Flag is set if you want to go the requested lock state directly.\n
 * 		The third parameter timeout specifies lock-timeout in milliseconds.
 * 		If the value 0 is selected, the power state remains locked until pm_unlock_state is called.
 * @param[in] state target power state which you want to lock - LCD_NORMAL, LCD_DIM, LCD_OFF
 * @param[in] flag set if you want to go the lock state directly
 *		GOTO_STATE_NOW - State is changed directly you want to lock.
 *		STAY_CUR_STATE - State is not changed directly and phone stay current state until timeout expired.
 * 				 (Default if there is no value in flag.)
 *		HOLD_KEY_BLOCK - Hold key is blocked during locking LCD_NORMAL or LCD_DIM.
 *				 Then LCD state transition to LCD_OFF is blocked.
 *				 If this flag is not set, phone state is lcd off after pressing hold key.
 *		GOTO_STATE_NOW and STAY_CUR_STATE can't be applied at the same time.
 * @param[in] timeout lock-timeout in miliseconds.
 * 					  0 is always lock until calling pm_unlock_state
 * 					  If you call this function with same state in duplicate,
 * 					  only last one will be processed and others are ignored.
 * @return 0 on success, -1 if failed
 * @see pm_unlock_state(), pm_change_state()
 * @par Example
 * @code
 * 	...
 * 	// Lock current state as LCD_NORMAL
 * 	result = pm_lock_state(LCD_NORMAL, GOTO_STATE_NOW, SET_TIMEOUT);
 * 	if( result < 0 )
 * 		printf("[ERROR] return value result =%d, \n",result);
 * 	else
 * 		printf("[SUCCESS]return value result =%d \n",result);
 *	...
 * @endcode
 */
	int pm_lock_state(unsigned int, unsigned int, unsigned int);

/**
 * @fn int pm_unlock_state	(unsigned int state,
 * 							 unsigned int flag)
 * @brief This API is used to Unlock the power state. \n
 * 		The parameter state specifies the power state which you want to unlock.
 * 		Some examples are LCD_NORMAL, LCD_DIM, LCD_OFF.\n
 * 		The second parameter flag is set if you want to go to the requested state directly after unlocking. (NOT SUPPOERTED YET)
 * 		This is valid only when the current state transition was blocked by the locking and this function call releases the blocking.
 * @param[in] state target power state which you want to unlock
 * @param[in] flag set timer which is going to the next state after unlocking
 * 		PM_SLEEP_MARGIN - If the current status is lcd off, pm reset timer to 5 second. If the current status is not lcd off, pm pm uses the existing timer.
 * 		PM_RESET_TIMER - Power-manager resets timer. (lcd normal : reset timer to predfined value which is set in setting module,  lcd dim or off : reset timer to 5 seconds)
 * 		PM_KEEP_TIMER - Power-manager uses the existing timer (if timer is already expired, pm changes the status) <br>
 * @return 0 on success, -1 if failed
 * @see pn_lock_state(), pm_change_state()
 * @par Example
 * @code
 * 	...
 *	//Un-lock NORMAL state so that power state change can occur with system-events
 *	result = pm_unlock_state(LCD_NORMAL,PM_RESET_TIMER);
 *	if( result < 0 )
 *		printf("[ERROR] return value result =%d, \n",result);
 *	else
 *		printf("[SUCCESS]return value result =%d \n",result);
 *	...
 * @endcode
 */
	int pm_unlock_state(unsigned int, unsigned int);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif				/* __POWER_MANAGER_LIBRARY_H__ */

