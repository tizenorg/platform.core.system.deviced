/*
 * test
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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


#ifndef __TEST_H__
#define __TEST_H__
#include <stdio.h>
#include <errno.h>

#include "core/list.h"
#include "core/common.h"
#include "libdeviced/dbus.h"

#ifdef ENABLE_TEST_DLOG
#define ENABLE_DLOG
#endif

#define LOG_TAG "AUTO_TEST"
#include "libdeviced/log-macro.h"

enum test_priority {
	TEST_PRIORITY_NORMAL = 0,
	TEST_PRIORITY_HIGH,
};

struct test_ops {
	enum test_priority priority;
	char *name;
	void (*init) (void *data);
	void (*exit) (void *data);
	int (*start) (void);
	int (*stop) (void);
	int (*status) (void);
};

enum test_ops_status {
	TEST_OPS_STATUS_UNINIT,
	TEST_OPS_STATUS_START,
	TEST_OPS_STATUS_STOP,
	TEST_OPS_STATUS_MAX,
};

void test_init(void *data);
void test_exit(void *data);

static inline int test_start(const struct test_ops *c)
{
	if (c && c->start)
		return c->start();

	return -EINVAL;
}

static inline int test_stop(const struct test_ops *c)
{
	if (c && c->stop)
		return c->stop();

	return -EINVAL;
}

static inline int test_get_status(const struct test_ops *c)
{
	if (c && c->status)
		return c->status();

	return -EINVAL;
}

#define TEST_OPS_REGISTER(c)	\
static void __CONSTRUCTOR__ module_init(void)	\
{	\
	add_test(c);	\
}	\
static void __DESTRUCTOR__ module_exit(void)	\
{	\
	remove_test(c);	\
}
DBusMessage *deviced_dbus_method_sync_with_reply(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[]);
void add_test(const struct test_ops *c);
void remove_test(const struct test_ops *c);
const struct test_ops *find_test(const char *name);

#endif
