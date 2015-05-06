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

#define POWER_LOCK_PATH         "/sys/power/wake_lock"
#define POWER_UNLOCK_PATH       "/sys/power/wake_unlock"
#define POWER_WAKEUP_PATH       "/sys/power/wakeup_count"
#define POWER_STATE_PATH        "/sys/power/state"

enum {
	POWER_UNLOCK = 0,
	POWER_LOCK,
};

struct _backlight_ops backlight_ops;
struct _power_ops power_ops;

static bool custom_status;
static int custom_brightness;
static int force_brightness;
static int default_brightness;

static struct display_device *display_dev;

static int bl_onoff(int on)
{
	if (!display_dev || !display_dev->set_state) {
		_E("there is no display device");
		return -ENOENT;
	}

	return display_dev->set_state(on);
}

static int bl_brt(int brightness, int delay)
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

	if (force_brightness > 0 && brightness != PM_DIM_BRIGHTNESS) {
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

static int system_suspend(void)
{
	int ret;

	_I("system suspend");
	ret = sys_set_str(POWER_STATE_PATH, "mem");
	_I("system resume (result : %d)", ret);
	return 0;
}

static int system_power_lock(void)
{
	_I("system power lock");
	return sys_set_str(POWER_LOCK_PATH, "mainlock");
}

static int system_power_unlock(void)
{
	_I("system power unlock");
	return sys_set_str(POWER_UNLOCK_PATH, "mainlock");
}

static int system_get_power_lock_support(void)
{
	static int power_lock_support = -1;
	int ret;

	if (power_lock_support >= 0)
		goto out;

	ret = sys_check_node(POWER_LOCK_PATH);
	if (ret < 0)
		power_lock_support = false;
	else
		power_lock_support = true;

	_I("system power lock : %s",
			(power_lock_support ? "support" : "not support"));

out:
	return power_lock_support;
}

static int get_lcd_power(void)
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

		bl_brt(start, LCD_PHASED_DELAY);
	}
}

static int backlight_on(enum device_flags flags)
{
	int ret = -1;
	int i;

	_D("LCD on %x", flags);

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
		ret = bl_onoff(DISPLAY_ON);
		if (get_lcd_power() == DISPLAY_ON) {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_ON, pm_cur_state);
#endif
			break;
		} else {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_ON_FAIL, pm_cur_state);
#endif
			_E("Failed to LCD on, through OAL");
			ret = -1;
		}
	}

	if (flags & LCD_PHASED_TRANSIT_MODE)
		change_brightness(LCD_PHASED_MIN_BRIGHTNESS,
		    default_brightness, LCD_PHASED_CHANGE_STEP);

	return ret;
}

static int backlight_off(enum device_flags flags)
{
	int ret = -1;
	int i;

	_D("LCD off %x", flags);

	if (flags & LCD_PHASED_TRANSIT_MODE)
		change_brightness(default_brightness,
		    LCD_PHASED_MIN_BRIGHTNESS, LCD_PHASED_CHANGE_STEP);

	for (i = 0; i < PM_LCD_RETRY_CNT; i++) {
		usleep(30000);
		ret = bl_onoff(DISPLAY_OFF);
		if (get_lcd_power() == DISPLAY_OFF) {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_OFF, pm_cur_state);
#endif
			break;
		} else {
#ifdef ENABLE_PM_LOG
			pm_history_save(PM_LOG_LCD_OFF_FAIL, pm_cur_state);
#endif
			_E("Failed to LCD off, through OAL");
			ret = -1;
		}
	}
	return ret;
}

static int backlight_dim(void)
{
	int ret;

	ret = bl_brt(PM_DIM_BRIGHTNESS, 0);
#ifdef ENABLE_PM_LOG
	if (!ret)
		pm_history_save(PM_LOG_LCD_DIM, pm_cur_state);
	else
		pm_history_save(PM_LOG_LCD_DIM_FAIL, pm_cur_state);
#endif
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
	} else {
		_I("custom brightness restored! %d", custom_brightness);
		ret = bl_brt(custom_brightness, 0);
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
	} else {
		ret = bl_brt(default_brightness, 0);
	}
	return ret;
}

static int backlight_standby(int force)
{
	int ret = -1;

	if ((get_lcd_power() == DISPLAY_ON) || force) {
		_I("LCD standby");
		ret = bl_onoff(DISPLAY_STANDBY);
	}

	return ret;
}

static int set_default_brt(int level)
{
	if (level < PM_MIN_BRIGHTNESS || level > PM_MAX_BRIGHTNESS)
		level = PM_DEFAULT_BRIGHTNESS;

	default_brightness = level;

	return 0;
}

static int check_wakeup_src(void)
{
	/*  TODO if nedded.
	 * return wackeup source. user input or device interrupts? (EVENT_DEVICE or EVENT_INPUT)
	 */
	return EVENT_DEVICE;
}

static int get_wakeup_count(int *cnt)
{
	int ret;
	int wakeup_count;

	if (!cnt)
		return -EINVAL;

	ret = sys_get_int(POWER_WAKEUP_PATH, &wakeup_count);
	if (ret < 0)
		return ret;

	*cnt = wakeup_count;
	return 0;
}

static int set_wakeup_count(int cnt)
{
	int ret;

	ret = sys_set_int(POWER_WAKEUP_PATH, cnt);
	if (ret < 0)
		return ret;

	return 0;
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

static void _init_ops(void)
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
	power_ops.power_lock = system_power_lock;
	power_ops.power_unlock = system_power_unlock;
	power_ops.get_power_lock_support = system_get_power_lock_support;
	power_ops.check_wakeup_src = check_wakeup_src;
	power_ops.get_wakeup_count = get_wakeup_count;
	power_ops.set_wakeup_count = set_wakeup_count;
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

	if(fd != -1)
		close(fd);

	return 0;
}
