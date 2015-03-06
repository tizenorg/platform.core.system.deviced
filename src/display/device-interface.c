/*
 * deviced
 *
 * Copyright (c) 2012 - 2015 Samsung Electronics Co., Ltd.
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
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <hw/display.h>

#include "core/log.h"
#include "core/devices.h"
#include "util.h"
#include "device-interface.h"
#include "vconf.h"
#include "core.h"
#include "device-node.h"

#define TOUCH_ON	1
#define TOUCH_OFF	0

#define LCD_PHASED_MIN_BRIGHTNESS	1
#define LCD_PHASED_MAX_BRIGHTNESS	100
#define LCD_PHASED_CHANGE_STEP		5
#define LCD_PHASED_DELAY		35000 /* microsecond */

#define POWER_LOCK_PATH		"/sys/power/wake_lock"
#define POWER_UNLOCK_PATH	"/sys/power/wake_unlock"

enum {
	POWER_UNLOCK = 0,
	POWER_LOCK,
};

typedef struct _PMSys PMSys;
struct _PMSys {
	int def_brt;
	int dim_brt;

	int (*sys_power_state) (PMSys *, int);
	int (*sys_power_lock) (PMSys *, int);
	int (*sys_get_power_lock_support) (PMSys *);
	int (*sys_get_lcd_power) (PMSys *);
	int (*bl_onoff) (PMSys *, int);
	int (*bl_brt) (PMSys *, int, int);
};

static PMSys *pmsys;
struct _backlight_ops backlight_ops;
struct _power_ops power_ops;

#ifdef ENABLE_X_LCD_ONOFF
#include "x-lcd-on.c"
static bool x_dpms_enable = false;
#endif

static int power_lock_support = -1;
static bool custom_status = false;
static int custom_brightness = 0;
static int force_brightness = 0;

static struct display_device *display_dev;

static int _bl_onoff(PMSys *p, int on)
{
	if (!display_dev || !display_dev->set_state) {
		_E("there is no display device");
		return -ENOENT;
	}

	return display_dev->set_state(on);
}

static int _bl_brt(PMSys *p, int brightness, int delay)
{
	int ret = -1;
	int prev;

	if (!display_dev ||
	    !display_dev->get_brightness ||
	    !display_dev->set_brightness) {
		_E("there is no display device");
		return -ENOENT;
	}

	if (delay > 0)
		usleep(delay);

	if (force_brightness > 0 && brightness != p->dim_brt) {
		_I("brightness(%d), force brightness(%d)",
		    brightness, force_brightness);
		brightness = force_brightness;
	}

	ret = display_dev->get_brightness(&prev);

	/* Update new brightness to vconf */
	if (!ret && (brightness != prev)) {
		vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, brightness);
	}

	/* Update device brightness */
	ret = display_dev->set_brightness(brightness);

	_I("set brightness %d, %d", brightness, ret);

	return ret;
}

static int _sys_power_state(PMSys *p, int state)
{
	if (state < POWER_STATE_SUSPEND || state > POWER_STATE_POST_RESUME)
		return 0;
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_STATE, state);
}

static int _sys_power_lock(PMSys *p, int state)
{
	if (state == POWER_LOCK)
		return sys_set_str(POWER_LOCK_PATH, "mainlock");
	else if (state == POWER_UNLOCK)
		return sys_set_str(POWER_UNLOCK_PATH, "mainlock");
	else
		return -EINVAL;
}

static int _sys_get_power_lock_support(PMSys *p)
{
	int value = 0;
	int ret;

	ret = sys_check_node(POWER_LOCK_PATH);
	if (ret < 0)
		return 0;
	return 1;
}

static int _sys_get_lcd_power(PMSys *p)
{
	enum display_state state;
	int ret;

	if (!display_dev || !display_dev->get_state) {
		_E("there is no display device");
		return -ENOENT;
	}

	ret = display_dev->get_state(&state);
	if (ret < 0)
		return ret;

	return state;
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
	char *val;

	val = getenv("PM_SYS_DIMBRT");
	p->dim_brt = (val ? atoi(val) : 0);
	p->sys_power_state = _sys_power_state;
	p->sys_power_lock = _sys_power_lock;
	p->sys_get_power_lock_support = _sys_get_power_lock_support;
	p->sys_get_lcd_power = _sys_get_lcd_power;
}

static void *_system_suspend_cb(void *data)
{
	int ret;

	_I("enter system suspend");
	if (pmsys && pmsys->sys_power_state)
		ret = pmsys->sys_power_state(pmsys, POWER_STATE_SUSPEND);
	else
		ret = -EFAULT;

	if (ret < 0)
		_E("Failed to system suspend! %d", ret);

	return NULL;
}

static int system_suspend(void)
{
	pthread_t pth;
	int ret;

	ret = pthread_create(&pth, 0, _system_suspend_cb, (void*)NULL);
	if (ret < 0) {
		_E("pthread creation failed!, suspend directly!");
		_system_suspend_cb((void*)NULL);
	} else {
		pthread_join(pth, NULL);
	}

	return 0;
}

