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


#ifndef __DEVMAN_H__
#define __DEVMAN_H__

#ifndef DEPRECATED
#define DEPRECATED __attribute__((deprecated))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** START: This code related to the opensource api will be removed */
#include "devman_managed.h"
/** END: Will be remved */

/**
 * @file        devman.h
 * @ingroup     DEVICE_MANAGER
 * @brief       This file contains the API for the status of devices
 * @author      Tizen2.0
 * @date        2010-01-24
 * @version     0.1
 */

/**
 * @defgroup DEVICE_MANAGER		Device Manager
 * @ingroup SYSTEM_FRAMEWORK
 * @brief Device Manager library
 *
 * Device manager provides APIs to control devices or to get the status of devices.
 * <br> Use devman.pc and include devman.h and devman_haptic.h files
 *
 * @addtogroup DEVICE_MANAGER
 * @{
 */

/**
 * @brief Enumerations of error code for Devman
 */
typedef enum {
    DEVMAN_ERROR_NONE              = 0,             /**< Successful */
    DEVMAN_ERROR_OPERATION_FAILED  = -1,            /**< Operation failed */
    DEVMAN_ERROR_NOT_SUPPORTED     = -2,            /**< Not supported in this device */
} devman_error_e;

/**
 * @par Description:
 * 	device type enum
 */
	typedef enum devtype_list {
		DEVTYPE_DISPLAY0,                    /**< main screen */
		DEVTYPE_DISPLAY1 = DEVTYPE_DISPLAY0, /**< sub screen */
		DEVTYPE_HAPTIC,                      /**< motor */
		DEVTYPE_JACK,                        /**< jack - Micro USB, 3.5 pi jack etc */
		DEVTYPE_LED,                         /**< LED */
		DEVTYPE_POWERSUPPLY,                 /**< battery, PMIC, etc about power */
		DEVTYPE_MAX,
		DEVTYPE_TOUCHSCREEN = -1,
		DEVTYPE_TOUCHKEY = -1,
		DEVTYPE_GPS = -1,
		DEVTYPE_UART = -1,
		DEVTYPE_MMC = -1,
		DEVTYPE_EFFECT_HAPTIC = -1,
	} devtype_t;

/**
 * @par Description:
 * 	DEVTYPE_POWERSUPPLY property for generic APIs
 */
	enum {
		/**< Current remaining battery */
		POWER_SUPPLY_PROP_CAPACITY,
		/**< Current remaining battery expressed 1/10000 */
		POWER_SUPPLY_PROP_CAPACITY_RAW,
		/**< Battery is full-charged.*/
		POWER_SUPPLY_PROP_CHARGE_FULL,
		/**< Battery is being charged now */
		POWER_SUPPLY_PROP_CHARGE_NOW,
		/**< iBattery status about cahrge */
		POWER_SUPPLY_PROP_BATTERY_HEALTH = 9,
	};

/**
 * @par Description:
 * 	DEVTYPE_DISPLAY0 and DEVTYPE_DISPLAY1 property for generic APIs
 */
	enum {
		DISPLAY_PROP_DISPLAY_COUNT,
		DISPLAY_PROP_BRIGHTNESS,
		DISPLAY_PROP_ACL_CONTROL,
		DISPLAY_PROP_ONOFF,
		DISPLAY_PROP_IMAGE_ENHANCE_MODE = 8,
		DISPLAY_PROP_IMAGE_ENHANCE_SCENARIO,
		DISPLAY_PROP_IMAGE_ENHANCE_TONE,
		DISPLAY_PROP_IMAGE_ENHANCE_OUTDOOR,
		DISPLAY_PROP_IMAGE_ENHANCE_INFO = 13,
		DISPLAY_PROP_MIN_BRIGHTNESS = -1,
		DISPLAY_PROP_MAX_BRIGHTNESS = -1,
		DISPLAY_PROP_GAMMA = -1,
	};

/**
 * @par Description:
 *  DEVTYPE_LED property for generic APIs
 */
	enum {
		LED_PROP_BRIGHTNESS,
		LED_PROP_MAX_BRIGHTNESS,
	};

