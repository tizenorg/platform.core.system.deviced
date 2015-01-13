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


#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <fnmatch.h>
#include "dd-deviced.h"
#include "log.h"
#include "device-notifier.h"
#include "device-handler.h"
#include "device-node.h"
#include "display/poll.h"
#include "devices.h"
#include "udev.h"
#include "common.h"
#include "list.h"
#include "proc/proc-handler.h"
#include "edbus-handler.h"
#include "devices.h"
#include "display/setting.h"
#include "display/core.h"

#define PREDEF_DEVICE_CHANGED		"device_changed"
#define PREDEF_POWER_CHANGED		POWER_SUBSYSTEM
#define PREDEF_UDEV_CONTROL		UDEV

#define TVOUT_X_BIN			"/usr/bin/xberc"
#define TVOUT_FLAG			0x00000001

#define MOVINAND_MOUNT_POINT		"/opt/media"
#define BUFF_MAX		255
#define SYS_CLASS_INPUT		"/sys/class/input"

#define USB_STATE_PLATFORM_PATH "/sys/devices/platform/jack/usb_online"
#define USB_STATE_SWITCH_PATH "/sys/devices/virtual/switch/usb_cable/state"

#define HDMI_NOT_SUPPORTED	(-1)
#ifdef ENABLE_EDBUS_USE
#include <E_DBus.h>
static E_DBus_Connection *conn;
#endif				/* ENABLE_EDBUS_USE */

struct input_event {
	long dummy[2];
	unsigned short type;
	unsigned short code;
	int value;
};

enum snd_jack_types {
	SND_JACK_HEADPHONE = 0x0001,
	SND_JACK_MICROPHONE = 0x0002,
	SND_JACK_HEADSET = SND_JACK_HEADPHONE | SND_JACK_MICROPHONE,
	SND_JACK_LINEOUT = 0x0004,
	SND_JACK_MECHANICAL = 0x0008,	/* If detected separately */
	SND_JACK_VIDEOOUT = 0x0010,
	SND_JACK_AVOUT = SND_JACK_LINEOUT | SND_JACK_VIDEOOUT,
};

#define CHANGE_ACTION		"change"
#define ENV_FILTER		"CHGDET"
#define USB_NAME		"usb"
#define USB_NAME_LEN		3

#define CHARGER_NAME 		"charger"
#define CHARGER_NAME_LEN	7

#define EARJACK_NAME 		"earjack"
#define EARJACK_NAME_LEN	7

#define EARKEY_NAME 		"earkey"
#define EARKEY_NAME_LEN	6

#define TVOUT_NAME 		"tvout"
#define TVOUT_NAME_LEN		5

#define HDMI_NAME 		"hdmi"
#define HDMI_NAME_LEN		4

#define HDCP_NAME 		"hdcp"
#define HDCP_NAME_LEN		4

#define HDMI_AUDIO_NAME 	"ch_hdmi_audio"
#define HDMI_AUDIO_LEN		13

#define CRADLE_NAME 		"cradle"
#define CRADLE_NAME_LEN	6

#define KEYBOARD_NAME 		"keyboard"
#define KEYBOARD_NAME_LEN	8

#define SWITCH_DEVICE_USB 	"usb_cable"

#define METHOD_GET_HDMI		"GetHDMI"
#define METHOD_GET_HDCP		"GetHDCP"
#define METHOD_GET_HDMI_AUDIO	"GetHDMIAudio"
#define SIGNAL_HDMI_STATE	"ChangedHDMI"
#define SIGNAL_HDCP_STATE	"ChangedHDCP"
#define SIGNAL_HDMI_AUDIO_STATE	"ChangedHDMIAudio"

#define HDCP_HDMI_VALUE(HDCP, HDMI)	((HDCP << 1) | HDMI)

#define METHOD_GET_CRADLE	"GetCradle"
#define SIGNAL_CRADLE_STATE	"ChangedCradle"

struct siop_data {
	int siop;
	int rear;
};

static int ss_flags = 0;

static int input_device_number;

/* Uevent */
static struct udev *udev = NULL;
/* Kernel Uevent */
static struct udev_monitor *mon = NULL;
static Ecore_Fd_Handler *ufdh = NULL;
static int ufd = -1;
static int hdmi_status = 0;

