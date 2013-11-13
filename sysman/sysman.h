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


#ifndef ___SYSMAN___
#define ___SYSMAN___

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include "sysman_managed.h"

#ifndef DEPRECATED
#define DEPRECATED __attribute__((deprecated))
#endif

/** 
 * @file        sysman.h
 * @ingroup     libsysman System Manager library
 * @brief       This library provides APIs related with memory, performance, processes, and so on.
 * @author      SLP2.0
 * @date        2010-01-24
 * @version     0.1
 */

/**
 * @defgroup libsysman System Manager library
 * @ingroup SYSTEM_FRAMEWORK
 * @brief System manager library
 *
 * This library provides APIs related with memory, performance, processes, and so on.
 * <br> Please use libslp-sysman-dev debian package and sysman.pc file for development.
 * <br> And include sysman.h file at your source codes as following.
 * @addtogroup libsysman System Manager library
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @breif Policy for low memory
 */
	enum mem_policy {
		OOM_LIKELY,	/**< For miscellaneous applications */
		OOM_IGNORE	/**< For daemons */
	};

/* sysman_util */

/**
 * @fn int sysman_get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size)
 * @brief This API is used to get the file name of command line of the process from the proc fs.
 * 		Caller process MUST allocate enough memory for the cmdline parameter. \n
 * 		Its size should be assigned to cmdline_size. \n
 * 		Internally it reads the 1st argument of /proc/{pid}/cmdline and copies it to cmdline.
 * @param[in] pid pid of the process that you want to get the file name of command line
 * @param[out] cmdline command line of the process that you want to get the file name <br>
 * 			Callers should allocate memory to this parameter before calling this function.
 * 			The allocated memory size must be large enough to store the file name.
 * 			The result will include the terminating null byte('\0') at the end of the string.
 * @return 0 on success, -1 if failed. If the size of cmdline is smaller than the result,
 * 			it will return -1 and errno will be set as EOVERFLOW.
 */
	int sysman_get_cmdline_name(pid_t pid, char *cmdline,
				    size_t cmdline_size);

/**
 * @fn char *sysman_get_apppath(pid_t pid, char *app_path, size_t app_path_size)
 * @brief This API is used to get the execution path of the process specified by the pid parameter.\n
 * 		Caller process MUST allocate enough memory for the app_path parameter. \n
 * 		Its size should be assigned to app_path_size. \n
 * 		Internally it reads a link of /proc/{pid}/exe and copies the path to app_path. 
 * @param[in] pid pid of the process that you want to get the executed path
 * @param[out] app_path the executed file path of the process <br>
 * 			Callers should allocate memory to this parameter before calling this function.
 * 			The allocated memory size must be large enough to store the executed file path.
 * 			The result will include the terminating null byte('\0') at the end of the string.
 * @param[in] app_path_size allocated memory size of char *app_path 
 * @return 0 on success, -1 if failed. If the size of app_path is smaller than the result,
 * 			it will return -1 and errno will be set as EOVERFLOW.
 */
	int sysman_get_apppath(pid_t pid, char *app_path, size_t app_path_size);

/* sysconf */

/**
 * @fn int sysconf_set_mempolicy(enum mem_policy mempol)
 * @brief This API is used to set the policy of the caller process when the phone has low available memory.
 * @param[in] mempol oom adjust value which you want to set
 * @return 0 on success, -1 if failed.
 * @see sysconf_set_mempolicy_bypid()
 */
	int sysconf_set_mempolicy(enum mem_policy mempol);

/**
 * @fn int sysconf_set_mempolicy_bypid(pid_t pid, enum mem_policy mempol)
 * @brief This API is used to set the policy of the process when the phone has low available memory.
 * @param[in] pid process id which you want to set
 * @param[in] mempol oom adjust value which you want to set
 * @return 0 on success, -1 if failed.
 */
	int sysconf_set_mempolicy_bypid(pid_t pid, enum mem_policy mempol);

/**
 * @fn int sysconf_set_permanent(void)
 * @brief This API is used to set itself as a permanent process.\n
 * 		If the permanent process is dead, system server will relaunch the process automatically.
 * @return 0 on success, -1 if failed.
 * @see sysconf_set_permanent_bypid()
 * @par Example
 * @code
 *      ...
 *      ret = sysconf_set_permanent();
 *      if( ret < 0 )
 *              printf("Fail to set a process as permanent\n");
 *      ...
 * @endcode
 */
	int sysconf_set_permanent(void);

/**
 * @fn int sysconf_set_permanent_bypid(pid_t pid)
 * @brief This API is used to set a process which has pid as a permanent process.\n
 *              If the permanent process is dead, system server will relaunch the process automatically.
 * @return 0 on success, -1 if failed.
 * @see sysconf_set_permanent()
 * @par Example
 * @code
 *      ...
 *      ret = sysconf_set_permanent_bypid(pid);
 *      if( ret < 0 )
 *              printf("Fail to set a process(%d) as permanent\n",pid);
 *      ...
 * @endcode
 */
	int sysconf_set_permanent_bypid(pid_t pid);

/**
 * @fn int sysconf_set_vip(pid_t pid)
 * @brief This API is used to set a process which has pid as Very Important Process(VIP) .\n
 * 		If the VIP process is dead, restarter program will be run. \n
 * 		Restarter program may kill almost processes and run rc.local scripts again.
 * @param[in] pid process id to be vip
 * @return 0 on success, -1 if failed.
 * @see sysconf_is_vip
 * @par Example
 * @code
 * 	...
 * 	ret = sysconf_set_vip(pid);
 * 	if( ret < 0 )
 * 		printf("Fail to set a process(%d) as VIP\n",pid);
 * 	...
 * @endcode
 */
	int sysconf_set_vip(pid_t pid);

/**
 * @fn int sysconf_is_vip(pid_t pid)
 * @brief This API is used to verify that process which has pid is Very Important Process(VIP) or not.
 * @param[in] pid process id to be vip
 * @return 1 on success, 0 if failed.
 * @see sysconf_set_vip
 * @par Example
 * @code
 * 	...
 * 	ret = sysconf_is_vip(pid);
 * 	if(ret)
 * 		printf("process(%d) is Very Important Process\n",pid);
 * 	...
 * @endcode
 */
	int sysconf_is_vip(pid_t pid);

	int sysman_set_timezone(char *tzpath_str);

	int sysman_call_predef_action(const char *type, int num, ...);

	int sysman_inform_foregrd(void);
	int sysman_inform_backgrd(void);
	int sysman_inform_active(pid_t pid);
	int sysman_inform_inactive(pid_t pid);

	int sysman_request_poweroff(void);
	int sysman_request_entersleep(void);
	int sysman_request_leavesleep(void);
	int sysman_request_reboot(void);

	int sysman_request_set_cpu_max_frequency(int val);
	int sysman_request_set_cpu_min_frequency(int val);

	int sysman_release_cpu_max_frequency(void);
	int sysman_release_cpu_min_frequency(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif				/* ___SYSMAN___ */
