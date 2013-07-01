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


#ifndef __PROC_HANDLER_H__
#define __PROC_HANDLER_H__

#define OOMADJ_SU                       (-17)
#define OOMADJ_INIT                     (-16)
#define OOMADJ_FOREGRD_LOCKED           (-15)
#define OOMADJ_FOREGRD_UNLOCKED         (-10)
#define OOMADJ_BACKGRD_LOCKED           (-5)
#define OOMADJ_BACKGRD_UNLOCKED         (1)

#define OOMADJ_APP_LIMIT		(-16)

int get_app_oomadj(int pid, int *oomadj);
int set_app_oomadj(int pid, int new_oomadj);

#endif /* __PROC_HANDLER_H__ */
