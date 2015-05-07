
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#include "core/log.h"
#include "device-interface.h"

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
	DPMSForceLevel(dpy, (CARD16)state);

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

	*state = dpms_state;
	return 0;
}
