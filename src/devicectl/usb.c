/*
 * devicectl
 *
 * Copyright (c) 2012 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
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
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <core/common.h>
#include <core/launch.h>
#include "usb.h"

#define USB_SDB "sdb"
#define USB_SSH "ssh"

#define ARG_MAX 10
#define CMD_MAX 128

static struct usb_sysfs {
	char *path;
	char *value;
} usb_confs[] = {
	{ "/sys/class/usb_mode/usb0/enable",          "0"    },
	{ "/sys/class/usb_mode/usb0/idVendor",        "04e8" },
	{ "/sys/class/usb_mode/usb0/idProduct",       NULL   },
	{ "/sys/class/usb_mode/usb0/funcs_fconf",     NULL   },
	{ "/sys/class/usb_mode/usb0/funcs_sconf",     NULL   },
	{ "/sys/class/usb_mode/usb0/bDeviceClass",    "239"  },
	{ "/sys/class/usb_mode/usb0/bDeviceSubClass", "2"    },
	{ "/sys/class/usb_mode/usb0/bDeviceProtocol", "1"    },
	{ "/sys/class/usb_mode/usb0/enable",          "1"    },
};

static int launch_app(char **argv)
{
	pid_t pid;

	if (!argv || !argv[0])
		return -EINVAL;

	pid = fork();

	if (pid < 0) {
		printf("fork() failed\n");
		return -ENOMEM;
	}

	if (pid > 0) { /*parent*/
		return pid;
	}

	/*child*/

	if (execvp(argv[0], argv) < 0)
		printf("execvp failed (%d)\n", errno);

	return 0;
}

static int write_sysfs(char *path, char *value)
{
	FILE *fp;
	int ret;

	if (!path || !value)
		return -ENOMEM;

	fp = fopen(path, "w");
	if (!fp) {
		printf("FAIL: fopen(%s)\n", path);
		return -ENOMEM;
	}

	ret = fwrite(value, sizeof(char), strlen(value), fp);
	fclose(fp);
	if (ret < strlen(value)) {
		printf("FAIL: fwrite(%s)\n", value);
		ret = -ENOMEM;
	}

	return ret;
}

static int set_usb_configuration(char *idproduct, char *fconf, char *sconf)
{
	int i, ret;

	usb_confs[2].value = idproduct;
	usb_confs[3].value = fconf;
	usb_confs[4].value = sconf;

	for (i = 0 ; i < ARRAY_SIZE(usb_confs); i++) {
		ret = write_sysfs(usb_confs[i].path, usb_confs[i].value);
		if (ret < 0) {
			printf("usb setting fails (%s), (%s)\n", usb_confs[i].path, usb_confs[i].value);
			return ret;
		}
	}

	return 0;
}

static int divide_cmd(char **command, int len, char *cmd)
{
	char *param, *next, *term;
	int cnt = 0;

	if (!cmd)
		return -EINVAL;

	term = strchr(cmd, '\0');
	if (!term)
		return -EINVAL;

	memset(command, 0, len);

	param = cmd;
	while (1) {
		if (*param == '\0')
			break;
		if (*param == ' ') {
			param++;
			continue;
		}

		next = strchr(param, ' ');
		if (!next) {
			command[cnt++] = param;
			break;
		}

		if (next == param) {
			param++;
			continue;
		}

		*next = '\0';
		command[cnt++] = param;
		param = next + 1;
	}

	return 0;
}

static int run_cmd(char *cmd)
{
	int ret;
	char *command[ARG_MAX];
	char in_cmd[CMD_MAX];

	if (!cmd)
		return -EINVAL;

	snprintf(in_cmd, sizeof(in_cmd), "%s", cmd);

	ret = divide_cmd(command, sizeof(command), in_cmd);
	if (ret < 0)
		return ret;

	ret = launch_app(command);
	if (ret < 0)
		return ret;

	return 0;

}

static int load_sdb(void)
{
	int ret;
	char *cmd[ARG_MAX];

	ret = set_usb_configuration("6860", "mtp", "mtp,acm,sdb");
	if (ret < 0)
		return ret;

	return run_cmd("/usr/bin/systemctl start sdbd.service");
}

static int load_ssh(void)
{
	int ret;

	ret = set_usb_configuration("6863", "rndis", " ");
	if (ret < 0)
		return ret;

	ret = run_cmd("/sbin/ifconfig usb0 192.168.129.3 up");
	if (ret < 0)
		return ret;

	ret = run_cmd("/sbin/route add -net 192.168.129.0 netmask 255.255.255.0 dev usb0");
	if (ret < 0)
		return ret;

	ret = run_cmd("/usr/bin/systemctl start sshd.service");
	if (ret < 0)
		return ret;

	return 0;
}

static int unload_sdb(void)
{
	int ret;

	ret = write_sysfs(usb_confs[0].path, usb_confs[0].value);
	if (ret < 0)
		return ret;

	ret = run_cmd("/usr/bin/systemctl stop sdbd.service");
	if (ret < 0)
		return ret;

	return 0;
}

static int unload_ssh(void)
{
	int ret;

	ret = write_sysfs(usb_confs[0].path, usb_confs[0].value);
	if (ret < 0)
		return ret;

	ret = run_cmd("/sbin/ifconfig usb0 down");
	if (ret < 0)
		return ret;

	ret = run_cmd("/usr/bin/systemctl stop sshd.service");
	if (ret < 0)
		return ret;

	return 0;
}

int load_usb_mode(char *opt)
{
	if (!opt) {
		printf("Failed: Forth parameter is NULL\n");
		return -EINVAL;
	}

	if (!strncmp(opt, USB_SDB, strlen(opt)))
		return load_sdb();

	if (!strncmp(opt, USB_SSH, strlen(opt)))
		return load_ssh();

	printf("Failed: Forth parameter is invalid (%s)\n", opt);
	return -EINVAL;
}

int unload_usb_mode(char *opt)
{
	if (!opt) {
		printf("Failed: Forth parameter is NULL\n");
		return -EINVAL;
	}

	if (!strncmp(opt, USB_SDB, strlen(opt)))
		return unload_sdb();

	if (!strncmp(opt, USB_SSH, strlen(opt)))
		return unload_ssh();

	printf("Failed: Forth parameter is invalid (%s)\n", opt);
	return -EINVAL;
}
