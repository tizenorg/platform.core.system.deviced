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


#include <stdio.h>

#include "util.h"
#include "display-actor.h"
#include "core/list.h"
#include "core/common.h"

static dd_list *actor_head;

void display_add_actor(struct display_actor_ops *actor)
{
	DD_LIST_APPEND(actor_head, actor);
}

static struct display_actor_ops *display_find_actor(enum display_actor_id id)
{
	dd_list *elem;
	struct display_actor_ops *actor;

	DD_LIST_FOREACH(actor_head, elem, actor) {
		if (actor->id == id)
			return actor;
	}
	return NULL;
}

int display_set_caps(enum display_actor_id id, unsigned int caps)
{
	struct display_actor_ops *actor;

	if (id <= 0 || !caps)
		return -EINVAL;

	actor = display_find_actor(id);
	if (!actor)
		return -EINVAL;

	actor->caps |= caps;

	return 0;
}

int display_reset_caps(enum display_actor_id id, unsigned int caps)
{
	struct display_actor_ops *actor;

	if (id <= 0 || !caps)
		return -EINVAL;

	actor = display_find_actor(id);
	if (!actor)
		return -EINVAL;

	actor->caps &= ~caps;

	return 0;
}

unsigned int display_get_caps(enum display_actor_id id)
{
	struct display_actor_ops *actor;

	if (id <= 0)
		return 0;

	actor = display_find_actor(id);
	if (!actor)
		return 0;

	return actor->caps;
}

int display_has_caps(unsigned int total_caps, unsigned int caps)
{
	if (!total_caps || !caps)
		return false;

	if ((total_caps & caps) == caps)
		return true;

	return false;
}

