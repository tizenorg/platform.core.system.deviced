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
#include <Ecore.h>
#include <device-node.h>

#include "util.h"
#include "core.h"
#include "core/common.h"
#include "core/devices.h"

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

static DBusMessage *e_dbus_getbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	int cmd;
	int brightness = -1;

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &brightness);

	_I("get brightness %d, %d", brightness, ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &brightness);

	return reply;
}

static DBusMessage *e_dbus_setbrightness(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;
	int cmd;
	int brightness;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &brightness);

	cmd = DISP_CMD(PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brightness);

	_I("set brightness %d, %d", brightness, ret);

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

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &val);

	pid = get_edbus_sender_pid(msg);
	ret = set_autobrightness_min(val);
	if (ret)
		_E("fail to set autobrightness min %d, %d by %d", val, ret, pid);
	else
		_I("set autobrightness min %d by %d", val, pid);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static struct edbus_method {
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

