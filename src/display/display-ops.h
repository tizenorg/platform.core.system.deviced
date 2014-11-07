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


#ifndef __DISPLAY_OPS_H__
#define __DISPLAY_OPS_H__

#include <errno.h>
#include "core/common.h"

struct display_ops {
	char *name;
	void (*init) (void *data);
	void (*exit) (void *data);
};

void display_ops_init(void *data);
void display_ops_exit(void *data);

#define DISPLAY_OPS_REGISTER(disp)	\
static void __CONSTRUCTOR__ module_init(void)	\
{	\
	add_display(disp);	\
}	\
static void __DESTRUCTOR__ module_exit(void)	\
{	\
	remove_display(disp);	\
}

void add_display(const struct display_ops *disp);
void remove_display(const struct display_ops *disp);
const struct display_ops *find_display_feature(const char *name);

#endif
