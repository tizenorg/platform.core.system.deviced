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


#ifndef __DBUS_H__
#define __DBUS_H__

#include <dbus/dbus.h>

/*
 * Template
 *
#define XXX_BUS_NAME                        "org.tizen.system.XXX"
#define XXX_OBJECT_PATH                     "/Org/Tizen/System/XXX"
#define XXX_INTERFACE_NAME                  XXX_BUS_NAME
#define XXX_PATH_YYY                        XXX_OBJECT_PATH"/YYY"
#define XXX_INTERFACE_YYY                   XXX_INTERFACE_NAME".YYY"
#define XXX_SIGNAL_ZZZ                      "ZZZ"
#define XXX_METHOD_ZZZ                      "ZZZ"
 */

/*
 * Device daemon
 */
#define DEVICED_BUS_NAME                    "org.tizen.system.deviced"
#define DEVICED_OBJECT_PATH                 "/Org/Tizen/System/DeviceD"
#define DEVICED_INTERFACE_NAME              DEVICED_BUS_NAME
/* Core service: get/set device status operations about device */
#define DEVICED_PATH_CORE                   DEVICED_OBJECT_PATH"/Core"
#define DEVICED_INTERFACE_CORE              DEVICED_INTERFACE_NAME".core"
/* Display service: start/stop display(pm), get/set brightness operations about display */
#define DEVICED_PATH_DISPLAY                DEVICED_OBJECT_PATH"/Display"
#define DEVICED_INTERFACE_DISPLAY           DEVICED_INTERFACE_NAME".display"
/* Pass service: start/stop pass operations about pass */
#define DEVICED_PATH_PASS                   DEVICED_OBJECT_PATH"/Pass"
#define DEVICED_INTERFACE_PASS              DEVICED_INTERFACE_NAME".pass"
/* Hall service: get hall status operations about hall */
#define DEVICED_PATH_HALL                   DEVICED_OBJECT_PATH"/Hall"
#define DEVICED_INTERFACE_HALL              DEVICED_INTERFACE_NAME".hall"
/* Power service: set resetkey disable operations about power */
#define DEVICED_PATH_POWER                  DEVICED_OBJECT_PATH"/Power"
#define DEVICED_INTERFACE_POWER             DEVICED_INTERFACE_NAME".power"
/* Storage service: get storage size operatioins about storage */
#define DEVICED_PATH_STORAGE                DEVICED_OBJECT_PATH"/Storage"
#define DEVICED_INTERFACE_STORAGE           DEVICED_INTERFACE_NAME".storage"
/* ODE service: request ode popup result operatioins about storage */
#define DEVICED_PATH_ODE                    DEVICED_OBJECT_PATH"/Ode"
#define DEVICED_INTERFACE_ODE               DEVICED_INTERFACE_NAME".ode"
/* Haptic service: operatioins about haptic */
#define DEVICED_PATH_HAPTIC                 DEVICED_OBJECT_PATH"/Haptic"
#define DEVICED_INTERFACE_HAPTIC            DEVICED_INTERFACE_NAME".haptic"
/* Lowmem service: get critical low status operations about Lowmem */
#define DEVICED_PATH_LOWMEM                 DEVICED_OBJECT_PATH"/Lowmem"
#define DEVICED_INTERFACE_LOWMEM            DEVICED_INTERFACE_NAME".lowmem"
/* Poweroff service: get power off status operations about Poweroff */
#define DEVICED_PATH_POWEROFF               DEVICED_OBJECT_PATH"/PowerOff"
#define DEVICED_INTERFACE_POWEROFF          DEVICED_INTERFACE_NAME".PowerOff"
/* Led service: play/stop led operations about led */
#define DEVICED_PATH_LED                    DEVICED_OBJECT_PATH"/Led"
#define DEVICED_INTERFACE_LED               DEVICED_INTERFACE_NAME".Led"
/* Block service: manage block device */
#define DEVICED_PATH_BLOCK                  DEVICED_OBJECT_PATH"/Block"
#define DEVICED_INTERFACE_BLOCK             DEVICED_INTERFACE_NAME".Block"
/* MMC service: mount/unmount/format mmc operations about mmc */
#define DEVICED_PATH_MMC                    DEVICED_OBJECT_PATH"/Mmc"
#define DEVICED_INTERFACE_MMC               DEVICED_INTERFACE_NAME".Mmc"
/* Process service: operations about process */
#define DEVICED_PATH_PROCESS                DEVICED_OBJECT_PATH"/Process"
#define DEVICED_INTERFACE_PROCESS           DEVICED_INTERFACE_NAME".Process"
/* Key service: operations about key */
#define DEVICED_PATH_KEY                    DEVICED_OBJECT_PATH"/Key"
#define DEVICED_INTERFACE_KEY               DEVICED_INTERFACE_NAME".Key"
/* USB client service: change usb connection mode */
#define DEVICED_PATH_USB                    DEVICED_OBJECT_PATH"/Usb"
#define DEVICED_INTERFACE_USB               DEVICED_INTERFACE_NAME".Usb"
/* USB start/stop service: operations about usb start/stop */
#define DEVICED_PATH_USB_CONTROL            DEVICED_OBJECT_PATH"/UsbControl"
#define DEVICED_INTERFACE_USB_CONTROL       DEVICED_INTERFACE_NAME".UsbControl"
/* USB host service: operations about usb start/stop */
#define DEVICED_PATH_USBHOST                DEVICED_OBJECT_PATH"/Usbhost"
#define DEVICED_INTERFACE_USBHOST           DEVICED_INTERFACE_NAME".Usbhost"
/* Cpu service: operations about cpu */
#define DEVICED_PATH_CPU                    DEVICED_OBJECT_PATH"/Cpu"
#define DEVICED_INTERFACE_CPU               DEVICED_INTERFACE_NAME".Cpu"
/* PmQos service: operations about pmqos */
#define DEVICED_PATH_PMQOS                  DEVICED_OBJECT_PATH"/PmQos"
#define DEVICED_INTERFACE_PMQOS             DEVICED_INTERFACE_NAME".PmQos"
/* Sysnoti service */
#define DEVICED_PATH_SYSNOTI                DEVICED_OBJECT_PATH"/SysNoti"
#define DEVICED_INTERFACE_SYSNOTI           DEVICED_INTERFACE_NAME".SysNoti"
/* ExtCon service */
#define DEVICED_PATH_EXTCON                 DEVICED_OBJECT_PATH"/ExtCon"
#define DEVICED_INTERFACE_EXTCON            DEVICED_INTERFACE_NAME".ExtCon"
/* Battery service */
#define DEVICED_PATH_BATTERY                DEVICED_OBJECT_PATH"/Battery"
#define DEVICED_INTERFACE_BATTERY           DEVICED_INTERFACE_NAME".Battery"
/* Time service */
#define DEVICED_PATH_TIME                DEVICED_OBJECT_PATH"/Time"
#define DEVICED_INTERFACE_TIME           DEVICED_INTERFACE_NAME".Time"

