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


#include "core/log.h"
#include "core/data.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "core/list.h"

#define EDBUS_INIT_RETRY_COUNT 5
#define NAME_OWNER_CHANGED "NameOwnerChanged"
#define NAME_OWNER_MATCH "type='signal',sender='org.freedesktop.DBus',\
	path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',\
	member='NameOwnerChanged',arg0='%s'"


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
	{ DEVICED_PATH_STORAGE, DEVICED_INTERFACE_STORAGE, NULL, NULL },
	{ DEVICED_PATH_HAPTIC , DEVICED_INTERFACE_HAPTIC , NULL, NULL },
	{ DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI, NULL, NULL },
	{ DEVICED_PATH_LED    , DEVICED_INTERFACE_LED    , NULL, NULL },
	/* Add new object & interface here*/
};

static Eina_List *edbus_handler_list;
static Eina_List *edbus_watch_list;
static int edbus_init_val;
static DBusConnection *conn;
static E_DBus_Connection *edbus_conn;
static DBusPendingCall *edbus_request_name;

static int register_edbus_interface(struct edbus_object *object)
{
	int ret;

	if (!object) {
		_E("object is invalid value!");
		return -1;
	}

	object->obj = e_dbus_object_add(edbus_conn, object->path, NULL);
	if (!object->obj) {
		_E("fail to add edbus obj");
		return -1;
	}

	object->iface = e_dbus_interface_new(object->interface);
	if (!object->iface) {
		_E("fail to add edbus interface");
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
		_E("invalid argument!");
		return -1;
	}

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender!");
		return -1;
	}

	send_msg = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
				    DBUS_PATH_DBUS,
				    DBUS_INTERFACE_DBUS,
				    "GetConnectionUnixProcessID");
	if (!send_msg) {
		_E("invalid send msg!");
		return -1;
	}

	ret = dbus_message_append_args(send_msg, DBUS_TYPE_STRING,
				    &sender, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("fail to append args!");
		dbus_message_unref(send_msg);
		return -1;
	}

	pending = e_dbus_message_send(edbus_conn, send_msg, NULL, -1, NULL);
	if (!pending) {
		_E("pending is null!");
		dbus_message_unref(send_msg);
		return -1;
	}

	dbus_message_unref(send_msg);

	/* block until reply is received */
	dbus_pending_call_block(pending);

	msg = dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	if (!msg) {
		_E("reply msg is null!");
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
		_E("fail to add edbus handler");
		return -1;
	}

	_E("add edbus service: %s", name);

	entry = malloc(sizeof(struct edbus_list));

	if (!entry) {
		_E("Malloc failed");
		return -1;
	}

	entry->signal_name = strndup(name, strlen(name));

	if (!entry->signal_name) {
		_E("Malloc failed");
		free(entry);
		return -1;
	}

	entry->handler = handler;
	edbus_handler_list = eina_list_prepend(edbus_handler_list, entry);
	if (!edbus_handler_list) {
		_E("eina_list_prepend failed");
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
		_E("fail to allocate new %s.%s signal", interface, name);
		return -1;
	}

	dbus_message_append_args(signal, type, value, DBUS_TYPE_INVALID);

	e_dbus_message_send(edbus_conn, signal, NULL, -1, NULL);

	dbus_message_unref(signal);
	return 0;
}

