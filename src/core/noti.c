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


#include <heynoti.h>
#include "log.h"

static int noti_fd;

int ss_noti_getfd()
{
	return noti_fd;
}

int ss_noti_send(char *filename)
{
	return heynoti_publish(filename);
}

int ss_noti_init()
{
	noti_fd = heynoti_init();
	if (noti_fd < 0) {
		PRT_TRACE_ERR("heynoti_init error");
		return -1;
	}

	if (heynoti_attach_handler(noti_fd) < 0) {
		PRT_TRACE_ERR("heynoti_attach_handler error");
		return -1;
	}

	return 0;
}

int ss_noti_add(const char *noti, void (*cb) (void *), void *data)
{
	if (noti_fd < 0)
		return -1;

	return heynoti_subscribe(noti_fd, noti, cb, data);
}
