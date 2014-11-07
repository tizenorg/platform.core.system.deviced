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
#include "devices.h"
#include "device-notifier.h"
#include "list.h"
#include "common.h"

#define LATE_INIT_WAIT_TIME	12
#define DEFAULT_LATE_INIT_VALUE	((Ecore_Timer *)0x0DEF0DEF)

struct device_notifier {
	enum device_notifier_type status;
	int (*func)(void *data);
};

static dd_list *device_notifier_list;
static Ecore_Timer *late_init_timer = DEFAULT_LATE_INIT_VALUE;

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

	notifier = malloc(sizeof(struct device_notifier));
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
		DD_LIST_REMOVE(device_notifier_list, notifier);
		free(notifier);
	}

	return 0;
}

void device_notify(enum device_notifier_type status, void *data)
{
	dd_list *n, *next;
	struct device_notifier *notifier;
	int cnt = 0;

	DD_LIST_FOREACH_SAFE(device_notifier_list, n, next, notifier) {
		if (status == notifier->status) {
			if (notifier->func) {
				notifier->func(data);
				cnt++;
			}
		}
	}
}

static void late_init_stop(void)
{
	if (late_init_timer == NULL ||
	    late_init_timer == DEFAULT_LATE_INIT_VALUE)
		return;
	ecore_timer_del(late_init_timer);
	late_init_timer = NULL;
}

static int booting_done(void *data)
{
	static int done = 0;

	if (data == NULL)
		goto out;

	done = (int)data;
	if (late_init_timer == NULL)
		return done;
	late_init_stop();
out:
	return done;
}

static Eina_Bool late_init_timer_cb(void *data)
{
	int done;

	late_init_stop();
	done = booting_done(NULL);
	if (done)
		return EINA_FALSE;
	_I("late booting done");
	device_notify(DEVICE_NOTIFIER_BOOTING_DONE, (void *)TRUE);
	return EINA_FALSE;
}

static void device_notifier_init(void *data)
{
	int ret;

	ret = check_systemd_active();
	if (ret == TRUE) {
		_I("restart booting done");
		return;
	}
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	late_init_timer = ecore_timer_add(LATE_INIT_WAIT_TIME,
						late_init_timer_cb, NULL);
	if (!late_init_timer)
		late_init_timer = DEFAULT_LATE_INIT_VALUE;
}

static void device_notifier_exit(void *data)
{
	dd_list *n;
	struct device_notifier *notifier;

	DD_LIST_FOREACH(device_notifier_list, n, notifier)
		DD_LIST_REMOVE(device_notifier_list, notifier);
		free(notifier);
}

static const struct device_ops notifier_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "notifier",
	.init     = device_notifier_init,
	.exit     = device_notifier_exit,
};

DEVICE_OPS_REGISTER(&notifier_device_ops)
