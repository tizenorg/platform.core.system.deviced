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


#include "core/log.h"
#include "core/data.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "core/devices.h"

#define EDBUS_INIT_RETRY_COUNT 5

struct edbus_list{
	char *signal_name;
	E_DBus_Signal_Handler *handler;
};

static struct edbus_object {
	const char *path;
	const char *interface;
	E_DBus_Object *obj;
	E_DBus_Interface *iface;
} edbus_objects[] = {
	{ DEVICED_PATH_CORE   , DEVICED_INTERFACE_CORE   , NULL, NULL },
	{ DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY, NULL, NULL },
	/* Add new object & interface here*/
};

static Eina_List *edbus_handler_list;
static int edbus_init_val;
static E_DBus_Connection *edbus_conn;
static DBusPendingCall *edbus_request_name;

static int register_edbus_interface(struct edbus_object *object)
{
	int ret;

	if (!object) {
		PRT_TRACE_ERR("object is invalid value!");
		return -1;
	}

	object->obj = e_dbus_object_add(edbus_conn, object->path, NULL);
	if (!object->obj) {
		PRT_TRACE_ERR("fail to add edbus obj");
		return -1;
	}

	object->iface = e_dbus_interface_new(object->interface);
	if (!object->iface) {
		PRT_TRACE_ERR("fail to add edbus interface");
		return -1;
	}

	e_dbus_object_interface_attach(object->obj, object->iface);

	return 0;
}

E_DBus_Interface *get_edbus_interface(const char *path)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(edbus_objects); i++)
		if (!strcmp(path, edbus_objects[i].path))
			return edbus_objects[i].iface;

	return NULL;
}

pid_t get_edbus_sender_pid(DBusMessage *msg)
{
	const char *sender;
	DBusMessage *send_msg;
	DBusPendingCall *pending;
	DBusMessageIter iter;
	int ret;
	pid_t pid;

	if (!msg) {
		PRT_TRACE_ERR("invalid argument!");
		return -1;
	}

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		PRT_TRACE_ERR("invalid sender!");
		return -1;
	}

	send_msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				    DBUS_PATH_DBUS,
				    DBUS_INTERFACE_DBUS,
				    "GetConnectionUnixProcessID");
	if (!send_msg) {
		PRT_TRACE_ERR("invalid send msg!");
		return -1;
	}

	ret = dbus_message_append_args(send_msg, DBUS_TYPE_STRING,
				    &sender, DBUS_TYPE_INVALID);
	if (!ret) {
		PRT_TRACE_ERR("fail to append args!");
		dbus_message_unref(send_msg);
		return -1;
	}

	pending = e_dbus_message_send(edbus_conn, send_msg, NULL, -1, NULL);
	if (!pending) {
		PRT_TRACE_ERR("pending is null!");
		dbus_message_unref(send_msg);
		return -1;
	}

	dbus_message_unref(send_msg);

	/* block until reply is received */
	dbus_pending_call_block(pending);

	msg = dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	if (!msg) {
		PRT_TRACE_ERR("reply msg is null!");
		return -1;
	}

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &pid);
	dbus_message_unref(msg);

	return pid;
}

static void unregister_edbus_signal_handle(void)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct edbus_list *entry;

	EINA_LIST_FOREACH_SAFE(edbus_handler_list, tmp, tmp_next, entry) {
		if (entry != NULL) {
			e_dbus_signal_handler_del(edbus_conn, entry->handler);
			edbus_handler_list = eina_list_remove(edbus_handler_list, entry);
			free(entry->signal_name);
			free(entry);
		}
	}
}

int register_edbus_signal_handler(const char *path, const char *interface,
		const char *name, E_DBus_Signal_Cb cb)
{
	Eina_List *tmp;
	struct edbus_list *entry;
	E_DBus_Signal_Handler *handler;

	EINA_LIST_FOREACH(edbus_handler_list, tmp, entry) {
		if (entry != NULL && strncmp(entry->signal_name, name, strlen(name)) == 0)
			return -1;
	}

	handler = e_dbus_signal_handler_add(edbus_conn, NULL, path,
				interface, name, cb, NULL);

	if (!handler) {
		PRT_TRACE_ERR("fail to add edbus handler");
		return -1;
	}

	PRT_TRACE_ERR("add edbus service: %s", name);

	entry = malloc(sizeof(struct edbus_list));

	if (!entry) {
		PRT_TRACE_ERR("Malloc failed");
		return -1;
	}

	entry->signal_name = strndup(name, strlen(name));

	if (!entry->signal_name) {
		PRT_TRACE_ERR("Malloc failed");
		free(entry);
		return -1;
	}

	entry->handler = handler;
	edbus_handler_list = eina_list_prepend(edbus_handler_list, entry);
	if (!edbus_handler_list) {
		PRT_TRACE_ERR("eina_list_prepend failed");
		free(entry->signal_name);
		free(entry);
		return -1;
	}
	return 0;
}

int broadcast_edbus_signal(const char *path, const char *interface,
		const char *name, int type, void *value)
{
	DBusMessage *signal;
	DBusMessageIter iter;
	DBusMessageIter val;
	char sig[2] = {type, '\0'};

	signal = dbus_message_new_signal(path, interface, name);
	if (!signal) {
		PRT_TRACE_ERR("fail to allocate new %s.%s signal", interface, name);
		return -1;
	}

	dbus_message_append_args(signal, type, value, DBUS_TYPE_INVALID);

	e_dbus_message_send(edbus_conn, signal, NULL, -1, NULL);

	dbus_message_unref(signal);
	return 0;
}

static void edbus_init(void *data)
{
	int retry = EDBUS_INIT_RETRY_COUNT;
	int i, r;

	while (--retry) {
		edbus_init_val = e_dbus_init();
		if (edbus_init_val)
			break;
		if (!retry) {
			PRT_TRACE_ERR("fail to init edbus");
			return;
		}
	}

	retry = EDBUS_INIT_RETRY_COUNT;
	while (--retry) {
		edbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
		if (edbus_conn)
			break;
		if (!retry) {
			PRT_TRACE_ERR("fail to get edbus");
			goto err_dbus_shutdown;
		}
	}

	retry = EDBUS_INIT_RETRY_COUNT;
	while (--retry) {
		edbus_request_name = e_dbus_request_name(edbus_conn, BUS_NAME, 0, NULL, NULL);
		if (edbus_request_name)
			break;
		if (!retry) {
			PRT_TRACE_ERR("fail to request edbus name");
			goto err_dbus_close;
		}
	}

	for (i = 0; i < ARRAY_SIZE(edbus_objects); i++) {
		r = register_edbus_interface(&edbus_objects[i]);
		if (r < 0)
			PRT_TRACE_ERR("fail to add obj & interface for %s",
				    edbus_objects[i].interface);

		PRT_TRACE("add new obj for %s", edbus_objects[i].interface);
	}

	PRT_TRACE("start edbus service");
	return;

err_dbus_close:
	e_dbus_connection_close(edbus_conn);
err_dbus_shutdown:
	e_dbus_shutdown();
	return;
}

static void edbus_exit(void *data)
{
	unregister_edbus_signal_handle();
	e_dbus_connection_close(edbus_conn);
	e_dbus_shutdown();
}

const struct device_ops edbus_device_ops = {
	.init = edbus_init,
	.exit = edbus_exit,
};
