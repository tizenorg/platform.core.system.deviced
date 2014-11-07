/*
 *  system-server
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: DongGi Jang <dg0402.jang@samsung.com>
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
 *
*/ 


#include <stdio.h>
#include <signal.h>
#include <sys/reboot.h>

int main(int argc, char *argv[])
{
	launch_evenif_exist("/etc/rc.d/rc.shutdown", "&");
        sleep(1);
        sync();
	reboot(RB_AUTOBOOT);
        return 0;
}
