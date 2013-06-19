/*
 * power-manager
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

#include <Ecore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "core.h"
#include "battery.h"

#define CHARGING_STATE(x)	((x) & CHRGR_FLAG)
#define FULL_CAPACITY_RAW	(10000)
#define FULL_CAPACITY		(100)
#define BATTERY_FULL_THRESHOLD	(98)
#define MAX_COUNT_UNCHARGING	(10)
#define MAX_COUNT_CHARGING	(10)
#define PRINT_ALL_BATT_NODE(x)	/*print_all_batt_node(x)*/

int (*get_battery_capacity)();

enum state_b {
	B_UNCHARGING = 0,
	B_CHARGING = 1,
	B_END = 2
};

typedef struct _batt_node {
	time_t clock;
	int capacity;
	struct _batt_node *preview;
	struct _batt_node *next;
} Batt_node;

enum state_a {
	A_TIMETOEMPTY = 0,
	A_TIMETOFULL = 1,
	A_END = 2
};

static Ecore_Timer *timeout_id = NULL;
static int noti_fd = 0;

static Batt_node *batt_head[B_END];
static Batt_node *batt_tail[B_END];
static int MAX_VALUE_COUNT[B_END] = {MAX_COUNT_UNCHARGING, MAX_COUNT_CHARGING};
static double avg_factor[B_END] = {-1.0, -1.0};
static int full_capacity = 0;
static int old_capacity = 0;
static int charging_state = 0;
static int multiply_value[B_END] = {-1, 1};
extern int system_wakeup_flag;

static void print_all_batt_node(enum state_b b_index)
{
	Batt_node *node = NULL;
	int cnt = 0;

	LOGINFO("print_all_batt_node [%d]", b_index);

	if(b_index < 0 || b_index >= B_END)
		return;

	if(batt_head[b_index] == NULL)
		return;

	node = batt_head[b_index];
	while(node != NULL) {
		cnt++;
		LOGINFO("[%d] capacity %5d, time %s", cnt, node->capacity,
				ctime(&node->clock));
		node = node->next;
	}
}

static int check_value_validity(enum state_b b_index,time_t clock,int capacity)
{
	time_t old_clock = 0;
	int old_capacity = 0;
	int capadiff = 0;

	if(b_index < 0 || b_index >= B_END)
		return -1;

	if(batt_head[b_index] == NULL)
		return 0;

	old_capacity = batt_head[b_index]->capacity;

	if(system_wakeup_flag == true) {
		LOGERR("check value validity : invalid cuz system suspend!");
		system_wakeup_flag = false;
		return -1;
	}
	/* capacity */
	capadiff = capacity - old_capacity;
	if((capadiff * multiply_value[b_index]) <= 0) {
		LOGERR("check value validity : capadiff(%d) wrong!", capadiff);
		return -1;
	}
	return 0;
}

static int add_batt_node(enum state_b b_index, time_t clock, int capacity)
{
	Batt_node *node = NULL;

	PRINT_ALL_BATT_NODE(b_index);

	if(b_index < 0 || b_index >= B_END)
		return -1;

	node = (Batt_node *) malloc(sizeof(Batt_node));
	if(node == NULL) {
		LOGERR("Not enough memory, add battery node fail!");
		return -1;
	}

	node->clock = clock;
	node->capacity = capacity;

	if(batt_head[b_index] == NULL && batt_tail[b_index] == NULL) {
		batt_head[b_index] = batt_tail[b_index] = node;
		node->preview = NULL;
		node->next = NULL;
	} else {
		node->next = batt_head[b_index];
		node->preview = NULL;
		batt_head[b_index]->preview = node;
		batt_head[b_index] = node;
	}
	PRINT_ALL_BATT_NODE(b_index);
	return 0;
}