static DBusHandlerResult message_filter(DBusConnection *connection,
		DBusMessage *message, void *data)
{
	char match[256];
	int ret;
	const char *iface, *member, *arg = NULL;
	char *watch;
	Eina_List *l;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	iface = dbus_message_get_interface(message);
	member = dbus_message_get_member(message);

	if (strcmp(iface, DBUS_INTERFACE_DBUS))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strcmp(member, NAME_OWNER_CHANGED))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	ret = dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &arg,
		    DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	_D("Argument : %s", arg);

	EINA_LIST_FOREACH(edbus_watch_list, l, watch) {
		if (strcmp(arg, watch)) continue;

		/* notify 'process terminated' to device notifiers */
		device_notify(DEVICE_NOTIFIER_PROCESS_TERMINATED, (void *)watch);

		/* remove registered sender */
		snprintf(match, sizeof(match), NAME_OWNER_MATCH, watch);
		dbus_bus_remove_match(conn, match, NULL);
		EINA_LIST_REMOVE(edbus_watch_list, watch);
		free(watch);
		break;
	}

	if (eina_list_count(edbus_watch_list) == 0) {
		dbus_connection_remove_filter(conn, message_filter, NULL);
		_I("remove message filter, no watcher!");
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

int register_edbus_watch(DBusMessage *msg)
{
	char match[256];
	const char *sender;
	char *watch;
	Eina_List *l;
	int ret;

	if (!msg) {
		_E("invalid argument!");
		return -EINVAL;
	}

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender!");
		return -EINVAL;
	}

	/* check the sender is already registered */
	EINA_LIST_FOREACH(edbus_watch_list, l, watch) {
		if (strcmp(sender, watch)) continue;

		_I("%s is already watched!", watch);
		return 0;
	}

	watch = strndup(sender, strlen(sender));
	if (!watch) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	/* Add message filter */
	if (eina_list_count(edbus_watch_list) == 0) {
		ret = dbus_connection_add_filter(conn, message_filter, NULL, NULL);
		if (!ret) {
			_E("fail to add message filter!");
			free(watch);
			return -ENOMEM;
		}
		_I("success to add message filter!");
	}

	/* Add sender to watch list */
	EINA_LIST_APPEND(edbus_watch_list, watch);

	snprintf(match, sizeof(match), NAME_OWNER_MATCH, watch);
	dbus_bus_add_match(conn, match, NULL);

	_I("%s is watched by dbus!", watch);

	return 0;
}

static void unregister_edbus_watch_all(void)
{
	char match[256];
	Eina_List *n, *next;
	struct edbus_list *watch;

	if (eina_list_count(edbus_watch_list) > 0)
		dbus_connection_remove_filter(conn, message_filter, NULL);

	EINA_LIST_FOREACH_SAFE(edbus_watch_list, n, next, watch) {
		snprintf(match, sizeof(match), NAME_OWNER_MATCH, watch);
		dbus_bus_remove_match(conn, match, NULL);
		EINA_LIST_REMOVE(edbus_watch_list, watch);
		free(watch);
	}
}

int register_edbus_method(const char *path, const struct edbus_method *edbus_methods, int size)
{
	E_DBus_Interface *iface;
	int ret;
	int i;

	iface = get_edbus_interface(path);

	if (!iface) {
		_E("fail to get edbus interface!");
		return -ENODEV;
	}

	for (i = 0; i < size; i++) {
		ret = e_dbus_interface_method_add(iface,
				edbus_methods[i].member,
				edbus_methods[i].signature,
				edbus_methods[i].reply_signature,
				edbus_methods[i].func);
		if (!ret) {
			_E("fail to add method %s!", edbus_methods[i].member);
			return -EINVAL;
		}
	}

	return 0;
}

void edbus_init(void *data)
{
	int retry = EDBUS_INIT_RETRY_COUNT;
	int i, r;

	while (--retry) {
		edbus_init_val = e_dbus_init();
		if (edbus_init_val)
			break;
		if (!retry) {
			_E("fail to init edbus");
			return;
		}
	}

	retry = EDBUS_INIT_RETRY_COUNT;
	while (--retry) {
		edbus_conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
		if (edbus_conn)
			break;
		if (!retry) {
			_E("fail to get edbus");
			goto err_dbus_shutdown;
		}
	}

	retry = EDBUS_INIT_RETRY_COUNT;
	while (--retry) {
		edbus_request_name = e_dbus_request_name(edbus_conn, BUS_NAME, 0, NULL, NULL);
		if (edbus_request_name)
			break;
		if (!retry) {
			_E("fail to request edbus name");
			goto err_dbus_close;
		}
	}

	for (i = 0; i < ARRAY_SIZE(edbus_objects); i++) {
		r = register_edbus_interface(&edbus_objects[i]);
		if (r < 0)
			_E("fail to add obj & interface for %s",
				    edbus_objects[i].interface);

		_I("add new obj for %s", edbus_objects[i].interface);
	}
	return;

err_dbus_close:
	e_dbus_connection_close(edbus_conn);
err_dbus_shutdown:
	e_dbus_shutdown();
	return;
}

void edbus_exit(void *data)
{
	unregister_edbus_signal_handle();
	unregister_edbus_watch_all();
	e_dbus_connection_close(edbus_conn);
	e_dbus_shutdown();
}
