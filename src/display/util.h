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


/**
 * @file	util.h
 * @brief	Utilities header for Power manager
 */
#ifndef __DEF_UTIL_H__
#define __DEF_UTIL_H__

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

#ifndef FEATURE_POWER_MANAGER_DLOG
#define FEATURE_POWER_MANAGER_DLOG
#endif

#ifdef FEATURE_POWER_MANAGER_DLOG
#define LOG_TAG	"POWER_MANAGER"
#include <dlog.h>
#define _D(fmt, arg...) \
	do { SLOGD(fmt, ##arg); } while(0)
#define _I(fmt, arg...) \
	do { SLOGI(fmt, ##arg); } while(0)
#define _E(fmt, arg...) \
	do { SLOGE(fmt, ##arg); } while(0)
#else
#  include <syslog.h>
#define _D(fmt, arg...) pm_log(LOG_INFO, fmt, ## arg)
#define _I(fmt, arg...) pm_log(LOG_INFO, fmt, ## arg)
#define _E(fmt, arg...) pm_log(LOG_ERR, fmt, ## arg)
#endif

#define SEC_TO_MSEC(x)	((x)*1000)
#define MSEC_TO_SEC(x)	(double(x)/1000)
/*
 * @brief write the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return 0 (always)
 */
extern int writepid(char *pidpath);

/*
 * @brief read the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return  pid : success, -1 : failed
 */
extern int readpid(char *pidpath);

/*
 * @brief daemonize function
 *
 * fork the process, kill the parent process
 * and replace all the standard fds to /dev/null.
 *
 * @return 0 : success, -1 : fork() error
 */
extern int daemonize(void);

/**
 * @brief  function to run a process
 *
 * fork the process, and run the other process if it is child.
 *
 * @return new process pid on success, -1 on error
 */
extern int exec_process(char *name);

/**
 * @brief  function to get the pkg name for AUL (Application Util Library)
 *
 * remove the path of exepath and make the "com.samsung.<exe_file_name>" string.
 *
 * @return new process pid on success, -1 on error
 */
extern char *get_pkgname(char *exepath);

/*
 * @brief logging function
 *
 * This is log wrapper
 *
 * @param[in] priority log pritority
 * @param[in] fmt format string
 */
extern void pm_log(int priority, char *fmt, ...);

/**
 * @}
 */
#endif