static int system_pre_suspend(void)
{
	_I("enter system pre suspend");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_PRE_SUSPEND);

	return 0;
}

static int system_post_resume(void)
{
	_I("enter system post resume");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_POST_RESUME);

	return 0;
}

static int system_power_lock(void)
{
	_I("system power lock");
	if (pmsys && pmsys->sys_power_lock)
		return pmsys->sys_power_lock(pmsys, POWER_LOCK);

	return 0;
}

static int system_power_unlock(void)
{
	_I("system power unlock");
	if (pmsys && pmsys->sys_power_lock)
		return pmsys->sys_power_lock(pmsys, POWER_UNLOCK);

	return 0;
}

static int system_get_power_lock_support(void)
{
	int value = -1;

	if (power_lock_support == -1) {
		if (pmsys && pmsys->sys_get_power_lock_support) {
			value = pmsys->sys_get_power_lock_support(pmsys);
			if (value == 1) {
					_I("system power lock : support");
					power_lock_support = 1;
			} else {
				_E("system power lock : not support");
				power_lock_support = 0;
			}
		} else {
			_E("system power lock : read fail");
			power_lock_support = 0;
		}
	}

	return power_lock_support;
}

static int get_lcd_power(void)
{
	if (pmsys && pmsys->sys_get_lcd_power) {
		return pmsys->sys_get_lcd_power(pmsys);
	}

	return -1;
}

void change_brightness(int start, int end, int step)
{
	int diff, val;
	int ret = -1;
	int prev;

	if (!display_dev ||
	    !display_dev->get_brightness) {
		_E("there is no display device");
		return;
	}

	if ((pm_status_flag & PWRSV_FLAG) &&
	    !(pm_status_flag & BRTCH_FLAG))
		return;

	ret = display_dev->get_brightness(&prev);
	if (ret < 0) {
		_E("fail to get brightness : %d", ret);
		return;
	}

	if (prev == end)
		return;

	_D("start %d end %d step %d", start, end, step);

	diff = end - start;

	if (abs(diff) < step)
		val = (diff > 0 ? 1 : -1);
	else
		val = (int)ceil(diff / step);

	while (start != end) {
		if (val == 0) break;

		start += val;
		if ((val > 0 && start > end) ||
		    (val < 0 && start < end))
			start = end;

		pmsys->bl_brt(pmsys, start, LCD_PHASED_DELAY);
	}
}

static int backlight_on(enum device_flags flags)
{
	int ret = -1;
	int i;

	_D("LCD on %x", flags);

	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
		ret = pmsys->bl_onoff(pmsys, DISPLAY_ON);
		if (get_lcd_power() == DISPLAY_ON) {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_ON, pm_cur_state);
#endif
			break;
		} else {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_ON_FAIL, pm_cur_state);
#endif
#ifdef ENABLE_X_LCD_ONOFF
			_E("Failed to LCD on, through xset");
#else
			_E("Failed to LCD on, through OAL");
#endif
			ret = -1;
		}
	}

	if (flags & LCD_PHASED_TRANSIT_MODE)
		change_brightness(LCD_PHASED_MIN_BRIGHTNESS,
		    pmsys->def_brt, LCD_PHASED_CHANGE_STEP);

	return ret;
}

static int backlight_off(enum device_flags flags)
{
	int ret = -1;
	int i;

	_D("LCD off %x", flags);

	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	if (flags & LCD_PHASED_TRANSIT_MODE)
		change_brightness(pmsys->def_brt,
		    LCD_PHASED_MIN_BRIGHTNESS, LCD_PHASED_CHANGE_STEP);

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
#ifdef ENABLE_X_LCD_ONOFF
		if (x_dpms_enable == false)
#endif
			usleep(30000);
		ret = pmsys->bl_onoff(pmsys, DISPLAY_OFF);
		if (get_lcd_power() == DISPLAY_OFF) {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_OFF, pm_cur_state);
#endif
			break;
		} else {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_OFF_FAIL, pm_cur_state);
#endif
#ifdef ENABLE_X_LCD_ONOFF
			_E("Failed to LCD off, through xset");
#else
			_E("Failed to LCD off, through OAL");
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
		ret = pmsys->bl_brt(pmsys, pmsys->dim_brt, 0);
#ifdef ENABLE_PM_LOG
		if (!ret)
			pm_history_save(PM_LOG_LCD_DIM, pm_cur_state);
		else
			pm_history_save(PM_LOG_LCD_DIM_FAIL, pm_cur_state);
#endif
	}
	return ret;
}

static int set_custom_status(bool on)
{
	custom_status = on;
	return 0;
}

static bool get_custom_status(void)
{
	return custom_status;
}

static int save_custom_brightness(void)
{
	int ret, brightness;

	if (!display_dev ||
	    !display_dev->get_brightness) {
		_E("there is no display device");
		return -ENOENT;
	}

	ret = display_dev->get_brightness(&brightness);

	custom_brightness = brightness;

	return ret;
}

