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


#include <stdbool.h>
#include <assert.h>
#include "core/log.h"
#include "core/edbus-handler.h"
#include "core/common.h"
#include "core/device-notifier.h"
#include "core/list.h"

#define EDBUS_INIT_RETRY_COUNT 5
#define NAME_OWNER_CHANGED "NameOwnerChanged"
#define NAME_OWNER_MATCH "type='signal',sender='org.freedesktop.DBus',\
	path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',\
	member='NameOwnerChanged',arg0='%s'"

/* -1 is a default timeout value, it's converted to 25*1000 internally. */
#define DBUS_REPLY_TIMEOUT	(-1)
#define RETRY_MAX 5

struct edbus_object {
	const char *path;
	const char *interface;
	E_DBus_Object *obj;
	E_DBus_Interface *iface;
};

struct edbus_list{
	char *signal_name;
	E_DBus_Signal_Handler *handler;
};

static struct edbus_object edbus_objects[] = {
	{ DEVICED_PATH_CORE   , DEVICED_INTERFACE_CORE   , NULL, NULL },
	{ DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY, NULL, NULL },
	{ DEVICED_PATH_POWER  , DEVICED_INTERFACE_POWER  , NULL, NULL },
	{ DEVICED_PATH_STORAGE, DEVICED_INTERFACE_STORAGE, NULL, NULL },
	{ DEVICED_PATH_HAPTIC , DEVICED_INTERFACE_HAPTIC , NULL, NULL },
	{ DEVICED_PATH_MMC    , DEVICED_INTERFACE_MMC    , NULL, NULL },
	{ DEVICED_PATH_PROCESS, DEVICED_INTERFACE_PROCESS, NULL, NULL },
	{ DEVICED_PATH_KEY    , DEVICED_INTERFACE_KEY    , NULL, NULL },
	{ DEVICED_PATH_CPU    , DEVICED_INTERFACE_CPU    , NULL, NULL },
	{ DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI, NULL, NULL },
	{ DEVICED_PATH_USB    , DEVICED_INTERFACE_USB    , NULL, NULL },
	{ DEVICED_PATH_USBHOST, DEVICED_INTERFACE_USBHOST, NULL, NULL },
	{ DEVICED_PATH_GPIO, DEVICED_INTERFACE_GPIO, NULL, NULL},
	{ DEVICED_PATH_HDMICEC, DEVICED_INTERFACE_HDMICEC, NULL, NULL},
	/* Add new object & interface here*/
};

static dd_list *edbus_object_list;
static dd_list *edbus_owner_list;
static dd_list *edbus_handler_list;
static dd_list *edbus_watch_list;
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
	dd_list *tmp;
	struct edbus_list *entry;

	DD_LIST_FOREACH(edbus_handler_list, tmp, entry) {
		e_dbus_signal_handler_del(edbus_conn, entry->handler);
		DD_LIST_REMOVE(edbus_handler_list, entry);
		free(entry->signal_name);
		free(entry);
	}
}

int register_edbus_signal_handler(const char *path, const char *interface,
		const char *name, E_DBus_Signal_Cb cb)
{
	dd_list *tmp;
	struct edbus_list *entry;
	E_DBus_Signal_Handler *handler;

	DD_LIST_FOREACH(edbus_handler_list, tmp, entry) {
		if (strncmp(entry->signal_name, name, strlen(name)) == 0)
			return -EEXIST;
	}

	handler = e_dbus_signal_handler_add(edbus_conn, NULL, path,
				interface, name, cb, NULL);

	if (!handler) {
		_E("fail to add edbus handler");
		return -ENOMEM;
	}

	entry = malloc(sizeof(struct edbus_list));

	if (!entry) {
		_E("Malloc failed");
		return -ENOMEM;
	}

	entry->signal_name = strndup(name, strlen(name));

	if (!entry->signal_name) {
		_E("Malloc failed");
		free(entry);
		return -ENOMEM;
	}

	entry->handler = handler;
	DD_LIST_PREPEND(edbus_handler_list, entry);
	if (!edbus_handler_list) {
		_E("eina_list_prepend failed");
		free(entry->signal_name);
		free(entry);
		return -ENOMEM;
	}
	return 0;
}

