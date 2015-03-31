/*
 * deviced
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
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
 * @file	core.h
 * @brief	 Power manager main loop header file
 */
#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include "poll.h"
#include "device-interface.h"
#include "setting.h"

#define WITHOUT_STARTNOTI	0x1
#define MASK_BIT 0x7		/* 111 */
#define MASK_DIM 0x1		/* 001 */
#define MASK_OFF 0x2		/* 010 */
#define MASK_SLP 0x4		/* 100 */

#define VCALL_FLAG		0x00000001
#define LOWBT_FLAG		0x00000100
#define CHRGR_FLAG		0x00000200
#define PWRSV_FLAG		0x00000400
#define SMAST_FLAG		0x00001000
#define BRTCH_FLAG		0x00002000
#define PWROFF_FLAG		0x00004000
#define DIMSTAY_FLAG		0x00008000

#define DEFAULT_NORMAL_TIMEOUT	30000
#define DEFAULT_DIM_TIMEOUT	5000
#define DEFAULT_OFF_TIMEOUT	1000

#define MASK32			0xffffffff

#define CHECK_OPS(d, op) (d != NULL && d->op != NULL)

#ifdef ENABLE_PM_LOG
#define MAX_LOG_COUNT 250

enum pm_log_type {
	PM_LOG_MIN = 0,
	PM_LOG_KEY_PRESS = PM_LOG_MIN,	/* key log */
	PM_LOG_KEY_LONG_PRESS,
	PM_LOG_KEY_RELEASE,
	PM_LOG_LCD_ON,			/* lcd log */
	PM_LOG_LCD_ON_FAIL,
	PM_LOG_LCD_DIM,
	PM_LOG_LCD_DIM_FAIL,
	PM_LOG_LCD_OFF,
	PM_LOG_LCD_OFF_FAIL,
	PM_LOG_SLEEP,
	PM_LOG_MAX
};

void pm_history_save(enum pm_log_type, int);
#endif

extern unsigned int pm_status_flag;

/*
 * State enumeration
 */
enum state_t {
	S_START = 0,
	S_NORMAL,		/*< normal state */
	S_LCDDIM,		/*< LCD dimming */
	S_LCDOFF,		/*< LCD off */
	S_SLEEP,		/*< system suspend */
	S_END
};

/*
 * Global variables
 *   pm_cur_state   : current state
 *   states      : state definitions
 *   trans_table : state transition table
 */
int pm_cur_state;
int pm_old_state;

/*
 * @brief State structure
 */
struct state {
	enum state_t state;					/**< state number */
	int (*trans) (int evt);		/**< transition function pointer */
	int (*action) (int timeout);	/**< enter action */
	int (*check) (int next);	/**< transition check function */
	int timeout;
} states[S_END];

/*
 * @brief Configuration structure
 */
struct display_config {
	double lock_wait_time;
	double longpress_interval;
	double lightsensor_interval;
	int lcdoff_timeout;
	int brightness_change_step;
	int lcd_always_on;
	int framerate_app[4];
	int control_display;
	int powerkey_doublepress;
	int alpm_on;
	int accel_sensor_on;
	int continuous_sampling;
};

/*
 * Global variables
 *   display_conf : configuration of display
 */
extern struct display_config display_conf;

/*
 * @brief Display Extension features
 */
struct display_function_info {
	void (*update_auto_brightness)(bool);
	int (*set_autobrightness_min)(int, char *);
	int (*reset_autobrightness_min)(char *, enum watch_id);
	int (*face_detection)(int, int, int);
};

extern struct display_function_info display_info;

struct display_keyfilter_ops {
	void (*init)(void);
	void (*exit)(void);
	int (*check)(void *, int);
	void (*set_powerkey_ignore)(int);
	int (*powerkey_lcdoff)(void);
	void (*backlight_enable)(bool);
};

extern const struct display_keyfilter_ops *keyfilter_ops;

/* If the bit in a condition variable is set,
 *  we cannot transit the state until clear this bit. */
int trans_condition;
pid_t idle_pid;
int check_processes(enum state_t prohibit_state);
extern struct state state[S_END];
int reset_lcd_timeout(char *name, enum watch_id id);
int check_lcdoff_lock_state(void);
/**
 * @}
 */

#endif