enum udev_subsystem_type {
	UDEV_INPUT,
	UDEV_PLATFORM,
	UDEV_SWITCH,
};

static const struct udev_subsystem {
	const enum udev_subsystem_type type;
	const char *str;
	const char *devtype;
} udev_subsystems[] = {
	{ UDEV_INPUT,            INPUT_SUBSYSTEM, NULL },
	{ UDEV_PLATFORM,         PLATFORM_SUBSYSTEM, NULL },
	{ UDEV_SWITCH,		 SWITCH_SUBSYSTEM, NULL },
};

static dd_list *udev_event_list;

static struct extcon_device {
	const enum extcon_type type;
	const char *str;
	int fd;
	int count;
} extcon_devices[] = {
	{ EXTCON_TA, "/csa/factory/batt_cable_count", 0, 0},
	{ EXTCON_EARJACK, "/csa/factory/earjack_count", 0, 0},
};

int extcon_set_count(int index)
{
	int r;
	int ret = 0;
	char buf[BUFF_MAX];

	extcon_devices[index].count++;

	if (extcon_devices[index].fd < 0) {
		_E("cannot open file(%s)", extcon_devices[index].str);
		return -ENOENT;
	}
	lseek(extcon_devices[index].fd, 0, SEEK_SET);
	_I("ext(%d) count %d", index, extcon_devices[index].count);
	snprintf(buf, sizeof(buf), "%d", extcon_devices[index].count);

	r = write(extcon_devices[index].fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;
	return ret;
}

static int extcon_get_count(int index)
{
	int fd;
	int r;
	int ret = 0;
	char buf[BUFF_MAX];

	fd = open(extcon_devices[index].str, O_RDWR);
	if (fd < 0)
		return -ENOENT;

	r = read(fd, buf, BUFF_MAX);
	if ((r >= 0) && (r < BUFF_MAX))
		buf[r] = '\0';
	else
		ret = -EIO;

	if (ret != 0) {
		close(fd);
		return ret;
	}
	extcon_devices[index].fd = fd;
	extcon_devices[index].count = atoi(buf);
	_I("get extcon(%d:%x) count %d",
		index, extcon_devices[index].fd, extcon_devices[index].count);

	return ret;
}

static int extcon_create_count(int index)
{
	int fd;
	int r;
	int ret = 0;
	char buf[BUFF_MAX];
	fd = open(extcon_devices[index].str, O_RDWR | O_CREAT, 0644);
	if (fd < 0) {
		_E("cannot open file(%s)", extcon_devices[index].str);
		return -ENOENT;
	}
	snprintf(buf, sizeof(buf), "%d", extcon_devices[index].count);
	r = write(fd, buf, strlen(buf));
	if (r < 0)
		ret = -EIO;

	if (ret != 0) {
		close(fd);
		_E("cannot write file(%s)", extcon_devices[index].str);
		return ret;
	}
	extcon_devices[index].fd = fd;
	_I("create extcon(%d:%x) %s",
		index, extcon_devices[index].fd, extcon_devices[index].str);
	return ret;
}

static int extcon_count_init(void)
{
	int i;
	int ret = 0;
	for (i = 0; i < ARRAY_SIZE(extcon_devices); i++) {
		if (extcon_get_count(i) >= 0)
			continue;
		ret = extcon_create_count(i);
		if (ret < 0)
			break;
	}
	return ret;
}

int get_usb_state_direct(void)
{
	FILE *fp;
	char str[2];
	int state;
	char *path;

	if (access(USB_STATE_PLATFORM_PATH, F_OK) == 0)
		path = USB_STATE_PLATFORM_PATH;
	else if (access(USB_STATE_SWITCH_PATH, F_OK) == 0)
		path = USB_STATE_SWITCH_PATH;
	else {
		_E("Cannot get direct path");
		return -ENOENT;
	}

	fp = fopen(path, "r");
	if (!fp) {
		_E("Cannot open jack node");
		return -ENOMEM;
	}

	if (!fgets(str, sizeof(str), fp)) {
		_E("cannot get string from jack node");
		fclose(fp);
		return -ENOMEM;
	}

	fclose(fp);

	return atoi(str);
}

static void usb_chgdet_cb(void *data)
{
	int val = -1;
	int ret = 0;
	char params[BUFF_MAX];

	if (data == NULL)
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_USB_ONLINE, &val);
	else
		val = *(int *)data;
	if (ret == 0) {
		if (val < 0)
			val = get_usb_state_direct();

		_I("jack - usb changed %d",val);
	} else {
		_E("fail to get usb_online status");
	}
}

