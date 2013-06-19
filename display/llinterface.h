/*
 * power-manager
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


/**
 * @file	llinterface.h
 * @brief	Power manager low-level interface module header
 */
#ifndef __PM_LLINTERFACE_H__
#define __PM_LLINTERFACE_H__

#define FLAG_X_DPMS		0x2

#define DEFAULT_DISPLAY 0

#define PM_MAX_BRIGHTNESS       100
#define PM_MIN_BRIGHTNESS       1
#ifdef TIZEN_EMUL
#	define PM_DEFAULT_BRIGHTNESS	100
#else
#	define PM_DEFAULT_BRIGHTNESS	60
#endif

#define PM_LCD_POWER_ON		0
#define PM_LCD_POWER_OFF	4

#define PM_LCD_RETRY_CNT	3

/*
 * Event type enumeration
 */
enum {
	EVENT_TIMEOUT = 0,	/*< time out event from timer */
	EVENT_DEVICE = EVENT_TIMEOUT,	/*< wake up by devices except input devices */
	EVENT_INPUT,		/*< input event from noti service */
	EVENT_END,
};

extern int init_sysfs(unsigned int);
extern int exit_sysfs(void);

struct _backlight_ops {
	int (*off)(void);
	int (*dim)(void);
	int (*on)(void);
	int (*restore)(void);
	int (*set_default_brt)(int level);
	int (*get_lcd_power)(void);
};

struct _power_ops {
	int (*suspend)(void);
	int (*pre_suspend)(void);
	int (*post_resume)(void);
	int (*check_wakeup_src)(void);
};

struct _battery_ops {
	int (*get_capacity)(void);
	int (*get_capacity_raw)(void);
	int (*get_charge_full)(void);
};

extern struct _backlight_ops backlight_ops;
extern struct _power_ops power_ops;
extern struct _battery_ops battery_ops;

#endif

