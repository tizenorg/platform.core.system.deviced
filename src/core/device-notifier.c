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


#include "log.h"
#include "device-notifier.h"
#include "list.h"
#include "common.h"

struct device_notifier {
	bool deleted;
	enum device_notifier_type status;
	int (*func)(void *data);
};

static dd_list *device_notifier_list;
static Ecore_Idler *idl;

#define FIND_NOTIFIER(a, b, d, e, f) \
	DD_LIST_FOREACH(a, b, d) \
		if (e == d->e && f == (d->f))

int register_notifier(enum device_notifier_type status, int (*func)(void *data))
{
	dd_list *n;
	struct device_notifier *data, *notifier;

	_I("%d, %x", status, func);

	if (!func) {
		_E("invalid func address!");
		return -EINVAL;
	}

	FIND_NOTIFIER(device_notifier_list, n, notifier, status, func) {
		_E("function is already registered! [%d, %x]",
		    status, func);
		return -EINVAL;
	}

	notifier = calloc(1, sizeof(struct device_notifier));
	if (!notifier) {
		_E("Fail to malloc for notifier!");
		return -ENOMEM;
	}

	notifier->status = status;
	notifier->func = func;

	DD_LIST_APPEND(device_notifier_list, notifier);

	return 0;
}

int unregister_notifier(enum device_notifier_type status, int (*func)(void *data))
{
	dd_list *n;
	struct device_notifier *notifier;

	if (!func) {
		_E("invalid func address!");
		return -EINVAL;
	}

	FIND_NOTIFIER(device_notifier_list, n, notifier, status, func) {
		_I("[%d, %x]", status, func);
		notifier->deleted = true;
	}

	return 0;
}

static Eina_Bool delete_unused_notifier_cb(void *data)
{
	dd_list *n;
	dd_list *next;
	struct device_notifier *notifier;

	DD_LIST_FOREACH_SAFE(device_notifier_list, n, next, notifier) {
		if (notifier->deleted) {
			DD_LIST_REMOVE_LIST(device_notifier_list, n);
			free(notifier);
		}
	}

	idl = NULL;
	return ECORE_CALLBACK_CANCEL;
}

void device_notify(enum device_notifier_type status, void *data)
{
	dd_list *n;
	struct device_notifier *notifier;

	DD_LIST_FOREACH(device_notifier_list, n, notifier) {
		if (!notifier->deleted && status == notifier->status) {
			if (notifier->func)
				notifier->func(data);
		}
	}

	if (!idl)
		idl = ecore_idler_add(delete_unused_notifier_cb, NULL);
}
