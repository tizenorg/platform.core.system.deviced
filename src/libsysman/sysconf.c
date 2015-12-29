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


#include <stdio.h>
#include <unistd.h>
#include <dd-deviced.h>

#include "sysman-priv.h"

API int sysconf_set_mempolicy_bypid(int pid, enum mem_policy mempol)
{
	ERR("Don't support this api anymore. Please use deviced api");
	return -1;
}

API int sysconf_set_mempolicy(enum mem_policy mempol)
{
	ERR("Don't support this api anymore. Please use deviced api");
	return -1;
}

API int sysconf_set_vip(int pid)
{
	return deviced_conf_set_vip(pid);
}

API int sysconf_is_vip(int pid)
{
	return deviced_conf_is_vip(pid);
}

API int sysconf_set_permanent_bypid(int pid)
{
	return deviced_conf_set_permanent_bypid(pid);
}

API int sysconf_set_permanent()
{
	pid_t pid = getpid();
	return deviced_conf_set_permanent_bypid(pid);
}
