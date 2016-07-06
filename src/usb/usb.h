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

#ifndef __USB_CLIENT_H__
#define __USB_CLIENT_H__

#define USB_MODE_STR_NONE      "NONE"
#define USB_MODE_STR_DEFAULT   "DEFAULT"
#define USB_MODE_STR_TETHERING "TETHERING"

#define USB_CONFIG_OPS_REGISTER(dev)    \
static void __CONSTRUCTOR__ usb_config_init(void)   \
{   \
	add_usb_config(dev);    \
}   \
static void __DESTRUCTOR__ usb_config_exit(void)    \
{   \
	remove_usb_config(dev); \
}

struct usb_config_ops {
	bool (*is_valid)(void);
	const struct usb_config_plugin_ops *(*load)(void);
	void (*release)(void);
};

/* TODO
 * move it to proper location */
struct usb_config_plugin_ops {
	int  (*init)(char *name);
	void (*deinit)(char *name);
	int  (*enable)(char *name);
	int  (*disable)(char *name);
	int  (*change)(char *name);
};

void add_usb_config(const struct usb_config_ops *ops);
void remove_usb_config(const struct usb_config_ops *ops);

int usb_change_mode(char *name);

#endif /* __USB_CLIENT_H__ */
