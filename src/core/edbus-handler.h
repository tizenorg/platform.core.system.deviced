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
#include "shared/dbus.h"

struct edbus_object {
	const char *path;
	const char *interface;
	E_DBus_Object *obj;
	E_DBus_Interface *iface;
};

struct edbus_method {
	const char *member;
	const char *signature;
	const char *reply_signature;
	E_DBus_Method_Cb func;
};

enum watch_id {
	WATCH_DISPLAY_AUTOBRIGHTNESS_MIN,
	WATCH_DISPLAY_LCD_TIMEOUT,
	WATCH_DISPLAY_LOCK_STATE,
	WATCH_DISPLAY_HOLD_BRIGHTNESS,
};

struct watch {
	enum watch_id id;
	char *name;
	int (*func)(char *name, enum watch_id id);
};

int register_edbus_interface_with_method(struct edbus_object *object,
		const struct edbus_method *edbus_methods, int size);
int register_edbus_method(const char *path, const struct edbus_method *edbus_methods, int size);
int register_edbus_signal_handler(const char *path, const char *interface,
		const char *name, E_DBus_Signal_Cb cb);
E_DBus_Interface *get_edbus_interface(const char *path);
pid_t get_edbus_sender_pid(DBusMessage *msg);
int broadcast_edbus_signal(const char *path, const char *interface,
		const char *name, const char *sig, char *param[]);
int register_edbus_watch(DBusMessage *msg, enum watch_id id, int (*func)(char *name, enum watch_id id));
int unregister_edbus_watch(DBusMessage *msg, enum watch_id id);

void edbus_init(void *data);
void edbus_exit(void *data);

#endif /* __EDBUS_HANDLE_H__ */
