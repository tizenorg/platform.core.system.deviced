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


/**
 * @file	lock-detector.c
 * @brief
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <Eina.h>

#include "util.h"
#include "core.h"
#include "core/list.h"

struct lock_info {
	unsigned long hash;
	char *name;
	int state;
	int count;
	long locktime;
	long unlocktime;
	long time;
};

#define LIMIT_COUNT	128

static Eina_List *lock_info_list;

static long get_time(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (long)(now.tv_sec * 1000 + now.tv_usec / 1000);
}

static void shrink_lock_info_list(void)
{
	Eina_List *l, *l_prev;
	struct lock_info *info;
	unsigned int count;

	count = eina_list_count(lock_info_list);
	if (count <= LIMIT_COUNT)
		return;
	_D("list is shrink : count %d", count);

	EINA_LIST_REVERSE_FOREACH_SAFE(lock_info_list, l, l_prev, info) {
		if (info->locktime == 0) {
			EINA_LIST_REMOVE_LIST(lock_info_list, l);
			if (info->name)
				free(info->name);
			free(info);
			count--;
		}
		if (count <= (LIMIT_COUNT / 2))
			break;
	}
}

int set_lock_time(const char *pname, int state)
{
	struct lock_info *info;
	Eina_List *l;
	unsigned long val;

	if (!pname)
		return -EINVAL;

	if (state < S_NORMAL || state > S_SLEEP)
		return -EINVAL;

	val = eina_hash_superfast(pname, strlen(pname));

	EINA_LIST_FOREACH(lock_info_list, l, info)
		if (info->hash == val && info->state == state) {
			info->count += 1;
			if (info->locktime == 0)
				info->locktime = get_time();
			info->unlocktime = 0;
			EINA_LIST_PROMOTE_LIST(lock_info_list, l);
			eina_list_data_set(l, info);
			return 0;
		}

	info = malloc(sizeof(struct lock_info));
	if (!info) {
		_E("Malloc is failed for lock_info!");
		return -ENOMEM;
	}

	info->hash = val;
	info->name = strndup(pname, strlen(pname));
	info->state = state;
	info->count = 1;
	info->locktime = get_time();
	info->unlocktime = 0;
	info->time = 0;

	EINA_LIST_APPEND(lock_info_list, info);

	return 0;
}

int set_unlock_time(const char *pname, int state)
{
	bool find = false;
	long diff;
	struct lock_info *info;
	Eina_List *l;
	unsigned long val;

	if (!pname)
		return -EINVAL;

	if (state < S_NORMAL || state > S_SLEEP)
		return -EINVAL;

	val = eina_hash_superfast(pname, strlen(pname));

	EINA_LIST_FOREACH(lock_info_list, l, info)
		if (info->hash == val && info->state == state) {
			EINA_LIST_PROMOTE_LIST(lock_info_list, l);
			find = true;
			break;
		}

	if (!find)
		return -EINVAL;

	if (info->locktime == 0)
		return -EINVAL;

	/* update time */
	info->unlocktime = get_time();
	diff = info->unlocktime - info->locktime;
	if (diff > 0)
		info->time += diff;
	info->locktime = 0;

	eina_list_data_set(l, info);

	if (eina_list_count(lock_info_list) > LIMIT_COUNT)
		shrink_lock_info_list();

	return 0;
}

void free_lock_info_list(void)
{
	Eina_List *l, *l_next;
	struct lock_info *info;

	if (!lock_info_list)
		return;

	EINA_LIST_FOREACH_SAFE(lock_info_list, l, l_next, info) {
		EINA_LIST_REMOVE(lock_info_list, l);
		if (info->name)
			free(info->name);
		free(info);
	}
	lock_info_list = NULL;
}

void print_lock_info_list(int fd)
{
	struct lock_info *info;
	Eina_List *l;
	char buf[255];

	if (!lock_info_list)
		return;

	snprintf(buf, sizeof(buf),
	    "current time : %u ms\n", get_time());
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf),
	    "[%10s %6s] %6s %10s %10s %10s %s\n", "hash", "state",
	    "count", "locktime", "unlocktime", "time", "process name");
	write(fd, buf, strlen(buf));

	EINA_LIST_FOREACH(lock_info_list, l, info) {
		long time = 0;
		if (info->locktime != 0 && info->unlocktime == 0)
			time = get_time() - info->locktime;
		snprintf(buf, sizeof(buf),
		    "[%10u %6d] %6d %10u %10u %10u %s\n",
		    info->hash,
		    info->state,
		    info->count,
		    info->locktime,
		    info->unlocktime,
		    info->time + time,
		    info->name);
		write(fd, buf, strlen(buf));
	}
}