/**
 * @par Description:
 * 	DEVTYPE_JACK property for generic APIs
 */
	enum {
		JACK_PROP_TA_ONLINE,		/**< Travel Adapter(Charger) */
		JACK_PROP_EARJACK_ONLINE,	/**< Earjack */
		JACK_PROP_EARKEY_PRESS,		/**< */
		JACK_PROP_HDMI_ONLINE,		/**< Digital tvout */
		JACK_PROP_USB_ONLINE,		/**< USB connection */
		JACK_PROP_CRADLE_ONLINE,	/**< Cradle connection */
		JACK_PROP_TVOUT_ONLINE,		/**< analog tvout */
		JACK_PROP_MIC_ONLINE = -1,
		JACK_PROP_USB_PATH = -1,
	};

/**
 * @par Description:
 * 	DEVTYPE_TOUCHKEY property for generic APIs
 */
	enum {
		TCKEY_PROP_FIRST,
		TCKEY_PROP_SECOND,
		TCKEY_PROP_THIRD,
	};

/**
 * @par Description:
 * 	DEVTYPE_UART property for generic APIs
 */
	enum {
		UART_PROP_SELECT_PATH,
	};

/**
 * @par Description:
 * 	DEVTYPE_MMC property for generic APIs
 */
	enum {
		MMC_PROP_FORMAT,
	};

/**
 * @par Description:
 * Motor property for generic APIs
 */
	enum {
		HAPTIC_PROP_LEVEL_MAX,
		HAPTIC_PROP_LEVEL,
		HAPTIC_PROP_ENABLE,
		HAPTIC_PROP_ONESHOT,
	};

/* Application level interfaces */

/**
 * @par Description:
 * 	This API is used to get the remaining battery percentage.\n
 * 	It gets the Battery percentage by calling device_get_property() function.\n
 * 	It returns integer value(0~100) that indicate remaining batterty percentage on success.\n
 * 	Or a negative value(-1) is returned on failure.
 * @return On success, integer value(0~100) is returned.
 *  Or a negative value(-1) is returned on failure.
 * @see device_is_battery_full(), device_get_battery_pct_raw()
 * @par Example
 * @code
 * 	...
 *	int battery;
 *	battery = device_get_battery_pct();
 *	if( battery < 0 )
 *		printf("Fail to get the remaining battery percentage.\n");
 *	else
 *		printf("remaining battery percentage : %d\n", battery);
 *	...
 * @endcode
 */
	int device_get_battery_pct(void);

/**
 * @par Description:
 * 	This API is used to get the remaining battery percentage expressed 1/10000.\n
 * 	It gets the Battery percentage by calling device_get_property() function.\n
 * 	It returns integer value(0~10000) that indicate remaining batterty percentage on success.\n
 * 	Or a negative value(-1) is returned on failure.
 * @return On success, integer value(0~10000) is returned.
 *  Or a negative value(-1) is returned on failure.
 * @see device_is_battery_full(), device_get_battery_pct()
 * @par Example
 * @code
 * 	...
 *	int battery;
 *	battery = device_get_battery_pct_raw();
 *	if( battery < 0 )
 *		printf("Fail to get the remaining battery percentage.\n");
 *	else
 *		printf("remaining battery percentage expressed 1/10000 : %d\n", battery);
 *	...
 * @endcode
 */
	int device_get_battery_pct_raw(void);

/**
 * @par Description:
 * 	This API is used to get the fully charged status of battery.\n
 *  It gets the fully charged status of Battery by calling device_get_property() function.\n
 * 	If the status of battery is full, it returns 1.\n
 * 	Or a negative value(-1) is returned, if the status of battery is not full.
 * @return 1 with battery full, or 0 on not full-charged, -1 if failed
 * @see device_get_battery_pct()
 * @par Example
 * @code
 *	...
 *	if ( device_is_battery_full() > 0 )
 *		printf("battery fully chared\n");
 *	...
 * @endcode
 */
	int device_is_battery_full(void);

#ifndef __DD_BATTERY_H__
/**
 * @par Description:
 *  Battery health status
 */
	enum {
		BAT_UNKNOWN = 0,	/**< */
		BAT_GOOD,			/**< */
		BAT_OVERHEAT,		/**< */
		BAT_DEAD,			/**< */
		BAT_OVERVOLTAGE,	/**< */
		BAT_UNSPECIFIED,	/**< */
		BAT_COLD,			/**< */
		BAT_HEALTH_MAX,		/**< */
	};
#endif

