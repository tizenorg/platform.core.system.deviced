/*
 * deviced
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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


/**
 * @file	util.h
 * @brief	Utilities header for Power manager
 */
#ifndef __DEF_UTIL_H__
#define __DEF_UTIL_H__

/**
 * @addtogroup POWER_MANAGER
 * @{
 */
#ifdef ENABLE_DEVICED_DLOG
#define ENABLE_DLOG
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "POWER_MANAGER"
#include "shared/log-macro.h"

#define SEC_TO_MSEC(x)	((x)*1000)
#define MSEC_TO_SEC(x)	((x)/1000)

/**
 * @}
 */
#endif
