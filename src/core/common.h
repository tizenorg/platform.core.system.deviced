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


#ifndef __CORE_COMMON_H__
#define __CORE_COMMON_H__

#include <Ecore.h>
#include <stdio.h>
#include <error.h>
#include <stdbool.h>
#include <unistd.h>

#define ARRAY_SIZE(name) (sizeof(name)/sizeof(name[0]))

/*
 * One byte digit has 3 position in decimal representation
 * 2 - 5
 * 4 - 10
 * 8 - 20
 * >8 - compile time error
 * plus 1 null termination byte
 * plus 1 for negative prefix
 */
#define MAX_DEC_SIZE(type) \
	(2 + (sizeof(type) <= 1 ? 3 : \
	sizeof(type) <= 2 ? 5 : \
	sizeof(type) <= 4 ? 10 : \
	sizeof(type) <= 8 ? 20 : \
	sizeof(int[-2*(sizeof(type) > 8)])))

#ifndef __CONSTRUCTOR__
#define __CONSTRUCTOR__ __attribute__ ((constructor))
#endif

#ifndef __DESTRUCTOR__
#define __DESTRUCTOR__ __attribute__ ((destructor))
#endif

#ifndef __WEAK__
#define __WEAK__ __attribute__ ((weak))
#endif

#ifndef max
#define max(a, b)			\
	__extension__ ({		\
		typeof(a) _a = (a);	\
		typeof(b) _b = (b);	\
		_a > _b ? _a : _b;	\
	})
#endif

#ifndef min
#define min(a, b)			\
	__extension__ ({		\
		typeof(a) _a = (a);	\
		typeof(b) _b = (b);	\
		_a < _b ? _a : _b;	\
	})
#endif

#ifndef clamp
#define clamp(x, low, high)						\
	__extension__ ({						\
		typeof(x) _x = (x);					\
		typeof(low) _low = (low);				\
		typeof(high) _high = (high);				\
		((_x > _high) ? _high : ((_x < _low) ? _low : _x));	\
	})
#endif

#ifndef SEC_TO_MSEC
#define SEC_TO_MSEC(x)		((x)*1000)
#endif
#ifndef MSEC_TO_USEC
#define MSEC_TO_USEC(x)		((unsigned int)(x)*1000)
#endif
#ifndef NSEC_TO_MSEC
#define NSEC_TO_MSEC(x)		((double)x/1000000)
#endif
#ifndef USEC_TO_MSEC
#define USEC_TO_MSEC(x)		((double)x/1000)
#endif

#ifndef safe_free
#define safe_free(x) safe_free_memory((void**)&(x))
#endif

static inline void safe_free_memory(void** mem)
{
	if (mem && *mem) {
		free(*mem);
		*mem = NULL;
	}
}

#define ret_value_if(expr, val) do { \
        if (expr) { \
                _E("(%s)", #expr); \
                return (val); \
        } \
} while (0)

#define ret_value_msg_if(expr, val, fmt, arg...) do {	\
	if (expr) {				\
		_E(fmt, ##arg);			\
		return val;			\
	}					\
} while (0)

#define ret_msg_if(expr, fmt, arg...) do {	\
	if (expr) {				\
		_E(fmt, ##arg);			\
		return;			\
	}					\
} while (0)

FILE * open_proc_oom_score_adj_file(int pid, const char *mode);
int get_exec_pid(const char *execpath);
int get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size);
int is_vip(int pid);
int run_child(int argc, const char *argv[]);
int remove_dir(const char *path, int del_dir);
int sys_check_node(char *path);
int sys_get_int(char *fname, int *val);
int sys_set_int(char *fname, int val);
int terminate_process(const char* partition, bool force);
int mount_check(const char* path);
void print_time(const char *prefix);
int manage_notification(char *title, char *content);

#endif	/* __CORE_COMMON_H__ */

