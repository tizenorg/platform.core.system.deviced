
#include <stdio.h>

#include "device-interface.h"
#include "core/edbus-handler.h"

#define ENLIGHTENMENT_BUS_NAME          "org.enlightenment.wm"
#define ENLIGHTENMENT_OBJECT_PATH       "/org/enlightenment/wm"
#define ENLIGHTENMENT_INTERFACE_NAME    ENLIGHTENMENT_BUS_NAME".dpms"

static int dpms = DPMS_OFF;

int dpms_set_power(enum dpms_state state)
{
	char *arr[1];
	char *str[32];
	int ret;

	snprintf(str, sizeof(str), "%d", state);
	arr[0] = str;
	ret = dbus_method_sync(ENLIGHTENMENT_BUS_NAME,
			ENLIGHTENMENT_OBJECT_PATH,
			ENLIGHTENMENT_INTERFACE_NAME,
			"set", "u", arr);

	if (ret < 0)
		return ret;

	dpms = state;
	return 0;
}

int dpms_get_power(enum dpms_state *state)
{
	if (!state)
		return -EINVAL;

	*state = dpms;
	return 0;
}