static int display_changed(void *data)
{
	enum state_t state;
	int ret, cradle = 0;

	if (!data)
		return 0;

	state = *(int*)data;
	if (state != S_NORMAL)
		return 0;

	ret = vconf_get_int(VCONFKEY_SYSMAN_CRADLE_STATUS, &cradle);
	if (ret >= 0 && cradle == DOCK_SOUND) {
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
		_I("sound dock is connected! dim lock is on.");
	}
	if (hdmi_status) {
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
		_I("hdmi is connected! dim lock is on.");
	}
	return 0;
}

static void cradle_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast cradle status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_CRADLE_STATE, "i", arr);
}

static int cradle_cb(void *data)
{
	static int old = 0;
	int val = 0;
	int ret = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;

	if (old == val)
		return old;

	old = val;
	cradle_send_broadcast(old);
	return old;
}

static void cradle_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	pm_change_internal(getpid(), LCD_NORMAL);

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_CRADLE_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}

	_I("jack - cradle changed %d", val);
	cradle_cb((void *)&val);
	if (vconf_set_int(VCONFKEY_SYSMAN_CRADLE_STATUS, val) != 0) {
		_E("failed to set vconf status");
		return;
	}

	if (val == DOCK_SOUND)
		pm_lock_internal(getpid(), LCD_DIM, STAY_CUR_STATE, 0);
	else if (val == DOCK_NONE)
		pm_unlock_internal(getpid(), LCD_DIM, PM_SLEEP_MARGIN);
}

void sync_cradle_status(void)
{
	int val;
	int status;
	if ((device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_CRADLE_ONLINE, &val) != 0) ||
	    vconf_get_int(VCONFKEY_SYSMAN_CRADLE_STATUS, &status) != 0)
		return;
	if ((val != 0 && status == 0) || (val == 0 && status != 0))
		cradle_chgdet_cb(NULL);
}

static void earkey_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_EARKEY_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}
	_I("jack - earkey changed %d", val);
	vconf_set_int(VCONFKEY_SYSMAN_EARJACKKEY, val);
}

static void tvout_chgdet_cb(void *data)
{
	_I("jack - tvout changed");
	pm_change_internal(getpid(), LCD_NORMAL);
}

static void hdcp_hdmi_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_I("broadcast hdmi status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_STATE, "i", arr);
}

static int hdcp_hdmi_cb(void *data)
{
	static int old = 0;
	int val = 0;
	int ret = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;
	val = HDCP_HDMI_VALUE(val, hdmi_status);

	if (old == val)
		return old;

	old = val;
	hdcp_hdmi_send_broadcast(old);
	return old;
}

static int hdmi_cec_execute(void *data)
{
	static const struct device_ops *ops = NULL;

	FIND_DEVICE_INT(ops, "hdmi-cec");

	return ops->execute(data);
}

static void hdmi_chgdet_cb(void *data)
{
	int val;
	int ret = 0;

	pm_change_internal(getpid(), LCD_NORMAL);
	if (device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_HDMI_SUPPORT, &val) == 0) {
		if (val!=1) {
			_I("target is not support HDMI");
			vconf_set_int(VCONFKEY_SYSMAN_HDMI, HDMI_NOT_SUPPORTED);
			return;
		}
	}

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_HDMI_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			return;
		}
	}

	_I("jack - hdmi changed %d", val);
	vconf_set_int(VCONFKEY_SYSMAN_HDMI, val);
	hdmi_status = val;
	device_notify(DEVICE_NOTIFIER_HDMI, &val);

	if(val == 1) {
		pm_lock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, STAY_CUR_STATE, 0);
	} else {
		pm_unlock_internal(INTERNAL_LOCK_HDMI, LCD_DIM, PM_SLEEP_MARGIN);
	}
	hdmi_cec_execute(&val);
}

