/*
 * devman
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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
 *
 * @ingroup SLP_PG
 * @defgroup SLP_PG_DEVMAN Device Manager
 * @{

<h1 class="pg">Introduction</h1>

<h2 class="pg">Purpose</h2>
The purpose of this document is to describe how applications can use Device Manager APIs. This document gives programming guidelines to application engineers.

<h2 class="pg">Scope</h2>
The scope of this document is limited to Device Manager API usage.

<br>
<h1 class="pg">Device Manager Library Overview</h1>
<h2 class="pg">General Device Manager</h2>
Device Manager library is provided to control the device and to get data about several devices. You can get the data about battery, charger, display and so on.<br>
Devman library uses sysfs for interfaces with device drivers and kernel. sysfs is a virtual file system provided by Linux 2.6 or above. Please refer to the web site,
http://www.kernel.org/pub/linux/kernel/people/mochel/doc/papers/ols-2005/mochel.pdf for more information about sysfs.
The following figure shows the basic architecture of devman library

<h3 class="pg" align="center">Device Manager Architecture</h3>
@image html SLP_devman_PG_architecture.png
<br>
<h2 class="pg">Haptic Device Manager</h2>
<h3 class="pg" align="center">Haptic Vibration Functional Level Architecture</h3>
@image html SLP_devman_PG_haptic_architecture.png

The client side is available in the form of a shared library to all the processes, whereas the server is a daemon.
As shown in the diagram applications/middleware frameworks can have the haptic vibration client library in the process context.
<br><br>
The haptic vibration client is available in form of a shared library. This library has APIs which support various features.
These features are playing a rhythmical vibration pattern, playing a monotonous vibration pattern, etc.
Applications can call these APIs to give vibration feedback on any event. This could also be used to indicate any events or changes of a state to the user.
<br><br>
The server interacts with the device driver interfaces and generates vibration feedback.
<br><br>
<h3 class="pg">Haptic Vibration Features</h3>
<b>Haptic client features</b>
-#	Available in shared library form.<br>
-#	Provides set of APIs to play haptic vibration patterns.<br>
-#	Provide unique access control mechanism through client server architecture.<br>
-#	Provision to play rhythmical vibration patterns.<br>
-#	Provides functionality for application to play a different vibration pattern for a different event.<br>
-#	Provides support for user defined duration for monotone playing or iteration for pattern playing.<br>
-#	Provides immediate stop control over the vibrations played.<br><br>

<b>Haptic server features</b>
-#	The actual implementation of the functionality supported by haptic-vibration library has been implemented through haptic-server.<br>
-#	Haptic-server processes the vibration pattern's play/stop requests sent by application through the haptic client library.<br>
-#	There is an internal database maintained for different patterns and their specification.<br>
-#	When application requests a particular pattern, haptic server checks this database for the specification of that pattern.<br>
-#	After getting data from database, server sends these specific details to device driver which plays the pattern.<br>
-#	Server can maintain multiple requests at a time on a priority basis.<br><br>

<h1 class="pg">Device Manager Funtions</h1>

<h2 class="pg">General Device Manager APIs</h2>
<i><b>API : device_get_battery_pct</b></i><br>
<b>Parameter :</b> void<br>
<b>Return :</b> int<br>
<b>Functionality :</b> This API is used to get the remaining battery percentage. On success, integer value(0~100) that indicate remaining battery percentage is returned.
Or a negative value (-1) is returned on failure.
<br><br>
<i><b>API : device_is_battery_full</b></i><br>
<b>Parameter :</b> void<br>
<b>Return :</b> int <br>
<b>Functionality :</b> This API is used to get the fully charged status of battery. On fully charged, the integer value (1) is returned,
(0) is returned if battery is not fully charged, a negative value (-1) is returned on failure.
<br><br>
<i><b>API : device_get_display_brt</b></i>
<br><b>Parameter In :</b> display_num_t num
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to get the current brightness of the display from sysfs.
The function returns the current brightness value on success and a negative value (-1) on failure.
<br><br>
<b>Enumerate values:</b>
@code
// Display number
typedef enum {
	DEV_DISPLAY_0,
	DEV_DISPLAY_1,
	DEV_MAIN_DISPLAY	= DEV_DISPLAY_0,
} display_num_t;
@endcode

<i><b>API : device_set_display_brt</b></i>
<br><b>Parameter In :</b> display_num_t num
<br><b>Parameter In :</b> int val
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to set the current brightness of the display using sysfs.
The parameter val should be set as a brightnesss value of your target.
The function returns the current brightness value on success and a negative value (-1) on failure.
<br><br>
<i><b>API : device_get_display_gamma</b></i>
<br></b>Parameter In :</b> display_num_t num
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to get the current gamma value of the display from sysfs.
The function returns the current brightness value on success and a negative value (-1) on failure.
<br><br>
<i><b>API : device_set_display_brt</b></i>
<br><b>Parameter In :</b> display_num_t num
<br><b>Parameter In :</b> int val
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to set the current brightness of the display using sysfs.
The parameter val should be set as a brightnesss value of your target.
The function returns the current brightness value on success and a negative value (-1) on failure.
<br><br>
<i><b>API : device_get_display_gamma</b></i>
<br><b>Parameter In :</b> display_num_t num
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to get the current gamma value of the display from sysfs.
The function returns the current brightness value on success and a negative value (-1) on failure.
<b>Enumerate values:</b>
@code
// LCD gamma values
typedef enum {
	LCD_GAMMA_22 = 1,            // 8500K , 2.2 GAMMA
	LCD_GAMMA_19 = 2,            // 8500K , 1.9 GAMMA
	LCD_GAMMA_17 = 3,            // 8500K , 1.7 GAMMA
	LCD_GAMMA_NORMAL =	LCD_GAMMA_22, // Normal screen
	LCD_GAMMA_PLAY_RECORD =	LCD_GAMMA_19, // Playing or recording cam
	LCD_GAMMA_PREVIEW =	LCD_GAMMA_17, // Preview
	LCD_GAMMA_MOVIE = 	LCD_GAMMA_19, // Movie
	LCD_GAMMA_CAMERA = 	11,		  // Camera
} display_gamma_t;
@endcode

<i><b>API : device_power_suspend</b></i>
<br><b>Parameter :</b> void
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to make the phone go to a suspend (sleep) state.
The suspend state consumes little battery power. If the caller process does not have the permission which is root, it returns failure.
The function returns 0 on success and a negative value (-1) on failure.
<br><br>
<i><b>API : device_get_property</b></i>
<br><b>Parameter In :</b> devtype_t devtype
<br><b>Parameter In :</b> int property
<br><b>Parameter Out :</b>  int *value
<br><b>Return :</b> int
<br><b>Functionality :</b>This generic API is used to get the property values of supported devices.
If the caller process does not have permission, it returns failure.
The function returns 0 on success and a negative value (-1) on failure.
<br><br>
<i><b>API : device_set_property</b></i>
<br><b>Parameter In :</b> devtype_t devtype
<br><b>Parameter In :</b> int property
<br><b>Parameter In :</b> int value
<br><b>Return :</b> int
<br><b>Functionality :</b>This generic API is used to set the property values of supported devices.
If the caller process does not have permission, it returns failure.
The function returns 0 on success and a negative value (-1) on failure.

<h2 class="pg">Haptic Device Manager APIs</h2>
<i><b>API : device_haptic_open</b></i>
<br><b>Parameter In :</b> haptic_dev_idx dev_idx , unsigned int mode
<br><b>Return :</b> int
<br><b>Functionality :</b> This API opens a Haptic-vibration device. On success it returns a dev_handle value.
In case of failure it returns a negative value. If the device is already open it returns (-1). <br>
The first in parameter dev_idx should be from a predefined haptic-device-index which is available in the typedef enum haptic_dev_idx.
The DEV_IDX_0 means first haptic-device-index of target , the DEV_IDX_1 means second haptic-device-index of target and the DEV_IDX_ALL means both of them.
The availability of the dev_idx value is dependent on the real target. Normally, set a DEV_IDX_0 value to the first haptic-device.<br>
The second in parameter mode is reserved for future so just set a 0 value<br>
<b>Note:</b> The device_haptic_open() must be called before all other haptic APIs are called.
The device_haptic_open() should have a matching call to device_haptic_close().
Applications call the device_haptic_open() only once if possible during application startup and call the device_haptic_close() during application shutdown.

<b>Enumerate values:</b>
@code
//Haptic_dev_idx ;
typedef enum haptic_dev_idx_t {
	DEV_IDX_0	 	= 	0x01,
	DEV_IDX_1	 	= 	0x02,
	DEV_IDX_ALL	       = 	0x04,
};
@endcode

<i><b>API : device_haptic_close</b></i>
<br><b>Parameter In :</b> int dev_handle
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b> This API closes a Haptic-vibration device. On success it returns a zero value.
In case of failure it returns a negative value. If the device is already closed it returns (-1).  <br>
The first in parameter dev_handle should be from the return value of device_haptic_open().
<br><br>
<i><b>API : device_haptic_play_pattern</b></i>
<br><b>Parameter In :</b> int dev_handle , int pattern , int iteration , int feedback_level
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b> This API plays a predefined rhythmic haptic-vibration pattern. <br>
The first in parameter dev_handle should be from the return value of device_haptic_open().<br>
The second in parameter pattern should be from a predefined pattern list which is available in an enumeration (effectvibe_pattern_list).
These patterns are rhythmic vibration patterns. <br>
The third in parameter iteration sets the number of iterations to be played. This should be less than the maximum iteration range set for the device (currently its 255).  <br>
The fourth in parameter is the vibration feedback intensity level. This level is already predefined by enumeration type value from HAPTIC _FEEDBACK_LEVEL_1
to HAPTIC _FEEDBACK_LEVEL_5. If you want to use the value selected by the user in the Setting application menu, just set -1 value.<br>
On success it returns a zero value. In case of failure it returns a negative value. <br>
<b>Note:</b> The actual behavior of the feedback play pattern and the intensity depends on the target hardware.

<b>Enumerate values:</b>
@code
//Effectvibe_pattern_list
enum effectvibe_pattern_list {
	EFFCTVIBE_TOUCH = 0,
	EFFCTVIBE_HW_TOUCH,
	EFFCTVIBE_NOTIFICATION,
	EFFCTVIBE_INCOMING_CALL01,
	EFFCTVIBE_INCOMING_CALL02,
	EFFCTVIBE_INCOMONG_CALL03,
	EFFCTVIBE_ALERTS_CALL,
	EFFCTVIBE_OPERATION,
	EFFCTVIBE_SILENT_MODE,
	EFFCTVIBE_PATTERN_END
};

//Feedback Level ;
enum {
	HAPTIC_FEEDBACK_LEVEL_AUTO = -1,
	HAPTIC_FEEDBACK_LEVEL_1 = 1,
	HAPTIC_FEEDBACK_LEVEL_2 = 2,
	HAPTIC_FEEDBACK_LEVEL_3 = 3,
	HAPTIC_FEEDBACK_LEVEL_4 = 4,
	HAPTIC_FEEDBACK_LEVEL_5 = 5,
};

//definition for infinite iteration ;
#define HAPTIC_INFINITE_ITERATION	256
@endcode

<i><b>API : device_haptic_play_file</b></i>
<br><b>Parameter In :</b> int dev_handle , const char *file_name , int iteration , int feedback_level
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b>This API plays a predefined rhythmic haptic-vibration pattern file (only supports .ivt type file, Immersion VibeTonz).<br>
The first in parameter dev_handle should be from the return value of device_haptic_open().<br>
The second in parameter file_name sets rhythmic vibration pattern file with path. It only supports .ivt type pattern file. <br>
The third in parameter iteration sets the number of iterations to be played. This should be less than the maximum iteration range set for the device (currently its 255).
If you want to play indefinitely, use HAPTIC_INFINITE_ITERATION defined value. But it depends on the target hardware.<br>
The fourth in parameter is the vibration feedback intensity level. This level is already predefined by enumeration type value from HAPTIC _FEEDBACK_LEVEL_1
to HAPTIC _FEEDBACK_LEVEL_5. If you want to use the value selected by the user in the Setting application menu, just set HAPTIC_FEEDBACK_LEVEL_AUTO value.
(But the application must have a main loop to use the HAPTIC_FEEDBACK_LEVEL_AUTO value ) <br>
On success it returns a zero value. In case of failure it returns a negative value. <br>
<b>Note:</b> The actual behavior of the feedback play pattern and the intensity depends on the target hardware.
<br><br>
<i><b>API : device_haptic_play_monotone</b></i>
<br><b>Parameter In :</b> int dev_handle ,  int duration
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b>This API plays a monotonous haptic-vibration pattern with a constant intensity.
In this monotone play, the intensity used is the value that the user has selected in the Setting application menu.<br>
The first in parameter dev_handle should be from the return value of device_haptic_open().<br>
The second in parameter duration defines the length of time this vibration should be played. This duration is in milliseconds.  <br>
On success it returns a zero value. In case of failure it returns a negative value. <br>
<b>Note:</b> The actual behavior of the feedback played and the intensity depends on the target hardware.
<br><br>
<i><b>API : device_haptic_stop_play</b></i>
<br><b>Parameter In :</b> int dev_handle
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b> This API stops the current vibration being played.<br>
The first in parameter dev_handle should be from the return value of device_haptic_open().<br>
On success it returns a zero value. In case of failure it returns a negative value.
<br><br>
<i><b>API : device_haptic_get_pattern_duration</b></i>
<br><b>Parameter In :</b> int dev_handle ,  int pattern
<br><b>Parameter Out :</b> int *duration
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b>This API gets a duration time value from a predefined rhythmic vibration pattern.<br>
The first in parameter dev_handle should be from the return value of device_haptic_open().<br>
The second in parameter pattern should be from a predefined pattern list which is available in an enumeration (effectvibe_pattern_list).<br>
The application can get a duration time value from the third out parameter duration when this API succeeds. The unit of duration is ms (millisecond)<br>
On success it returns a zero value. In case of failure it returns a negative value. <br>
<b>Note:</b> The actual behavior of the feedback played and the intensity depends on the target hardware.
<br><br>
<i><b>API : device_haptic_get_file_duration</b></i>
<br><b>Parameter In :</b> int dev_handle ,  const char *file_name
<br><b>Parameter Out :</b> int *duration
<br><b>Parameter Return :</b> int
<br><b>Functionality :</b>This API gets a duration time value from a predefined rhythmic vibration pattern file (only supports .ivt type file).<br>
The first in parameter dev_handle should be from the return value of device_haptic_open().<br>
The second in parameter file_name sets rhythmic vibration pattern file with path. It only supports .ivt type pattern file.<br>
The application can get a duration time value from the third out parameter duration when this API succeeds. The unit of duration is ms (millisecond)<br>
On success it returns a zero value. In case of failure it returns a negative value. <br>
<b>Note:</b>The actual behavior of the feedback played and the intensity depends on the target hardware.<br>

<br><b>Sample Code <Simple program showing how to use haptic-vibration APIs></b>
@code
#include <stdio.h>
#include <devman_haptic.h>
#define HAPTIC_TEST_ITERATION 10

int main()
{
	int ret_val=0;
	int dev_handle;

	printf("\n Haptic vibration test : Start of the program \n");

	//Open the haptic device
	dev_handle = device_haptic_open(DEV_IDX_0,0);
	if(dev_handle < 0)
		return -1;

	//Play a rhythmic pattern
	ret_val = device_haptic_play_pattern(dev_handle, EFFCTVIBE_NOTIFICATION,
			HAPTIC_TEST_ITERATION , HAPTIC_FEEDBACK_LEVEL_3);
	if(ret_val !=0)
		return -1;

	//Play a monotone pattern for 1s == 1000ms
	ret_val = device_haptic_play_monotone(dev_handle, 1000);
	if(ret_val !=0)
		return -1;

	//Demo for a stop pattern API, playing a monotone for 10s
	ret_val = device_haptic_play_monotone(dev_handle, 10000);
	if(ret_val !=0)
		return -1;

	sleep(1);

	//Stop the pattern immediately
	ret_val = device_haptic_stop_play(dev_handle);
	if(ret_val !=0)
		return -1;

	//Close the device
	ret_val = device_haptic_close(dev_handle);
	if(ret_val !=0)
		return -1;
}
@endcode

 @}
**/
