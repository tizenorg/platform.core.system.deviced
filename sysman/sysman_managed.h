/*
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


#ifndef ___SYSMAN_MANAGED___
#define ___SYSMAN_MANAGED___

#include <sys/time.h>

/**
 * @file        sysman_managed.h
 * @ingroup     libsysman System Manager library
 * @brief       This library provides APIs related with memory, performance, processes, and so on.
 * @author      SLP2.0
 * @date        2010-01-24
 * @version     0.1
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn int sysman_get_pid(const char *execpath)
 * @brief This API is used to get the pid of the process which has the specified execpath.\n
 * 		Internally, this API searches /proc/{pid}/cmdline and compares the parameter execpath with 1st argument of cmdline. \n
 * 		If there is no process that has same execpath in /proc/{pid}/cmdline, it will return -1.
 * @param[in] execpath program path which you want to know whether it is run or not
 * @return pid when the program is running, -1 if it is not.
 */
	int sysman_get_pid(const char *execpath);

/**
 * @fn int sysman_set_datetime(time_t timet)
 * @brief This API is used to set date time.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * @param[in] time_t type of time which you want to set.
 * @return pid when the program is running, -1 if param is less than 0 or when failed set datetime.
 */
	int sysman_set_datetime(time_t timet);

/**
 * @brief This structure defines the data for receive result of mmc operations(mount/unmount/format)
 */
	struct mmc_contents {
		void (*mmc_cb) (int result, void* data);/**< user callback function for receive result of mmc operations */
		void* user_data;/**< input data for callback function's second-param(data) */
	};

/**
 * @fn int sysman_request_mount_mmc(struct mmc_contents *mmc_data)
 * @brief This API is used to mount mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when mount operation is finished, cb of mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(mount success) ,  -2(already mounted), non-zero except (-2) (mount fail) \n
 * 		[mount fail value] \n
 * 		1 : operation not permmitted \n
 * 		2 : no such file or directory \n
 * 		6 : no such device or address \n
 * 		12 : out of memory \n
 * 		13 : A component of a path was not searchable \n
 * 		14 : bad address \n
 * 		15 : block device is requested \n
 * 		16 : device or resource busy \n
 * 		19 : filesystemtype not configured in the kernel \n
 * 		20 : target, or a prefix of source, is not a directory \n
 * 		22 : point does not exist \n
 * 		24 : table of dummy devices is full \n
 * 		36 : requested name is too long \n
 * 		40 : Too many links encountered during pathname resolution. \n
 * 			Or, a move was attempted, while target is a descendant of source \n
 * @param[in] mmc_data for receive result of mount operation
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
	int sysman_request_mount_mmc(struct mmc_contents *mmc_data);

/**
 * @fn int sysman_request_unmount_mmc(struct mmc_contents *mmc_data,int option)
 * @brief This API is used to unmount mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when unmount opeation is finished, cb of mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(unmount success) , non-zero(unmount fail) \n
 * 		[unmount fail value] \n
 * 		1 : operation not permmitted \n
 * 		2 : no such file or directory \n
 * 		11 : try again \n
 * 		12 : out of memory \n
 * 		14 : bad address \n
 * 		16 : device or resource busy \n
 * 		22 : point does not exist \n
 * 		36 : requested name is too long \n
 * @param[in] mmc_data for receive result of unmount operation
 * @param[in] option is must be 1(just only support for force unmount)
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
	int sysman_request_unmount_mmc(struct mmc_contents *mmc_data, int option);
/**
 * @fn int sysman_request_format_mmc(struct mmc_contents *mmc_data)
 * @brief This API is used to format mmc.\n
 * 		Internally, this API call predefined action API. That is send a notify message. \n
 * 		and when format opeation is finished, cb of mmc_content struct is called with cb's param1(result). \n
 * 		means of param1 - 0(format success) , -1(format fail)
 * @param[in] mmc_data for receive result of format operation
 * @return  non-zero on success message sending, -1 if message sending is failed.
 */
	int sysman_request_format_mmc(struct mmc_contents *mmc_data);

#ifdef __cplusplus
}
#endif
#endif /* ___SYSMAN_MANAGED___ */
