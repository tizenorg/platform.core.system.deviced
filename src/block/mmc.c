/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <vconf.h>
#include <sys/types.h>
#include <attr/xattr.h>

#include "core/log.h"
#include "core/common.h"
#include "block/block.h"

static void mmc_update_state(int state)
{
	static int old = -1;

	if (old == state)
		return;

	if (vconf_set_int(VCONFKEY_SYSMAN_MMC_STATUS, state) == 0)
		old = state;
}

static void mmc_mount(struct block_data *data, int result)
{
	int r;

	/* Only the primary partition is valid. */
	if (!data || !data->primary)
		return;

	if (result < 0) {
		mmc_update_state(VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
		return;
	}

	/* Give a transmutable attribute to mount_point */
	r = setxattr(data->mount_point, "security.SMACK64TRANSMUTE",
			"TRUE", strlen("TRUE"), 0);
	if (r < 0)
		_E("setxattr error : %d", errno);

	mmc_update_state(VCONFKEY_SYSMAN_MMC_MOUNTED);
}

static void mmc_unmount(struct block_data *data, int result)
{
	/* Only the primary partition is valid. */
	if (!data || !data->primary)
		return;

	if (result == 0)
		mmc_update_state(VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED);
	else
		mmc_update_state(VCONFKEY_SYSMAN_MMC_MOUNTED);
}

static void mmc_format(struct block_data *data, int result)
{
	/* Only the primary partition is valid. */
	if (!data || !data->primary)
		return;

	if (data->state == BLOCK_MOUNT)
		mmc_update_state(VCONFKEY_SYSMAN_MMC_MOUNTED);
}

static void mmc_insert(struct block_data *data)
{
	/* Do nothing */
}

static void mmc_remove(struct block_data *data)
{
	/* Only the primary partition is valid. */
	if (!data || !data->primary)
		return;

	mmc_update_state(VCONFKEY_SYSMAN_MMC_REMOVED);
}

const struct block_dev_ops mmc_block_ops = {
	.name       = "mmc",
	.block_type = BLOCK_MMC_DEV,
	.mounted    = mmc_mount,
	.unmounted  = mmc_unmount,
	.formatted  = mmc_format,
	.inserted   = mmc_insert,
	.removed    = mmc_remove,
};

BLOCK_DEVICE_OPS_REGISTER(&mmc_block_ops)
