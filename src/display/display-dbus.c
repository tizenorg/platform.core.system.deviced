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


/**
 * @file	display-dbus.c
 * @brief	dbus interface
 *
 */

#include <error.h>
#include <stdbool.h>
#include <Ecore.h>
#include <device-node.h>

#include "util.h"
#include "core.h"
#include "core/common.h"
#include "core/devices.h"

#define DISPLAY_DIM_BRIGHTNESS  0

static DBusMessage *e_dbus_start(E_DBus_Object *obj, DBusMessage *msg)
{
	display_device_ops.init(NULL);
	return dbus_message_new_method_return(msg);
}

static DBusMessage *e_dbus_stop(E_DBus_Object *obj, DBusMessage *msg)
{
	display_device_ops.exit(NULL);
	return dbus_message_new_method_return(msg);
}

static DBusMessage *e_dbus_lockstate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state_str;
	char *option1_str;
	char *option2_str;
	int timeout;
	pid_t pid;
	int state;
	int flag;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &state_str,
		    DBUS_TYPE_STRING, &option1_str,
		    DBUS_TYPE_STRING, &option2_str,
		    DBUS_TYPE_INT32, &timeout, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (!state_str || timeout < 0) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else {
		_E("%s state is invalid, dbus ignored!", state_str);
		ret = -EINVAL;
		goto out;
	}

	if (!strcmp(option1_str, STAYCURSTATE_STR))
		flag = STAY_CUR_STATE;
	else if (!strcmp(option1_str, GOTOSTATENOW_STR))
		flag = GOTO_STATE_NOW;
	else {
		_E("%s option is invalid. set default option!", option1_str);
		flag = STAY_CUR_STATE;
	}

	ret = pm_lock_internal(pid, state, flag, timeout);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *e_dbus_unlockstate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state_str;
	char *option_str;
	pid_t pid;
	int state;
	int flag;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &state_str,
		    DBUS_TYPE_STRING, &option_str, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (!state_str) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else {
		_E("%s state is invalid, dbus ignored!", state_str);
		ret = -EINVAL;
		goto out;
	}

	if (!strcmp(option_str, SLEEP_MARGIN_STR))
		flag = PM_SLEEP_MARGIN;
	else if (!strcmp(option_str, RESET_TIMER_STR))
		flag = PM_RESET_TIMER;
	else if (!strcmp(option_str, KEEP_TIMER_STR))
		flag = PM_KEEP_TIMER;
	else {
		_E("%s option is invalid. set default option!", option_str);
		flag = PM_RESET_TIMER;
	}

	ret = pm_unlock_internal(pid, state, flag);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *e_dbus_changestate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	char *state_str;
	pid_t pid;
	int state;
	int ret;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &state_str, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (!state_str) {
		_E("message is invalid!");
		ret = -EINVAL;
		goto out;
	}

	pid = get_edbus_sender_pid(msg);
	if (kill(pid, 0) == -1) {
		_E("%d process does not exist, dbus ignored!", pid);
		ret = -ESRCH;
		goto out;
	}

	if (!strcmp(state_str, PM_LCDON_STR))
		state = LCD_NORMAL;
	else if (!strcmp(state_str, PM_LCDDIM_STR))
		state = LCD_DIM;
	else if (!strcmp(state_str, PM_LCDOFF_STR))
		state = LCD_OFF;
	else {
		_E("%s state is invalid, dbus ignored!", state_str);
		ret = -EINVAL;
		goto out;
	}

	ret = pm_change_internal(pid, state);
out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *e_dbus_getdisplaycount(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, cnt, ret;

	cmd = DISP_CMD(PROP_DISPLAY_DISPLAY_COUNT, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &cnt);
	if (ret >= 0)
		ret = cnt;

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *e_dbus_getbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, ret;

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brt);
	if (ret >= 0)
		ret = brt;

	_I("get brightness %d, %d", brt, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &brt);
	return reply;
}

static DBusMessage *e_dbus_setbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, autobrt, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brt);

	if (brt == DISPLAY_DIM_BRIGHTNESS) {
		_E("application can not set this value(DIM VALUE:%d)", brt);
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &autobrt) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		autobrt = SETTING_BRIGHTNESS_AUTOMATIC_OFF;
	}

	if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		_D("auto_brightness state is ON, can not change the brightness value");
		ret = 0;
		goto error;
	}

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brt);
	if (ret < 0)
		goto error;

	if (vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt) != 0)
		_E("Failed to set VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");

	if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, brt) != 0)
		_E("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");

	_I("set brightness %d, %d", brt, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *e_dbus_holdbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, brt, autobrt, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brt);

	if (brt == DISPLAY_DIM_BRIGHTNESS) {
		_E("application can not set this value(DIM VALUE:%d)", brt);
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &autobrt) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		autobrt = SETTING_BRIGHTNESS_AUTOMATIC_OFF;
	}

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_ON);

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brt);
	if (ret < 0)
		goto error;

	if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		_D("Auto brightness will be paused");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_PAUSE);
	}

	if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, brt) != 0)
		_E("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");

	_I("hold brightness %d, %d", brt, ret);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;

}

