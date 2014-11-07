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


#ifndef __DD_DEVICED_MANAGED_H__
#define __DD_DEVICED_MANAGED_H__

#include <sys/time.h>
#include "dd-mmc.h"

/**
 * @file        dd-deviced-managed.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CAPI_SYSTEM_DEVICED
 * @{
 */

/**
 * @fn int deviced_get_pid(const char *execpath)
 * @brief This API is used to get the pid of the process which has the specified execpath.\n
 * 		Internally, this API searches /proc/{pid}/cmdline and compares the parameter execpath with 1st argument of cmdline. \n
 * 		If there is no process that has same execpath in /proc/{pid}/cmdline, it will return -1.
 * @param[in] execpath program path which you want to know whether it is run or not
 * @return pid when the program is running, -1 if it is not.
 */
int deviced_get_pid(const char *execpath);

/**
 * @fn int deviced_set_datetime(time_t timet)
 * @brief This API is used to set date time.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * @param[in] timet type of time which you want to set.
 * @return pid when the program is running, -1 if param is less than 0 or when failed set datetime.
 */
int deviced_set_datetime(time_t timet);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif /* __DD_DEVICED_MANAGED_H__ */
