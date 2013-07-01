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
#include <sysman.h>

int SS_PREDEFINE_ACT_FUNC(int argc, char **argv)
{
	int i;
	printf("kqwekrqkwerqwer\n");
	for (i = 0; i < argc; i++)
		printf("%s\n", argv[i]);
	return 0;
}

int SS_IS_ACCESSABLE_FUNC(int pid)
{
	printf("qwerqerqewr %d\n", pid);
	return 1;
}

int SS_UI_VIEWABLE_FUNC()
{
	printf("kakak viewable\n");
	return 1;
}
