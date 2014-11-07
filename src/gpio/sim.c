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


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "core/common.h"
#include "core/devices.h"
#include "core/log.h"
#include "gpio.h"

#define GPIO_CHECK_PATH		"/sys/class/gpio/sim"
#define GPIO_CHECK_STATUS	"/sys/class/gpio/sim/status"

static int sim_init(void)
{
	static int type = GPIO_DEVICE_UNKNOWN;
	int fd;

	if (type != GPIO_DEVICE_UNKNOWN)
		goto out;

	fd = open(GPIO_CHECK_PATH, O_RDONLY);
	if (fd == -1) {
		_E("%s open error: %s", GPIO_CHECK_PATH, strerror(errno));
		type = GPIO_DEVICE_NOT_EXIST;
		goto out;
	}
	close(fd);
	type = GPIO_DEVICE_EXIST;
out:
	return type;
}

static int sim_status(void)
{
	static int type = GPIO_DEVICE_UNKNOWN;
	int val = GPIO_DEVICE_UNKNOWN;
	int ret;
	int fd;
	char buf[2];

	type = sim_init();
	if (type != GPIO_DEVICE_EXIST)
		goto out;

	fd = open(GPIO_CHECK_STATUS, O_RDONLY);
	if (fd == -1) {
		_E("%s open error: %s", GPIO_CHECK_STATUS, strerror(errno));
		goto out;
	}

	ret = read(fd, buf, 1);
	close(fd);
	if (ret != 1) {
		_E("fail to get status %d", ret);
		goto out;
	}
	buf[1] = '\0';
	val = atoi(buf);
	_I("device is (%d)", val);
out:
	return val;
}

static const struct gpio_device sim_gpio = {
	.type   = GPIO_DEVICE_SIM,
	.name   = "sim",
	.init   = sim_init,
	.status = sim_status,
};

static void __CONSTRUCTOR__ module_init(void)
{
	register_gpio_device(&sim_gpio);
}
