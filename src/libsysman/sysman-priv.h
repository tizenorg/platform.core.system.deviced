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


#ifndef ___SYSNMAN_PRIVATE___
#define ___SYSNMAN_PRIVATE___

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef API
#define API __attribute__ ((visibility("default")))
#endif

#define __PRTD(fmt, arg...) \
	do { \
		if (getenv("SLP_DEBUG")) { \
			fprintf(stdout, "[%s:%d] "fmt"\n", __FILE__, __LINE__, ##arg); \
		} \
	} while (0)
#define _NOUT(fmt, arg...) do { } while (0)

#ifdef SLP_DEBUG
#  define DBG _PRTD
#else
#  define DBG _NOUT
#endif

#define ENABLE_DLOG_OUT 1
#ifdef ENABLE_DLOG_OUT
#  define LOG_TAG	"LIBSYSMAN"
#  include <dlog.h>
#  define _PRTD	SLOGD
#  define ERR	SLOGE
#  define INFO	SLOGI
#else
#  define _PRTD __PRTD
#  define ERR 	perror
#  define INFO(fmt, arg...) \
	  do { fprintf(stdout, "[%s:%d] "fmt"\n", __FILE__, __LINE__, ##arg); } while (0)
#endif

#define SYSMAN_MAXARG 16
#define SYSMAN_MAXSTR 100
#define BUFF_MAX 255

	struct sysnoti {
		int pid;
		int cmd;
		char *type;
		char *path;
		int argc;
		char *argv[SYSMAN_MAXARG];
	};

	int util_launch_app_cmd(const char *cmdline);

#ifdef __cplusplus
}
#endif
#endif				/* ___SYSMAN_PRIVATE___ */
