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


#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>
#include <assert.h>

#include "core/log.h"
#include "core/common.h"
#include "core/config-parser.h"
#include "config.h"

#define MAX_RATIO_CONF_FILE	"/etc/deviced/mmc.conf"
#define MAX_RATIO_PATH		"/sys/class/block/mmcblk%d/bdi/max_ratio"
#define MAX_RATIO_CONFIG_RETRY	5
#define MAX_RATIO_DURATION		100

static struct mmc_policy_type {
	int max_ratio;
} mmc_policy = {
	.max_ratio = MAX_RATIO_DURATION,
};

static void mmc_max_ratio(const char *devpath)
{
	char buf[PATH_MAX];
	FILE *fp;
	int ret;
	int partition_num;
	int dev_num;
	int retry;

	sscanf(devpath, "/dev/mmcblk%dp%d", &dev_num, &partition_num);
	snprintf(buf, PATH_MAX, MAX_RATIO_PATH, dev_num);

	for (retry = MAX_RATIO_CONFIG_RETRY; retry > 0 ; retry--) {
		ret = sys_set_int(buf, mmc_policy.max_ratio);
		if (ret == 0)
			break;
	}
	if (ret < 0)
		_E("fail path : %s max_ratio: %d ret %d", buf, mmc_policy.max_ratio, ret);
	else
		_D("mmc bdi max : %d", mmc_policy.max_ratio);
}

static int load_config(struct parse_result *result, void *user_data)
{
	struct mmc_policy_type *policy = user_data;
	char *name;
	char *value;

	_D("%s,%s,%s", result->section, result->name, result->value);

	if (!policy)
		return -EINVAL;

	if (!MATCH(result->section, "MMC"))
		return -EINVAL;

	name = result->name;
	value = result->value;
	if (MATCH(name, "MaxRatio"))
		policy->max_ratio = atoi(value);

	return 0;
}

void mmc_load_config(void)
{
	int ret;
	ret = config_parse(MAX_RATIO_CONF_FILE, load_config, &mmc_policy);
	if (ret < 0)
		_E("Failed to load %s, %d Use default value!", MAX_RATIO_CONF_FILE, ret);
}

void mmc_set_config(enum mmc_config_type type, const char *devpath)
{
	switch(type) {
	case MAX_RATIO:
		mmc_max_ratio(devpath);
	break;
	}
}
