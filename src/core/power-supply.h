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


#ifndef __POWER_SUPPLY_H__
#define __POWER_SUPPLY_H__

#define CHARGER_STATUS_SIGNAL      "ChargerStatus"
#define CHARGE_NOW_SIGNAL          "ChargeNow"
#define CHARGE_LEVEL_SIGNAL        "BatteryStatusLow"
#define CHARGE_CAPACITY_SIGNAL     "GetPercent"
#define CHARGE_CAPACITY_LAW_SIGNAL "GetPercentRaw"
#define CHARGE_HEALTH_SIGNAL       "GetHealth"
#define CHARGE_FULL_SIGNAL         "IsFull"

int check_abnormal_popup(void);
int check_lowbat_charge_device(int bInserted);
void power_supply(void *data);
int power_supply_init(void *data);
void power_supply_status_init(void);
void power_supply_timer_start(void);
void power_supply_timer_stop(void);
void power_supply_broadcast(char *sig, int status);
#endif /* __POWER_SUPPLY_H__ */
