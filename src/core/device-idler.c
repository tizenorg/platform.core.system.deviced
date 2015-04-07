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


#include <Ecore.h>
#include <glib.h>
#include <errno.h>

#include "log.h"

struct device_request {
	int (*func)(void *data);
	void *data;
};

static GQueue req_queue = G_QUEUE_INIT;
static Ecore_Idler *idler;

static int free_request(struct device_request *req)
{
	if (!req)
		return -EINVAL;

	free(req);
}

static Eina_Bool idler_cb(void *data)
{
	struct device_request *req;

	req = g_queue_pop_head(&req_queue);
	if (req) {
		if (req->func)
			req->func(req->data);
		free_request(req);
	}

	if (g_queue_is_empty(&req_queue)) {
		idler = NULL;
		return ECORE_CALLBACK_CANCEL;
	}

	return ECORE_CALLBACK_RENEW;
}

static void process_next_request_in_idle(void)
{
	struct device_request *req;

	if (g_queue_is_empty(&req_queue))
		return;

	if (idler)
		return;

	idler = ecore_idler_add(idler_cb, NULL);
	/**
	 * if idler is null,
	 * it means whole system might be an abnormal state.
	 * so it just prints out error log.
	 */
	if (!idler)
		_E("fail to add request to idler : %s", strerror(errno));
}

int add_idle_request(int (*func)(void *data), void *data)
{
	struct device_request *req;

	if (!func) {
		_E("invalid argumet : func(NULL)");
		return -EINVAL;
	}

	req = calloc(1, sizeof(struct device_request));
	if (!req) {
		_E("fail to allocate request : %s", strerror(errno));
		return -errno;
	}

	req->func = func;
	req->data = data;

	g_queue_push_tail(&req_queue, req);
	process_next_request_in_idle();
}
