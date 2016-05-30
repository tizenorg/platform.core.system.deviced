/*
 * deviced-vibrator
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
#include <fcntl.h>
#include <sys/reboot.h>

#include "display/core.h"
#include "core/log.h"
#include "core/common.h"
#include "shared/dbus.h"
#include "edbus-handler.h"
#include "haptic.h"

static void sig_quit(int signo)
{
	_D("received SIGTERM signal %d", signo);
}

static void sig_usr1(int signo)
{
	_D("received SIGUSR1 signal %d, deviced'll be finished!", signo);

	ecore_main_loop_quit();
}

int main(int argc, char **argv)
{
	int ret;

	ecore_init();
	edbus_init(NULL);
	ret = haptic_probe();
	if (ret != 0) {
		_E("[haptic] probe fail");
		return ret;
	}
	haptic_init();

	signal(SIGTERM, sig_quit);
	signal(SIGUSR1, sig_usr1);

	ecore_main_loop_begin();

	haptic_exit();
	edbus_exit(NULL);
	ecore_shutdown();
	return 0;
}
