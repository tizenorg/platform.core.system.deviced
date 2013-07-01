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


/**
 * @file	util.c
 * @brief	Utilities for Power manager
 *
 * This file includes logging, daemonize
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "util.h"

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

/**
 * @brief logging function
 *
 * This is log wrapper
 *
 * @param[in] priority log pritority
 * @param[in] fmt format string
 */
void pm_log(int priority, char *fmt, ...)
{
	va_list ap;
	char buf[NAME_MAX];	/* NAME_MAX is 255 */
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(priority, "%s", buf);
	printf("\x1b[1;33;44m[PowerManager] %s\x1b[0m\n\n", buf);
}

/**
 * @brief write the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return 0 (always)
 */
int writepid(char *pidpath)
{
	FILE *fp;

	fp = fopen(pidpath, "w");
	if (fp != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	return 0;
}

/**
 * @brief read the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return  pid : success, -1 : failed
 */
int readpid(char *pidpath)
{
	FILE *fp;
	int ret = -1;

	fp = fopen(pidpath, "r");
	if (fp != NULL) {
		if (fscanf(fp, "%5d", &ret) == EOF)
			_E("readpid fscanf failed!");
		fclose(fp);
	}

	return ret;
}

/**
 * @brief daemonize function
 *
 * fork the process, kill the parent process
 * and replace all the standard fds to /dev/null.
 *
 * @return 0 : success, -1 : fork() error
 */
int daemonize(void)
{
	pid_t pid;
	int fd;

	pid = fork();
	if (pid < 0)
		return -1;
	else if (pid != 0)
		exit(0);

	setsid();
	if (chdir("/") == -1) {
		_E("Failed to chdir");
	}

	close(0);
	close(1);
	close(2);

	fd = open("/dev/null", O_RDONLY);
	if (fd == -1) {
		_E("daemonize open failed [/dev/null RD]");
	}
	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		_E("daemonize open failed [/dev/null RDWR]");
	}
	dup(1);

	return 0;
}

/**
 * @brief  function to run a process
 *
 * fork the process, and run the other process if it is child.
 *
 * @return new process pid on success, -1 on error
 */

int exec_process(char *name)
{
	int ret, pid;
	int i;

	if (name[0] == '\0')
		return 0;

	pid = fork();
	switch (pid) {
	case -1:
		_E("Fork error");
		ret = -1;
		break;
	case 0:
		for (i = 0; i < _NSIG; i++)
			signal(i, SIG_DFL);
		execlp(name, name, NULL);
		_E("execlp() error : %s\n", strerror(errno));
		exit(-1);
		break;
	default:
		ret = pid;
		break;
	}
	return ret;
}

char *get_pkgname(char *exepath)
{
	char *filename;
	char pkgname[NAME_MAX];

	filename = strrchr(exepath, '/');
	if (filename == NULL)
		filename = exepath;
	else
		filename = filename + 1;

	snprintf(pkgname, NAME_MAX, "deb.com.samsung.%s", filename);

	return strdup(pkgname);
}

/**
 * @}
 */