static DBusMessage *e_dbus_releasebrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, bat, charger, changed, setting, autobrt, ret = 0;

	if (vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat) != 0) {
		_E("Failed to get VCONFKEY_SYSMAN_BATTERY_STATUS_LOW value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SYSMAN_CHARGER_STATUS, &charger) != 0) {
		_E("Failed to get VCONFKEY_SYSMAN_CHARGER_STATUS value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM, &changed) != 0) {
		_E("Failed to get VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &setting) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_LCD_BRIGHTNESS value");
		ret = -EPERM;
		goto error;
	}

	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &autobrt) != 0) {
		_E("Failed to get VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT value");
		ret = -EPERM;
		goto error;
	}

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_OFF);

	// check dim state
	if (bat <= VCONFKEY_SYSMAN_BAT_CRITICAL_LOW &&
		charger == VCONFKEY_SYSMAN_CHARGER_DISCONNECTED && !changed) {
		_D("batt warning low : brightness is not changed!");
		device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, 0);
		goto error;
	}

	if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_OFF) {
		device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, setting);
		if (vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, setting) != 0) {
			_E("Failed to set VCONFKEY_PM_CURRENT_BRIGHTNESS value");
		}
	} else if (autobrt == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		_D("Auto brightness will be enable");
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_ON);
	}

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *e_dbus_getaclstatus(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, st, ret;

	cmd = DISP_CMD(PROP_DISPLAY_ACL_CONTROL, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &st);
	if (ret >= 0)
		ret = st;

	_I("get acl status %d, %d", st, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *e_dbus_setaclstatus(E_DBus_Object *ob, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int cmd, st, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &st);

	cmd = DISP_CMD(PROP_DISPLAY_ACL_CONTROL, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, st);

	_I("set acl status %d, %d", st, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *e_dbus_setframerate(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, cmd, ret;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_I("there is no message");
		ret = -EINVAL;
		goto error;
	}

	_I("set frame rate %d", val);

	cmd = DISP_CMD(PROP_DISPLAY_FRAME_RATE, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, val);

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *e_dbus_getautobrightnessinterval(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val;

	val = get_autobrightness_interval();
	_I("get autobrightness interval %d", val);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &val);
	return reply;
}

static DBusMessage *e_dbus_setautobrightnessinterval(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int rate, ret;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &rate);

	ret = set_autobrightness_interval(rate);
	if (ret)
		_E("fail to set autobrightness interval %d, %d", rate, ret);
	else
		_I("set autobrightness interval %d", rate);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *e_dbus_setautobrightnessmin(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int val, ret;
	pid_t pid;
	char *sender;

	sender = dbus_message_get_sender(msg);
        if (!sender) {
                _E("invalid sender name!");
                ret = -EINVAL;
		goto error;
        }
	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &val);

	pid = get_edbus_sender_pid(msg);
	ret = set_autobrightness_min(val, sender);
	if (ret) {
		_I("fail to set autobrightness min %d, %d by %d", val, ret, pid);
	} else {
		register_edbus_watch(msg);
		_I("set autobrightness min %d by %d", val, pid);
	}
error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *e_dbus_lockscreenbgon(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret = 0;
	char *on;

	ret = dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &on,
		    DBUS_TYPE_INVALID);

	if (!ret) {
		_E("fail to update lcdscreen bg on state %d", ret);
		ret = -EINVAL;
		goto error;
	}

	if (!strcmp(on, "true"))
		update_pm_setting(SETTING_LOCK_SCREEN_BG, true);
	else if (!strcmp(on, "false"))
		update_pm_setting(SETTING_LOCK_SCREEN_BG, false);
	else
		ret = -EINVAL;

error:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static const struct edbus_method {
	const char *member;
	const char *signature;
	const char *reply_signature;
	E_DBus_Method_Cb func;
} edbus_methods[] = {
	{ "start",           NULL,  NULL, e_dbus_start },
	{ "stop",            NULL,  NULL, e_dbus_stop },
	{ "lockstate",     "sssi",   "i", e_dbus_lockstate },
	{ "unlockstate",     "ss",   "i", e_dbus_unlockstate },
	{ "changestate",      "s",   "i", e_dbus_changestate },
	{ "getbrightness",   NULL,   "i", e_dbus_getbrightness },
	{ "setbrightness",    "i",   "i", e_dbus_setbrightness },
	{ "setframerate",     "i",   "i", e_dbus_setframerate },
	{ "getautobrightnessinterval",  NULL,   "i", e_dbus_getautobrightnessinterval },
	{ "setautobrightnessinterval",   "i",   "i", e_dbus_setautobrightnessinterval },
	{ "setautobrightnessmin", "i", "i", e_dbus_setautobrightnessmin },
	{ "LockScreenBgOn", "s", "i", e_dbus_lockscreenbgon },
	{ "GetDisplayCount", NULL,   "i", e_dbus_getdisplaycount },
	{ "GetBrightness",   NULL,   "i", e_dbus_getbrightness },
	{ "SetBrightness",    "i",   "i", e_dbus_setbrightness },
	{ "HoldBrightness",   "i",   "i", e_dbus_holdbrightness },
	{ "ReleaseBrightness", NULL, "i", e_dbus_releasebrightness },
	{ "GetAclStatus",    NULL,   "i", e_dbus_getaclstatus },
	{ "SetAclStatus",    NULL,   "i", e_dbus_setaclstatus },
	/* Add methods here */
};

int init_pm_dbus(void)
{
	E_DBus_Interface *iface;
	int ret;
	int i;

	iface = get_edbus_interface(DEVICED_PATH_DISPLAY);

	_I("%s, %x", DEVICED_PATH_DISPLAY, iface);

	if (!iface) {
		_E("fail to get edbus interface!");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(edbus_methods); i++) {
		ret = e_dbus_interface_method_add(iface,
				    edbus_methods[i].member,
				    edbus_methods[i].signature,
				    edbus_methods[i].reply_signature,
				    edbus_methods[i].func);
		if (!ret) {
			_E("fail to add method %s!", edbus_methods[i].member);
			return -1;
		}
	}

	return 0;
}