/**
 * @par Description:
 *  This API is used to get the battery health status.\n
 *  It gets the battery health status by calling device_get_property() function.\n
 * 	It returns integer value(0~6) that indicate battery health status on success.\n
 *  Or a negative value(-1) is returned, if the status of battery is not full.
 * @return interger value, -1 if failed\n
 * (0 BATTERY_UNKNOWN, 1 GOOD, 2 OVERHEAT, 3 DEAD, 4 OVERVOLTAGE, 5 UNSPECIFIED, 6 COLD)
 * @par Example
 * @code
 *  ...
 *  int bat_health;
 *  bat_health =device_get_battery_health();
 *  if(bat_health != BAT_GOOD)
 *      printf("battery health is not good\n");
 *  ...
 * @endcode
 */
	int device_get_battery_health(void);

/**
 * @par Description:
 * 	Display number
 */
	typedef enum {
		DEV_DISPLAY_0,	/**< */
		DEV_DISPLAY_1,	/**< */
		DEV_MAIN_DISPLAY = DEV_DISPLAY_0,	/**< */
	} display_num_t;

/**
 * @par Description:
 * 	This API is used to get the current brightness of the display.\n
 * 	It gets the current brightness of the display by calling device_get_property() function.\n
 * 	It returns integer value which is the current brightness on success.\n
 * 	Or a negative value(-1) is returned on failure.
 * @param[in] num display number that you want to get the brightness value
 * @return current brightness value on success, -1 if failed
 * @see device_set_diplay_brt()
 * @par Example
 * @code
 * 	...
 * 	int cur_brt;
 * 	cur_brt = device_get_display_brt(0);
 * 	if( cur_brt < 0 )
 * 		printf("Fail to get the current brightness of the display.\n");
 * 	else
 * 		printf("Current brightness of the display is %d\n", cur_brt);
 * 	...
 * @endcode
 */
	int device_get_display_brt(display_num_t num);

/**
 * @par Description:
 *  This API is used to set the current brightness of the display and system brightness value in settings.\n
 *  It sets the current brightness of the display by calling device_set_property() function.\n
 *  MUST use this API very carefully. \n
 *  This api is different from device_set_display_brt api.\n
 *  device_set_display_brt api will change only device brightness value.\n
 *  but this api will change device brightness as well as system brightness value.\n
 * @param[in] num display number that you want to set the brightness value
 * @param[in] val brightness value that you want to set
 * @return 0 on success, -1 if failed
 * @see device_set_display_brt()
 * @par Example
 * @code
 *  ...
 *  if( device_set_display_brt_with_settings(0,6) < 0 )
 *      printf("Fail to set the current brightness of the display0\n");
 *  else
 *      printf("The current brightness of the display0 is set 6\n");
 *  ...
 * @endcode
 */
	int device_set_display_brt_with_settings(display_num_t lcdnum, int val);

/**
 * @par Description:
 * 	This API is used to set the current brightness of the display.\n
 *  It sets the current brightness of the display by calling device_set_property() function.\n
 * 	MUST use this API very carefully. \n
 * 	you MUST set original brightness by device_release_brt_ctrl(),
 * 	after you finish your job using this API.
 * @param[in] num display number that you want to set the brightness value
 * @param[in] val brightness value that you want to set
 * @return 0 on success, -1 if failed
 * @see device_get_diplay_brt(), device_release_brt_ctrl()
 * @par Example
 * @code
 * 	...
 * 	if( device_set_display_brt(0,6) < 0 )
 * 	    printf("Fail to set the current brightness of the display0\n");
 * 	else
 * 	    printf("The current brightness of the display0 is set 6\n");
 * 	...
 * @endcode
 */
	int device_set_display_brt(display_num_t num, int val);

/**
 * @par Description:
 * 	This API is used to release brightness control.\n
 *  It sets the current brightness of the display by calling device_set_property() function.\n
 * 	MUST call this API after you finished the job which need to change the brightness.
 * @param[in] num display number
 * @return 0 on success, -1 if failed
 * @see device_set_display_brt()
 * @par Example
 * @code
 * 	...
 * 	org_val = device_get_display_brt(0);
 * 	device_set_display_brt(0,1);
 * 	...
 * 	ret = device_release_brt_ctrl(0);
 * 	if( ret < 0 )
 * 		printf("Fail to release brightness control\n");
 * 	...
 * @endcode
 */
	int device_release_brt_ctrl(display_num_t num);