int broadcast_edbus_signal(const char *path, const char *interface,
		const char *name, const char *sig, char *param[])
{
	DBusMessage *msg;
	DBusMessageIter iter;
	int r;

	msg = dbus_message_new_signal(path, interface, name);
	if (!msg) {
		_E("fail to allocate new %s.%s signal", interface, name);
		return -EPERM;
	}

	dbus_message_iter_init_append(msg, &iter);
	r = append_variant(&iter, sig, param);
	if (r < 0) {
		_E("append_variant error(%d)", r);
		return -EPERM;
	}

	r = dbus_connection_send(conn, msg, NULL);
	dbus_message_unref(msg);

	if (r != TRUE) {
		_E("dbus_connection_send error(%s:%s-%s)",
		    path, interface, name);
		return -ECOMM;
	}

	return 0;
}

static DBusHandlerResult message_filter(DBusConnection *connection,
		DBusMessage *message, void *data)
{
	char match[256];
	int ret;
	const char *iface, *member, *arg = NULL;
	struct watch *watch;
	dd_list *n;

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

	DD_LIST_FOREACH(edbus_watch_list, n, watch) {
		if (strcmp(arg, watch->name)) continue;

		if (watch->func)
			watch->func(watch->name, watch->id);

		DD_LIST_REMOVE(edbus_watch_list, watch);
		free(watch->name);
		free(watch);
	}

	/* remove registered sender */
	snprintf(match, sizeof(match), NAME_OWNER_MATCH, arg);
	dbus_bus_remove_match(conn, match, NULL);


	if (DD_LIST_LENGTH(edbus_watch_list) == 0) {
		dbus_connection_remove_filter(conn, message_filter, NULL);
		_I("remove message filter, no watcher!");
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

int register_edbus_watch(DBusMessage *msg, enum watch_id id, int (*func)(char *name, enum watch_id id))
{
	char match[256];
	const char *sender;
	struct watch *watch;
	dd_list *n;
	int ret;
	bool matched = false;

	if (!msg) {
		_E("invalid argument!");
		return -EINVAL;
	}

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender!");
		return -EINVAL;
	}

	/* check the sender&id is already registered */
	DD_LIST_FOREACH(edbus_watch_list, n, watch) {
		if (strcmp(sender, watch->name))
			continue;
		if (id != watch->id) {
			matched = true;
			continue;
		}

		_I("%s(%d) is already watched!", watch->name, watch->id);

		return 0;
	}

        watch = malloc(sizeof(struct watch));
        if (!watch) {
                _E("Fail to malloc for watch!");
                return -ENOMEM;
        }

	watch->id = id;
	watch->func = func;
	watch->name = strndup(sender, strlen(sender));

	if (!watch->name) {
		_E("Fail to malloc for watch name");
		free(watch);
		return -ENOMEM;
	}

	/* Add message filter */
	if (DD_LIST_LENGTH(edbus_watch_list) == 0) {
		ret = dbus_connection_add_filter(conn, message_filter, NULL, NULL);
		if (!ret) {
			_E("fail to add message filter!");
			free(watch->name);
			free(watch);
			return -ENOMEM;
		}
		_I("success to add message filter!");
	}

	/* Add watch to watch list */
	DD_LIST_APPEND(edbus_watch_list, watch);

	if (!matched) {
		snprintf(match, sizeof(match), NAME_OWNER_MATCH, watch->name);
		dbus_bus_add_match(conn, match, NULL);
	}

	_I("%s(%d) is watched by dbus!", watch->name, watch->id);

	return 0;
}

int unregister_edbus_watch(DBusMessage *msg, enum watch_id id)
{
	char match[256];
	const char *sender;
	struct watch *watch;
	dd_list *n;
	bool matched = false;

	if (!msg) {
		_E("invalid argument!");
		return -EINVAL;
	}

	sender = dbus_message_get_sender(msg);
	if (!sender) {
		_E("invalid sender!");
		return -EINVAL;
	}

	DD_LIST_FOREACH(edbus_watch_list, n, watch) {
		if (strcmp(sender, watch->name))
			continue;

		if (id != watch->id) {
			matched = true;
			continue;
		}
		DD_LIST_REMOVE(edbus_watch_list, watch);
		free(watch->name);
		free(watch);
	}

	/* remove match */
	if (!matched) {
                snprintf(match, sizeof(match), NAME_OWNER_MATCH, sender);
                dbus_bus_remove_match(conn, match, NULL);

		if (DD_LIST_LENGTH(edbus_watch_list) == 0)
			dbus_connection_remove_filter(conn, message_filter,
			    NULL);
	}

	return 0;
}

static void unregister_edbus_watch_all(void)
{
	char match[256];
	dd_list *n;
	struct watch *watch;

	if (DD_LIST_LENGTH(edbus_watch_list) > 0)
		dbus_connection_remove_filter(conn, message_filter, NULL);

	DD_LIST_FOREACH(edbus_watch_list, n, watch) {
		snprintf(match, sizeof(match), NAME_OWNER_MATCH, watch->name);
		dbus_bus_remove_match(conn, match, NULL);
		DD_LIST_REMOVE(edbus_watch_list, watch);
		free(watch->name);
		free(watch);
	}
}

static int register_method(E_DBus_Interface *iface,
		const struct edbus_method *edbus_methods, int size)
{
	int ret;
	int i;

	assert(iface);
	assert(edbus_methods);

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

int register_edbus_interface_and_method(const char *path,
		const char *interface,
		const struct edbus_method *edbus_methods, int size)
{
	struct edbus_object *obj;
	dd_list *elem;
	int ret;

	if (!path || !interface || !edbus_methods || size < 1) {
		_E("invalid parameter");
		return -EINVAL;
	}

	/* find matched obj */
	DD_LIST_FOREACH(edbus_object_list, elem, obj) {
		if (strncmp(obj->path, path, strlen(obj->path)) == 0 &&
		    strncmp(obj->interface, interface, strlen(obj->interface)) == 0) {
			_I("found matched item : obj(%p)", obj);
			break;
		}
	}

	/* if there is no matched obj */
	if (!obj) {
		obj = malloc(sizeof(struct edbus_object));
		if (!obj) {
			_E("fail to allocate %s interface(%d)", path, ret);
			return -ENOMEM;
		}

		obj->path = strdup(path);
		obj->interface = strdup(interface);

		ret = register_edbus_interface(obj);
		if (ret < 0) {
			_E("fail to register %s interface(%d)", obj->path, ret);
			free(obj->path);
			free(obj->interface);
			free(obj);
			return ret;
		}

		DD_LIST_APPEND(edbus_object_list, obj);
	}

	ret = register_method(obj->iface, edbus_methods, size);
	if (ret < 0) {
		_E("fail to register %s method(%d)", obj->path, ret);
		return ret;
	}

	return 0;
}

int unregister_edbus_interface_all(void)
{
	struct edbus_object *obj;
	dd_list *elem, *n;

	DD_LIST_FOREACH_SAFE(edbus_object_list, elem, n, obj) {
		DD_LIST_REMOVE(edbus_object_list, obj);
		free(obj->path);
		free(obj->interface);
		free(obj);
	}

	return 0;
}

int register_edbus_method(const char *path, const struct edbus_method *edbus_methods, int size)
{
	E_DBus_Interface *iface;
	int ret;

	if (!path || !edbus_methods || size < 1) {
		_E("invalid parameter");
		return -EINVAL;
	}

	iface = get_edbus_interface(path);
	if (!iface) {
		_E("fail to get edbus interface!");
		return -ENODEV;
	}

	ret = register_method(iface, edbus_methods, size);
	if (ret < 0) {
		_E("fail to register %s method(%d)", path, ret);
		return ret;
	}

	return 0;
}

static void request_name_cb(void *data, DBusMessage *msg, DBusError *error)
{
	DBusError err;
	unsigned int val;
	int r;

	if (!msg) {
		_D("invalid DBusMessage!");
		return;
	}

	dbus_error_init(&err);
	r = dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &val, DBUS_TYPE_INVALID);
	if (!r) {
		_E("no message : [%s:%s]", err.name, err.message);
		dbus_error_free(&err);
		return;
	}

	_I("Request Name reply : %d", val);
}

static void check_owner_name(void)
{
	DBusError err;
	DBusMessage *msg;
	DBusMessageIter iter;
	char *pa[1];
	char exe_name[PATH_MAX];
	char *entry;
	dd_list *n;
	int pid;

	DD_LIST_FOREACH(edbus_owner_list, n, entry) {
		pa[0] = entry;
		msg = dbus_method_sync_with_reply(E_DBUS_FDO_BUS,
			E_DBUS_FDO_PATH,
			E_DBUS_FDO_INTERFACE,
			"GetConnectionUnixProcessID", "s", pa);

		if (!msg) {
			_E("invalid DBusMessage!");
			return;
		}

		dbus_error_init(&err);
		dbus_message_iter_init(msg, &iter);

		dbus_message_iter_get_basic(&iter, &pid);
		if (get_cmdline_name(pid, exe_name, PATH_MAX) != 0)
			goto out;
		_I("%s(%d)", exe_name, pid);

out:
		dbus_message_unref(msg);
		dbus_error_free(&err);
	}
}

static void check_owner_list(void)
{
	DBusError err;
	DBusMessage *msg;
	DBusMessageIter array, iter, item, iter_val;
	char *pa[1];
	char *name;
	char *entry;

	pa[0] = DEVICED_BUS_NAME;
	msg = dbus_method_sync_with_reply(E_DBUS_FDO_BUS,
			E_DBUS_FDO_PATH,
			E_DBUS_FDO_INTERFACE,
			"ListQueuedOwners", "s", pa);

	if (!msg) {
		_E("invalid DBusMessage!");
		return;
	}

	dbus_error_init(&err);
	dbus_message_iter_init(msg, &array);

	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_ARRAY)
		goto out;
	dbus_message_iter_recurse(&array, &item);
	while (dbus_message_iter_get_arg_type(&item) == DBUS_TYPE_STRING) {
		dbus_message_iter_get_basic(&item, &name);
		entry = strndup(name, strlen(name));
		DD_LIST_APPEND(edbus_owner_list, entry);
		if (!edbus_owner_list) {
			_E("append failed");
			free(entry);
			goto out;
		}
		dbus_message_iter_next(&item);
	}

out:
	dbus_message_unref(msg);
	dbus_error_free(&err);
}

