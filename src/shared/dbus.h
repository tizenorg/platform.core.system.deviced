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


#ifndef __DBUS_H__
#define __DBUS_H__

#include <E_DBus.h>

#define BUS_NAME		"org.tizen.system.deviced"
#define OBJECT_PATH		"/Org/Tizen/System/DeviceD"
#define INTERFACE_NAME		BUS_NAME

#define DEVICED_PATH_DISPLAY		OBJECT_PATH"/Display"
#define DEVICED_INTERFACE_DISPLAY	INTERFACE_NAME".display"

#define DEVICED_PATH_HAPTIC		OBJECT_PATH"/Haptic"
#define DEVICED_INTERFACE_HAPTIC	INTERFACE_NAME".haptic"

struct dbus_byte {
	char *data;
	int size;
};

DBusMessage *deviced_dbus_method_sync(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[]);

#endif
