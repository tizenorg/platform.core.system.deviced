/*
 * devman
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/poll.h>
#include <errno.h>
#include <signal.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <device-node.h>

#include "devlog.h"
#include "devman_internal.h"

char fifo_path[NAME_MAX];
struct pollfd fifo_pollfd;

static void sig_quit(int signo)
{
	DEVERR("[DISPLAY_WD] display_wd will be exit for signal %d", signo);
	if(fifo_pollfd.fd >= 0)
		close(fifo_pollfd.fd);
	if (access(fifo_path, F_OK) == 0)
		unlink(fifo_path);
}

int main(void)
{
	int r = -1;
	int ret = -1;
	int val = -1;
	int auto_brightness_state = -1;
	fifo_pollfd.fd = -1;
	setsid();

	signal(SIGPIPE, sig_quit);

	snprintf(fifo_path, NAME_MAX, "%s.%d", DISPLAY_WD_FIFO, getppid());
	fifo_pollfd.fd = open(fifo_path, O_WRONLY);
	if (fifo_pollfd.fd < 0) {
		DEVERR("[DISPLAY_WD] Cannot open the fifo file - %s.",
			fifo_path);
		return -1;
	}

	/* waitting for parent process ready */
	usleep(10000);
	ret = write(fifo_pollfd.fd, "ack", strlen("ack") + 1);

	close(fifo_pollfd.fd);

	fifo_pollfd.fd = open(fifo_path, O_RDONLY);
	if (fifo_pollfd.fd < 0) {
		DEVERR("[DISPLAY_WD] Cannot open the fifo file - %s.",
			fifo_path);
		return -1;
	}

	fifo_pollfd.events = (POLLIN | POLLHUP);
	ret = 0;
	while (ret != DISPLAY_WD_CANCEL) {
		DEVLOG("[DISPLAY_WD] wait....");
		if (poll(&fifo_pollfd, 1, -1) < 0) {
			DEVERR("[DISPLAY_WD] Cannot poll the fifo file - %s", fifo_path);
			close(fifo_pollfd.fd);
			return -1;
		}
		if (fifo_pollfd.revents & POLLIN) {
			r = read(fifo_pollfd.fd, &ret, sizeof(int));
			if (r < 0)
				DEVERR("read() failed(%d)", r);
			if (ret == DISPLAY_WD_CANCEL) {
				DEVERR("[DISPLAY_WD] Canceled. - %s, %d", fifo_path, ret);
				close(fifo_pollfd.fd);
				return -1;
			}
		}
		if (fifo_pollfd.revents & POLLHUP)
			break;
	}
	close(fifo_pollfd.fd);
	unlink(fifo_path);

	DEVLOG("[DISPLAY_WD] occurs POLLHUP");

	vconf_set_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, VCONFKEY_PM_CUSTOM_BRIGHTNESS_OFF);
	if (vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &auto_brightness_state) == 0) {
		if (auto_brightness_state == SETTING_BRIGHTNESS_AUTOMATIC_OFF) {
			ret = vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, &val);
			if (ret == 0 && val > 0) {
				device_set_property(DEVICE_TYPE_DISPLAY, PROP_DISPLAY_BRIGHTNESS, val);
			}
		} else if (auto_brightness_state == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
			DEVLOG("Auto brightness is enable");
			vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_ON);
		}
	}

	return 0;
}
