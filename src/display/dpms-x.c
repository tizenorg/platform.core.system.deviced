/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include <stdbool.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#include "core/log.h"
#include "device-interface.h"

static CARD16 dpms_state_to_DPMSMode(enum dpms_state state)
{
	if (state == DPMS_ON)
		return DPMSModeOn;
	else if (state == DPMS_STANDBY)
		return DPMSModeStandby;
	else if (state == DPMS_SUSPEND)
		return DPMSModeSuspend;
	return DPMSModeOff;
}

static enum dpms_state DPMSMode_to_dpms_state(CARD16 state)
{
	if (state == DPMSModeOn)
		return DPMS_ON;
	else if (state == DPMSModeStandby)
		return DPMS_STANDBY;
	else if (state == DPMSModeSuspend)
		return DPMS_SUSPEND;
	return DPMS_OFF;
}

int dpms_set_power(enum dpms_state state)
{
	Display *dpy;

	if (state < DPMS_ON || state > DPMS_OFF)
		return -EINVAL;

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		_E("fail to open display");
		return -EPERM;
	}

	DPMSEnable(dpy);
	DPMSForceLevel(dpy, dpms_state_to_DPMSMode(state));

	XCloseDisplay(dpy);
	return 0;
}

int dpms_get_power(enum dpms_state *state)
{
	Display *dpy;
	int dummy;
	CARD16 dpms_state = DPMSModeOff;
	BOOL onoff;

	if (!state)
		return -EINVAL;

	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		_E("fail to open display");
		return -EPERM;
	}

	if (DPMSQueryExtension(dpy, &dummy, &dummy)) {
		if (DPMSCapable(dpy))
			DPMSInfo(dpy, &dpms_state, &onoff);
	}

	XCloseDisplay(dpy);

	*state = DPMSMode_to_dpms_state(dpms_state);
	return 0;
}
