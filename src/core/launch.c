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
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <vconf.h>

#include "log.h"
#include "launch.h"
#include "common.h"

#define MAX_ARGS 255

#define _S(str) ((str == NULL) ? "" : str)

static int set_current_lang(void)
{
	char *lang;
	int ret;

	lang = vconf_get_str(VCONFKEY_LANGSET);
	if (lang == NULL)
		return -1;
	ret = setenv("LANG", lang, 1);
	if (ret < 0)
		return -1;
	free(lang);
	return 0;
}


static void prepare_exec(void)
{
	int i;
	int maxfd;
	FILE *fp;

	maxfd = getdtablesize();
	for (i = 3; i < maxfd; i++)
		close(i);

	for (i = 0; i < _NSIG; i++)
		signal(i, SIG_DFL);

}

static int parse_cmd(const char *cmdline, char **argv, int max_args)
{
	const char *p;
	char *buf, *bufp;
	int nargs = 0;
	int escape = 0, squote = 0, dquote = 0;
	int bufsize;

	if (cmdline == NULL || cmdline[0] == '\0')
		return -1;
	bufsize = strlen(cmdline)+1;
	bufp = buf = malloc(bufsize);
	if (bufp == NULL || buf == NULL)
		return -1;
	memset(buf, 0, bufsize);
	p = cmdline;

	while (*p) {
		if (escape) {
			*bufp++ = *p;
			escape = 0;
		} else {
			switch (*p) {
				case '\\':
					escape = 1;
					break;
				case '"':
					if (squote)
						*bufp++ = *p;
					else
						dquote = !dquote;
					break;
				case '\'':
					if (dquote)
						*bufp++ = *p;
					else
						squote = !squote;
					break;
				case ' ':
					if (!squote && !dquote) {
						*bufp = '\0';
						if (nargs < max_args)
							argv[nargs++] = strdup(buf);
						bufp = buf;
						break;
					}
				default:
					*bufp++ = *p;
					break;
			}
		}
		p++;
	}

	if (bufp != buf) {
		*bufp = '\0';
		if (nargs < max_args)
			argv[nargs++] = strdup(buf);
	}

	argv[nargs++] = NULL;

	free(buf);
	return nargs;
}

int launch_app_with_nice(const char *file, char *const argv[], pid_t *pid, int _nice)
{
	int ret;
	int _pid;

	if (file == NULL || access(file, X_OK) != 0) {
		_E("launch app error: Invalid input");
		errno = EINVAL;
		return -1;
	}

	if (pid && (*pid > 0 && kill(*pid, 0) != -1))
		return *pid;

	_pid = fork();

	if (_pid == -1) {
		_E("fork error: %s", strerror(errno));
		/* keep errno */
		return -1;
	}

	if (_pid > 0) {     /* parent */
		if (pid)
			*pid = _pid;
		return _pid;
	}

	/* child */
	prepare_exec();

	ret = nice(_nice);

	if (ret == -1 && errno != 0)
		_E("nice error: %s", strerror(errno));

	ret = execvp(file, argv);

	/* If failed... */
	_E("exec. error: %s", strerror(errno));
	return -2;
}

int launch_app_cmd_with_nice(const char *cmdline, int _nice)
{
	int i;
	int nargs;
	int ret;
	char *argv[MAX_ARGS + 1];

	nargs = parse_cmd(cmdline, argv, MAX_ARGS + 1);
	if (nargs == -1) {
		_E("launch app error: Invalid input");
		errno = EINVAL;
		return -1;
	}

	ret = launch_app_with_nice(argv[0], argv, NULL, _nice);

	for (i = 0; i < nargs; i++)
		free(argv[i]);

	return ret;
}

int launch_app_cmd(const char *cmdline)
{
	return launch_app_cmd_with_nice(cmdline, 0);
}

int launch_if_noexist(const char *execpath, const char *arg, ...)
{
	char *buf;
	int pid;
	int nice_value = 0;
	int flag = 0;
	int buf_size = -1;
	va_list argptr;

	if (execpath == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (pid = get_exec_pid(execpath) > 0)
		return pid;

	va_start(argptr, arg);
	flag = va_arg(argptr, int);

	if (flag & LAUNCH_NICE)
		nice_value = va_arg(argptr, int);

	va_end(argptr);

	set_current_lang();
	arg = _S(arg);

	buf_size = strlen(execpath) + strlen(arg) + 10;
	buf = malloc(buf_size);
	if (buf == NULL) {
		/* Do something for not enought memory error */
		_E("Malloc failed");
		return -1;
	}

	snprintf(buf, buf_size, "%s %s", execpath, arg);
	//pid = launch_app_cmd_with_nice(buf, nice_value, flag);
	pid = launch_app_cmd_with_nice(buf, nice_value);
	if (pid == -2)
		exit(EXIT_FAILURE);
	free(buf);

	return pid;
}

int launch_evenif_exist(const char *execpath, const char *arg, ...)
{
	char *buf;
	int pid;
	int nice_value = 0;
	int flag = 0;
	int buf_size = -1;

	va_list argptr;

	if (execpath == NULL) {
		errno = EINVAL;
		return -1;
	}

	va_start(argptr, arg);
	flag = va_arg(argptr, int);

	if (flag & LAUNCH_NICE)
		nice_value = va_arg(argptr, int);

	va_end(argptr);

	set_current_lang();

	arg = _S(arg);

	buf_size = strlen(execpath) + strlen(arg) + 10;
	buf = malloc(buf_size);
	if (buf == NULL) {
		// Do something for not enought memory error
		_E("Malloc failed");
		return -1;
	}

	snprintf(buf, buf_size, "%s %s", execpath, arg);
	//pid = launch_app_cmd_with_nice(buf, nice_value, flag);
	pid = launch_app_cmd_with_nice(buf, nice_value);
	if (pid == -2)
		exit(EXIT_FAILURE);
	free(buf);

	return pid;
}

int launch_after_kill_if_exist(const char *execpath, const char *arg, ...)
{
	char *buf;
	int pid;
	int flag;
	int buf_size;
	int exist_pid;
	va_list argptr;
	int nice_value = 0;

	if (execpath == NULL) {
		errno = EINVAL;
		return -1;
	}

	if ((exist_pid = get_exec_pid(execpath)) > 0)
		kill(exist_pid, SIGTERM);

	va_start(argptr, arg);
	flag = va_arg(argptr, int);

	if (flag & LAUNCH_NICE)
		nice_value = va_arg(argptr, int);

	va_end(argptr);

	set_current_lang();

	arg = _S(arg);

	buf_size = strlen(execpath) + strlen(arg) + 10;
	buf = malloc(buf_size);
	if (buf == NULL) {
		/* Do something for not enought memory error */
		_E("Malloc Failed");
		return -1;
	}

	snprintf(buf, buf_size, "%s %s", execpath, arg);
	//pid = launch_app_cmd_with_nice(buf, nice_value, flag);
	pid = launch_app_cmd_with_nice(buf, nice_value);
	if (pid == -2)		/* It means that the 'execvp' return -1 */
		exit(EXIT_FAILURE);
	free(buf);

	return pid;

}
