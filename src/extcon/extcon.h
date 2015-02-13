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


#ifndef __EXTCON_H__
#define __EXTCON_H__

/**
 * Extcon cable name is shared with kernel extcon class.
 * So do not change below strings.
 */
#define EXTCON_CABLE_USB              "USB"
#define EXTCON_CABLE_USB_HOST         "USB-Host"
#define EXTCON_CABLE_TA               "TA"
#define EXTCON_CABLE_HDMI             "HDMI"
#define EXTCON_CABLE_DOCK             "Dock"
#define EXTCON_CABLE_MIC_IN           "Microphone"
#define EXTCON_CABLE_HEADPHONE_OUT    "Headphone"

struct extcon_ops {
	const char *name;
	int status;
	enum device_notifier_type noti;
	void (*init)(void *data);
	void (*exit)(void *data);
};

#define EXTCON_OPS_REGISTER(dev) \
static void __CONSTRUCTOR__ extcon_init(void) \
{ \
	add_extcon(dev); \
} \
static void __DESTRUCTOR__ extcon_exit(void) \
{ \
	remove_extcon(dev); \
}

void add_extcon(struct extcon_ops *dev);
void remove_extcon(struct extcon_ops *dev);

int extcon_get_status(const char *name);

#endif /* __EXTCON_H__ */
