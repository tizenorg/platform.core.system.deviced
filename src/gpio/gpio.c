/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>

#include "core/log.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/edbus-handler.h"
#include "core/list.h"
#include "gpio.h"

static dd_list *gpio_head;

static const struct gpio_device default_gpio = {
	.name = "default-gpio",
};

void register_gpio_device(const struct gpio_device *gpio)
{
	DD_LIST_APPEND(gpio_head, (void*)gpio);
}

void unregister_gpio_device(const struct gpio_device *gpio)
{
	DD_LIST_REMOVE(gpio_head, (void*)gpio);
}

int check_default_gpio_device(const struct gpio_device *gpio)
{
	return (gpio == &default_gpio);
}

const struct gpio_device *find_gpio_device(const char *name)
{
	dd_list *elem;
	const struct gpio_device *gpio;

	DD_LIST_FOREACH(gpio_head, elem, gpio) {
		if (!strcmp(gpio->name, name))
			return gpio;
	}

	gpio = &default_gpio;
	return gpio;
}

static DBusMessage *check_gpio_status(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	const struct gpio_device *gpio;
	char *name;
	int ret;
	int status = GPIO_DEVICE_UNKNOWN;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INVALID);
	if (!ret) {
		status = -EBADMSG;
		goto out;
	}

	gpio = find_gpio_device(name);
	if (check_default_gpio_device(gpio))
		goto out;
	status = gpio->status();
		_D("%s %d", name, status);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &status);
	return reply;
}

static const struct edbus_method edbus_methods[] = {
	{ "GetStatus",          "s", "i", check_gpio_status },
};

static void gpio_init(void *data)
{
	struct gpio_device *gpio;
	dd_list *elem;
	int ret;

	DD_LIST_FOREACH(gpio_head, elem, gpio) {
		gpio->init();
		gpio->status();
	}

	ret = register_edbus_method(DEVICED_PATH_GPIO, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);
}

const struct device_ops gpio_device_ops = {
	.name     = "gpio",
	.init     = gpio_init,
};

DEVICE_OPS_REGISTER(&gpio_device_ops)
