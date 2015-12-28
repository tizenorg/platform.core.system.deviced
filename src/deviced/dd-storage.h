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


#ifndef __DD_STORAGE_H__
#define __DD_STORAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-storage.h
 * @defgroup    CAPI_SYSTEM_DEVICED_STORAGE_MODULE Storage
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of storage
 * @section CAPI_SYSTEM_DEVICED_STORAGE_MODULE_HEADER Required Header
 *   \#include <dd-storage.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_STORAGE_MODULE
 * @{
 */

/**
 * @par Description
 * Storage path type
 */
enum storage_path_type {
	STORAGE_DEFAULT = 0,
	STORAGE_INTERNAL,
	STORAGE_EXTERNAL,
	STORAGE_MAX,
};

/**
 * @par Description:
 *  This API is used go get storage path.\n
 * @param[in] type storage path type
 * @param[in] path pointer to a buffer where the path is stored
 * @param[in] size size of buffer
 * @return 0 on success, -1 if failed
 * @see storage_path_type
 * @par Example
 * @code
 *  ...
 *  if ( storage_get_path(STORAGE_INTERNAL, path, 50) < 0 )
 *          printf("Get storage path failed\n");
 *  ...
 * @endcode
 */
int storage_get_path(int type, char *path, int size);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_STORAGE_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
