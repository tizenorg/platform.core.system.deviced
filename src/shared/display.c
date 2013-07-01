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
#include <vconf.h>
#include <errno.h>
#include <device-node.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <linux/limits.h>

#include "log.h"
#include "dd-display.h"

#define DISPLAY_MAX_BRIGHTNESS  100
#define DISPLAY_MIN_BRIGHTNESS  1
#define DISPLAY_DIM_BRIGHTNESS  0

#define SOCK_PATH			"/tmp/pm_sock"
#define SHIFT_UNLOCK			4
#define SHIFT_UNLOCK_PARAMETER		12
#define SHIFT_CHANGE_STATE		8
#define TIMEOUT_RESET_BIT		0x80

struct disp_lock_msg {
	pid_t pid;
	unsigned int cond;
	unsigned int timeout;
	unsigned int timeout2;
};

API int display_get_count(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_DISPLAY_COUNT, &val);
	if (r < 0)
		return r;

	return val;
}

API int display_get_max_brightness(void)
{
	return DISPLAY_MAX_BRIGHTNESS;
}

API int display_get_min_brightness(void)
{
	return DISPLAY_MIN_BRIGHTNESS;
}

API int display_get_brightness(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, &val);
	if (r < 0)
		return r;

	return val;
}

API int display_set_brightness_with_setting(int val)
{
	int auto_brt_state;
	int r;

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brt_state) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		errno = EPERM;
		return -errno;
	}

	if (val == DISPLAY_DIM_BRIGHTNESS) {
		_D("application can not set this value(DIM VALUE:%d)", val);
		errno = EPERM;
		return -errno;
	}

	if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		_D("auto_brightness state is ON, can not change the brightness value");
		return 0;
	}

	r = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, val);
	if (r < 0)
		return r;

	if (vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, val) != 0) {
		_E("Failed to set VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");
	}

	if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, val) != 0) {
		_E("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
	}

	return 0;
}

API int display_set_brightness(int val)
{
	int auto_brt_state;
	int r;

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brt_state) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		errno = EPERM;
		return -errno;
	}

	if (val == DISPLAY_DIM_BRIGHTNESS) {
		_D("application can not set this value(DIM VALUE:%d)", val);
		errno = EPERM;
		return -errno;
	}

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_ON);
	r = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, val);
	if (r < 0)
		return r;

	if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		_D("Auto brightness will be paused");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_PAUSE);
	}

	if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, val) != 0) {
		_E("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
	}

	return 0;
}

API int display_release_brightness(void)
{
	int bat_state;
	int setting_val;
	int auto_brt_state;
	int charger_state;
	int brt_changed_state;
	int r;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state) != 0) {
		_E("Failed to get VCONFKEY_SYSMAN_BATTERY_STATUS_LOW value");
		errno = EPERM;
		return -1;
	}

	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &charger_state) != 0) {
		_E("Failed to get VCONFKEY_SYSMAN_CHARGER_STATUS value");
		errno = EPERM;
		return -1;
	}

	if (vconf_get_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM, &brt_changed_state) != 0) {
		_E("Failed to get VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM value");
		errno = EPERM;
		return -1;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &setting_val) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");
		errno = EPERM;
		return -1;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brt_state) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		errno = EPERM;
		return -1;
	}

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_OFF);

	// check dim state
	if (bat_state <= VCONFKEY_SYSMAN_BAT_WARNING_LOW &&
		charger_state == VCONFKEY_SYSMAN_CHARGER_DISCONNECTED && !brt_changed_state) {
		_D("batt warning low : brightness is not changed!");
		device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, 0);
		return 0;
	}

	if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_OFF) {
		device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, setting_val);
		if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, setting_val) != 0) {
			_E("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
		}
	} else if (auto_brt_state == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		_D("Auto brightness will be enable");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_ON);
	}

	return 0;
}

API int display_get_acl_status(void)
{
	int val;
	int r;

	r = device_get_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_ACL_CONTROL, &val);
	if (r < 0)
		return r;

	return val;
}

API int display_set_acl_status(int val)
{
	int r;

	r = device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_ACL_CONTROL, val);
	if (r < 0)
		return r;

	return 0;
}

static int send_msg(unsigned int s_bits, unsigned int timeout, unsigned int timeout2)
{
	int rc = 0;
	int sock;
	struct disp_lock_msg p;
	struct sockaddr_un remote;

	p.pid = getpid();
	p.cond = s_bits;
	p.timeout = timeout;
	p.timeout2 = timeout2;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock == -1) {
		_E("pm socket() failed");
		return -1;
	}

	remote.sun_family = AF_UNIX;
	if(strlen(SOCK_PATH) >= sizeof(remote.sun_path)) {
		_E("socket path is vey long");
		return -1;
	}
	strncpy(remote.sun_path, SOCK_PATH, sizeof(remote.sun_path));

	rc = sendto(sock, (void *)&p, sizeof(p), 0, (struct sockaddr *)&remote,
		    sizeof(struct sockaddr_un));
	if (rc == -1) {
		_E("pm socket sendto() failed");
	} else
		rc = 0;

	close(sock);
	return rc;
}

API int display_change_state(unsigned int s_bits)
{
	/* s_bits is LCD_NORMAL 0x1, LCD_DIM 0x2, LCD_OFF 0x4, SUSPEND 0x8
	 * Stage change to NORMAL       0x100
	 * Stage change to LCDDIM       0x200
	 * Stage change to LCDOFF       0x400
	 * Stage change to SLEEP        0x800
	 * */
	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
	case SUSPEND:
	case POWER_OFF:
		break;
	default:
		return -1;
	}
	return send_msg(s_bits << SHIFT_CHANGE_STATE, 0, 0);
}

API int display_lock_state(unsigned int s_bits, unsigned int flag,
		      unsigned int timeout)
{
	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}
	if (flag & GOTO_STATE_NOW)
		/* if the flag is true, go to the locking state directly */
		s_bits = s_bits | (s_bits << SHIFT_CHANGE_STATE);

	return send_msg(s_bits, timeout, 0);
}

API int display_unlock_state(unsigned int s_bits, unsigned int flag)
{
	switch (s_bits) {
	case LCD_NORMAL:
	case LCD_DIM:
	case LCD_OFF:
		break;
	default:
		return -1;
	}

	s_bits = (s_bits << SHIFT_UNLOCK);
	s_bits = (s_bits | (flag << SHIFT_UNLOCK_PARAMETER));
	return send_msg(s_bits, 0, 0);
}
