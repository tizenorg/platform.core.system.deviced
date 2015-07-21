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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "log.h"

static int parent(pid_t pid)
{
	int status;

	/* wait for child */
	if (waitpid(pid, &status, 0) != -1) {
		/* terminated normally */
		if (WIFEXITED(status)) {
			_I("%d terminated by exit(%d)", pid, WEXITSTATUS(status));
			return WEXITSTATUS(status);
		} else if (WIFSIGNALED(status))
			_I("%d terminated by signal %d", pid, WTERMSIG(status));
		else if (WIFSTOPPED(status))
			_I("%d stopped by signal %d", pid, WSTOPSIG(status));
	} else
		_I("%d waitpid() failed : %d", pid, errno);

	return -EAGAIN;
}

static void child(int argc, const char *argv[])
{
	int i, r;

	for (i = 0; i < _NSIG; ++i)
		signal(i, SIG_DFL);

	r = execv(argv[0], (char **)argv);
	if (r < 0)
		exit(EXIT_FAILURE);
}

int run_child(int argc, const char *argv[])
{
	pid_t pid;
	struct sigaction act, oldact;
	int r = 0;
	FILE *fp;

	if (!argv)
		return -EINVAL;

	fp = fopen(argv[0], "r");
	if (fp == NULL) {
		_E("fail %s (%d)", argv[0], errno);
		return -errno;
	}
	fclose(fp);

	/* Use default signal handler */
	act.sa_handler = SIG_DFL;
	act.sa_sigaction = NULL;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGCHLD, &act, &oldact) < 0)
		return -errno;

	pid = fork();
	if (pid < 0) {
		_E("failed to fork");
		r = -errno;
	} else if (pid == 0) {
		child(argc, argv);
	} else
		r = parent(pid);

	if (sigaction(SIGCHLD, &oldact, NULL) < 0)
		_E("failed to restore sigaction");

	return r;
}
