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
#include <stdlib.h>
#include <libintl.h>
#include <locale.h>
#include <vconf.h>
#include <syspopup_caller.h>
#include <notification.h>
#include "core/log.h"
#include "sys_pci_noti.h"

static void show_tickernoti(char *msg)
{
	if (!msg)
		return;

	notification_error_e noti_err = NOTIFICATION_ERROR_NONE;
	noti_err = notification_status_message_post(gettext(msg));
	if (noti_err != NOTIFICATION_ERROR_NONE)
		_E("FAIL: notification_status_message_post(msg)");
}

static void pci_noti(pci_noti_type iPCI)
{
	char *lang;
	char *r;
	int ret = -1;
	const int arrSize = 2;
	char str_tickernoti[arrSize];

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
	bindtextdomain("sys_pci_noti","/usr/share/system-server/sys_pci_noti/res/locale/");
	textdomain("sys_pci_noti");

	if (iPCI == CB_NOTI_PCI_REMOVED)
		show_tickernoti(_("IDS_COM_POP_KEYBOARD_DISCONNECTED_ABB2"));

	else if (iPCI == CB_NOTI_PCI_INSERTED)
		show_tickernoti(_("IDS_COM_POP_KEYBOARD_CONNECTED_ABB2"));

}

int main(int argc, char *argv[])
{
	int r = 0;
	int handle = 0;
	int bNoti = -1;
	pci_noti_type cb_type = -1;

	if (argc == 2) {
		cb_type = (pci_noti_type)atoi(argv[1]);
		pci_noti(cb_type);
	}
	else {
		_E("FAIL param error");
	}

	return 0;
}
