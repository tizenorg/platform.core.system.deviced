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

#include "test.h"

static dd_list *dd_head;

void add_test(const struct test_ops *d)
{
	if (d->priority == TEST_PRIORITY_HIGH)
		DD_LIST_PREPEND(dd_head, d);
	else
		DD_LIST_APPEND(dd_head, d);
}

void remove_test(const struct test_ops *d)
{
	DD_LIST_REMOVE(dd_head, d);
}

const struct test_ops *find_test(const char *name)
{
	dd_list *elem;
	const struct test_ops *d;

	DD_LIST_FOREACH(dd_head, elem, d) {
		if (!strcmp(d->name, name))
			return d;
	}
	return NULL;
}

void test_init(void *data)
{
	dd_list *elem;
	const struct test_ops *d;

	DD_LIST_FOREACH(dd_head, elem, d) {
		_D("[%s] initialize", d->name);
		if (d->init)
			d->init(data);
	}
}

void test_exit(void *data)
{
	dd_list *elem;
	const struct test_ops *d;

	DD_LIST_FOREACH(dd_head, elem, d) {
		_D("[%s] deinitialize", d->name);
		if (d->exit)
			d->exit(data);
	}
}
