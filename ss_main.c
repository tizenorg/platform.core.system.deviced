/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <systemd/sd-daemon.h>
#include <stdio.h>
#include <fcntl.h>
#include <heynoti.h>
#include <sys/reboot.h>

#include "ss_log.h"
#include "ss_core.h"
#include "ss_sig_handler.h"
#include "ss_device_handler.h"
#include "ss_pmon_handler.h"
#include "ss_sysnoti.h"
#include "ss_noti.h"
#include "ss_queue.h"
#include "ss_predefine.h"
#include "ss_bs.h"
#include "ss_procmgr.h"
#include "ss_timemgr.h"
#include "ss_cpu_handler.h"
#include "include/ss_data.h"
#include "edbus-handler.h"

static void fini(struct ss_main_data *ad)
{
}

static void init_ad(struct ss_main_data *ad)
{
	memset(ad, 0x0, sizeof(struct ss_main_data));
}

static void writepid(char *pidpath)
{
	FILE *fp;

	fp = fopen(pidpath, "w");
	if (fp != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
}

static void system_server_init(struct ss_main_data *ad)
{
	ad->sysnoti_fd = ss_sysnoti_init();
	if (ss_noti_init() < 0)
		PRT_TRACE_ERR("init noti error");

	ss_queue_init();
	ss_core_init(ad);
	ss_signal_init();
	ss_predefine_internal_init();
	ss_process_manager_init();
	ss_time_manager_init();
	ss_cpu_handler_init();

	ss_lowmem_init(ad);
	ss_lowbat_init(ad);
	ss_usb_init();
	ss_ta_init();
	ss_pmon_init(ad);
	ss_device_change_init(ad);
	ss_mmc_init();
	ss_bs_init();
}

#define SS_PIDFILE_PATH		"/var/run/.system_server.pid"
static void sig_quit(int signo)
{
	PRT_TRACE_ERR("received SIGTERM signal %d", signo);
	if (is_power_off() == 1)
		reboot(RB_POWER_OFF);
}
static int system_main(int argc, char **argv)
{
	struct ss_main_data ad;

	init_ad(&ad);
	if ((ad.noti_fd = heynoti_init()) < 0) {
		PRT_TRACE_ERR("Hey Notification Initialize failed");
		fini(&ad);
		return 0;
	}
	if (heynoti_attach_handler(ad.noti_fd) != 0) {
		PRT_TRACE_ERR("fail to attach hey noti handler");
		fini(&ad);
		return 0;
	}
	edbus_init();
	system_server_init(&ad);
	signal(SIGTERM, sig_quit);

	// Notyfication to systemd
	if (sd_booted())
		sd_notify(0, "READY=1");

	ecore_main_loop_begin();

	fini(&ad);
	edbus_fini();
	ecore_shutdown();

	return 0;
}

int main(int argc, char **argv)
{
	writepid(SS_PIDFILE_PATH);
	ecore_init();
	return system_main(argc, argv);
}