static void hdcp_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast hdcp status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDCP_STATE, "i", arr);
}

static int hdcp_chgdet_cb(void *data)
{
	static int old = 0;
	int val = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;
	if (old == val)
		return old;

	old = val;
	hdcp_send_broadcast(old);
	return old;
}

static void hdmi_audio_send_broadcast(int status)
{
	static int old = 0;
	char *arr[1];
	char str_status[32];

	if (old == status)
		return;

	_D("broadcast hdmi audio status %d", status);
	old = status;
	snprintf(str_status, sizeof(str_status), "%d", status);
	arr[0] = str_status;

	broadcast_edbus_signal(DEVICED_PATH_SYSNOTI, DEVICED_INTERFACE_SYSNOTI,
			SIGNAL_HDMI_AUDIO_STATE, "i", arr);
}

static int hdmi_audio_chgdet_cb(void *data)
{
	static int old = 0;
	int val = 0;

	if (data == NULL)
		return old;

	val = *(int *)data;
	if (old == val)
		return old;

	old = val;
	hdmi_audio_send_broadcast(old);
	return old;
}

static void keyboard_chgdet_cb(void *data)
{
	int val = -1;
	int ret = 0;

	if (data)
		val = *(int *)data;
	else {
		ret = device_get_property(DEVICE_TYPE_EXTCON, PROP_EXTCON_KEYBOARD_ONLINE, &val);
		if (ret != 0) {
			_E("failed to get status");
			vconf_set_int(VCONFKEY_SYSMAN_SLIDING_KEYBOARD, VCONFKEY_SYSMAN_SLIDING_KEYBOARD_NOT_SUPPORTED);
			return;
		}
	}
	_I("jack - keyboard changed %d", val);
	if(val != 1)
		val = 0;
	vconf_set_int(VCONFKEY_SYSMAN_SLIDING_KEYBOARD, val);
}

static void ums_unmount_cb(void *data)
{
	umount(MOVINAND_MOUNT_POINT);
}

#ifdef ENABLE_EDBUS_USE
static void cb_xxxxx_signaled(void *data, DBusMessage * msg)
{
	char *args;
	DBusError err;

	dbus_error_init(&err);
	if (dbus_message_get_args
	    (msg, &err, DBUS_TYPE_STRING, &args, DBUS_TYPE_INVALID)) {
		if (!strcmp(args, "action")) ;	/* action */
	}

	return;
}
#endif				/* ENABLE_EDBUS_USE */

static int earjack_execute(void *data)
{
	static const struct device_ops *ops = NULL;

	FIND_DEVICE_INT(ops, "earjack");

	return ops->execute(data);
}

static int siop_execute(const char *siop, const char *rear)
{
	static const struct device_ops *ops = NULL;
	struct siop_data params;

	FIND_DEVICE_INT(ops, PROC_OPS_NAME);

	if (!siop)
		params.siop = 0;
	else
		params.siop = atoi(siop);
	if (!rear)
		params.rear = 0;
	else
		params.rear = atoi(rear);
	return ops->execute((void *)&params);
}

static int changed_device(const char *name, const char *value)
{
	int val = 0;
	int *state = NULL;
	int i;

	if (!name)
		goto out;

	if (value) {
		val = atoi(value);
		state = &val;
	}

	if (strncmp(name, USB_NAME, USB_NAME_LEN) == 0)
		usb_chgdet_cb((void *)state);
	else if (strncmp(name, EARJACK_NAME, EARJACK_NAME_LEN) == 0)
		earjack_execute((void *)state);
	else if (strncmp(name, EARKEY_NAME, EARKEY_NAME_LEN) == 0)
		earkey_chgdet_cb((void *)state);
	else if (strncmp(name, TVOUT_NAME, TVOUT_NAME_LEN) == 0)
		tvout_chgdet_cb((void *)state);
	else if (strncmp(name, HDMI_NAME, HDMI_NAME_LEN) == 0)
		hdmi_chgdet_cb((void *)state);
	else if (strncmp(name, HDCP_NAME, HDCP_NAME_LEN) == 0) {
		hdcp_chgdet_cb((void *)state);
		hdcp_hdmi_cb((void *)state);
	}
	else if (strncmp(name, HDMI_AUDIO_NAME, HDMI_AUDIO_LEN) == 0)
		hdmi_audio_chgdet_cb((void *)state);
	else if (strncmp(name, CRADLE_NAME, CRADLE_NAME_LEN) == 0)
		cradle_chgdet_cb((void *)state);
	else if (strncmp(name, KEYBOARD_NAME, KEYBOARD_NAME_LEN) == 0)
		keyboard_chgdet_cb((void *)state);
out:
	return 0;
}

