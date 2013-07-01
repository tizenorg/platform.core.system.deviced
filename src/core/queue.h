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


#ifndef __SS_QUEUE_H__
#define __SS_QUEUE_H__

#include "sysnoti.h"

struct ss_action_entry {
	int owner_pid;
	void *handle;
	char *type;
	char *path;
	int (*predefine_action) ();
	int (*ui_viewable) ();
	int (*is_accessible) (int caller_sockfd);
};

enum ss_run_state {
	SS_STATE_INIT,
	SS_STATE_RUNNING,
	SS_STATE_DONE
};

struct ss_run_queue_entry {
	enum ss_run_state state;
	struct ss_action_entry *action_entry;
	int forked_pid;
	int argc;
	char *argv[SYSMAN_MAXARG];
};

int ss_action_entry_add_internal(char *type,
				 int (*predefine_action) (),
				 int (*ui_viewable) (),
				 int (*is_accessible) (int));
int ss_action_entry_add(struct sysnoti *msg);
int ss_action_entry_call_internal(char *type, int argc, ...);
int ss_action_entry_call(struct sysnoti *msg, int sockfd);

int ss_run_queue_run(enum ss_run_state state,
		     int (*run_func) (void *, struct ss_run_queue_entry *),
		     void *user_data);

struct ss_run_queue_entry *ss_run_queue_find_bypid(int pid);
int ss_run_queue_add(struct ss_action_entry *act_entry, int argc, char **argv);
int ss_run_queue_del(struct ss_run_queue_entry *entry);
int ss_run_queue_del_bypid(int pid);

#endif /* __SS_QUEUE_H__ */
