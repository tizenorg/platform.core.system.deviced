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
#include <signal.h>
#include <assert.h>
#include <sys/signalfd.h>

#include "log.h"
#include "devices.h"
#include "list.h"

struct sigchld_info {
	pid_t pid;
	void (*func)(int, void *);
	void *data;
};

static Ecore_Fd_Handler *fdh;
static dd_list *sigchld_list;

int register_sigchld_control(pid_t pid,
		void (*func)(int ret, void *data), void *data)
{
	struct sigchld_info *info;
	dd_list *elem;

	DD_LIST_FOREACH(sigchld_list, elem, info) {
		if (info->pid == pid && info->func == func)
			return -EEXIST;
	}

	info = malloc(sizeof(struct sigchld_info));
	if (!info)
		return -errno;

	info->pid = pid;
	info->func = func;
	info->data = data;

	DD_LIST_APPEND(sigchld_list, info);
	_D("%s added (%d, %x)", __func__, info->pid, info->func);
	return 0;
}

int unregister_sigchld_control(pid_t pid,
		void (*func)(int ret, void *data))
{
	struct sigchld_info *info;
	dd_list *elem;
	dd_list *next;

	DD_LIST_FOREACH_SAFE(sigchld_list, elem, next, info) {
		if (info->pid == pid && info->func == func) {
			DD_LIST_REMOVE_LIST(sigchld_list, elem);
			free(info);
			_D("%s removed (%d, %x)", __func__,
					info->pid, info->func);
		}
	}

	return 0;
}

static void sigchld_process(struct signalfd_siginfo *fdsi)
{
	struct sigchld_info *info;
	dd_list *elem;
	dd_list *next;

	assert(fdsi);

	/* skip sigchld action of the no interest process */
	DD_LIST_FOREACH(sigchld_list, elem, info) {
		if (info->pid == fdsi->ssi_pid)
			break;
	}

	/* not matched pid */
	if (!info)
		return;

	_E("%s pid : %d, signo: %d, status : %d, utime : %d, stime : %d",
			__func__, fdsi->ssi_pid, fdsi->ssi_signo,
			fdsi->ssi_status, fdsi->ssi_utime, fdsi->ssi_stime);

	DD_LIST_FOREACH_SAFE(sigchld_list, elem, next, info) {
		if (info->pid == fdsi->ssi_pid) {
			if (info->func)
				info->func(fdsi->ssi_status, info->data);
			DD_LIST_REMOVE_LIST(sigchld_list, elem);
			free(info);
		}
	}
}

static Eina_Bool signal_handler_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	int sfd;
	struct signalfd_siginfo fdsi;
	ssize_t s;

	if (!ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ))
		return ECORE_CALLBACK_RENEW;

	sfd = ecore_main_fd_handler_fd_get(fd_handler);
	assert(sfd >= 0);

	s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
	if (s != sizeof(struct signalfd_siginfo))
		return ECORE_CALLBACK_RENEW;

	if (fdsi.ssi_signo == SIGCHLD)
		sigchld_process(&fdsi);

	return ECORE_CALLBACK_RENEW;
}

static void signal_init(void *data)
{
	sigset_t mask;
	int sfd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	/* Block signals so that they aren't handled
	   according to their default dispositions */
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		_E("fail to change the blocked signals");
		return;
	}

	sfd = signalfd(-1, &mask, 0);
	if (sfd == -1) {
		_E("fail to create a file descriptor for accepting signals");
		return;
	}

	fdh = ecore_main_fd_handler_add(sfd, ECORE_FD_READ,
			signal_handler_cb, NULL, NULL, NULL);
	if (!fdh) {
		_E("fail to add fd handler");
		close(sfd);
		return;
	}
}

static void signal_exit(void)
{
	int sfd;

	if (!fdh)
		return;

	sfd = ecore_main_fd_handler_fd_get(fdh);

	/* Should be deleted fd handler before closing fd.
	   if not, it might make crash or instability. */
	ecore_main_fd_handler_del(fdh);
	fdh = NULL;

	if (sfd)
		close(sfd);
}

static const struct device_ops signal_device_ops = {
	.name     = "signal",
	.init     = signal_init,
	.exit     = signal_exit,
};

DEVICE_OPS_REGISTER(&signal_device_ops)