static int booting_done(void *data)
{
	static int done = 0;
	int ret;
	int val;

	if (data == NULL)
		return done;
	done = *(int*)data;
	if (done == 0)
		return done;

	_I("booting done");

	/* set initial state for devices */
	input_device_number = 0;
	cradle_chgdet_cb(NULL);
	keyboard_chgdet_cb(NULL);
	hdmi_chgdet_cb(NULL);
	return done;
}

static Eina_Bool uevent_kernel_control_cb(void *data, Ecore_Fd_Handler *fd_handler)
{
	struct udev_device *dev = NULL;
	struct udev_list_entry *list_entry = NULL;
	struct uevent_handler *l;
	dd_list *elem;
	const char *subsystem = NULL;
	const char *env_name = NULL;
	const char *env_value = NULL;
	const char *devpath;
	const char *devnode;
	const char *action;
	int ret = -1;
	int i, len;

	if ((dev = udev_monitor_receive_device(mon)) == NULL)
		return EINA_TRUE;

	subsystem = udev_device_get_subsystem(dev);

	for (i = 0; i < ARRAY_SIZE(udev_subsystems); i++) {
		len = strlen(udev_subsystems[i].str);
		if (!strncmp(subsystem, udev_subsystems[i].str, len))
			break;
	}

	if (i >= ARRAY_SIZE(udev_subsystems))
		goto out;

	devpath = udev_device_get_devpath(dev);

	switch (udev_subsystems[i].type) {
	case UDEV_INPUT:
		/* check new input device */
		if (!fnmatch(INPUT_PATH, devpath, 0)) {
			action = udev_device_get_action(dev);
			devnode = udev_device_get_devnode(dev);
			if (!strcmp(action, UDEV_ADD))
				device_notify(DEVICE_NOTIFIER_INPUT_ADD, (void *)devnode);
			else if (!strcmp(action, UDEV_REMOVE))
				device_notify(DEVICE_NOTIFIER_INPUT_REMOVE, (void *)devnode);
			goto out;
		}
		break;
	case UDEV_SWITCH:
		env_name = udev_device_get_property_value(dev, "SWITCH_NAME");
		env_value = udev_device_get_property_value(dev, "SWITCH_STATE");
		changed_device(env_name, env_value);
		break;
	case UDEV_PLATFORM:
		env_value = udev_device_get_property_value(dev, ENV_FILTER);
		if (!env_value)
			break;
		changed_device(env_value, NULL);
		break;
	}

out:

	DD_LIST_FOREACH(udev_event_list, elem, l) {
		if (!strncmp(subsystem, l->subsystem, strlen(subsystem)) &&
		    l->uevent_func)
			l->uevent_func(dev);
	}

	udev_device_unref(dev);
	return EINA_TRUE;
}

static int uevent_kernel_control_stop(void)
{
	struct udev_device *dev = NULL;

	if (ufdh) {
		ecore_main_fd_handler_del(ufdh);
		ufdh = NULL;
	}
	if (ufd >= 0) {
		close(ufd);
		ufd = -1;
	}
	if (mon) {
		dev = udev_monitor_receive_device(mon);
		if (dev) {
			udev_device_unref(dev);
			dev = NULL;
		}
		udev_monitor_unref(mon);
		mon = NULL;
	}
	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}
	return 0;
}

