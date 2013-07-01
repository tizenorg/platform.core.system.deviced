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


#include <dlfcn.h>
#include "data.h"
#include "core.h"
#include "queue.h"
#include "log.h"

#define SS_PREDEFINE_ACT_FUNC_STR		"ss_predefine_action"
#define SS_IS_ACCESSIBLE_FUNC_STR		"ss_is_accessible"
#define SS_UI_VIEWABLE_FUNC_STR			"ss_ui_viewable"

static Eina_List *predef_act_list;
static Eina_List *run_queue;

static struct ss_action_entry *ss_action_entry_find(char *type)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_action_entry *data;

	EINA_LIST_FOREACH_SAFE(predef_act_list, tmp, tmp_next, data) {
		if ((data != NULL) && (!strcmp(data->type, type)))
			return data;
	}

	return NULL;
}

int ss_action_entry_add_internal(char *type,
				 int (*predefine_action) (),
				 int (*ui_viewable) (),
				 int (*is_accessible) (int))
{
	struct ss_action_entry *data;

	data = malloc(sizeof(struct ss_action_entry));

	if (data == NULL) {
		PRT_TRACE_ERR("Malloc failed");
		return -1;
	}

	data->type = NULL;
	if (ss_action_entry_find(type) != NULL)
		goto err;

	data->handle = NULL;
	data->predefine_action = predefine_action;
	if (data->predefine_action == NULL)
		goto err;

	data->is_accessible = is_accessible;
	data->ui_viewable = ui_viewable;
	data->owner_pid = getpid();
	data->type = strdup(type);
	data->path = strdup("");

	predef_act_list = eina_list_prepend(predef_act_list, data);

	PRT_TRACE_ERR("[SYSMAN] add predefine action entry suceessfully - %s",
		  data->type);
	return 0;
 err:
	if (data->type != NULL)
		PRT_TRACE_ERR("[SYSMAN] add predefine action entry -%s",
			      data->type);
	free(data);
	return -1;
}

int ss_action_entry_add(struct sysnoti *msg)
{
	struct ss_action_entry *data;

	data = malloc(sizeof(struct ss_action_entry));

	if (data == NULL) {
		PRT_TRACE_ERR("Malloc failed");
		return -1;
	}

	if (ss_action_entry_find(msg->type) != NULL)
		goto err;

	data->handle = dlopen(msg->path, RTLD_LAZY);
	if (!data->handle) {
		PRT_TRACE_ERR("cannot find such library");
		goto err;
	}

	data->predefine_action = dlsym(data->handle, SS_PREDEFINE_ACT_FUNC_STR);
	if (data->predefine_action == NULL) {
		PRT_TRACE_ERR("cannot find predefine_action symbol : %s",
			      SS_PREDEFINE_ACT_FUNC_STR);
		goto err;
	}

	data->is_accessible = dlsym(data->handle, SS_IS_ACCESSIBLE_FUNC_STR);
	data->ui_viewable = dlsym(data->handle, SS_UI_VIEWABLE_FUNC_STR);
	data->owner_pid = msg->pid;
	data->type = strdup(msg->type);
	data->path = strdup(msg->path);

	predef_act_list = eina_list_prepend(predef_act_list, data);

	PRT_TRACE_ERR("[SYSMAN]add predefine action entry suceessfully - %s",
		  data->type);
	return 0;
 err:
	PRT_TRACE_ERR("[SYSMAN] FAIL predefine action entry - %s", msg->type);
	free(data);
	return -1;
}

int ss_action_entry_call_internal(char *type, int argc, ...)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_action_entry *data;
	va_list argptr;
	int i;
	char *args = NULL;
	char *argv[SYSMAN_MAXARG];

	if (argc > SYSMAN_MAXARG || type == NULL)
		return -1;

	EINA_LIST_FOREACH_SAFE(predef_act_list, tmp, tmp_next, data) {
		if ((data != NULL) && (!strcmp(data->type, type))) {
			va_start(argptr, argc);
			for (i = 0; i < argc; i++) {
				args = va_arg(argptr, char *);
				if (args != NULL)
					argv[i] = strdup(args);
				else
					argv[i] = NULL;
			}
			va_end(argptr);

			int ret;
			ret=ss_run_queue_add(data, argc, argv);
			PRT_TRACE_ERR("ss_run_queue_add : %d",ret);
			ret=ss_core_action_run();
			PRT_TRACE_ERR("ss_core_action_run : %d",ret);
			return 0;
		}
	}

	return 0;
}

