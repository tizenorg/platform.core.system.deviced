/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
 */


#ifndef __SS_DEVICE_HANDLER_H__
#define __SS_DEVICE_HANDLER_H__

#include "data.h"

/* MMC functions */
int ss_mmc_init();
int ss_mmc_inserted();
int ss_mmc_removed();

/* USB Storage */
int _ss_usb_storage_init(void);

/* Battery functions */
int ss_lowbat_init(struct ss_main_data *ad);
int ss_lowbat_is_charge_in_now();
int ss_lowbat_set_charge_on(int onoff);
int ss_lowbat_monitor(void *data);

/* Low memory functions */
int ss_lowmem_init(struct ss_main_data *ad);

/* USB functions */
int ss_usb_init();

/* TA functions */
int ss_ta_init();

/* device change init */
int ss_device_change_init(struct ss_main_data *ad);

#endif /* __SS_DEVICE_HANDLER_H__ */