static int uevent_kernel_control_start(void)
{
	struct uevent_handler *l;
	dd_list *elem;
	int i, ret;

	if (udev && mon) {
		_E("uevent control routine is alreay started");
		return -EINVAL;
	}

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EINVAL;
		}
	}

	mon = udev_monitor_new_from_netlink(udev, UDEV);
	if (mon == NULL) {
		_E("error udev_monitor create");
		goto stop;
	}

	if (udev_monitor_set_receive_buffer_size(mon, UDEV_MONITOR_SIZE) != 0) {
		_E("fail to set receive buffer size");
		goto stop;
	}

	for (i = 0; i < ARRAY_SIZE(udev_subsystems); i++) {
		ret = udev_monitor_filter_add_match_subsystem_devtype(mon,
			    udev_subsystems[i].str, udev_subsystems[i].devtype);
		if (ret < 0) {
			_E("error apply subsystem filter");
			goto stop;
		}
	}

	DD_LIST_FOREACH(udev_event_list, elem, l) {
		ret = udev_monitor_filter_add_match_subsystem_devtype(mon,
				l->subsystem, NULL);
		if (ret < 0) {
			_E("error apply subsystem filter");
			goto stop;
		}
	}

	ret = udev_monitor_filter_update(mon);
	if (ret < 0)
		_E("error udev_monitor_filter_update");

	ufd = udev_monitor_get_fd(mon);
	if (ufd == -1) {
		_E("error udev_monitor_get_fd");
		goto stop;
	}

	ufdh = ecore_main_fd_handler_add(ufd, ECORE_FD_READ,
			uevent_kernel_control_cb, NULL, NULL, NULL);
	if (!ufdh) {
		_E("error ecore_main_fd_handler_add");
		goto stop;
	}

	if (udev_monitor_enable_receiving(mon) < 0) {
		_E("error unable to subscribe to udev events");
		goto stop;
	}

	return 0;
stop:
	uevent_kernel_control_stop();
	return -EINVAL;

}

int register_kernel_uevent_control(const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *elem;
	int r;
	bool matched = false;

	if (!uh)
		return -EINVAL;

	/* if udev is not initialized, it just will be added list */
	if (!udev || !mon)
		goto add_list;

	/* check if the same subsystem is already added */
	DD_LIST_FOREACH(udev_event_list, elem, l) {
		if (!strncmp(l->subsystem, uh->subsystem, strlen(l->subsystem))) {
			matched = true;
			break;
		}
	}

	/* the first request to add subsystem */
	if (!matched) {
		r = udev_monitor_filter_add_match_subsystem_devtype(mon,
				uh->subsystem, NULL);
		if (r < 0) {
			_E("fail to add %s subsystem : %d", uh->subsystem, r);
			return -EPERM;
		}
	}

	r = udev_monitor_filter_update(mon);
	if (r < 0)
		_E("fail to update udev monitor filter : %d", r);

add_list:
	DD_LIST_APPEND(udev_event_list, uh);
	return 0;
}

int unregister_kernel_uevent_control(const struct uevent_handler *uh)
{
	struct uevent_handler *l;
	dd_list *n, *next;

	DD_LIST_FOREACH_SAFE(udev_event_list, n, next, l) {
		if (!strncmp(l->subsystem, uh->subsystem, strlen(l->subsystem)) &&
		    l->uevent_func == uh->uevent_func) {
			DD_LIST_REMOVE(udev_event_list, l);
			return 0;
		}
	}

	return -ENOENT;
}

int uevent_udev_get_path(const char *subsystem, dd_list **list)
{
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices, *dev_list_entry;
	int ret;

	if (!udev) {
		udev = udev_new();
		if (!udev) {
			_E("error create udev");
			return -EIO;
		}
	}

	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
		return -EIO;

	ret = udev_enumerate_add_match_subsystem(enumerate, subsystem);
	if (ret < 0)
		return -EIO;

	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0)
		return -EIO;

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		path = udev_list_entry_get_name(dev_list_entry);
		_D("subsystem : %s, path : %s", subsystem, path);
		DD_LIST_APPEND(*list, (void*)path);
	}

	return 0;
}

