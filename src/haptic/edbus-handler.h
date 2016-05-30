/*
 * deviced-vibrator
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


#ifndef __EDBUS_HANDLE_H__
#define __EDBUS_HANDLE_H__

#include <E_DBus.h>
#include "shared/dbus.h"

struct edbus_method {
	const char *member;
	const char *signature;
	const char *reply_signature;
	E_DBus_Method_Cb func;
};
int register_edbus_interface_and_method(const char *path,
		const char *interface,
		const struct edbus_method *edbus_methods, int size);
int register_edbus_method(const char *path, const struct edbus_method *edbus_methods, int size);
E_DBus_Interface *get_edbus_interface(const char *path);
int register_edbus_watch(const char *sender,
		void (*func)(const char *sender, void *data), void *data);
int unregister_edbus_watch(const char *sender,
		void (*func)(const char *sender, void *data));

void edbus_init(void *data);
void edbus_exit(void *data);

#endif /* __EDBUS_HANDLE_H__ */
