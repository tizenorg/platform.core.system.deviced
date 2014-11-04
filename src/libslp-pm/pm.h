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


#ifndef __POWER_MANAGER_LIBRARY_I_H__
#define __POWER_MANAGER_LIBRARY_I_H__

#ifndef API
#define API __attribute__ ((visibility("default")))
#endif

#if defined(ENABLE_DLOG_OUT)
#  define LOG_TAG	"PMAPI"
#  include <dlog.h>
#  define ERR	SLOGE
#else
#  define ERR	perror
#endif

#endif				/* __POWER_MANAGER_LIBRARY_I_H__ */