void edbus_init(void *data)
{
	DBusError error;
	int retry = 0;
	int i, ret;

	dbus_threads_init_default();
	dbus_error_init(&error);

	do {
		edbus_init_val = e_dbus_init();
		if (edbus_init_val)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to init edbus");
			return;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);

	retry = 0;
	do {
		conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
		if (conn)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to get dbus");
			goto out1;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);

	retry = 0;
	do {
		edbus_conn = e_dbus_connection_setup(conn);
		if (edbus_conn)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to get edbus");
			goto out2;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);

	retry = 0;
	do {
		edbus_request_name = e_dbus_request_name(edbus_conn, DEVICED_BUS_NAME,
				DBUS_NAME_FLAG_REPLACE_EXISTING, request_name_cb, NULL);
		if (edbus_request_name)
			break;
		if (retry == EDBUS_INIT_RETRY_COUNT) {
			_E("fail to request edbus name");
			goto out3;
		}
		retry++;
	} while (retry <= EDBUS_INIT_RETRY_COUNT);

	for (i = 0; i < ARRAY_SIZE(edbus_objects); i++) {
		ret = register_edbus_interface(&edbus_objects[i]);
		if (ret < 0) {
			_E("fail to add obj & interface for %s",
				    edbus_objects[i].interface);
			return;
		}
		_D("add new obj for %s", edbus_objects[i].interface);
	}
	check_owner_list();
	check_owner_name();
	return;

out3:
	e_dbus_connection_close(edbus_conn);
out2:
	dbus_connection_set_exit_on_disconnect(conn, FALSE);
out1:
	e_dbus_shutdown();
}

void edbus_exit(void *data)
{
	unregister_edbus_signal_handle();
	unregister_edbus_watch_all();
	unregister_edbus_interface_all();
	e_dbus_connection_close(edbus_conn);
	e_dbus_shutdown();
}
