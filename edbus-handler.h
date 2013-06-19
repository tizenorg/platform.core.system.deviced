/*
 *  deviced
 *
 * Copyright (c) 2010 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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


#ifndef __EDBUS_HANDLE_H__
#define __EDBUS_HANDLE_H__

#include <E_DBus.h>

#define BUS_NAME		"org.tizen.system.deviced"
#define OBJECT_PATH		"/Org/Tizen/System/DeviceD"
#define INTERFACE_NAME		BUS_NAME

void edbus_init(void);
void edbus_fini(void);
int register_edbus_signal_handler(char *signal_name, E_DBus_Signal_Cb cb);

#endif /* __SS_EDBUS_HANDLE_H__ */
