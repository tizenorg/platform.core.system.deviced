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


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <vconf.h>
#include <Ecore.h>
#include "util.h"
#include "core.h"
#include "poll.h"
#include "device-node.h"
#include "display-actor.h"
#include "core/common.h"
#include "core/devices.h"
#include "core/device-notifier.h"
#include "core/edbus-handler.h"
#include "power/power-handler.h"
#include "led/touch-key.h"

#include <linux/input.h>
#ifndef KEY_SCREENLOCK
#define KEY_SCREENLOCK		0x98
#endif
#ifndef SW_GLOVE
#define SW_GLOVE		0x16
#endif

#define PREDEF_LEAVESLEEP	"leavesleep"
#define POWEROFF_ACT			"poweroff"
#define PWROFF_POPUP_ACT		"pwroff-popup"
#define USEC_PER_SEC			1000000
#define COMBINATION_INTERVAL		0.5	/* 0.5 second */

#define KEY_MAX_DELAY_TIME		700	/* ms */

#define KEY_RELEASED		0
#define KEY_PRESSED		1
#define KEY_BEING_PRESSED	2

#define KEY_COMBINATION_STOP		0
#define KEY_COMBINATION_START		1
#define KEY_COMBINATION_SCREENCAPTURE	2

#define SIGNAL_CHANGE_HARDKEY		"ChangeHardkey"
#define SIGNAL_LCDON_BY_POWERKEY	"LCDOnByPowerkey"
#define SIGNAL_LCDOFF_BY_POWERKEY	"LCDOffByPowerkey"

#define TOUCH_RELEASE		(-1)

#define GLOVE_MODE	1

int __WEAK__ get_glove_state(void);
void __WEAK__ switch_glove_key(int val);

static struct timeval pressed_time;
static Ecore_Timer *longkey_timeout_id = NULL;
static Ecore_Timer *combination_timeout_id = NULL;
static int cancel_lcdoff;
static int key_combination = KEY_COMBINATION_STOP;
static int menu_pressed = false;
static bool touch_pressed = false;
static int skip_lcd_off = false;
static bool powerkey_pressed = false;
static const struct device_ops *touchled;

static inline int current_state_in_on(void)
{
	return (pm_cur_state == S_LCDDIM || pm_cur_state == S_NORMAL);
}

static inline void restore_custom_brightness(void)
{
	if (pm_cur_state == S_LCDDIM &&
	    backlight_ops.get_custom_status())
		backlight_ops.custom_update();
}

static int power_execute(void *data)
{
	static const struct device_ops *ops = NULL;

	FIND_DEVICE_INT(ops, POWER_OPS_NAME);

	return ops->execute(data);
}

static void longkey_pressed()
{
	char *opt;
	unsigned int caps;

	_I("Power key long pressed!");
	cancel_lcdoff = 1;

	caps = display_get_caps(DISPLAY_ACTOR_POWER_KEY);

	if (display_has_caps(caps, DISPLAY_CAPA_LCDON)) {
		/* change state - LCD on */
		pm_change_internal(getpid(), LCD_NORMAL);
		(*pm_callback)(INPUT_POLL_EVENT, NULL);
	}

	if (!display_has_caps(caps, DISPLAY_CAPA_LCDOFF)) {
		_D("No poweroff capability!");
		return;
	}

	opt = PWROFF_POPUP_ACT;
	power_execute(opt);
}

static Eina_Bool longkey_pressed_cb(void *data)
{
	longkey_pressed();
	longkey_timeout_id = NULL;

	return EINA_FALSE;
}

static Eina_Bool combination_failed_cb(void *data)
{
	key_combination = KEY_COMBINATION_STOP;
	combination_timeout_id = NULL;

	return EINA_FALSE;
}

static unsigned long timediff_usec(struct timeval t1, struct timeval t2)
{
	unsigned long udiff;

	udiff = (t2.tv_sec - t1.tv_sec) * USEC_PER_SEC;
	udiff += (t2.tv_usec - t1.tv_usec);

	return udiff;
}

static void stop_key_combination(void)
{
	key_combination = KEY_COMBINATION_STOP;
	if (combination_timeout_id > 0) {
		ecore_timer_del(combination_timeout_id);
		combination_timeout_id = NULL;
	}
}

static inline void check_key_pair(int code, int new, int *old)
{
	if (new == *old)
		_E("key pair is not matched! (%d, %d)", code, new);
	else
		*old = new;
}

static inline void broadcast_lcdon_by_powerkey(void)
{
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_LCDON_BY_POWERKEY, NULL, NULL, false);
}

