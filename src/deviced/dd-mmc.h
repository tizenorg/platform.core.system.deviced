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


#ifndef __DD_MMC_H__
#define __DD_MMC_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-mmc.h
 * @defgroup    CAPI_SYSTEM_DEVICED_MMC_MODULE MMC
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @brief       This file provides the API for control of mmc(sd-card)
 * @section CAPI_SYSTEM_DEVICED_MMC_MODULE_HEADER Required Header
 *   \#include <dd-mmc.h>
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_MMC_MODULE
 * @{
 */

/**
 * @brief This structure defines the data for receive result of mmc operations(mount/unmount/format)
 */
struct mmc_contents {
	void (*mmc_cb) (int result, void* data);/**< user callback function for receive result of mmc operations */
	void* user_data;/**< input data for callback function's second-param(data) */
};

/**
 * @fn int deviced_request_mount_mmc(struct mmc_contents *mmc_data)
 * @brief This API is used to mount mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when mount operation is finished, cb of deviced_mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(mount success) and negative value if failed \n
 * 		[mount fail value] \n
 * 		-1 : operation not permmitted \n
 * 		-2 : no such file or directory \n
 * 		-6 : no such device or address \n
 * 		-12 : out of memory \n
 * 		-13 : A component of a path was not searchable \n
 * 		-14 : bad address \n
 * 		-15 : block device is requested \n
 * 		-16 : device or resource busy \n
 * 		-19 : filesystemtype not configured in the kernel \n
 * 		-20 : target, or a prefix of source, is not a directory \n
 * 		-22 : point does not exist \n
 * 		-24 : table of dummy devices is full \n
 * 		-36 : requested name is too long \n
 * 		-40 : Too many links encountered during pathname resolution. \n
 * 			Or, a move was attempted, while target is a descendant of source \n
 * @param[in] mmc_data for receive result of mount operation
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
int deviced_request_mount_mmc(struct mmc_contents *mmc_data);

/**
 * @fn int deviced_request_unmount_mmc(struct mmc_contents *mmc_data,int option)
 * @brief This API is used to unmount mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when unmount opeation is finished, cb of deviced_mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(unmount success) and negative value if failed \n
 * 		[unmount fail value] \n
 * 		-1 : operation not permmitted \n
 * 		-2 : no such file or directory \n
 * 		-11 : try again \n
 * 		-12 : out of memory \n
 * 		-14 : bad address \n
 * 		-16 : device or resource busy \n
 * 		-22 : point does not exist \n
 * 		-36 : requested name is too long \n
 * @param[in] mmc_data for receive result of unmount operation
 * @param[in] option type of unmount option \n
 *		0 : Normal unmount \n
 *			(if other process still access a sdcard, \n
 *			 unmount will be failed.) \n
 *		1 : Force unmount \n
 *			(if other process still access a sdcard, \n
 *			this process will be received SIGTERM or SIGKILL.)
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
int deviced_request_unmount_mmc(struct mmc_contents *mmc_data, int option);

/**
 * @fn int deviced_request_format_mmc(struct mmc_contents *mmc_data)
 * @brief This API is used to format mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when format opeation is finished, cb of deviced_mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(format success) , -1(format fail)
 * @param[in] mmc_data for receive result of format operation
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
int deviced_request_format_mmc(struct mmc_contents *mmc_data);

/**
 * @fn int deviced_format_mmc(struct mmc_contents *mmc_data, int option)
 * @brief This API is used to format mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when format opeation is finished, cb of deviced_mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(format success) and negative value if failed \n
 *		[format fail value] \n
 *		-22 : Invalid argument(EINVAL) \n
 * @param[in] mmc_data for receive result of format operation
 * @param[in] option FMT_NORMAL is 0, FMT_FORCE is 1
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
int deviced_format_mmc(struct mmc_contents *mmc_data, int option);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_MMC_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
