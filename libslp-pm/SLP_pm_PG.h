/*
 *  libslp-pm
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


/**
 *
 * @ingroup SLP_PG
 * @defgroup SLP_PG_PM Power Manager
 * @{
 
<h1 class="pg">Introduction</h1>

<h2 class="pg">Purpose</h2>
The purpose of this document is to describe how applications can use Power Manager APIs to save system power consumption. 
This document gives programming guidelines to application engineers. 

<h2 class="pg">Scope</h2>
The scope of this document is limited to Power Manager API usage.

<br>
<h1 class="pg">Power Manager Architecture</h1>
The Power Manager (PM) consists of a client API library and a Power Manager daemon.

<h3 class="pg" align="center">Power Manager Architecture</h3>
@image html SLP_pm_PG_architecture.png
<br>
Power Manager daemon controls a kernel power management module and peripheral device drivers to save system power consumption. 
It manages the system power states and makes state transitions according to the events. <br>
There are four power states ? Normal (i.e. LCD on), LCD dimming, LCD off and Sleep.

<h3 class="pg" align="center">Power Manager State Diagram</h3>
@image html SLP_pm_PG_state_diagram.png

Applications can put conditions on specific state transitions. For example, Video Player applications do not want the Power Manager 
to allow the LCD to be in a dimming state or to turn off the LCD backlight. 
For this purpose, Video Player applications can use Power Manager APIs to send the condition to Power Manager.

<h1 class="pg">Power Manager Features</h1>
- There are four power states, Normal(i.e. LCD on), LCD dimming, LCD off and Sleep.
- If there is no user input for a certain time, PM changes the state of PM to a state that has lower power consumption.
- When user input or another system interrupt occurs, PM rolls the state back immediately.
- However, some applications may want PM not to change the state, for example music players that do not want to suspend, 
can sustain PM in the state required by using the pm_lock_power_state() API.

<h1 class="pg">Power Manager Funtions</h1>

<h2 class="pg">Power Manager API Introduction</h2>
<i><b>API : pm_lock_state</b></i>
<br><b>Parameter In :</b> unsigned int state, unsigned int flag, unsigned int timeout
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to lock a particular power-state as the current power-state.<br>
The parameter state specifies the power state which you want to lock LCD_NORMAL, LCD_DIM, LCD_OFF. \n
The second parameter Flag is set if you want to go the requested lock state directly.\n
The third parameter timeout specifies lock-timeout in milliseconds. 
If the value 0 is selected, the power state remains locked until pm_unlock_state is called.<br>
This function returns 0 on success and a negative value (-1) on failure.
<br><br>
<i><b>API : pm_unlock_state</b></i>
<br><b>Parameter In :</b> unsigned int state, unsigned int flag 
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to Unlock the power state.<br>
The parameter state specifies the power state which you want to unlock .Some examples are LCD_NORMAL, LCD_DIM, LCD_OFF.<br>
The second parameter flag is set if you want to go to the requested state directly after unlocking. <br>
PM_SLEEP_MARGIN - If the current status is lcd off, power-manager reset timer to 5 second. If the current status is not lcd off, power-manager uses the existing timer. <br>
PM_RESET_TIMER - Power-manager resets timer. (lcd normal : reset timer to predfined value which is set in setting module,  lcd dim or off : reset timer to 5 seconds)<br>
PM_KEEP_TIMER - Power-manager uses the existing timer (if timer is already expired, pwoer-manager changes the status) <br>
This is valid only when the current state transition was blocked by the locking and this function call releases the blocking.<br>
This function returns 0 on success and a negative value (-1) on failure.
<br><br>
<i><b>API : pm_change_state</b></i>
<br><b>Parameter In :</b> unsigned int (power state)
<br><b>Return :</b> int
<br><b>Functionality :</b> This API is used to change the power manager state by force.<br>
This function returns 0 on success, -1 if failed.

<b>Power state:</b>
@code
// POWER STATES 
#define LCD_NORMAL	0x1	// NORMAL state 
#define LCD_DIM		0x2	// LCD dimming state 
#define LCD_OFF		0x4	// LCD off state 
#define SUSPEND		0x8	// Sleep state 
@endcode

<h2 class="pg">Sample Code</h2>
@code
#include <pmapi.h>
int main()
{
	int result;

	// Lock current state as LCD_NORMAL 
	result = pm_lock_state(LCD_NORMAL, GOTO_STATE_NOW, 0); 
	if( result < 0 ) {
		printf("[ERROR] return value result =%d, \n",result);
	}
	else
		printf("[SUCCESS]return value result =%d \n",result);

	// Do something here  

	//Un-lock NORMAL state so that power state change can occur with system-events 

	result = pm_unlock_state(LCD_NORMAL, PM_RESET_TIMER); 
	if( result < 0 ) {
		printf("[ERROR] return value result =%d, \n",result);
	}
	else
		printf("[SUCCESS]return value result =%d \n",result);

	// change the state into LCD ON
	result = pm_change_state(LCD_NORMAL);
	if( result < 0 ) 
		printf("[ERROR] return value result =%d, \n",result);
	else
		printf("[SUCCESS]return value result =%d \n",result);

	return 0;
}
@endcode

 @}
**/
