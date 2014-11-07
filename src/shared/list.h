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


#ifndef __LIST_H__
#define __LIST_H__

#include <glib.h>

#define LIST_ADD(head, data)                            \
	do {                                                \
		head = g_list_append(head, data);               \
	} while(0)

#define LIST_DEL(head, data)                            \
	do {                                                \
		head = g_list_remove(head, data);               \
	} while(0)

#define LIST_FIND(head, node, t, name, data)            \
	do {                                                \
		t *tmp;                                         \
		GList *elem;                                    \
		for (elem = head; elem; elem = elem->next) {    \
			tmp = elem->data;                           \
			if (tmp->##name != data)                    \
				continue;                               \
			node = tmp;                                 \
		}                                               \
	} while(0)

#define LIST_FOREACH(head, item)                        \
		for (item = head; item; item = item->next)

#endif
