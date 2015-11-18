/*
 * deviced
 *
 * Copyright (c) 2012 - 2015 Samsung Electronics Co., Ltd.
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

#ifndef __APPS_H__
#define __APPS_H__


#include "core/edbus-handler.h"
#include "core/common.h"

#define APP_POWEROFF "poweroff"
#define APP_DEFAULT  "system"
#define APP_KEY_TYPE "_SYSPOPUP_CONTENT_"

int launch_system_app(char *type, int num, ...);
int launch_message_post(char *type);

#endif /* __APPS_H__ */

