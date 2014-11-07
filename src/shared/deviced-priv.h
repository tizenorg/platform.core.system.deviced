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


#ifndef __SYSTEM_PRIVATE__
#define __SYSTEM_PRIVATE__

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_NOTI_MAXARG 16
#define SYSTEM_NOTI_MAXSTR 255
#define BUFF_MAX 255

struct sysnoti {
	int pid;
	int cmd;
	char *type;
	char *path;
	int argc;
	char *argv[SYSTEM_NOTI_MAXARG];
};

	int util_launch_app_cmd(const char *cmdline);

#ifdef __cplusplus
}
#endif
#endif				/* __SYSTEM_PRIVATE__ */
