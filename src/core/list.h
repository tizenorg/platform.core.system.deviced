/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __LIST_H__
#define __LIST_H__

#include <Ecore.h>

#define EINA_LIST_APPEND(a, b) \
	a = eina_list_append(a, b)

#define EINA_LIST_REMOVE(a, b) \
	a = eina_list_remove(a, b)

#define EINA_LIST_REMOVE_LIST(a, b) \
	a = eina_list_remove_list(a, b)

#define EINA_LIST_FREE_LIST(a) \
	a = eina_list_free(a)

#ifdef EINA_LIST
typedef Eina_List dd_list;
#define DD_LIST_PREPEND(a, b)   \
	        a = eina_list_prepend(a, b)
#define DD_LIST_APPEND(a, b)    \
	        a = eina_list_append(a, b)
#define DD_LIST_REMOVE(a, b)    \
	        a = eina_list_remove(a, b)
#define DD_LIST_LENGTH(a)               \
	        eina_list_count(a)
#define DD_LIST_NTH(a, b)                       \
	        eina_list_nth(a, b)
#define DD_LIST_FREE_LIST(a)    \
	        a = eina_list_free(a)
#define DD_LIST_FOREACH(head, elem, node)       \
	        EINA_LIST_FOREACH(head, elem, node)
#define DD_LIST_FOREACH_SAFE(head, elem, elem_next, node) \
	        EINA_LIST_FOREACH_SAFE(head, elem, elem_next, node)

#else
#include <glib.h>
typedef GList dd_list;
#define DD_LIST_PREPEND(a, b)           \
	        a = g_list_prepend(a, (gpointer)b)
#define DD_LIST_APPEND(a, b)            \
	        a = g_list_append(a, (gpointer)b)
#define DD_LIST_REMOVE(a, b)            \
	        a = g_list_remove(a, (gpointer)b)
#define DD_LIST_LENGTH(a)                       \
	        g_list_length(a)
#define DD_LIST_NTH(a, b)                       \
	        g_list_nth_data(a, b)
#define DD_LIST_FREE_LIST(a)        \
	        g_list_free(a)
#define DD_LIST_FOREACH(head, elem, node)       \
	        for (elem = head, node = NULL; elem && ((node = elem->data) != NULL); elem = elem->next, node = NULL)
#define DD_LIST_FOREACH_SAFE(head, elem, elem_next, node) \
	        for (elem = head, elem_next = g_list_next(elem), node = NULL; \
				                        elem && ((node = elem->data) != NULL); \
				                        elem = elem_next, elem_next = g_list_next(elem), node = NULL)

#endif

#endif
