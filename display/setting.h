/*
 * power-manager
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
*/


/*
 * @file	setting.h
 * @brief	Power manager setting module header
 */
#ifndef __PM_SETTING_H__
#define __PM_SETTING_H__

#include <vconf.h>

/*
 * @addtogroup POWER_MANAGER
 * @{
 */

enum {
	SETTING_BEGIN = 0,
	SETTING_TO_NORMAL = SETTING_BEGIN,
	SETTING_BRT_LEVEL,
	SETTING_LOCK_SCREEN,
	SETTING_POWER_SAVING,
	SETTING_POWER_SAVING_DISPLAY,
	SETTING_GET_END,
	SETTING_PM_STATE = SETTING_GET_END,
	SETTING_LOW_BATT,
	SETTING_CHARGING,
	SETTING_POWEROFF,
	SETTING_END
};

extern int (*update_pm_setting) (int key_idx, int val);

extern int get_setting_brightness();

/*
 * @brief setting initialization function
 *
 * get the variables if it exists. otherwise, set the default.
 * and register some callback functions.
 *
 * @internal
 * @param[in] func configuration change callback function
 * @return 0 : success, -1 : error
 */
extern int init_setting(int (*func) (int key_idx, int val));

extern int exit_setting();

/*
 * get normal state timeout from SLP-setting SLP_SETTING_LCD_TIMEOUT_NORMAL
 *
 * @internal
 * @param[out] timeout timeout variable pointer
 * @return 0 : success, -1 : error
 */
extern int get_run_timeout(int *timeout);

/*
 * get LCD dim state timeout from environment variable.
 *
 * @internal
 * @param[out] timeout timeout variable pointer
 * @return 0 : success, -1 : error
 */
extern int get_dim_timeout(int *timeout);

/*
 * get LCD off state timeout from environment variable.
 *
 * @internal
 * @param[out] timeout timeout variable pointer
 * @return 0 : success, -1 : error
 */
extern int get_off_timeout(int *timeout);

/*
 * get USB connection status from SLP-setting SLP_SETTING_USB_STATUS
 *
 * @internal
 * @param[out] val usb connection status variable pointer, 0 is disconnected, others is connected.
 * @return 0 : success, -1 : error
 */
extern int get_usb_status(int *val);

/*
 * set Current power manager state at SLP-setting "memory/pwrmgr/state"
 *
 * @internal
 * @param[in] val current power manager state.
 * @return 0 : success, -1 : error
 */
extern int set_setting_pmstate(int val);

/*
 * get charging status at SLP-setting "memory/Battery/Charger"
 *
 * @internal
 * @param[in] val charging or not (1 or 0 respectively).
 * @return 0 : success, -1 : error
 */
extern int get_charging_status(int *val);

/*
 * get current battery low status at SLP-setting "memory/Battery/Status/Low"
 *
 * @internal
 * @param[in] val current low battery status
 * @return 0 : success, -1 : error
 */
extern int get_lowbatt_status(int *val);

/*
 * @}
 */

#endif
