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


#include <stdio.h>
#include <stdlib.h>
#include <svi.h>
#include <notification.h>
#include <libintl.h>
#include <locale.h>
#include <vconf.h>
#include "ss_log.h"
#include "sys_device_noti.h"

#define BATTERY_FULL_ICON_PATH		"/usr/share/system-server/sys_device_noti/batt_full_icon.png"
#define DEV_NOTI_LOCALE_DIR		"/usr/share/system-server/sys_device_noti/res/locale"
#define DEV_NOTI_PACKAGE_NAME		"sys_device_noti"

static int battery_full_noti(int bNoti)
{
	char *lang;
	char *r;
	int charge_full = bNoti;

	notification_h noti = NULL;
	notification_error_e noti_err = NOTIFICATION_ERROR_NONE;

	if (charge_full == 1) {
		lang = vconf_get_str(VCONFKEY_LANGSET);
		if (lang) {
			setenv("LANG", lang, 1);
			setenv("LC_MESSAGES", lang, 1);
			r = setlocale(LC_ALL, "");
			if (r == NULL) {
				setlocale(LC_ALL, lang);
			}
			free(lang);
		}
		bindtextdomain(DEV_NOTI_PACKAGE_NAME,DEV_NOTI_LOCALE_DIR);
		textdomain(DEV_NOTI_PACKAGE_NAME);
		noti_err = notification_delete_all_by_type(NULL, NOTIFICATION_TYPE_NOTI);
		PRT_TRACE("[BAT_FULL_NOTI] add notification for battery full\n");

		noti = notification_new (NOTIFICATION_TYPE_NOTI,
					 NOTIFICATION_GROUP_ID_NONE,
					 NOTIFICATION_PRIV_ID_NONE);
		if (noti == NULL) {
			PRT_TRACE("[BAT_FULL_NOTI] Errot noti == NULL\n");
			return -1;
		}

		noti_err = notification_set_text_domain(noti, DEV_NOTI_PACKAGE_NAME, DEV_NOTI_LOCALE_DIR);
		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] set text domain\n");
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_text (noti,
						  NOTIFICATION_TEXT_TYPE_TITLE,
						  _("IDS_IDLE_POP_BATTERY_FULLY_CAHRGED"),
						  "IDS_IDLE_POP_BATTERY_FULLY_CAHRGED",
						  NOTIFICATION_VARIABLE_TYPE_NONE);

		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_title : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_text (noti,
						  NOTIFICATION_TEXT_TYPE_CONTENT,
						  _("IDS_QP_BODY_UNPLUG_CHARGER"),
						  "IDS_QP_BODY_UNPLUG_CHARGER",
						  NOTIFICATION_VARIABLE_TYPE_NONE);

		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_content : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_text (noti,
						  NOTIFICATION_TEXT_TYPE_CONTENT_FOR_DISPLAY_OPTION_IS_OFF,
						  _("IDS_QP_BODY_UNPLUG_CHARGER"),
						  "IDS_QP_BODY_UNPLUG_CHARGER",
						  NOTIFICATION_VARIABLE_TYPE_NONE);

		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_content : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_time(noti, time(NULL));
		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_time : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_image(noti, NOTIFICATION_IMAGE_TYPE_ICON, BATTERY_FULL_ICON_PATH);
		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_image : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_property (noti,
						      NOTIFICATION_PROP_DISABLE_APP_LAUNCH
						      | NOTIFICATION_PROP_DISABLE_TICKERNOTI 
						      | NOTIFICATION_PROP_VOLATILE_DISPLAY);

		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_property : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_set_display_applist(noti, NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY);
		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_set_display_applist : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_insert(noti, NULL);
		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_insert : %d\n", noti_err);
			goto NOTI_MEM_FREE;
		}

		noti_err = notification_free(noti);
		if (noti_err != NOTIFICATION_ERROR_NONE) {
			PRT_TRACE("[BAT_FULL_NOTI] Error notification_free : %d\n", noti_err);
			return -1;
		}
	} else {
		noti_err = notification_delete_all_by_type(NULL, NOTIFICATION_TYPE_NOTI);
		PRT_TRACE("[BAT_FULL_NOTI] Leave battery full status\n");
	}

	return 0;

NOTI_MEM_FREE:
	noti_err = notification_free(noti);
	if (noti_err != NOTIFICATION_ERROR_NONE)
		PRT_TRACE("[BAT_FULL_NOTI] Error notification_free : %d\n", noti_err);
	return -1;
}
int main(int argc, char *argv[])
{
	int r;
	int handle = 0;
	int bNoti = -1;
	sound_type snd = -1;
	vibration_type vib = -1;
	cb_noti_type cb_type = -1;

	if (argc == 3)
		bNoti = atoi(argv[2]);

	cb_type = (cb_noti_type)atoi(argv[1]);
	switch (cb_type) {
		case CB_NOTI_BATT_CHARGE:
			vib = SVI_VIB_OPERATION_CHARGERCONN;
			snd = SVI_SND_OPERATION_CHARGERCONN;
			break;
		case CB_NOTI_BATT_FULL:
			battery_full_noti(bNoti);
			if (bNoti == 1) {
				vib = SVI_VIB_OPERATION_FULLCHARGED;
				snd = SVI_SND_OPERATION_FULLCHARGED;
				break;
			} else
				return 0;
		default:
			PRT_TRACE("sys_device_noti cb_type error(%d)",cb_type);
			break;
	}

	r = svi_init(&handle); /* Initialize SVI */

	if (r != SVI_SUCCESS) {
		PRT_TRACE("Cannot initialize SVI.\n");
	} else {
		r = svi_play(handle, vib, snd);
		if (r != SVI_SUCCESS)
			PRT_TRACE("Cannot play sound or vibration.\n");

		r = svi_fini(handle); /* Finalize SVI */
		if (r != SVI_SUCCESS)
			PRT_TRACE("Cannot close SVI.\n");
	}

	return 0;
}
