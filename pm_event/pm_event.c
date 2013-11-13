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
#include <string.h>
#include <limits.h>

#define PM_EVENT_NOTI_PATH	"/opt/share/noti/pm_event"
#define PM_EVENT_CMD_LENGTH		50

int main(int argc, char *argv[])
{
	if(argc != 3) {
		return -1;
	}

	if(strlen(argv[1]) + strlen(argv[2]) > PATH_MAX) {
		return -1;
	}

	int i;
	char buf[PATH_MAX + PM_EVENT_CMD_LENGTH];
	snprintf(buf, (PATH_MAX + PM_EVENT_CMD_LENGTH), "echo %s %s >> %s",
		argv[1], argv[2], PM_EVENT_NOTI_PATH);
	system(buf);

	return 1;
}