int ss_action_entry_call(struct sysnoti *msg, int sockfd)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_action_entry *data;

	EINA_LIST_FOREACH_SAFE(predef_act_list, tmp, tmp_next, data) {
		if ((data != NULL) && (!strcmp(data->type, msg->type))) {
			if (data->is_accessible != NULL
			    && data->is_accessible(sockfd) == 0) {
				PRT_TRACE_ERR
				    ("%d cannot call that predefine module",
				     msg->pid);
				return -1;
			}
			int ret;
			ret=ss_run_queue_add(data, msg->argc, msg->argv);
			PRT_TRACE_ERR("ss_run_queue_add : %d",ret);
			ret=ss_core_action_run();
			PRT_TRACE_ERR("ss_core_action_run : %d",ret);
			return 0;
		}
	}

	PRT_TRACE_EM("[SYSMAN] cannot found action");
	return -1;
}

int ss_run_queue_add(struct ss_action_entry *act_entry, int argc, char **argv)
{
	struct ss_run_queue_entry *rq_entry;
	int i;

	rq_entry = malloc(sizeof(struct ss_run_queue_entry));

	if (rq_entry == NULL) {
		PRT_TRACE_ERR("Malloc failed");
		return -1;
	}

	rq_entry->state = SS_STATE_INIT;
	rq_entry->action_entry = act_entry;
	rq_entry->forked_pid = 0;
	if ( argc < 0 ) {
		rq_entry->argc = 0;
	} else {
		rq_entry->argc = argc;
		for (i = 0; i < argc; i++)
			rq_entry->argv[i] = argv[i];
	}

	run_queue = eina_list_prepend(run_queue, rq_entry);

	PRT_TRACE_EM("[SYSMAN] new action called : %s", act_entry->type);
	return 0;
}

int ss_run_queue_run(enum ss_run_state state,
		     int (*run_func) (void *, struct ss_run_queue_entry *),
		     void *user_data)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_run_queue_entry *rq_entry;

	EINA_LIST_FOREACH_SAFE(run_queue, tmp, tmp_next, rq_entry) {
		if ((rq_entry != NULL) && (rq_entry->state == state))
			run_func(user_data, rq_entry);
	}

	return 0;
}

struct ss_run_queue_entry *ss_run_queue_find_bypid(int pid)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_run_queue_entry *rq_entry;

	EINA_LIST_FOREACH_SAFE(run_queue, tmp, tmp_next, rq_entry) {
		if ((rq_entry != NULL) && (rq_entry->forked_pid == pid))
			return rq_entry;
	}

	return NULL;
}

int ss_run_queue_del(struct ss_run_queue_entry *entry)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_run_queue_entry *rq_entry;
	int i;

	EINA_LIST_FOREACH_SAFE(run_queue, tmp, tmp_next, rq_entry) {
		if ((rq_entry != NULL) && (rq_entry == entry)) {
			run_queue = eina_list_remove(run_queue, rq_entry);
			PRT_TRACE_EM("[SYSMAN] action deleted : %s",
				     rq_entry->action_entry->type);
			for (i = 0; i < rq_entry->argc; i++) {
				if (rq_entry->argv[i])
					free(rq_entry->argv[i]);
			}
			free(rq_entry);
		}
	}

	return 0;
}

int ss_run_queue_del_bypid(int pid)
{
	Eina_List *tmp;
	Eina_List *tmp_next;
	struct ss_run_queue_entry *rq_entry;
	int i;

	EINA_LIST_FOREACH_SAFE(run_queue, tmp, tmp_next, rq_entry) {
		if ((rq_entry != NULL) && (rq_entry->forked_pid == pid)) {
			run_queue = eina_list_remove(run_queue, rq_entry);
			PRT_TRACE_EM("[SYSMAN] action deleted : %s",
				     rq_entry->action_entry->type);
			for (i = 0; i < rq_entry->argc; i++) {
				if (rq_entry->argv[i])
					free(rq_entry->argv[i]);
			}
			free(rq_entry);
		}
	}

	return 0;
}
