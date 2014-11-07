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

#ifndef __GPIO_HANDLER_H__
#define __GPIO_HANDLER_H__

#include <errno.h>
#include "core/common.h"

#define GPIO_DEVICE_CLOSED	0
#define GPIO_DEVICE_OPENED	1

enum gpio_device_check_type {
	GPIO_DEVICE_UNKNOWN = -1,
	GPIO_DEVICE_NOT_EXIST = 0,
	GPIO_DEVICE_EXIST = 1,
};
enum gpio_device_type {
	GPIO_DEVICE_HALL = 0,
	GPIO_DEVICE_BUZZER,
	GPIO_DEVICE_SIM,
};

struct gpio_device {
	enum gpio_device_type type;
	const char *name;
	int (*init) (void);
	int (*status) (void);
};

void register_gpio_device(const struct gpio_device *gpio);
void unregister_gpio_device(const struct gpio_device *gpio);
const struct gpio_device *find_gpio_device(const char *name);
int check_default_gpio_device(const struct gpio_device *dev);

#define NOT_SUPPORT_GPIO(gpio) \
	((check_default_gpio_device(gpio))? 1 : 0)

#define FIND_GPIO_INT(gpio, name) do { \
	if (!gpio) gpio = find_gpio_device(name); if(check_default_gpio_device(gpio)) return -ENODEV; \
} while(0)

#define FIND_GPIO_VOID(gpio, name) do { \
	if (!gpio) gpio = find_gpio_device(name); if(check_default_gpio_device(gpio)) return; \
} while(0)


#endif /* __GPIO_HANDLER_H__ */
