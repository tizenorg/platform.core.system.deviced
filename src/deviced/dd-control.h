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
 * @defgroup    CAPI_SYSTEM_DEVICED_CONTROL_MODULE Control
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API to enable/disable devices
 * @section CAPI_SYSTEM_DEVICED_CONTROL_MODULE_HEADER Required Header
 *   \#include <dd-control.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_CONTROL_MODULE
 * @{
 */

/**
 * @par Description
 * Control Device type
 */
enum control_device_type {
/* Add device define here  */
	DEVICE_CONTROL_MMC,
	DEVICE_CONTROL_USBCLIENT,
	DEVICE_CONTROL_MAX,
};

/**
 * @par Description:
 *  This API is used to enable/disable mmc device.\n
 * @param[in] enable enable/disable mmc device
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  if( deviced_mmc_control(1) < 0 )
 *      printf("Enable mmc device failed\n");
 *  ...
 * @endcode
 */
int deviced_mmc_control(bool enable);

/**
 * @par Description:
 *  This API is used to enable/disable usb device.\n
 * @param[in] enable enable/disable usb device
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  if( deviced_usb_control(1) < 0 )
 *      printf("Enable usb device failed\n");
 *  ...
 * @endcode
 */
int deviced_usb_control(bool enable);

/* Only USB-manager will use the api */
/**
 * @par Description:
 * @return
 * @par Example
 * @code
 * @endcode
 * @todo describe function
 */
int deviced_get_usb_control(void);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_CONTROL_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