static int reap_batt_node(enum state_b b_index, int max_count)
{
	Batt_node *node = NULL;
	Batt_node *tmp = NULL;
	int cnt = 0;

	PRINT_ALL_BATT_NODE(b_index);

	if(b_index < 0 || b_index >= B_END)
		return -1;

	if(max_count <= 0)
		return -1;

	node = batt_head[b_index];

	while(node != NULL) {
		if(cnt >= max_count) break;
		cnt++;
		node = node->next;
	}

	if(node != NULL && node != batt_tail[b_index]) {
		batt_tail[b_index] = node;
		node = node->next;
		batt_tail[b_index]->next = NULL;
		while(node != NULL) {
			tmp = node;
			node = node->next;
			free(tmp);
		}
	}
	PRINT_ALL_BATT_NODE(b_index);
	return 0;
}

static int del_all_batt_node(enum state_b b_index)
{
	Batt_node *node = NULL;

	PRINT_ALL_BATT_NODE(b_index);

	if(b_index < 0 || b_index >= B_END)
		return -1;
	if(batt_head[b_index] == NULL)
		return 0;

	while(batt_head[b_index] != NULL) {
		node = batt_head[b_index];
		batt_head[b_index] = batt_head[b_index]->next;
		free(node);
	}
	batt_tail[b_index] = NULL;
	PRINT_ALL_BATT_NODE(b_index);
	return 0;
}

static float update_factor(enum state_b b_index)
{
	Batt_node *node = NULL;
	double factor = 0.0;
	double total_factor = 0.0;
	int cnt = 0;
	double timediff = 0.0;
	double capadiff = 0.0;

	if(b_index < 0 || b_index >= B_END)
		return 0;

	if(batt_head[b_index] == NULL || batt_head[b_index]->next == NULL)
		return  avg_factor[b_index];

	node = batt_head[b_index];
	while(1) {
		timediff = difftime(node->clock, node->next->clock);
		capadiff = node->capacity - node->next->capacity;
		if(capadiff < 0)
			capadiff*=(-1);
		if(capadiff != 0)
			factor = timediff / capadiff;
		total_factor += factor;

		node = node->next;
		cnt++;

		/*LOGINFO("[%d] timediff(%lf) / capadiff(%lf) = factor(%lf)",
			cnt, timediff, capadiff, factor);*/
		factor = 0.0;

		if(node == NULL || node->next == NULL)
			break;
		if(cnt >= MAX_VALUE_COUNT[b_index]) {
			reap_batt_node(b_index, MAX_VALUE_COUNT[b_index]);
			break;
		}
	}
	LOGINFO(" sum = %lf", total_factor);
	total_factor /= (float)cnt;
	LOGINFO(" avg_factor = %lf", total_factor);

	return total_factor;
}

static void update_time(enum state_a a_index, int seconds)
{
	int clock;

	if(a_index < 0 || a_index >= A_END)
		return;

	if(seconds <= 0)
		return;

	switch(a_index) {
		case A_TIMETOFULL:
			vconf_set_int(VCONFKEY_PM_BATTERY_TIMETOFULL,
				seconds);
			LOGINFO("update time[%d,%d]", a_index, seconds);
			break;
		case A_TIMETOEMPTY:
			vconf_set_int(VCONFKEY_PM_BATTERY_TIMETOEMPTY,
				seconds);
			LOGINFO("update time[%d,%d]", a_index, seconds);
			break;
	}
}