/* Apps service */
#define DEVICED_PATH_APPS               DEVICED_OBJECT_PATH"/Apps"
#define DEVICED_INTERFACE_APPS           DEVICED_INTERFACE_NAME".Apps"

/* GPIO service: status check about gpio */
#define DEVICED_PATH_GPIO                    DEVICED_OBJECT_PATH"/Gpio"
#define DEVICED_INTERFACE_GPIO               DEVICED_INTERFACE_NAME".Gpio"

/* HDMICEC service: status check about gpio */
#define DEVICED_PATH_HDMICEC                    DEVICED_OBJECT_PATH"/HdmiCec"
#define DEVICED_INTERFACE_HDMICEC               DEVICED_INTERFACE_NAME".HdmiCec"

/*
 * Resource daemon
 */
#define RESOURCED_BUS_NAME                  "org.tizen.resourced"
#define RESOURCED_OBJECT_PATH               "/Org/Tizen/ResourceD"
#define RESOURCED_INTERFACE_NAME            RESOURCED_BUS_NAME

#define RESOURCED_PATH_PROCESS              RESOURCED_OBJECT_PATH"/Process"
#define RESOURCED_INTERFACE_PROCESS         RESOURCED_INTERFACE_NAME".process"
#define RESOURCED_METHOD_ACTIVE             "Active"

/*
 * Popup launcher
 */
