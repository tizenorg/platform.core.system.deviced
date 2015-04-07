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


#include "log.h"
#include "list.h"
#include "common.h"

struct device_request {
	int (*func)(void *data);
	void *data;
};

static dd_queue device_req_queue = DD_QUEUE_INIT;
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

	req = DD_QUEUE_POP_HEAD(&device_req_queue);
	if (req) {
		if (req->func)
			req->func(req->data);
		free_request(req);
	}

	if (DD_QUEUE_IS_EMPTY(&device_req_queue)) {
		idler = NULL;
		return ECORE_CALLBACK_CANCEL;
	}

	return ECORE_CALLBACK_RENEW;	
}

static void process_next_request_in_idle(void)
{
	struct device_request *req;

	if (DD_QUEUE_IS_EMPTY(&device_req_queue))
		return;

	if (idler)
		return;

	idler = ecore_idler_add(idler_cb, NULL);
	if (!idler) {
		_E("fail to add request to idler");
		/* if failed to add idler, it executes a request oneself. */
		req = DD_QUEUE_POP_HEAD(&device_req_queue);
		if (req && req->func)
			req->func(req->data);
		free_request(req);
		process_next_request_in_idle();
	}
}

int add_idler_request(int (*func)(void *data), void *data)
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

	DD_QUEUE_PUSH_TAIL(&device_req_queue, req);
	process_next_request_in_idle();
}
