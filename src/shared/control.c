/*
 * deviced
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <vconf.h>
#include <errno.h>
#include <device-node.h>

#include "log.h"
#include "dd-control.h"

static int deviced_control_common(int device, bool enable)
{
	char buf_pid[6];
	char buf_dev[3];
	char buf_enable[2];

	snprintf(buf_pid, sizeof(buf_pid), "%d", getpid());
	snprintf(buf_dev, sizeof(buf_dev), "%d", device);
	snprintf(buf_enable, sizeof(buf_enable), "%d", enable);

	return deviced_call_predef_action(CONTROL_HANDLER_NAME, 3, buf_pid,
		    buf_dev, buf_enable);
}

/*
 * example of control api
 * API int deviced_display_control(bool enable)
 * {
 *	return deviced_control_common(DEVICE_CONTROL_DISPLAY, enable);
 * }
 */

