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


#ifndef __DD_BATTERY_H__
#define __DD_BATTERY_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-battery.h
 * @defgroup    CAPI_SYSTEM_DEVICED_BATTERY_MODULE Battery
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of battery
 * @section CAPI_SYSTEM_DEVICED_BATTERY_MODULE_HEADER Required Header
 *   \#include <dd-battery.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_BATTERY_MODULE
 * @{
 */

/**
 * @par Description
 * Battery Health status
 */
enum {
	BAT_UNKNOWN = 0,
	BAT_GOOD,
	BAT_OVERHEAT,
	BAT_DEAD,
	BAT_OVERVOLTAGE,
	BAT_UNSPECIFIED,
	BAT_COLD,
	BAT_HEALTH_MAX,
};

/**
 * @par Description:
 *      This API is used to get the remaining battery percentage.\n
 *      It gets the Battery percentage by calling device_get_property() function.\n
 *      It returns integer value(0~100) that indicates remaining batterty percentage on success.\n
 *      Or a negative value(-1) is returned on failure.
 * @return On success, integer value(0~100) is returned.
 *  Or a negative value(-1) is returned on failure.
 * @see battery_is_full(), battery_get_percent_raw()
 * @par Example
 * @code
 *  ...
 *  int battery;
 *  battery = battery_get_percent();
 *  if( battery < 0 )
 *      printf("Fail to get the remaining battery percentage.\n");
 *  else
 *      printf("remaining battery percentage : %d\n", battery);
 *  ...
 * @endcode
 */
int battery_get_percent(void);

/**
 * @par Description:
 *      This API is used to get the remaining battery percentage expressed 1/10000.\n
 *      It gets the Battery percentage by calling device_get_property() function.\n
 *      It returns integer value(0~10000) that indicates remaining batterty percentage on success.\n
 *      Or a negative value(-1) is returned on failure.
 * @return On success, integer value(0~10000) is returned.
 *  Or a negative value(-1) is returned on failure.
 * @see battery_is_full(), battery_get_percent()
 * @par Example
 * @code
 *  ...
 *  int battery;
 *  battery = battery_get_percent_raw();
 *  if( battery < 0 )
 *      printf("Fail to get the remaining battery percentage.\n");
 *  else
 *      printf("remaining battery percentage expressed 1/10000 : %d\n", battery);
 *  ...
 * @endcode
 */
int battery_get_percent_raw(void);

/**
 * @par Description:
 *      This API is used to get the fully charged status of battery.\n
 *  It gets the fully charged status of Battery by calling device_get_property() function.\n
 *      If the status of battery is full, it returns 1.\n
 *      Or a negative value(-1) is returned, if the status of battery is not full.
 * @return 1 with battery full, or 0 on not full-charged, -1 if failed
 * @see battery_get_percent()
 * @par Example
 * @code
 *  ...
 *  if( battery_is_full() > 0 )
 *      printf("battery fully chared\n");
 *  ...
 * @endcode
 */
int battery_is_full(void);

/**
 * @par Description:
 *  This API is used to get the battery health status.\n
 *  It gets the battery health status by calling device_get_property() function.\n
 *      It returns integer value(0~7) that indicate battery health status on success.\n
 *  Or a negative value(-1) is returned, if the status of battery is not full.
 * @return integer value, -1 if failed\n
 * (0 BATTERY_UNKNOWN, 1 GOOD, 2 OVERHEAT, 3 DEAD, 4 OVERVOLTAGE, 5 UNSPECIFIED, 6 COLD, 7 BAT_HEALTH_MAX)
 * @par Example
 * @code
 *  ...
 *  int bat_health;
 *  bat_health = battery_get_health();
 *  if( bat_health != BAT_GOOD )
 *      printf("battery health is not good\n");
 *  ...
 * @endcode
 */
int battery_get_health(void);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_BATTERY_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
