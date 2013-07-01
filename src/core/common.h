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


#ifndef _SS_COMMON_H
#define _SS_COMMON_H

#include <unistd.h>

#define ARRAY_SIZE(name) (sizeof(name)/sizeof(name[0]))

FILE * open_proc_oom_adj_file(int pid, const char *mode);
int get_exec_pid(const char *execpath);
int get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size);
int is_vip(int pid);

#endif	/* _SS_COMMON_H */

