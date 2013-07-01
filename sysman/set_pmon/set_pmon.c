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
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sysman.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFF_MAX 255

int set_oomadj(int pid, int oomadj_val)
{
	char buf[BUFF_MAX];
	FILE *fp;

	snprintf(buf, BUFF_MAX, "/proc/%d/oom_adj", pid);
	fp = fopen(buf, "w");
	if (fp == NULL)
		return -1;
	fprintf(fp, "%d", oomadj_val);
	fclose(fp);

	return 0;
}

int check_and_setpmon(int pid, int idx, char **argv, char type)
{
	int fd;
	char buf[BUFF_MAX];
	char *filename;

	snprintf(buf, BUFF_MAX, "/proc/%d/cmdline", pid);
	fd = open(buf, O_RDONLY);
	if (fd < 0)
		return -1;

	read(fd, buf, BUFF_MAX-1);
	close(fd);

	buf[BUFF_MAX-1]='\0';

	filename = strrchr(buf, '/');
	if (filename == NULL)
		filename = buf;
	else
		filename = filename + 1;

	while (idx > 1) {	/* argv[1] is monitoring type */
		if (!strcmp(argv[idx], filename)) {
			switch (type) {
			case 'v':
				printf("====> found, %s - set vip\n",
				       argv[idx]);
				set_oomadj(pid, -17);
				sysconf_set_vip(pid);
				return 0;
			case 'p':
				printf("====> found, %s - set permanent\n",
				       argv[idx]);
				sysconf_set_permanent_bypid(pid);
				return 0;
			default:
				break;
			}
		}
		idx--;
	}
	return -1;
}

int main(int argc, char **argv)
{
	int pid = -1;
	char type;
	DIR *dp;
	struct dirent *dentry;

	if (argc < 3 ||
	    argv[1][0] != '-' ||
	    argv[1][1] == '\0' || (argv[1][1] != 'v' && argv[1][1] != 'p')) {
		printf("Usage: %s <monitoring type> <process name>\n", argv[0]);
		printf("Monitoring types: -v \t VIP process\n");
		printf("                  -p \t Permanent process\n");
		return 1;
	}

	dp = opendir("/proc");
	if (!dp) {
		printf("open /proc : %s\n", strerror(errno));
		return 1;
	}
	type = argv[1][1];

	while ((dentry = readdir(dp)) != NULL) {
		if (!isdigit(dentry->d_name[0]))
			continue;

		pid = atoi(dentry->d_name);
		check_and_setpmon(pid, argc - 1, argv, type);
	}

	closedir(dp);
	return 0;
}