static int custom_backlight_update(void)
{
	int ret = 0;

	if (custom_brightness < PM_MIN_BRIGHTNESS ||
	    custom_brightness > PM_MAX_BRIGHTNESS)
		return -EINVAL;

	if ((pm_status_flag & PWRSV_FLAG) && !(pm_status_flag & BRTCH_FLAG)) {
		ret = backlight_dim();
	} else if (pmsys && pmsys->bl_brt) {
		_I("custom brightness restored! %d", custom_brightness);
		ret = pmsys->bl_brt(pmsys, custom_brightness, 0);
	}

	return ret;
}

static int set_force_brightness(int level)
{
	if (level < 0 ||  level > PM_MAX_BRIGHTNESS)
		return -EINVAL;

	force_brightness = level;

	return 0;
}

static int backlight_update(void)
{
	int ret = 0;

	if (get_custom_status()) {
		_I("custom brightness mode! brt no updated");
		return 0;
	}
	if ((pm_status_flag & PWRSV_FLAG) && !(pm_status_flag & BRTCH_FLAG)) {
		ret = backlight_dim();
	} else if (pmsys && pmsys->bl_brt) {
		ret = pmsys->bl_brt(pmsys, pmsys->def_brt, 0);
	}
	return ret;
}

static int backlight_standby(int force)
{
	int ret = -1;
	if (!pmsys || !pmsys->bl_onoff)
		return -1;

	if ((get_lcd_power() == DISPLAY_ON) || force) {
		_I("LCD standby");
		ret = pmsys->bl_onoff(pmsys, DISPLAY_STANDBY);
	}

	return ret;
}

static int set_default_brt(int level)
{
	if (!pmsys)
		return -EFAULT;

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

static int set_brightness(int val)
{
	if (!display_dev || !display_dev->set_brightness) {
		_E("there is no display device");
		return -ENOENT;
	}

	return display_dev->set_brightness(val);
}

static int get_brightness(int *val)
{
	if (!display_dev || !display_dev->get_brightness) {
		_E("there is no display device");
		return -ENOENT;
	}

	return display_dev->get_brightness(val);
}

void _init_ops(void)
{
	backlight_ops.off = backlight_off;
	backlight_ops.dim = backlight_dim;
	backlight_ops.on = backlight_on;
	backlight_ops.update = backlight_update;
	backlight_ops.standby = backlight_standby;
	backlight_ops.set_default_brt = set_default_brt;
	backlight_ops.get_lcd_power = get_lcd_power;
	backlight_ops.set_custom_status = set_custom_status;
	backlight_ops.get_custom_status = get_custom_status;
	backlight_ops.save_custom_brightness = save_custom_brightness;
	backlight_ops.custom_update = custom_backlight_update;
	backlight_ops.set_force_brightness = set_force_brightness;
	backlight_ops.set_brightness = set_brightness;
	backlight_ops.get_brightness = get_brightness;

	power_ops.suspend = system_suspend;
	power_ops.pre_suspend = system_pre_suspend;
	power_ops.post_resume = system_post_resume;
	power_ops.power_lock = system_power_lock;
	power_ops.power_unlock = system_power_unlock;
	power_ops.get_power_lock_support = system_get_power_lock_support;
	power_ops.check_wakeup_src = check_wakeup_src;
}

int display_service_load(void)
{
	struct hw_info *info;
	int r;

	r = hw_get_info(DISPLAY_HARDWARE_DEVICE_ID,
			(const struct hw_info **)&info);
	if (r < 0) {
		_E("fail to load display shared library : %d", r);
		return -ENOENT;
	}

	if (!info->open) {
		_E("fail to open display device : open(NULL)");
		return -EPERM;
	}

	r = info->open(info, NULL, (struct hw_common **)&display_dev);
	if (r < 0) {
		_E("fail to get display device structure : %d", r);
		return -EPERM;
	}

	_D("display device structure load success");
	return 0;
}

int display_service_free(void)
{
	struct hw_info *info;

	if (!display_dev)
		return -ENOENT;

	info = display_dev->common.info;

	assert(info);

	info->close((struct hw_common *)display_dev);

	return 0;
}

int init_sysfs(unsigned int flags)
{
	int ret;

	pmsys = (PMSys *) malloc(sizeof(PMSys));
	if (pmsys == NULL) {
		_E("Not enough memory to alloc PM Sys");
		return -1;
	}

	memset(pmsys, 0x0, sizeof(PMSys));

	_init_pmsys(pmsys);
	_init_bldev(pmsys, flags);

	if (pmsys->bl_onoff == NULL || pmsys->sys_power_state == NULL) {
		_E("We have no managable resource to reduce the power consumption");
		return -1;
	}

	_init_ops();

	return 0;
}

int exit_sysfs(void)
{
	int fd;
	const struct device_ops *ops = NULL;

	fd = open("/tmp/sem.pixmap_1", O_RDONLY);
	if (fd == -1) {
		_E("X server disable");
		backlight_on(NORMAL_MODE);
	}

	backlight_update();

	ops = find_device("touchscreen");
	if (!check_default(ops))
		ops->start(NORMAL_MODE);

	ops = find_device("touchkey");
	if (!check_default(ops))
		ops->start(NORMAL_MODE);

	free(pmsys);
	pmsys = NULL;
	if(fd != -1)
		close(fd);

	return 0;
}
