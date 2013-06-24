/*
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

#ifndef __SS_DATA_H__
#define __SS_DATA_H__

#include <Ecore.h>
#include <unistd.h>

enum {
	WIN_CREATE = 0,
	WIN_SHOW,
	WIN_DELETE,
	WIN_MAX
};

#define FM_RADIO_APP		"FM_radio"
#define MULTIMEDIA_APP		"music"
#define BLUETOOTH_APP		"bluetooth"
#define VOICERECORDER_APP	"voicerecorder"

#define MMC_DEV			"/dev/mmcblk"

#define VCONFKEY_INTERNAL_ADDED_USB_STORAGE 	"memory/private/sysman/added_storage_uevent"
#define VCONFKEY_INTERNAL_REMOVED_USB_STORAGE	"memory/private/sysman/removed_storage_uevent"

#define PREDEF_CALL			"call"
#define PREDEF_LOWMEM			"lowmem"
#define PREDEF_LOWBAT			"lowbat"
#define PREDEF_USBCON			"usbcon"
#define PREDEF_POWEROFF			"poweroff"
#define PREDEF_REBOOT			"reboot"
#define PREDEF_PWROFF_POPUP		"pwroff-popup"
#define PREDEF_BACKGRD			"backgrd"
#define PREDEF_FOREGRD			"foregrd"
#define PREDEF_ACTIVE			"active"
#define PREDEF_USB_STORAGE_ADD		"usbstorage-add"
#define PREDEF_USB_STORAGE_REMOVE	"usbstorage-remove"
#define PREDEF_INACTIVE			"inactive"
#define PREDEF_HAPTIC			"haptic"

#define OOMADJ_SET			"oomadj_set"
#define LOW_MEM_ACT			"low_mem_act"
#define OOM_MEM_ACT			"oom_mem_act"

#define WARNING_LOW_BAT_ACT		"warning_low_bat_act"
#define CRITICAL_LOW_BAT_ACT		"critical_low_bat_act"
#define POWER_OFF_BAT_ACT		"power_off_bat_act"
#define CHARGE_BAT_ACT			"charge_bat_act"
#define CHARGE_CHECK_ACT			"charge_check_act"
#define CHARGE_ERROR_ACT			"charge_error_act"

#define PREDEF_EARJACKCON		"earjack_predef_internal"

#define PREDEF_SET_DATETIME		"set_datetime"
#define PREDEF_SET_TIMEZONE		"set_timezone"

#define PREDEF_MOUNT_MMC		"mountmmc"
#define PREDEF_UNMOUNT_MMC		"unmountmmc"
#define PREDEF_FORMAT_MMC		"formatmmc"
#define PREDEF_CHECK_SMACK_MMC	"checksmackmmc"
#define PREDEF_CHECK_MMC		"checkmmc"
#define PREDEF_CHECK_MMC_PROC	"checkmmcproc"

#define PREDEF_LED		"led"

#define PREDEF_SET_MAX_FREQUENCY	"set_max_frequency"     
#define PREDEF_SET_MIN_FREQUENCY	"set_min_frequency"     
#define PREDEF_RELEASE_MAX_FREQUENCY	"release_max_frequency" 
#define PREDEF_RELEASE_MIN_FREQUENCY	"release_min_frequency" 

#define PREDEF_FLIGHT_MODE		"flightmode"

#define PREDEF_DEVICE_CHANGED	"device_changed"
#define PREDEF_INTERNAL_POWEROFF	"internal_poweroff"

#define PROCESS_GROUP_SET		"process_group_set"

#define OOMADJ_SU                       (-17)
#define OOMADJ_INIT                     (-16)
#define OOMADJ_FOREGRD_LOCKED           (-15)
#define OOMADJ_FOREGRD_UNLOCKED         (-10)
#define OOMADJ_BACKGRD_LOCKED           (-5)
#define OOMADJ_BACKGRD_UNLOCKED         (1)

#define OOMADJ_APP_LIMIT		(-16)

#define MOVINAND_MOUNT_POINT		"/opt/media"
#define MMC_MOUNT_POINT			"/opt/storage/sdcard"

struct ui_contention_info {

};

struct ss_main_data {
	int sysnoti_fd;
	int noti_fd;
};

#endif /* __SS_DATA_H__ */