static inline void broadcast_lcdoff_by_powerkey(void)
{
	broadcast_edbus_signal(DEVICED_PATH_DISPLAY, DEVICED_INTERFACE_DISPLAY,
	    SIGNAL_LCDOFF_BY_POWERKEY, NULL, NULL, false);
}

static inline bool switch_on_lcd(void)
{
	if (current_state_in_on())
		return false;

	if (backlight_ops.get_lcd_power() == DPMS_ON)
		return false;

	broadcast_lcdon_by_powerkey();

	lcd_on_direct(LCD_ON_BY_POWER_KEY);

	return true;
}

static inline void switch_off_lcd(void)
{
	if (!current_state_in_on())
		return;

	if (backlight_ops.get_lcd_power() == DPMS_OFF)
		return;

	broadcast_lcdoff_by_powerkey();

	lcd_off_procedure();
}

static void process_combination_key(struct input_event *pinput)
{
	if (pinput->value == KEY_PRESSED) {
		if (key_combination == KEY_COMBINATION_STOP) {
			key_combination = KEY_COMBINATION_START;
			combination_timeout_id = ecore_timer_add(
			    COMBINATION_INTERVAL,
			    (Ecore_Task_Cb)combination_failed_cb, NULL);
		} else if (key_combination == KEY_COMBINATION_START) {
			if (combination_timeout_id > 0) {
				ecore_timer_del(combination_timeout_id);
				combination_timeout_id = NULL;
			}
			if (longkey_timeout_id > 0) {
				ecore_timer_del(longkey_timeout_id);
				longkey_timeout_id = NULL;
			}
			_I("capture mode");
			key_combination = KEY_COMBINATION_SCREENCAPTURE;
			skip_lcd_off = true;
		}
		menu_pressed = true;
	} else if (pinput->value == KEY_RELEASED) {
		if (key_combination != KEY_COMBINATION_SCREENCAPTURE)
			stop_key_combination();
		menu_pressed = false;
	}
}


static int process_menu_key(struct input_event *pinput)
{
	int caps;

	caps = display_get_caps(DISPLAY_ACTOR_MENU_KEY);

	if (!display_has_caps(caps, DISPLAY_CAPA_LCDON)) {
		if (current_state_in_on()) {
			process_combination_key(pinput);
			return false;
		}
		_D("No lcd-on capability!");
		return true;
	} else if (pinput->value == KEY_PRESSED) {
		switch_on_lcd();
	}

	process_combination_key(pinput);

	return false;
}

static int decide_lcdoff(void)
{
	/* It's not needed if it's already LCD off state */
	if (!current_state_in_on() &&
	    backlight_ops.get_lcd_power() != DPMS_ON)
		return false;

	/*
	 * This flag is set at the moment
	 * that LCD is turned on by power key
	 * LCD has not to turned off in the situation.
	 */
	if (skip_lcd_off)
		return false;

	/* LCD is not turned off when powerkey is pressed,not released */
	if (powerkey_pressed)
		return false;

	/* LCD-off is blocked at the moment poweroff popup shows */
	if (cancel_lcdoff)
		return false;

	/* LCD-off is blocked at the moment volumedown key is pressed */
	if (menu_pressed)
		return false;

	/* LCD-off is blocked when powerkey and volmedown key are pressed */
	if (key_combination == KEY_COMBINATION_SCREENCAPTURE)
		return false;

	return true;
}

static int lcdoff_powerkey(void)
{
	int ignore = true;

	if (decide_lcdoff() == true) {
		check_processes(S_NORMAL);
		check_processes(S_LCDDIM);

		if (!check_holdkey_block(S_NORMAL) &&
		    !check_holdkey_block(S_LCDDIM)) {
			if (display_info.update_auto_brightness)
				display_info.update_auto_brightness(false);
			switch_off_lcd();
			delete_condition(S_NORMAL);
			delete_condition(S_LCDDIM);
			update_lcdoff_source(VCONFKEY_PM_LCDOFF_BY_POWERKEY);
			pm_change_internal(getpid(), LCD_OFF);
		}
	} else {
		ignore = false;
	}
	cancel_lcdoff = 0;

	return ignore;
}

