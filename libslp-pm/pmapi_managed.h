/*
 *  libslp-pm
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Seunghun Pi <sh.pi@samsung.com>
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


#ifndef __POWER_MANAGER_LIBRARY_MANAGED_H__
#define __POWER_MANAGER_LIBRARY_MANAGED_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn int pm_change_state(unsigned int state);
 * @brief This API is used to change the power manager state by force.
 * @param[in] state power manager state - LCD_NORMAL, LCD_DIM, LCD_OFF
 * @return 0 on success, -1 if failed.
 * @see pm_lock_state(), pm_unlock_state()
 * @pat Example
 * @code
 * 	...
 * 	result = pm_change_state(LCD_OFF);
 * 	if( result < 0 )
 *              printf("[ERROR] return value result =%d, \n",result);
 *      else
 *              printf("[SUCCESS]return value result =%d \n",result);
 *      ...
 * @endcode
 */
	int pm_change_state(unsigned int);

#ifdef __cplusplus
}
#endif
#endif				/* __POWER_MANAGER_LIBRARY_MANAGED_H__ */
