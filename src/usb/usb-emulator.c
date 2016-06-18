/*
 * deviced
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
#include <stdbool.h>
#include "core/common.h"
#include "core/log.h"
#include "usb.h"

static bool usb_valid(void)
{
#ifdef EMULATOR
	_I("This is emulator device");
	return true;
#else
	return false;
#endif
}

static const struct usb_config_plugin_ops *usb_load(void)
{
	return NULL;
}

static const struct usb_config_ops usb_config_emulator_ops = {
	.is_valid  = usb_valid,
	.load      = usb_load,
};

USB_CONFIG_OPS_REGISTER(&usb_config_emulator_ops)