/**
 * @par Description:
 *  This API is used to get the min brightness of the display.\n
 *  It gets the current brightness of the display by calling device_get_property() function.\n
 *  It returns integer value which is the min brightness on success.\n
 *  Or a negative value(-1) is returned on failure
 * @param[in] num display number
 * @return min brightness value on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  int min_brt;
 *  min_brt = device_get_min_brt(0);
 *  if( min_brt < 0 )
 *      printf("Fail to get the min brightness of the display.\n");
 *  else
 *      printf("Min brightness of the display is %d\n", min_brt);
 *  ...
 * @endcode
 */
	int device_get_min_brt(display_num_t num);

/**
 * @par Description:
 *  This API is used to get the max brightness of the display.\n
 *  It gets the current brightness of the display by calling device_get_property() function.\n
 *  It returns integer value which is the max brightness on success.\n
 *  Or a negative value(-1) is returned on failure
 * @param[in] num display number
 * @return max brightness value on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  int max_brt;
 *  max_brt = device_get_max_brt(0);
 *  if( max_brt < 0 )
 *      printf("Fail to get the max brightness of the display.\n");
 *  else
 *      printf("Max brightness of the display is %d\n", max_brt);
 *  ...
 * @endcode
 */
	int device_get_max_brt(display_num_t num);

/**
 * @par Description:
 * 	LCD gamma values
 */
	typedef enum {
		LCD_GAMMA_22 = 1,			/**< 8500K , 2.2 GAMMA*/
		LCD_GAMMA_19 = 2,			/**< 8500K , 1.9 GAMMA*/
		LCD_GAMMA_17 = 3,			/**< 8500K , 1.7 GAMMA*/
		LCD_GAMMA_NORMAL = LCD_GAMMA_22,			/**< Normal screen */
		LCD_GAMMA_PLAY_RECORD = LCD_GAMMA_19,		/**< Playing or recording cam */
		LCD_GAMMA_PREVIEW = LCD_GAMMA_17,			/**< Preview */
		LCD_GAMMA_MOVIE = LCD_GAMMA_19,				/**< Movie */
		LCD_GAMMA_CAMERA = 11,							/**< Camera */
	} display_gamma_t;

/**
 * @par Description:
 * 	This API is used to get the current gamma of the display.\n
 *  It gets the current gamma of the display by calling device_get_property() function.\n
 *  It returns enum value which is the current gamma on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @param[in] num display number that you want to get the gamma value
 * @return current gamma enum value on success, -1 if failed
 * @see device_set_diplay_gamma()
 * @par Example
 * @code
 *  ...
 *  int cur_brt;
 *  cur_brt = device_get_display_gamma(0);
 *  if( cur_brt < 0 )
 *     printf("Fail to get the current gamma of the display.\n");
 *  else
 *     printf("Current gamma of the display is %d\n", cer_brt);
 *  ...
 * @endcode
 */
	int device_get_display_gamma(display_num_t num);

/**
 * @par Description:
 * 	This API is used to set the specific gamma value of the display .\n
 *  It sets the specific gamma value of the display by calling device_set_property() function.\n
 *  MUST use this API very carefully. \n
 *  you MUST set original gamma by device_release_gamma_ctrl(),
 *  after you finish your job using this API.
 * @param[in] num display number that you want to set the gamma value
 * @param[in] new_val lcd gamma enum value that you want to set
 * @return 0 on success, -1 if failed
 * @see device_get_diplay_gammat(), device_release_gamma_ctrl()
 * @par Example
 * @code
 *  ...
 *  if( device_set_display_gamma(0,1) < 0 )
 *      printf("Fail to set the specific gamma of the display0\n");
 *  else
 *      printf("The gamma of the display0 is set 1(LCD_GAMMA_22)\n");
 *  ...
 * @endcode
 */
	int device_set_display_gamma(display_num_t num,
			display_gamma_t new_val);

/**
 * @par Description:
 * 	This API is used to release gamma control.\n
 *  It sets the gamma of the display by calling device_set_property() function.\n
 *  MUST call this API after you finished the job which need to change the gamma.
 * @param[in] num display number
 * @param[in] org_val original gamma enums value before you control the gamma
 * @return 0 on success, -1 if failed
 * @see device_set_display_gamma()
 * @par Example
 * @code
 *  ...
 *  org_val = device_get_display_gamma(0);
 *  device_set_display_gamma(0,2);
 *  ...
 *  ret = device_release_gamma_ctrl(0, org_val);
 *  if( ret < 0 )
 *      printf("Fail to release gamma control\n");
 *  ...
 * @endcode
 */
	int device_release_gamma_ctrl(display_num_t num,
			display_gamma_t org_val);

