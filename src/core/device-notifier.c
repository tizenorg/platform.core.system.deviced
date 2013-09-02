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


#include <Ecore.h>

#include "log.h"
#include "devices.h"
#include "device-notifier.h"
#include "list.h"
#include "common.h"

struct device_notifier {
	enum device_notifier_type status;
	int (*func)(void *data);
};

static Eina_List *device_notifier_list;

#define FIND_NOTIFIER(a, b, c, d, e, f) \
	EINA_LIST_FOREACH_SAFE(a, b, c, d) \
		if (e == d->e && f == (d->f))

int register_notifier(enum device_notifier_type status, int (*func)(void *data))
{
	Eina_List *n, *next;
	struct device_notifier *data, *notifier;

	_I("%d, %x", status, func);

	if (!func) {
		_E("invalid func address!");
		return -EINVAL;
	}

	FIND_NOTIFIER(device_notifier_list, n, next, notifier, status, func) {
		_E("function is already registered! [%d, %x]",
		    status, func);
		return -EINVAL;
	}

	notifier = malloc(sizeof(struct device_notifier));
	if (!notifier) {
		_E("Fail to malloc for notifier!");
		return -ENOMEM;
	}

	notifier->status = status;
	notifier->func = func;

	EINA_LIST_APPEND(device_notifier_list, notifier);

	return 0;
}

int unregister_notifier(enum device_notifier_type status, int (*func)(void *data))
{
	Eina_List *n, *next;
	struct device_notifier *notifier;

	if (!func) {
		_E("invalid func address!");
		return -EINVAL;
	}

	FIND_NOTIFIER(device_notifier_list, n, next, notifier, status, func) {
		_I("[%d, %x]", status, func);
		free(notifier);
		EINA_LIST_REMOVE_LIST(device_notifier_list, n);
	}

	return 0;
}

void device_notify(enum device_notifier_type status, void *data)
{
	Eina_List *n;
	struct device_notifier *notifier;
	int cnt = 0;

	EINA_LIST_FOREACH(device_notifier_list, n, notifier) {
		if (status == notifier->status) {
			if (notifier->func) {
				notifier->func(data);
				cnt++;
			}
		}
	}
}

static void device_notifier_exit(void *data)
{
	Eina_List *n, *next;
	struct device_notifier *notifier;

	EINA_LIST_FOREACH_SAFE(device_notifier_list, n, next, notifier)
		if (notifier) {
			free(notifier);
			EINA_LIST_REMOVE_LIST(device_notifier_list, n);
		}

	_I("all deleted!");
}

static const struct device_ops notifier_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name = "notifier",
	.exit = device_notifier_exit,
};

DEVICE_OPS_REGISTER(&notifier_device_ops)
