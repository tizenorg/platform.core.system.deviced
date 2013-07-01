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


#ifndef __EDBUS_HANDLE_H__
#define __EDBUS_HANDLE_H__

#include <E_DBus.h>

#define BUS_NAME		"org.tizen.system.deviced"
#define OBJECT_PATH		"/Org/Tizen/System/DeviceD"
#define INTERFACE_NAME		BUS_NAME

/*
 * Core service
 *   get/set device status
 *   operations about device
 */
#define DEVICED_PATH_CORE		OBJECT_PATH"/Core"
#define DEVICED_INTERFACE_CORE		INTERFACE_NAME".core"

/*
 * Display service
 *   start/stop display(pm)
 *   get/set brightness
 *   operations about display
 */
#define DEVICED_PATH_DISPLAY		OBJECT_PATH"/Display"
#define DEVICED_INTERFACE_DISPLAY	INTERFACE_NAME".display"

int register_edbus_signal_handler(const char *path, const char *interface,
		const char *name, E_DBus_Signal_Cb cb);
E_DBus_Interface *get_edbus_interface(const char *path);
pid_t get_edbus_sender_pid(DBusMessage *msg);
int broadcast_edbus_signal(const char *path, const char *interface,
		const char *name, int type, void *value);

#endif /* __SS_EDBUS_HANDLE_H__ */