static int process_power_key(struct input_event *pinput)
{
	int ignore = true;
	static int value = KEY_RELEASED;
	unsigned int caps;

	caps = display_get_caps(DISPLAY_ACTOR_POWER_KEY);

	switch (pinput->value) {
	case KEY_RELEASED:
		powerkey_pressed = false;
		check_key_pair(pinput->code, pinput->value, &value);

		if (!display_conf.powerkey_doublepress) {
			if (display_has_caps(caps, DISPLAY_CAPA_LCDOFF))
				ignore = lcdoff_powerkey();
			else
				_D("No lcdoff capability!");
		} else if (skip_lcd_off) {
			ignore = false;
		}

		if (!display_has_caps(caps, DISPLAY_CAPA_LCDON))
			ignore = true;

		stop_key_combination();
		if (longkey_timeout_id > 0) {
			ecore_timer_del(longkey_timeout_id);
			longkey_timeout_id = NULL;
		}

		break;
	case KEY_PRESSED:
		powerkey_pressed = true;
		if (display_has_caps(caps, DISPLAY_CAPA_LCDON)) {
			skip_lcd_off = switch_on_lcd();
		} else {
			_D("No lcdon capability!");
			skip_lcd_off = false;
		}
		check_key_pair(pinput->code, pinput->value, &value);
		_I("power key pressed");
		pressed_time.tv_sec = (pinput->time).tv_sec;
		pressed_time.tv_usec = (pinput->time).tv_usec;
		if (key_combination == KEY_COMBINATION_STOP) {
			/* add long key timer */
			longkey_timeout_id = ecore_timer_add(
				    display_conf.longpress_interval,
				    (Ecore_Task_Cb)longkey_pressed_cb, NULL);
			key_combination = KEY_COMBINATION_START;
			combination_timeout_id = ecore_timer_add(
				    COMBINATION_INTERVAL,
				    (Ecore_Task_Cb)combination_failed_cb, NULL);
		} else if (key_combination == KEY_COMBINATION_START) {
			if (combination_timeout_id > 0) {
				ecore_timer_del(combination_timeout_id);
				combination_timeout_id = NULL;
			}
			_I("capture mode");
			key_combination = KEY_COMBINATION_SCREENCAPTURE;
			skip_lcd_off = true;
			ignore = false;
		}
		if (skip_lcd_off)
			ignore = false;
		cancel_lcdoff = 0;

		break;
	case KEY_BEING_PRESSED:
		if (timediff_usec(pressed_time, pinput->time) >
		    (display_conf.longpress_interval * USEC_PER_SEC))
			longkey_pressed();
		break;
	}
	return ignore;
}

static int process_screenlock_key(struct input_event *pinput)
{
	if (pinput->value != KEY_RELEASED) {
		stop_key_combination();
		return true;
	}

	if (!current_state_in_on())
		return false;

	check_processes(S_NORMAL);
	check_processes(S_LCDDIM);

	if (!check_holdkey_block(S_NORMAL) && !check_holdkey_block(S_LCDDIM)) {
		delete_condition(S_NORMAL);
		delete_condition(S_LCDDIM);
		update_lcdoff_source(VCONFKEY_PM_LCDOFF_BY_POWERKEY);

		/* LCD off forcly */
		pm_change_internal(-1, LCD_OFF);
	}

	return true;
}

static void sound_vibrate_hardkey(void)
{
	/* device notify(vibrator) */
	device_notify(DEVICE_NOTIFIER_TOUCH_HARDKEY, NULL);
	/* sound(dbus) */
	broadcast_edbus_signal(DEVICED_PATH_KEY, DEVICED_INTERFACE_KEY,
			SIGNAL_CHANGE_HARDKEY, NULL, NULL, false);
}

static void process_hardkey_backlight(struct input_event *pinput)
{
	int opt;

	_E("pinput->value : %d", pinput->value);
	if (pinput->value == KEY_PRESSED) {
		if (touch_pressed) {
			_I("Touch is pressed, then hard key is not working!");
			return;
		}
		/* Sound & Vibrate only in unlock state */
		if (get_lock_screen_state() == VCONFKEY_IDLE_UNLOCK
		    || get_lock_screen_bg_state())
			sound_vibrate_hardkey();

		if (touchled && touchled->execute) {
			opt = TOUCHLED_PRESS;
			touchled->execute(&opt);
		}
	} else if (pinput->value == KEY_RELEASED) {
		/* if lockscreen is idle lock */
		if (get_lock_screen_state() == VCONFKEY_IDLE_LOCK) {
			_D("Lock state, key backlight is off when phone is unlocked!");
			return;
		}

		if (touchled && touchled->execute) {
			opt = TOUCHLED_RELEASE;
			touchled->execute(&opt);
		}
	}
}

