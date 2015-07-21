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
#include "log.h"

#ifdef DEBUG
void __cyg_profile_func_enter(void *, void *)
	__attribute__ ((no_instrument_function));
void __cyg_profile_func_exit(void *, void *)
	__attribute__ ((no_instrument_function));

int g_trace_depth = -2;

void __cyg_profile_func_enter(void *func, void *caller)
{
	g_trace_depth++;
}

void __cyg_profile_func_exit(void *func, void *caller)
{
	g_trace_depth--;
}
#endif