static DBusMessage *dbus_cradle_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = cradle_cb(NULL);
	_I("cradle %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_hdcp_hdmi_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = hdcp_hdmi_cb(NULL);
	_I("hdmi %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_hdcp_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = hdcp_chgdet_cb(NULL);
	_I("hdcp %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_hdmi_audio_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessage *reply;
	int ret;

	ret = hdmi_audio_chgdet_cb(NULL);
	_I("hdmi audio %d", ret);

	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);
	return reply;
}

static DBusMessage *dbus_device_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv[2];

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv[0],
		    DBUS_TYPE_STRING, &argv[1], DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
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

	changed_device(argv[0], argv[1]);

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

static DBusMessage *dbus_udev_handler(E_DBus_Object *obj, DBusMessage *msg)
{
	DBusError err;
	DBusMessageIter iter;
	DBusMessage *reply;
	pid_t pid;
	int ret;
	int argc;
	char *type_str;
	char *argv;

	dbus_error_init(&err);

	if (!dbus_message_get_args(msg, &err,
		    DBUS_TYPE_STRING, &type_str,
		    DBUS_TYPE_INT32, &argc,
		    DBUS_TYPE_STRING, &argv, DBUS_TYPE_INVALID)) {
		_E("there is no message");
		ret = -EINVAL;
		goto out;
	}

	if (argc < 0) {
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

	if (strncmp(argv, "start", strlen("start")) == 0) {
		uevent_kernel_control_start();
	} else if (strncmp(argv, "stop", strlen("stop")) == 0) {
		uevent_kernel_control_stop();
	}

out:
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret);

	return reply;
}

void internal_pm_change_state(unsigned int s_bits)
{
	pm_change_internal(getpid(), s_bits);
}

static const struct edbus_method edbus_methods[] = {
	{ PREDEF_DEVICE_CHANGED, "siss",    "i", dbus_device_handler },
	{ PREDEF_UDEV_CONTROL,   "sis","i", dbus_udev_handler },
	{ METHOD_GET_HDCP,       NULL, "i", dbus_hdcp_handler },
	{ METHOD_GET_HDMI_AUDIO, NULL, "i", dbus_hdmi_audio_handler },
	{ METHOD_GET_HDMI,       NULL, "i", dbus_hdcp_hdmi_handler },
	{ METHOD_GET_CRADLE,     NULL, "i", dbus_cradle_handler },
};

static int device_change_poweroff(void *data)
{
	uevent_kernel_control_stop();
	return 0;
}

static void device_change_init(void *data)
{
	int ret;

	if (extcon_count_init() != 0)
		_E("fail to init extcon files");
	register_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);
	register_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	register_notifier(DEVICE_NOTIFIER_LCD, display_changed);
	ret = register_edbus_method(DEVICED_PATH_SYSNOTI, edbus_methods, ARRAY_SIZE(edbus_methods));
	if (ret < 0)
		_E("fail to init edbus method(%d)", ret);

	/* dbus noti change cb */
#ifdef ENABLE_EDBUS_USE
	e_dbus_init();
	conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!conn)
		_E("check system dbus running!\n");

	e_dbus_signal_handler_add(conn, NULL, "/system/uevent/xxxxx",
				  "system.uevent.xxxxx",
				  "Change", cb_xxxxx_signaled, data);
#endif				/* ENABLE_EDBUS_USE */
	if (uevent_kernel_control_start() != 0) {
		_E("fail uevent control init");
		return;
	}
}

static void device_change_exit(void *data)
{
	int i;
	unregister_notifier(DEVICE_NOTIFIER_POWEROFF, device_change_poweroff);
	unregister_notifier(DEVICE_NOTIFIER_BOOTING_DONE, booting_done);
	unregister_notifier(DEVICE_NOTIFIER_LCD, display_changed);
	for (i = 0; i < ARRAY_SIZE(extcon_devices); i++) {
		if (extcon_devices[i].fd <= 0)
			continue;
		close(extcon_devices[i].fd);
	}

}

static const struct device_ops change_device_ops = {
	.priority = DEVICE_PRIORITY_NORMAL,
	.name     = "device change",
	.init     = device_change_init,
	.exit     = device_change_exit,
};

DEVICE_OPS_REGISTER(&change_device_ops)
