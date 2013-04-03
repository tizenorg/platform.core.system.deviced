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


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "util.h"
#include "llinterface.h"
#include "conf.h"
#include "vconf.h"
#include "core.h"
#include "device-node.h"

#define DISP_INDEX_BIT		4
#define COMBINE_DISP_CMD(cmd, prop, index)	(cmd = (prop | (index << DISP_INDEX_BIT)))

typedef struct _PMSys PMSys;
struct _PMSys {
	int def_brt;
	int dim_brt;

	int (*sys_power_state) (PMSys *, int);
	int (*sys_get_lcd_power) (PMSys *);
	int (*bl_onoff) (PMSys *, int);
	int (*bl_brt) (PMSys *, int);
	int (*sys_get_battery_capacity) (PMSys *p);
	int (*sys_get_battery_capacity_raw) (PMSys *p);
	int (*sys_get_battery_charge_full) (PMSys *p);
};

static PMSys *pmsys;
struct _backlight_ops backlight_ops;
struct _power_ops power_ops;
struct _battery_ops battery_ops;

#ifdef ENABLE_X_LCD_ONOFF
#include "x-lcd-on.c"
static bool x_dpms_enable = false;
#endif

static int _bl_onoff(PMSys *p, int on)
{
	int cmd = 0;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_ONOFF, DEFAULT_DISPLAY);
	return device_set_property(DEVICE_TYPE_DISPLAY, cmd, on);
}

static int _bl_brt(PMSys *p, int brightness)
{
	int cmd;
	int ret;
	int old_brt;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &old_brt);

	/* Update new brightness to vconf */
	if (!ret && (brightness != old_brt))
		vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, brightness);

	/* Update device brightness */
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brightness);

	LOGERR("set brightness %d, %d", brightness, ret);

	return ret;
}

static int _sys_power_state(PMSys *p, int state)
{
	if (state < POWER_STATE_SUSPEND || state > POWER_STATE_POST_RESUME)
		return 0;
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_STATE, state);
}

static int _sys_get_lcd_power(PMSys *p)
{
	int value = -1;
	int ret = -1;
	int cmd = 0;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_ONOFF, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &value);

	if (ret < 0 || value < 0)
		return -1;

	return value;
}

static int _sys_get_battery_capacity(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY, &value);

	if(ret < 0)
		return -1;

	if(value < 0)
		return 0;

	return value;
}

static int _sys_get_battery_capacity_raw(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY_RAW,
	    &value);

	if(ret < 0)
		return -1;

	if(value < 0)
		return 0;

	return value;
}

static int _sys_get_battery_charge_full(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_FULL,
	    &value);

	if(ret < 0)
		return -1;

	if(value < 0)
		return 0;

	return value;
}

static void _init_bldev(PMSys *p, unsigned int flags)
{
	int ret;
	//_update_curbrt(p);
	p->bl_brt = _bl_brt;
	p->bl_onoff = _bl_onoff;
#ifdef ENABLE_X_LCD_ONOFF
	if (flags & FLAG_X_DPMS) {
		p->bl_onoff = pm_x_set_lcd_backlight;
		x_dpms_enable = true;
	}
#endif
}

static void _init_pmsys(PMSys *p)
{
	char tmp[NAME_MAX];

	get_env(EN_SYS_DIMBRT, tmp, sizeof(tmp));

	p->dim_brt = atoi(tmp);
	p->sys_power_state = _sys_power_state;
	p->sys_get_lcd_power = _sys_get_lcd_power;
	p->sys_get_battery_capacity = _sys_get_battery_capacity;
	p->sys_get_battery_capacity_raw = _sys_get_battery_capacity_raw;
	p->sys_get_battery_charge_full = _sys_get_battery_charge_full;
}

static void *_system_suspend_cb(void *data)
{
	LOGINFO("enter system suspend");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_SUSPEND);
	return 0;
}

static int system_suspend(void)
{
	pthread_t pth;
	int ret;

	ret = pthread_create(&pth, 0, _system_suspend_cb, (void*)NULL);
	if (ret < 0) {
		LOGERR("pthread creation failed!, suspend directly!");
		_system_suspend_cb((void*)NULL);
	} else {
		pthread_join(pth, NULL);
	}

	return 0;
}

static int system_pre_suspend(void)
{
	LOGINFO("enter system pre suspend");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_PRE_SUSPEND);

	return 0;
}

static int system_post_resume(void)
{
	LOGINFO("enter system post resume");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_POST_RESUME);

	return 0;
}

static int battery_capacity(void)
{
	if (pmsys && pmsys->sys_get_battery_capacity)
		return pmsys->sys_get_battery_capacity(pmsys);

	return 0;
}

static int battery_capacity_raw(void)
{
	if (pmsys && pmsys->sys_get_battery_capacity_raw)
		return pmsys->sys_get_battery_capacity_raw(pmsys);

	return 0;
}

