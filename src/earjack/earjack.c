/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <vconf.h>

#include "core/log.h"
#include "display/poll.h"
#include "extcon/extcon.h"

#define SIGNAL_EARJACK_STATE	"ChangedEarjack"

static void earjack_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast earjack status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_EARJACK_STATE, "i", arr);
}

static int earjack_update(int status)
{
	_I("jack - earjack changed %d", status);
	vconf_set_int(VCONFKEY_SYSMAN_EARJACK, status);
	earjack_send_broadcast(status);
	if (status != 0)
		internal_pm_change_state(LCD_NORMAL);
}

static struct extcon_ops earjack_extcon_ops = {
	.name   = EXTCON_CABLE_HEADPHONE_OUT,
	.update = earjack_update,
};

EXTCON_OPS_REGISTER(&earjack_extcon_ops)
