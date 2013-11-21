/*
 * test
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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


#include "test.h"

static int test_main(int argc, char **argv)
{
	test_init((void *)NULL);
	ecore_main_loop_begin();
	test_exit((void *)NULL);
	ecore_shutdown();
	return 0;
}

int main(int argc, char **argv)
{
	ecore_init();
	return test_main(argc, argv);
}

