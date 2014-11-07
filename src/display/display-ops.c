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


#include <stdio.h>

#include "util.h"
#include "display-ops.h"
#include "core/list.h"
#include "core/common.h"

static dd_list *disp_head;

void add_display(const struct display_ops *disp)
{
	DD_LIST_APPEND(disp_head, disp);
}

void remove_display(const struct display_ops *disp)
{
	DD_LIST_REMOVE(disp_head, disp);
}

const struct display_ops *find_display_feature(const char *name)
{
	dd_list *elem;
	const struct display_ops *disp;

	DD_LIST_FOREACH(disp_head, elem, disp) {
		if (!strcmp(disp->name, name))
			return disp;
	}
	return NULL;
}

void display_ops_init(void *data)
{
	dd_list *elem;
	const struct display_ops *disp;

	DD_LIST_FOREACH(disp_head, elem, disp) {
		_D("[%s] initialize", disp->name);
		if (disp->init)
			disp->init(data);
	}
}

void display_ops_exit(void *data)
{
	dd_list *elem;
	const struct display_ops *disp;

	DD_LIST_FOREACH(disp_head, elem, disp) {
		_D("[%s] deinitialize", disp->name);
		if (disp->exit)
			disp->exit(data);
	}
}

