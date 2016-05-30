/*
 * deviced-vibrator
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


#ifndef __HAPTIC_H__
#define __HAPTIC_H__

#include <stdbool.h>
#include "core/common.h"
#include "haptic-plugin-intf.h"

#define HAPTIC_OPS_REGISTER(dev)	\
static void __CONSTRUCTOR__ module_init(void)	\
{	\
	add_haptic(dev);	\
}	\
static void __DESTRUCTOR__ module_exit(void)	\
{	\
	remove_haptic(dev);	\
}

enum haptic_type {
	HAPTIC_STANDARD,
	HAPTIC_EXTERNAL,
};

struct haptic_ops {
	enum haptic_type type;
	bool (*is_valid)(void);
	const struct haptic_plugin_ops *(*load)(void);
	void (*release)(void);
};

void add_haptic(const struct haptic_ops *ops);
void remove_haptic(const struct haptic_ops *ops);

int haptic_probe(void);
void haptic_init(void);
void haptic_exit(void);

#endif /* __HAPTIC_H__ */
