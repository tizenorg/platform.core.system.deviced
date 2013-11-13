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


/**
 *
 * @ingroup SLP_PG
 * @defgroup SLP_PG_SYSMAN System Manager
 * @{
 
<h1 class="pg">Introduction</h1>

<h2 class="pg">Purpose</h2>
The purpose of the document is to describe how applications can use the System Manager APIs. This document gives programming guidelines to application engineers. 

<h2 class="pg">Scope</h2>
The scope of this document is limited to System Manager API usage.

<br>
<h1 class="pg">System Manager Library Overview</h1>
Sysman library provides convenience functions to get data about processes or to run processes. 
It reads the proc file system to check whether a process exists or not, gets a command line and execution path, gets the pid of a process etc. 
Some APIs of the sysman library use some device files also. If processes which call APIs of sysman library don't have the correct permission, they will fail. 
Most functions return -1 on failure and the errno will be set.<br>
Please refer the manual pages of proc(Linux Programmer's Manual - PROC(5) http://www.kernel.org/doc/man-pages/man5/proc.5.html) 
or errno(Linux Programmer's Manual - ERRNO(3) http://www.kernel.org/doc/man-pages/man3/errno.3.html) for more details.

<h1 class="pg">System Manager Funtions</h1>

<h2 class="pg">System Manager APIs</h2>
<i><b>API : sysman_get_pid</b></i>
<br><b>Parameter In :</b> const char *execpath
<br><b>Return :</b> int 
<br><b>Functionality :</b> This API is used to get the pid of the process which has the specified execpath.<br>
Internally, this API searches /proc/{pid}/cmdline and compares the parameter execpath with 1st argument of cmdline. 
If there is no process that has same execpath in /proc/{pid}/cmdline, it will return (-1). 
<br><br>
<i><b>API : sysman_get_cmdline_name</b></i>
<br><b>Parameter In :</b> pid_t pid
<br><b>Parameter Out :</b> char *cmdline
<br><b>Parameter In :</b> size_t cmdline_size
<br><b>Return :</b> int 
<br><b>Functionality :</b>This API is used to get the file name in the command line.<br>
Caller process MUST allocate enough memory for the cmdline parameter. Its size should be assigned to cmdline_size.<br>
Internally it reads the 1st argument of /proc/{pid}/cmdline and copies it to cmdline.<br>
The function returns 0 on success and a negative value (-1) on failure.
<br><br>
<i><b>API : sysman_get_apppath</b></i>
<br><b>Parameter In :</b> pid_t pid
<br><b>Parameter Out :</b> char *app_path
<br><b>Parameter In :</b> size_t app_path_size
<br><b>Return :</b> int 
<br><b>Functionality :</b> This API is used to get the execution path of the process specified by the pid parameter.<br>
Caller process MUST allocate enough memory for the app_path parameter. Its size should be assigned to app_path_size.<br>
Internally it reads a link of /proc/{pid}/exe and copies the path to app_path. <br>
The function returns 0 on success and a negative value (-1) on failure.
<br><br>
<i><b>API : sysconf_set_mempolicy</b></i>
<br><b>Parameter In :</b> enum mem_policy mempol
<br><b>Return :</b> int 
<br><b>Functionality :</b> This API is used to set the policy of the caller process for the situation of low available memory.<br>
The function returns 0 on success and a negative value (-1) on failure.<br>
If the caller process has no permission, it will be failed.

<b>Enumerate values</b>
@code
enum mem_policy {
	OOM_LIKELY , // For micelloneous applications 
	OOM_NORMAL , // For fundamental applications 
	OOM_IGNORE  // For daemon 
};
@endcode

 @}
**/
