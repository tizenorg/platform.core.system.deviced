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


#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vconf.h>
#include "ss_core.h"
#include "edbus-handler.h"

#define PRT_TRACE_ERR(format, args...) do { \
	char buf[255];\
	snprintf(buf, 255, format, ##args);\
	write(2, buf, strlen(buf));\
} while (0);

#define PRT_TRACE(format, args...) do { \
	char buf[255];\
	snprintf(buf, 255, format, ##args);\
	write(1, buf, strlen(buf));\
} while (0);

#define SIGNAL_NAME_POWEROFF_POPUP	"poweroffpopup"

static struct sigaction sig_child_old_act;
static struct sigaction sig_pipe_old_act;

static void sig_child_handler(int signo, siginfo_t *info, void *data)
{
	pid_t pid;
	int status;

	pid = waitpid(info->si_pid, &status, 0);
	if (pid == -1) {
		PRT_TRACE_ERR("SIGCHLD received\n");
		return;
	}

	PRT_TRACE("sig child actend call - %d\n", info->si_pid);

	ss_core_action_clear(info->si_pid);
}

static void sig_pipe_handler(int signo, siginfo_t *info, void *data)
{

}

static void poweroff_popup_edbus_signal_handler(void *data, DBusMessage *msg)
{
	DBusError err;
	char *str;
	int val = 0;

	if (dbus_message_is_signal(msg, INTERFACE_NAME, SIGNAL_NAME_POWEROFF_POPUP) == 0) {
		PRT_TRACE_ERR("there is no power off popup signal");
		return;
	}

	dbus_error_init(&err);

	if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID) == 0) {
		PRT_TRACE_ERR("there is no message");
		return;
	}

	if (strncmp(str, PREDEF_PWROFF_POPUP, strlen(PREDEF_PWROFF_POPUP)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_POPUP;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_POWEROFF)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_DIRECT;
	else if (strncmp(str, PREDEF_POWEROFF, strlen(PREDEF_REBOOT)) == 0)
		val = VCONFKEY_SYSMAN_POWER_OFF_RESTART;
	if (val == 0) {
		PRT_TRACE_ERR("not supported message : %s", str);
		return;
	}
	vconf_set_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, val);
}

void ss_signal_init(void)
{
	struct sigaction sig_act;

	sig_act.sa_handler = NULL;
	sig_act.sa_sigaction = sig_child_handler;
	sig_act.sa_flags = SA_SIGINFO;
	sigemptyset(&sig_act.sa_mask);
	sigaction(SIGCHLD, &sig_act, &sig_child_old_act);

	sig_act.sa_handler = NULL;
	sig_act.sa_sigaction = sig_pipe_handler;
	sig_act.sa_flags = SA_SIGINFO;
	sigemptyset(&sig_act.sa_mask);
	sigaction(SIGPIPE, &sig_act, &sig_pipe_old_act);
	register_edbus_signal_handler(SIGNAL_NAME_POWEROFF_POPUP, (void *)poweroff_popup_edbus_signal_handler);
}
