/*
 * deviced
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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


#ifndef __DISPLAY_FLAGS_H__
#define __DISPLAY_FLAGS_H__

#include <errno.h>
#include "core/common.h"

unsigned int pm_status_flag;


#define FLAG_SET(a, b) \
	do { (a) |= (b); } while (0)
#define FLAG_UNSET(a, b) \
	do { (a) &= ~(b); } while (0)
#define IS_FLAG_SET(a, b) \
	((a) & (b) ? true : false)
#define FLAG_CLEAR(a) \
	do { (a) = 0; } while (0)


#define LOWBT_FLAG      0x00000100 /* low battery */
#define CHRGR_FLAG      0x00000200 /* chanrger */
#define BRTCH_FLAG      0x00000400 /* brightness changed */
#define PWROFF_FLAG     0x00000800 /* power off */
#define DIMSTAY_FLAG    0x00001000 /* dim stay */

#define PM_STATUS_SET(a) \
	FLAG_SET(pm_status_flag, (a))
#define PM_STATUS_UNSET(a) \
	FLAG_UNSET(pm_status_flag, (a))
#define IS_PM_STATUS_SET(a) \
	IS_FLAG_SET(pm_status_flag, (a))


#endif /* __DISPLAY_FLAGS_H__ */

