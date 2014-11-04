/*
 * devman
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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


#ifndef __DEVLOG_H__
#define __DEVLOG_H__

#define FEATURE_DEVMAN_DLOG

#ifdef FEATURE_DEVMAN_DLOG
    #define LOG_TAG "DEVMAN"
    #include <dlog.h>
    #define DEVLOG(fmt, args...)		SLOGD(fmt, ##args)
    #define DEVERR(fmt, args...)		SLOGE(fmt, ##args)
#else
    #define DEVLOG(x, ...)
    #define DEVERR(x, ...)
#endif

#endif // __DEVLOG_H__