static int battery_charge_full(void)
{
	if (pmsys && pmsys->sys_get_battery_charge_full)
		return pmsys->sys_get_battery_charge_full(pmsys);

	return 0;
}

static int get_lcd_power(void)
{
	if (pmsys && pmsys->sys_get_lcd_power) {
		return pmsys->sys_get_lcd_power(pmsys);
	}

	return -1;
}

static int backlight_on(void)
{
	int ret = -1;
	int i;

	LOGINFO("LCD on");

	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
		ret = pmsys->bl_onoff(pmsys, STATUS_ON);
		if (get_lcd_power() == PM_LCD_POWER_ON) {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_ON, pm_cur_state);
#endif
			break;
		} else {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_ON_FAIL, pm_cur_state);
#endif
#ifdef ENABLE_X_LCD_ONOFF
			LOGERR("Failed to LCD on, through xset");
#else
			LOGERR("Failed to LCD on, through OAL");
#endif
			ret = -1;
		}
	}
	return ret;
}

static int backlight_off(void)
{
	int ret = -1;
	int i;

	LOGINFO("LCD off");

	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
#ifdef ENABLE_X_LCD_ONOFF
		if (x_dpms_enable == false)
#endif
			usleep(30000);
		ret = pmsys->bl_onoff(pmsys, STATUS_OFF);
		if (get_lcd_power() == PM_LCD_POWER_OFF) {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_OFF, pm_cur_state);
#endif
			break;
		} else {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_OFF_FAIL, pm_cur_state);
#endif
#ifdef ENABLE_X_LCD_ONOFF
			LOGERR("Failed to LCD off, through xset");
#else
			LOGERR("Failed to LCD off, through OAL");
#endif
			ret = -1;
		}
	}
	return ret;
}

static int backlight_dim(void)
{
	int ret = 0;
	if (pmsys && pmsys->bl_brt) {
		ret = pmsys->bl_brt(pmsys, pmsys->dim_brt);
#ifdef ENABLE_PM_LOG
		if (!ret)
			pm_history_save(PM_LOG_LCD_DIM, pm_cur_state);
		else
			pm_history_save(PM_LOG_LCD_DIM_FAIL, pm_cur_state);
#endif
	}
	return ret;
}

static int backlight_restore(void)
{
	int ret = 0;
	int val = -1;

	ret = vconf_get_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, &val);
	if (ret == 0 && val == VCONFKEY_PM_CUSTOM_BRIGHTNESS_ON) {
		LOGINFO("custom brightness mode! brt no restored");
		return 0;
	}
	if ((pm_status_flag & PWRSV_FLAG) && !(pm_status_flag & BRTCH_FLAG)) {
		ret = backlight_dim();
	} else if (pmsys && pmsys->bl_brt) {
		ret = pmsys->bl_brt(pmsys, pmsys->def_brt);
	}
	return ret;
}

static int set_default_brt(int level)
{
	if (level < PM_MIN_BRIGHTNESS || level > PM_MAX_BRIGHTNESS)
		level = PM_DEFAULT_BRIGHTNESS;
	pmsys->def_brt = level;

	return 0;
}

static int check_wakeup_src(void)
{
	/*  TODO if nedded.
	 * return wackeup source. user input or device interrupts? (EVENT_DEVICE or EVENT_INPUT)
	 */
	return EVENT_DEVICE;
}

void _init_ops(void)
{
	backlight_ops.off = backlight_off;
	backlight_ops.dim = backlight_dim;
	backlight_ops.on = backlight_on;
	backlight_ops.restore = backlight_restore;
	backlight_ops.set_default_brt = set_default_brt;
	backlight_ops.get_lcd_power = get_lcd_power;

	power_ops.suspend = system_suspend;
	power_ops.pre_suspend = system_pre_suspend;
	power_ops.post_resume = system_post_resume;
	power_ops.check_wakeup_src = check_wakeup_src;

	battery_ops.get_capacity = battery_capacity;
	battery_ops.get_capacity_raw = battery_capacity_raw;
	battery_ops.get_charge_full = battery_charge_full;
}

int init_sysfs(unsigned int flags)
{
	int ret;

	pmsys = (PMSys *) malloc(sizeof(PMSys));
	if (pmsys == NULL) {
		LOGERR("Not enough memory to alloc PM Sys");
		return -1;
	}

	memset(pmsys, 0x0, sizeof(PMSys));

	_init_pmsys(pmsys);
	_init_bldev(pmsys, flags);

	if (pmsys->bl_onoff == NULL || pmsys->sys_power_state == NULL) {
		LOGERR
		    ("We have no managable resource to reduce the power consumption");
		return -1;
	}

	_init_ops();

	return 0;
}

int exit_sysfs(void)
{
	int fd;

	fd = open("/tmp/sem.pixmap_1", O_RDONLY);
	if (fd == -1) {
		LOGERR("X server disable");
		backlight_on();
	}

	backlight_restore();
	free(pmsys);
	pmsys = NULL;
	if(fd != -1)
		close(fd);

	return 0;
}
