/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "common.h"
#include "log.h"
#include "dbus.h"

/* -1 is a default timeout value, it's converted to 25*1000 internally. */
#define DBUS_REPLY_TIMEOUT	(-1)

struct pending_call_data {
	dbus_pending_cb func;
	void *data;
};

int append_variant(DBusMessageIter *iter, const char *sig, char *param[])
{
	char *ch;
	int i;
	int int_type;
	dbus_bool_t bool_type;
	uint64_t int64_type;
	DBusMessageIter arr;
	struct dbus_byte *byte;

	if (!sig || !param)
		return 0;

	for (ch = (char*)sig, i = 0; *ch != '\0'; ++i, ++ch) {
		switch (*ch) {
		case 'b':
			bool_type = (atoi(param[i])) ? TRUE:FALSE;
			dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &bool_type);
			break;
		case 'i':
			int_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &int_type);
			break;
		case 'u':
			int_type = strtoul(param[i], NULL, 10);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &int_type);
			break;
		case 't':
			int64_type = atoll(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &int64_type);
			break;
		case 's':
			dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &param[i]);
			break;
		case 'a':
			++i, ++ch;
			switch (*ch) {
			case 'y':
				dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arr);
				byte = (struct dbus_byte*)param[i];
				dbus_message_iter_append_fixed_array(&arr, DBUS_TYPE_BYTE, &(byte->data), byte->size);
				dbus_message_iter_close_container(iter, &arr);
				break;
			default:
				break;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

DBusMessage *dbus_method_sync_with_reply(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	int r;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		_E("dbus_bus_get error");
		return NULL;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)",
			path, interface, method);
		return NULL;
	}

	dbus_message_iter_init_append(msg, &iter);
	r = append_variant(&iter, sig, param);
	if (r < 0) {
		_E("append_variant error(%d) %s %s:%s-%s",
			r, dest, path, interface, method);
		dbus_message_unref(msg);
		return NULL;
	}

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	if (!reply) {
		_E("dbus_connection_send error(No reply) %s %s:%s-%s",
			dest, path, interface, method);
	}

	if (dbus_error_is_set(&err)) {
		_E("dbus_connection_send error(%s:%s) %s %s:%s-%s",
			err.name, err.message, dest, path, interface, method);
		dbus_error_free(&err);
		reply = NULL;
	}

	dbus_message_unref(msg);
	return reply;
}

int dbus_method_sync(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	int ret, result;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		_E("dbus_bus_get error");
		return -EPERM;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)",
			path, interface, method);
		return -EBADMSG;
	}

	dbus_message_iter_init_append(msg, &iter);
	ret = append_variant(&iter, sig, param);
	if (ret < 0) {
		_E("append_variant error(%d) %s %s:%s-%s",
			ret, dest, path, interface, method);
		dbus_message_unref(msg);
		return ret;
	}

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	dbus_message_unref(msg);
	if (!reply) {
		_E("dbus_connection_send error(%s:%s) %s %s:%s-%s",
			err.name, err.message, dest, path, interface, method);
		dbus_error_free(&err);
		return -ECOMM;
	}

	ret = dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
	dbus_message_unref(reply);
	if (!ret) {
		_E("no message : [%s:%s] %s %s:%s-%s",
			err.name, err.message, dest, path, interface, method);
		dbus_error_free(&err);
		return -ENOMSG;
	}

	return result;
}

int dbus_method_async(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	int ret;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		_E("dbus_bus_get error");
		return -EPERM;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)",
			path, interface, method);
		return -EBADMSG;
	}

	dbus_message_iter_init_append(msg, &iter);
	ret = append_variant(&iter, sig, param);
	if (ret < 0) {
		_E("append_variant error(%d) %s %s:%s-%s",
			ret, dest, path, interface, method);
		dbus_message_unref(msg);
		return ret;
	}

	ret = dbus_connection_send(conn, msg, NULL);
	dbus_message_unref(msg);
	if (ret != TRUE) {
		_E("dbus_connection_send error(%s %s:%s-%s)",
			dest, path, interface, method);
		return -ECOMM;
	}

	return 0;
}

static void cb_pending(DBusPendingCall *pending, void *user_data)
{
	DBusMessage *msg;
	DBusError err;
	struct pending_call_data *data = user_data;
	int ret;

	ret = dbus_pending_call_get_completed(pending);
	if (!ret) {
		_I("dbus_pending_call_get_completed() fail");
		dbus_pending_call_unref(pending);
		return;
	}

	dbus_error_init(&err);
	msg = dbus_pending_call_steal_reply(pending);
	if (!msg) {
		_E("no message : [%s:%s]", err.name, err.message);

		if (data->func) {
			dbus_set_error(&err, "org.tizen.system.deviced.NoReply",
					"There was no reply to this method call");
			data->func(data->data, NULL, &err);
			dbus_error_free(&err);
		}
		return;
	}

	ret = dbus_set_error_from_message(&err, msg);
	if (ret) {
		_E("error msg : [%s:%s]", err.name, err.message);

		if (data->func)
			data->func(data->data, NULL, &err);
		dbus_error_free(&err);
	} else {
		if (data->func)
			data->func(data->data, msg, &err);
	}

	dbus_message_unref(msg);
	dbus_pending_call_unref(pending);
}

int dbus_method_async_with_reply(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[], dbus_pending_cb cb, int timeout, void *data)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusPendingCall *pending = NULL;
	struct pending_call_data *pdata;
	int ret;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		_E("dbus_bus_get error");
		return -EPERM;
	}

	/* this function should be invoked to receive dbus messages
	 * does nothing if it's already been done */
	dbus_connection_setup_with_g_main(conn, NULL);

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)",
			path, interface, method);
		return -EBADMSG;
	}

	dbus_message_iter_init_append(msg, &iter);
	ret = append_variant(&iter, sig, param);
	if (ret < 0) {
		_E("append_variant error(%d)%s %s:%s-%s",
			ret, dest, path, interface, method);
		dbus_message_unref(msg);
		return ret;
	}

	ret = dbus_connection_send_with_reply(conn, msg, &pending, timeout);
	if (!ret) {
		dbus_message_unref(msg);
		_E("dbus_connection_send error(%s %s:%s-%s)",
			dest, path, interface, method);
		return -ECOMM;
	}

	dbus_message_unref(msg);

	if (cb && pending) {
		pdata = malloc(sizeof(struct pending_call_data));
		if (!pdata)
			return -ENOMEM;

		pdata->func = cb;
		pdata->data = data;

		ret = dbus_pending_call_set_notify(pending, cb_pending, pdata, free);
		if (!ret) {
			free(pdata);
			dbus_pending_call_cancel(pending);
			return -ECOMM;
		}
	}

	return 0;
}

static void __CONSTRUCTOR__ dbus_init(void)
{
	dbus_threads_init_default();
}