/**
 * @par Description:
 * 	This API is used to get number of displays on the phone.\n
 *  It gets the current number of displays by calling device_get_display_count() function.\n
 *  It returns enum value which is the current number on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 * 	...
 * 	ret = device_get_display_count();
 * 	if( ret < 0 )
 *      printf("Fail to get number of displays\n");
 *  ...
 * @endcode
 */
	int device_get_display_count(void);

/**
 * @par Description:
 * mode - dynamic, standard, natural, movie
 */
	enum image_enhance_mode {
		MODE_DYNAMIC = 0,
		MODE_STANDARD,
		MODE_NATURAL,
		MODE_MOVIE,
	};

/**
 * @par Description:
 *  This API is used to get image enhance mode.\n
 *  It returns enum value which is the current mode on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return enum value for current mode on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  ret = device_get_image_enhance_mode();
 *  if( ret < 0 )
 *      printf("Fail to get current image enhance mode\n");
 *  ...
 * @endcode
 */
	int device_get_image_enhance_mode(void);

/**
 * @par Description:
 *  This API is used to set image enhance mode.\n
 * @param[in] val mode enum vlaue
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  if( device_set_image_enhance_mode(MODE_DYNAMIC) < 0 )
 *      printf("Fail to set the image enhance mode\n");
 *  ...
 * @endcode
 */
	int device_set_image_enhance_mode(int val);

/**
 * @par Description:
 * scenario - ui, gallery, video, vtcall, camera, browser, negative, bypass
 */
	enum image_enhance_scenario {
		SCENARIO_UI = 0,
		SCENARIO_GALLERY,
		SCENARIO_VIDEO,
		SCENARIO_VTCALL,
		SCENARIO_CAMERA,
		SCENARIO_BROWSER,
		SCENARIO_NEGATIVE,
		SCENARIO_BYPASS,
	};

/**
 * @par Description:
 *  This API is used to get image enhance scenario.\n
 *  It returns enum value which is the current scenario on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return enum value for current wcenario on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  ret = device_get_image_enhance_scenario();
 *  if( ret < 0 )
 *      printf("Fail to get current image enhance scenarioe\n");
 *  ...
 * @endcode
 */
	int device_get_image_enhance_scenario(void);

/**
 * @par Description:
 *  This API is used to set image enhance scenario.\n
 * @param[in] val scenario enum vlaue
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  if( device_set_image_enhance_scenario(SCENARIO_UI) < 0 )
 *      printf("Fail to set the image enhance scenario\n");
 *  ...
 * @endcode
 */
	int device_set_image_enhance_scenario(int val);

/**
 * @par Description:
 * tone - normal, warm, cold
 */
	enum image_enhance_tone {
		TONE_NORMAL = 0,
		TONE_WARM,
		TONE_COLD,
	};

/**
 * @par Description:
 *  This API is used to get image enhance tone.\n
 *  It returns enum value which is the current mode on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return enum value for current tone on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  ret = device_get_image_enhance_tone();
 *  if( ret < 0 )
 *      printf("Fail to get current image enhance tone\n");
 *  ...
 * @endcode
 */
	int device_get_image_enhance_tone(void);

/**
 * @par Description:
 *  This API is used to set image enhance tone.\n
 * @param[in] val tone vlaue
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  if( device_set_image_enhance_tone(TONE_NORMAL) < 0 )
 *      printf("Fail to set the image enhance tone\n");
 *  ...
 * @endcode
 */
	int device_set_image_enhance_tone(int val);

/**
 * @par Description:
 * outdoor - off, on
 */
	enum image_enhance_outdoor {
		OUTDOOR_OFF = 0,
		OUTDOOR_ON,
	};

/**
 * @par Description:
 *  This API is used to get image enhance outdoor.\n
 *  It returns enum value which is the current outdoor on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return enum value for current outdoor on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  ret = device_get_image_enhance_ourdoor();
 *  if( ret < 0 )
 *      printf("Fail to get current image enhance outdoor\n");
 *  ...
 * @endcode
 */
	int device_get_image_enhance_outdoor(void);

/**
 * @par Description:
 *  This API is used to set image enhance outdoor.\n
 * @param[in] val outdoor vlaue
 * @return 0 on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  if( device_set_image_enhance_outdoor(TONE_NORMAL) < 0 )
 *      printf("Fail to set the image enhance outdoor\n");
 *  ...
 * @endcode
 */
	int device_set_image_enhance_outdoor(int val);

