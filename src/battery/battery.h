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


#ifndef __BATTERY_H__
#define __BATTERY_H__

#define BATTERY_LEVEL_CHECK_FULL	95
#define BATTERY_LEVEL_CHECK_HIGH	15
#define BATTERY_LEVEL_CHECK_LOW		5
#define BATTERY_LEVEL_CHECK_CRITICAL	1

#define LOWBAT_OPT_WARNING		1
#define LOWBAT_OPT_POWEROFF		2
#define LOWBAT_OPT_CHARGEERR		3
#define LOWBAT_OPT_CHECK		4

#define METHOD_NAME_MAX			32
struct battery_config_info {
	int normal;
	int warning;
	int critical;
	int poweroff;
	int realoff;
};

int battery_charge_err_low_act(void *data);
int battery_charge_err_high_act(void *data);
#endif /* __BATTERY_H__ */