int battinfo_calculation(void)
{
	time_t clock;
	int capacity = 0;
	int estimated_time = 0;
	int tmp = 0;

	capacity = get_battery_capacity();

	if(capacity <= 0)
		return -1;
	if(capacity == old_capacity)
		return 0;

	old_capacity = capacity;

	if(get_charging_status(&tmp) == 0)
		charging_state = (tmp > 0 ? EINA_TRUE : EINA_FALSE);

	clock = time(NULL);
	if(charging_state == EINA_TRUE) {
		del_all_batt_node(B_UNCHARGING);
		if((capacity * 100 / full_capacity)
				>= BATTERY_FULL_THRESHOLD) {
			if(battery_ops.get_charge_full()) {
				del_all_batt_node(B_CHARGING);
				LOGINFO("battery fully charged!");
				update_time(A_TIMETOFULL, 0);
				return 0;
			}
		}
		if(batt_head[B_CHARGING] == NULL) {
			add_batt_node(B_CHARGING, clock, capacity);
		} else {
			add_batt_node(B_CHARGING, clock, capacity);
			avg_factor[B_CHARGING] = update_factor(B_CHARGING);
		}
		estimated_time = (float)(full_capacity - capacity) *
				avg_factor[B_CHARGING];
		update_time(A_TIMETOFULL, estimated_time);
	} else {
		del_all_batt_node(B_CHARGING);
		if(system_wakeup_flag == true) {
			del_all_batt_node(B_UNCHARGING);
			system_wakeup_flag = false;
		}
		if(batt_head[B_UNCHARGING] == NULL) {
			add_batt_node(B_UNCHARGING, clock, capacity);
		} else {
			add_batt_node(B_UNCHARGING, clock, capacity);
			avg_factor[B_UNCHARGING] = update_factor(B_UNCHARGING);
		}
		estimated_time = (float)capacity * avg_factor[B_UNCHARGING];
		update_time(A_TIMETOEMPTY, estimated_time);
	}
	return 0;
}

static Eina_Bool battinfo_cb(void *data)
{
	battinfo_calculation();
	return ECORE_CALLBACK_RENEW;
}

static int init_battery_func(void)
{
	int ret = -1;

	ret = battery_ops.get_capacity_raw();
	if(ret >= 0) {
		get_battery_capacity = battery_ops.get_capacity_raw;
		full_capacity = FULL_CAPACITY_RAW;
		LOGINFO("init_battery_func : full capacity(%d)", full_capacity);
		return 0;
	}

	ret = battery_ops.get_capacity();
	if(ret >= 0) {
		get_battery_capacity = battery_ops.get_capacity;
		full_capacity = FULL_CAPACITY;
		LOGINFO("init_battery_func : full capacity(%d)", full_capacity);
		return 0;
	}

	LOGERR("init_battery_func : fail to get battery info!");
	return -1;
}

int start_battinfo_gathering(int timeout)
{
	int ret;

	LOGINFO("Start battery gathering!");

	if(timeout < 0) {
		LOGERR("invalid timeout value [%d]!", timeout);
		return -1;
	}
	if(init_battery_func() != 0)
		return -1;

	old_capacity = 0;
	battinfo_calculation();

	if(timeout > 0)	{
		/* Using g_timer for gathering battery info */
		timeout_id = ecore_timer_add(timeout,
			(Ecore_Task_Cb)battinfo_cb, NULL);
	} else if(timeout == 0) {
		/* Using heynoti from system-server(udev)
					for gathering battery info */
	        if((noti_fd = heynoti_init()) < 0) {
	                LOGERR("heynoti init failed!");
	                return -1;
	        }
		ret = heynoti_subscribe(noti_fd, "device_charge_chgdet",
				(void *)battinfo_calculation, (void *)NULL);
		if(ret != 0) {
			LOGERR("heynoti subscribe fail!");
			return -1;
		}

		ret = heynoti_attach_handler(noti_fd);
		if(ret != 0) {
			LOGERR("heynoti attach handler fail!");
			return -1;
		}
	}
	return 0;
}

void end_battinfo_gathering(void)
{
	LOGINFO("End battery gathering!");

	if (!timeout_id) {
		ecore_timer_del(timeout_id);
		timeout_id = NULL;
	}
	if (noti_fd > 0) {
		heynoti_close(noti_fd);
		noti_fd = 0;
	}

	del_all_batt_node(B_UNCHARGING);
	del_all_batt_node(B_CHARGING);
}