static int check_key(struct input_event *pinput, int fd)
{
	int ignore = true;

	switch (pinput->code) {
	case KEY_MENU:
		ignore = process_menu_key(pinput);
		break;
	case KEY_POWER:
		ignore = process_power_key(pinput);
		break;
	case KEY_SCREENLOCK:
		ignore = process_screenlock_key(pinput);
		break;
	case KEY_BACK:
	case KEY_PHONE:
		stop_key_combination();
		if (current_state_in_on()) {
			process_hardkey_backlight(pinput);
			ignore = false;
		}
		break;
	case KEY_VOLUMEUP:
	case KEY_VOLUMEDOWN:
	case KEY_CAMERA:
	case KEY_EXIT:
	case KEY_CONFIG:
	case KEY_MEDIA:
	case KEY_MUTE:
	case KEY_PLAYPAUSE:
	case KEY_PLAYCD:
	case KEY_PAUSECD:
	case KEY_STOPCD:
	case KEY_NEXTSONG:
	case KEY_PREVIOUSSONG:
	case KEY_REWIND:
	case KEY_FASTFORWARD:
		stop_key_combination();
		if (current_state_in_on())
			ignore = false;
		break;
	case 0x1DB:
	case 0x1DC:
	case 0x1DD:
	case 0x1DE:
		stop_key_combination();
		break;
	default:
		stop_key_combination();
		ignore = false;
	}
#ifdef ENABLE_PM_LOG
	if (pinput->value == KEY_PRESSED)
		pm_history_save(PM_LOG_KEY_PRESS, pinput->code);
	else if (pinput->value == KEY_RELEASED)
		pm_history_save(PM_LOG_KEY_RELEASE, pinput->code);
#endif
	return ignore;
}

static int check_key_filter(void *data, int fd)
{
	struct input_event *pinput = data;
	int ignore = true;
	static int old_fd, code, value;

	assert(pinput);

	switch (pinput->type) {
	case EV_KEY:
		if (pinput->code == BTN_TOUCH &&
			pinput->value == KEY_RELEASED)
			touch_pressed = false;
		/*
		 * Normally, touch press/release events don't occur
		 * in lcd off state. But touch release events can occur
		 * in the state abnormally. Then touch events are ignored
		 * when lcd is off state.
		 */
		if (pinput->code == BTN_TOUCH && !current_state_in_on())
			break;
		if (pinput->code == code && pinput->value == value) {
			_E("Same key(%d, %d) is polled [%d,%d]",
				code, value, old_fd, fd);
		}
		old_fd = fd;
		code = pinput->code;
		value = pinput->value;

		ignore = check_key(pinput, fd);
		restore_custom_brightness();

		break;
	case EV_REL:
		ignore = false;
		break;
	case EV_ABS:
		if (current_state_in_on())
			ignore = false;
		restore_custom_brightness();

		touch_pressed =
			(pinput->value == TOUCH_RELEASE ? false : true);
		break;
	case EV_SW:
		if (!get_glove_state || !switch_glove_key)
			break;
		if (pinput->code == SW_GLOVE &&
			get_glove_state() == GLOVE_MODE) {
			switch_glove_key(pinput->value);
		}
		break;
	}

	if (ignore)
		return 1;

	return 0;
}

/*
 * Default capability
 * powerkey := LCDON | LCDOFF | POWEROFF
 * homekey  := LCDON
 */
static struct display_actor_ops display_powerkey_actor = {
	.id	= DISPLAY_ACTOR_POWER_KEY,
	.caps	= DISPLAY_CAPA_LCDON |
		  DISPLAY_CAPA_LCDOFF |
		  DISPLAY_CAPA_POWEROFF,
};

static struct display_actor_ops display_menukey_actor = {
	.id	= DISPLAY_ACTOR_MENU_KEY,
	.caps	= DISPLAY_CAPA_LCDON,
};

static void keyfilter_init(void)
{
	display_add_actor(&display_powerkey_actor);
	display_add_actor(&display_menukey_actor);

	touchled = find_device(TOUCHLED_NAME);
}

static void key_backlight_enable(bool enable)
{
	int opt;

	if (!touchled || !touchled->execute)
		return;

	if (enable)
		opt = TOUCHLED_DIRECT_ON;
	else
		opt = TOUCHLED_DIRECT_OFF;

	touchled->execute(&opt);
}

static const struct display_keyfilter_ops normal_keyfilter_ops = {
	.init			= keyfilter_init,
	.check			= check_key_filter,
	.set_powerkey_ignore	= NULL,
	.powerkey_lcdoff	= NULL,
	.backlight_enable	= key_backlight_enable,
};
const struct display_keyfilter_ops *keyfilter_ops = &normal_keyfilter_ops;

