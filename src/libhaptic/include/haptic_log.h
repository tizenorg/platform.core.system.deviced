/*
 *  haptic
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jiyoung Yun <jy910.yun@samsung.com>
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

#ifndef __HAPTIC_LOG_H__
#define __HAPTIC_LOG_H__

#define FEATURE_HAPTIC_DLOG

#ifdef FEATURE_HAPTIC_DLOG
    #define LOG_TAG "HAPTIC"
    #include <dlog.h>
    #define HAPTIC_LOG(fmt, args...)       SLOGD(fmt, ##args)
    #define HAPTIC_ERROR(fmt, args...)     SLOGE(fmt, ##args)
#else
    #define HAPTIC_LOG(x, ...)
    #define HAPTIC_ERROR(x, ...)
#endif

#endif //__HAPTIC_LOG_H__
