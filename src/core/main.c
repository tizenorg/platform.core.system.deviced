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


#include <stdio.h>
#include <fcntl.h>
#include <sys/reboot.h>

#include "display/core.h"
#include "log.h"
#include "common.h"
#include "edbus-handler.h"
#include "devices.h"
#include "shared/dbus.h"
#include "device-notifier.h"
#include "battery/battery.h"

#define PIDFILE_PATH		"/var/run/.deviced.pid"

static void writepid(char *pidpath)
{
	FILE *fp;

	fp = fopen(pidpath, "w");
	if (fp != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
}

static void sig_quit(int signo)
{
	_D("received SIGTERM signal %d", signo);
}

static void sig_usr1(int signo)
{
	_D("received SIGUSR1 signal %d, deviced'll be finished!", signo);

	ecore_main_loop_quit();
}

static int deviced_main(int argc, char **argv)
{
	int ret;

	edbus_init(NULL);
	devices_init(NULL);
	ret = check_systemd_active();
	if (ret == TRUE) {
		_I("notify relaunch");
		device_notify(DEVICE_NOTIFIER_BOOTING_DONE, &ret);
	}
	signal(SIGTERM, sig_quit);
	signal(SIGUSR1, sig_usr1);

	ecore_main_loop_begin();

	devices_exit(NULL);
	edbus_exit(NULL);
	ecore_shutdown();
	return 0;
}

int main(int argc, char **argv)
{
	writepid(PIDFILE_PATH);
	ecore_init();
	return deviced_main(argc, argv);
}
