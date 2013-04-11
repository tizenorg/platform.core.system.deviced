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

#define EINA_LIST_FREE(a) \
	a = eina_list_free(a)

#endif
