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


#ifndef __DD_DISPLAY_H__
#define __DD_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-display.h
 * @ingroup     DEVICED_LIBRARY
 * @brief       This file provides for control of display
 */

#define LCD_NORMAL	0x1   /**< NORMAL state */
#define LCD_DIM		0x2  /**< LCD dimming state */
#define LCD_OFF		0x4  /**< LCD off state */
#define SUSPEND		0x8  /**< Sleep state */
#define POWER_OFF	0x16  /**< Sleep state */
#define SETALL (LCD_DIM | LCD_OFF | LCD_NORMAL)	/*< select all state - not supported yet */

/* parameters for display_lock_state() */
#define STAY_CUR_STATE	0x1
#define GOTO_STATE_NOW	0x2
#define HOLD_KEY_BLOCK	0x4

/* paramters for display_unlock_state() */
#define PM_SLEEP_MARGIN	0x0	/**< keep guard time for unlock */
#define PM_RESET_TIMER	0x1	/**< reset timer for unlock */
#define PM_KEEP_TIMER	0x2	/**< keep timer for unlock */

int display_get_count(void);
int display_get_max_brightness(void);
int display_get_min_brightness(void);
int display_get_brightness(void);
int display_set_brightness_with_setting(int val);
int display_set_brightness(int val);
int display_release_brightness(void);
int display_get_acl_status(void);
int display_set_acl_status(int val);
int display_lock_state(unsigned int, unsigned int, unsigned int);
int display_unlock_state(unsigned int, unsigned int);
int display_change_state(unsigned int);

#ifdef __cplusplus
}
#endif
#endif
