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


#ifndef __SS_NOTI_H__
#define __SS_NOTI_H__

int ss_noti_getfd(void);
int ss_noti_send(char *filename);
int ss_noti_add(const char *noti, void (*cb) (void *), void *data);
int ss_noti_init(void);

#endif /* __SS_NOTI_H__ */
