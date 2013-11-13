/*
 *  libslp-pm
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: DongGi Jang <dg0402.jang@samsung.com>
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
 *
*/ 

#include <dd-display.h>
#include "pmapi.h"
#include "pm.h"

API int pm_change_state(unsigned int s_bits)
{
	return display_change_state(s_bits);
}

API int pm_lock_state(unsigned int s_bits, unsigned int flag,
		    unsigned int timeout)
{
	return display_lock_state(s_bits, flag, timeout);
}

API int pm_unlock_state(unsigned int s_bits, unsigned int flag)
{
	return display_unlock_state(s_bits, flag);
}