#define POPUP_BUS_NAME                      "org.tizen.system.popup"
#define POPUP_OBJECT_PATH                   "/Org/Tizen/System/Popup"
#define POPUP_INTERFACE_NAME                POPUP_BUS_NAME
/* LED */
#define POPUP_PATH_LED                      POPUP_OBJECT_PATH"/Led"
#define POPUP_INTERFACE_LED                 POPUP_INTERFACE_NAME".Led"
/* TICKER */
#define POPUP_PATH_TICKER                   POPUP_OBJECT_PATH"/Ticker"
#define POPUP_INTERFACE_TICKER              POPUP_INTERFACE_NAME".Ticker"
/* Power off */
#define POPUP_PATH_POWEROFF                 POPUP_OBJECT_PATH"/Poweroff"
#define POPUP_INTERFACE_POWEROFF            POPUP_INTERFACE_NAME".Poweroff"
/* Low battery */
#define POPUP_PATH_LOWBAT                   POPUP_OBJECT_PATH"/Lowbat"
#define POPUP_INTERFACE_LOWBAT              POPUP_INTERFACE_NAME".Lowbat"
/* Low memory */
#define POPUP_PATH_LOWMEM                   POPUP_OBJECT_PATH"/Lowmem"
#define POPUP_INTERFACE_LOWMEM              POPUP_INTERFACE_NAME".Lowmem"
/* MMC */
#define POPUP_PATH_MMC                      POPUP_OBJECT_PATH"/Mmc"
#define POPUP_INTERFACE_MMC                 POPUP_INTERFACE_NAME".Mmc"
/* USB */
#define POPUP_PATH_USB                      POPUP_OBJECT_PATH"/Usb"
#define POPUP_INTERFACE_USB                 POPUP_INTERFACE_NAME".Usb"
/* USB otg */
#define POPUP_PATH_USBOTG                   POPUP_OBJECT_PATH"/Usbotg"
#define POPUP_INTERFACE_USBOTG              POPUP_INTERFACE_NAME".Usbotg"
/* USB host */
#define POPUP_PATH_USBHOST                  POPUP_OBJECT_PATH"/Usbhost"
#define POPUP_INTERFACE_USBHOST             POPUP_INTERFACE_NAME".Usbhost"
/* System */
#define POPUP_PATH_SYSTEM                   POPUP_OBJECT_PATH"/System"
#define POPUP_INTERFACE_SYSTEM              POPUP_INTERFACE_NAME".System"
/* Crash */
#define POPUP_PATH_CRASH                    POPUP_OBJECT_PATH"/Crash"
#define POPUP_INTERFACE_CRASH               POPUP_INTERFACE_NAME".Crash"
/* ODE */
#define POPUP_PATH_ODE                      POPUP_OBJECT_PATH"/Ode"
#define POPUP_INTERFACE_ODE                 POPUP_INTERFACE_NAME".Ode"
/* Battery */
#define POPUP_PATH_BATTERY                  POPUP_OBJECT_PATH"/Battery"
#define POPUP_INTERFACE_BATTERY             POPUP_INTERFACE_NAME".Battery"
/* Servant */
#define POPUP_PATH_SERVANT                  POPUP_OBJECT_PATH"/Servant"
#define POPUP_IFACE_SERVANT                 POPUP_INTERFACE_NAME".Servant"

#define POPUP_PATH_APP                      POPUP_OBJECT_PATH"/Apps"
#define POPUP_IFACE_APP                     POPUP_BUS_NAME".Apps"

#define POPUP_METHOD_LAUNCH                 "PopupLaunch"
#define POPUP_METHOD_TERMINATE              "AppTerminateByPid"
#define POPUP_KEY_CONTENT                   "_SYSPOPUP_CONTENT_"

/*
 * Crash daemon
 */
#define CRASHD_BUS_NAME                     "org.tizen.system.crashd"
#define CRASHD_OBJECT_PATH                  "/Org/Tizen/System/CrashD"
#define CRASHD_INTERFACE_NAME               CRASHD_BUS_NAME

#define CRASHD_PATH_CRASH                   CRASHD_OBJECT_PATH"/Crash"
#define CRASHD_INTERFACE_CRASH              CRASHD_INTERFACE_NAME".Crash"

struct dbus_byte {
	const char *data;
	int size;
};

int append_variant(DBusMessageIter *iter, const char *sig, char *param[]);

DBusMessage *dbus_method_sync_with_reply(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[]);

int dbus_method_sync(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[]);

int dbus_method_sync_timeout(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[], int timeout);

int dbus_method_async(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[]);

typedef void (*dbus_pending_cb)(void *data, DBusMessage *msg, DBusError *err);

int dbus_method_async_with_reply(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[], dbus_pending_cb cb, int timeout, void *data);

int check_systemd_active(void);
#endif
