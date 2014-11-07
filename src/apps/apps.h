/*
 * deviced
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#ifndef __APPS_H__
#define __APPS_H__


#include "core/edbus-handler.h"
#include "core/common.h"

enum apps_enable_type{
	APPS_DISABLE = 0,
	APPS_ENABLE = 1,
};

#define APPS_OPS_REGISTER(dev)	\
static void __CONSTRUCTOR__ module_init(void)	\
{	\
	add_apps(dev);	\
}	\
static void __DESTRUCTOR__ module_exit(void)	\
{	\
	remove_apps(dev);	\
}

struct apps_data {
	const char *name;
	void *data;
};

struct apps_ops {
	const char *name;
	void (*init) (void);
	void (*exit) (void);
	int (*launch)(void *data);
	int (*terminate)(void *data);
};

void add_apps(const struct apps_ops *dev);
void remove_apps(const struct apps_ops *dev);

#endif /* __APPS_H__ */

