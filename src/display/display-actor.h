/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#ifndef __DISPLAY_ACTOR_H__
#define __DISPLAY_ACTOR_H__

#include <errno.h>
#include "core/common.h"

enum display_actor_id {
	DISPLAY_ACTOR_POWER_KEY	= 1,
	DISPLAY_ACTOR_MENU_KEY,
	DISPLAY_ACTOR_API,
	DISPLAY_ACTOR_GESTURE,
};

struct display_actor_ops {
	enum display_actor_id id;
	unsigned int caps;
};

enum display_capability {
	DISPLAY_CAPA_BRIGHTNESS		= 1 << 0,
	DISPLAY_CAPA_LCDON		= 1 << 1,
	DISPLAY_CAPA_LCDOFF		= 1 << 2,
	DISPLAY_CAPA_POWEROFF		= 1 << 3,
};

void display_add_actor(struct display_actor_ops *actor);
int display_set_caps(enum display_actor_id id, unsigned int caps);
int display_reset_caps(enum display_actor_id id, unsigned int caps);
unsigned int display_get_caps(enum display_actor_id id);
int display_has_caps(unsigned int total_caps, unsigned int caps);

#endif