/**
 * @par Description:
 *  This API is used to get information about image enhance function.\n
 *  It returns 1, this device can use enhance mode.\n
 *  Or a negative value(-1) is returned, it doesn't support enhance mode.
 * @return 1 is returned on support for enhance mode, -1 if not.
 * @par Example
 * @code
 *  ...
 *  ret = device_get_image_enhance_info();
 *  if( ret < 0 )
 *      printf("Not support image enhance mode on this device\n");
 *  ...
 * @endcode
 */
	int device_get_image_enhance_info(void);

/**
 * @par Description:
 *  This API is used to get the current brightness of the led.\n
 *  It gets the current brightness of the led by calling device_get_property() function.\n
 *  It returns integer value which is the current brightness on success.\n
 *  Or a negative value(-1) is returned on failure.
 * @return current brightness value on success, -1 if failed
 * @see device_set_led_brt()
 * @par Example
 * @code
 *  ...
 *  int cur_brt;
 *  cur_brt = device_get_led_brt();
 *  if( cur_brt < 0 )
 *      printf("Fail to get the current brightness of the led.\n");
 *  else
 *      printf("Current brightness of the led is %d\n", cur_brt);
 *  ...
 * @endcode
 */
	int device_get_led_brt(void);

/**
 * @par Description:
 *  This API is used to set the current brightness of the led.\n
 *      It sets the current brightness of the led by calling device_set_property() function.\n
 * @param[in] val brightness value that you want to set
 * @return 0 on success, -1 if failed
 * @see device_get_led_brt()
 * @par Example
 * @code
 *  ...
 *  if( device_set_led_brt(1) < 0 )
 *     printf("Fail to set the brightness of the led\n");
 * @endcode
 */
	int device_set_led_brt(int val);

/**
 * @par Description:
 *  This API is used to set the current brightness of the led without noti.\n
 *      It sets the current brightness of the led by calling device_set_property() function.\n
 * @param[in] val brightness value that you want to set
 * @return 0 on success, -1 if failed
 * @see device_get_led_brt()
 * @par Example
 * @code
 *  ...
 *  if( device_set_led_brt_without_noti(1) < 0 )
 *     printf("Fail to set the brightness of the led\n");
 * @endcode
 */
	int device_set_led_brt_without_noti(int val);

/**
 * @par Description:
 *  This API is used to get the max brightness of the led.\n
 *  It gets the current brightness of the led by calling device_get_property() function.\n
 *  It returns integer value which is the max brightness on success.\n
 *  Or a negative value(-1) is returned on failure
 * @return max brightness value on success, -1 if failed
 * @par Example
 * @code
 *  ...
 *  int max_brt;
 *  max_brt = device_get_max_led(0);
 *  if( max_brt < 0 )
 *      printf("Fail to get the max brightness of the led.\n");
 *  ...
 * @endcode
 */
	int device_get_max_led(void);

/**
 * @par Description:
 *  This API is used to get the current state for acl.\n
 *  It gets the current state for acl by calling device_get_property() function.\n
 * @param[in] num display number that you want to set the gamma value
 * @return current status for acl(1 on, 0 off) on success, -1 if failed
 * @see device_set_acl_control_status()
 * @par Example
 * @code
 *  ...
 *  int acl_stat;
 *  acl_stat = device_get_acl_control_status(0);
 *  if( acl_stat < 0 )
 *      printf("Fail to get the current status for acl.\n");
 *  else
 *      printf("Current status for acl is %d\n", cur_brt);
 *  ...
 * @endcode
 */
	int device_get_acl_control_status(display_num_t num);

/**
 * @par Description:
 *  This API is used to set the current status for acl.\n
 *  It sets the current status for acl by calling device_set_property() function.\n
 * @param[in] num display number that you want to set the brightness value
 * @param[in] val status for acl(1 on, 0 off) that you want to set
 * @return 0 on success, -1 if failed
 * @see device_get_acl_control_status()
 * @par Example
 * @code
 *  ...
 *  if( device_set_acl_control_status(0, 1) < 0 )
 *      printf("Fail to set the current status for acl\n");
 *  else
 *      printf("The current status for acl is set 6\n");
 *  ...
 * @endcode
 */
	int device_set_acl_control_status(display_num_t num, int val);

/**
 * @} // end of internal APIs
 */

#ifdef __cplusplus
}
#endif
#endif
