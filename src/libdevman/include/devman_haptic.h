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


#ifndef __DEVMAN_HAPTIC_H__
#define __DEVMAN_HAPTIC_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        devman_haptic.h
 * @ingroup     DEVICE_MANAGER
 * @brief       This file contains the prototypes of the haptic API
 * @author      SLP2.0
 * @date        2010-01-24
 * @version     0.1
 */

/**
 * @addtogroup DEVICE_MANAGER
 * @{
 */


/**
 * @par Description:
 * 	The pattern list for haptic.\n
 * 	Note: The patterns will be removed except the patterns which are used for BEAT UX after 9. Dec. 2011.
 */
	enum effectvibe_pattern_list {
		EFFCTVIBE_TOUCH = 0,		/**< for BeatUX */
		EFFCTVIBE_HW_TOUCH,		/**< for BeatUX */
		EFFCTVIBE_NOTIFICATION,		/**< for BeatUX */
		EFFCTVIBE_INCOMING_CALL01,	/**< for BeatUX */
		EFFCTVIBE_INCOMING_CALL02,	/**< for BeatUX */
		EFFCTVIBE_INCOMING_CALL03,	/**< for BeatUX */
		EFFCTVIBE_ALERTS_CALL,		/**< for BeatUX */
		EFFCTVIBE_OPERATION,		/**< for BeatUX */
		EFFCTVIBE_SILENT_MODE,		/**< for BeatUX */

		EFFCTVIBE_PATTERN_END
	};

/**
 * @par Description:
 *      priority level for haptic.
 */
	enum haptic_priority_level {
		HAPTIC_PRIORITY_LEVEL_MIN = 0,			/**< Minimum effect priority for developers */
		HAPTIC_PRIORITY_LEVEL_MAX_DEV,			/**< Maximum effect priority for developers */
		HAPTIC_PRIORITY_LEVEL_MAX_OEM,  		/**< Maximum effect priority for OEMs */
	};

/**
 * @par Description:
 *      feedback level for haptic.
 */
	enum haptic_feedback_level {
		HAPTIC_FEEDBACK_LEVEL_AUTO = -1,	/**< auto feedback level */
		HAPTIC_FEEDBACK_LEVEL_0,			/**< feedback level 0 */
		HAPTIC_FEEDBACK_LEVEL_1,			/**< feedback level 1 */
		HAPTIC_FEEDBACK_LEVEL_2,			/**< feedback level 2 */
		HAPTIC_FEEDBACK_LEVEL_3,			/**< feedback level 3 */
		HAPTIC_FEEDBACK_LEVEL_4,			/**< feedback level 4 */
		HAPTIC_FEEDBACK_LEVEL_5,			/**< feedback level 5 */

		HAPTIC_FEEDBACK_LEVEL_END,
	};

/**
 * @par Description:
 *	infinite iteration count
 */
#define HAPTIC_INFINITE_ITERATION	256

/**
 * @par Description:
 *	Return Values
 */
#define HAPTIC_SUCCESS            						 0
#define HAPTIC_ALREADY_INITIALIZED						-1
#define HAPTIC_NOT_OPENED         						-2
#define HAPTIC_INVALID_ARGUMENT   						-3
#define HAPTIC_FAIL               						-4
#define HAPTIC_NOT_SUPPORTED      						-13
#define HAPTIC_ALREADY_EXIST      						-14

/**
 * @par Description:
 * Motor device index. We support two motors now.
 */
	typedef enum haptic_dev_idx_t {
		DEV_IDX_0 = 0x01,		/**< 1st motor */
		DEV_IDX_1 = 0x02,		/**< 2nd motor */
		DEV_IDX_ALL = 0x04,		/**< both of them */
	} haptic_dev_idx;

/**
 * @fn int device_haptic_open(haptic_dev_idx dev_idx, unsigned int mode)
 * @par Description:
 * 	This API opens a Haptic-vibration device. \n
 * 	On success it returns a dev_handle value. In case of failure it returns a negative value. \n
 * 	If the device is already open it returns (-1).\n
 * 	The first in parameter dev_idx should be from a predefined haptic-device-index which is available in the typedef enum haptic_dev_idx.\n
 * 	The DEV_IDX_0 means first haptic-device-index of target , the DEV_IDX_1 means second haptic-device-index of target \n
 * 	and the DEV_IDX_ALL means both of them. \n
 * 	The availability of the dev_idx value is dependent on the real target. Normally, set a DEV_IDX_0 value to the first haptic-device.\n
 * 	The second in parameter mode is reserved for future so just set a 0 value\n
 * 	Note: The device_haptic_open() must be called before all other haptic APIs are called. \n
 * 	The device_haptic_open() should have a matching call to device_haptic_close().\n
 * 	Applications call the device_haptic_open() only once if possible during application startup and call the device_haptic_close() during application shutdown.
 * @param[in] dev_idx set a device index (predefined enum value by haptic_dev_idx)
 * @param[in] mode just set a "0" value (not support current , reserved for future)
 * @return if it succeed, it return dev_handle value , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int ret_val=0;
 *      int dev_handle;
 *
 * 	//Open the haptic device
 * 	dev_handle = device_haptic_open(DEV_IDX_0,0);
 * 	if(dev_handle < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_open(haptic_dev_idx dev_idx, unsigned int mode) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_close(int dev_handle)
 * @par Description:
 * 	This API closes a Haptic-vibration device. \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	If the device is already closed it returns (-1).  \n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().
 * hatic device close
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_open()
 * @par Example:
 * @code
 *      ...
 *	//Close the device
 *	ret_val = device_haptic_close(dev_handle);
 *	if(ret_val != 0)
 *		 return -1
 *      ...
 * @endcode
 */

	int device_haptic_close(int dev_handle) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_pattern(int dev_handle, int pattern , int iteration , int feedback_level )
 * @par Description:
 * 	This API plays a predefined rhythmic haptic-vibration pattern. \n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter pattern should be from a predefined pattern list which is available in an enumeration (effectvibe_pattern_list).
 * 	These patterns are rhythmic vibration patterns. \n
 * 	The third in parameter iteration sets the number of iterations to be played.
 * 	This should be less than the maximum iteration range set for the device (currently its 255).  \n
 * 	The fourth in parameter is the vibration feedback intensity level.
 * 	This level is already predefined by enumeration type value from HAPTIC_FEEDBACK_LEVEL_1 to HAPTIC_FEEDBACK_LEVEL_5.
 * 	If you want to use the value selected by the user in the Setting application menu, just set -1 value.\n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback play pattern and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] pattern set predefined pattern enum value from effectvibe_pattern_list
 * @param[in] iteration set iteration count
 * @param[in] feedback_level set feed_back level value ( it is dependent on target's hardware )
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_play_file(), device_haptic_play_monoton(), device_haptic_stop_play()
 * @par Example
 * @code
 * 	...
 * 	//Play a rhythmic pattern
 * 	ret_val = device_haptic_play_pattern(dev_handle, EFFCTVIBE_POPUP, HAPTIC_TEST_ITERATION , HAPTIC_FEEDBACK_LEVEL_3);
 * 	if(ret_val !=0)
 * 		return -1;
 * 	...
 * @endcode
 */

	int device_haptic_play_pattern(int dev_handle, int pattern,
				       int iteration, int feedback_level) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_file(int dev_handle, const char *file_name , int iteration , int feedback_level  )
 * @par Description:
 * 	This API plays a predefined rhythmic haptic-vibration pattern file (only supports .ivt type file, Immersion VibeTonz).\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter file_name sets rhythmic vibration pattern file with path.
 * 	It only supports *.ivt type pattern file. \n
 * 	The third in parameter iteration sets the number of iterations to be played.
 * 	This should be less than the maximum iteration range set for the device (currently its 255). 
 * 	If you want to play indefinitely, use HAPTIC_INFINITE_ITERATION defined value. But it depends on the target hardware.\n
 * 	The fourth in parameter is the vibration feedback intensity level.
 * 	This level is already predefined by enumeration type value from HAPTIC _FEEDBACK_LEVEL_1 to HAPTIC _FEEDBACK_LEVEL_5.
 * 	If you want to use the value selected by the user in the Setting application menu, just set HAPTIC_FEEDBACK_LEVEL_AUTO value.
 * 	(But the application must have a main loop to use the HAPTIC_FEEDBACK_LEVEL_AUTO value ) \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback play pattern and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] file_name set file name with path
 * @param[in] iteration set iteration count
 * @param[in] feedback_level set feed_back level value ( it is dependent on target's hardware )
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_play_pattern(), device_haptic_play_monotone(), device_haptic_stop_play()
 * @par Example
 * @code
 *      ...
 *      ret_val = device_haptic_play_file(dev_handle, "test.ivt", HAPTIC_TEST_ITERATION , HAPTIC_FEEDBACK_LEVEL_3);
 *      if(ret_val !=0)
 *      	return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_file(int dev_handle, const char *file_name,
				    int iteration, int feedback_level) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_file_with_priority(int device_handle, const char *file_name, int priority_level, int iteration, int feedback_level)
 * @par Description:
 * 	This API plays a predefined rhythmic haptic-vibration pattern file with priority(only supports .ivt type file, Immersion VibeTonz).\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter file_name sets rhythmic vibration pattern file with path.
 * 	It only supports *.ivt type pattern file. \n
 *	The third in parameter prioritizes between the different effect. \n
 *	This level is already predefined by enumeration type value from hantpic_priority_level.
 *	This value must be between HAPTIC_PRIORITY_LEVEL_MIN and HAPTIC_PRIORITY_LEVEL_MAX_OEM.
 * 	The fourth in parameter iteration sets the number of iterations to be played.
 * 	This should be less than the maximum iteration range set for the device (currently its 255).
 * 	If you want to play indefinitely, use HAPTIC_INFINITE_ITERATION defined value. But it depends on the target hardware.\n
 * 	The fifth in parameter is the vibration feedback intensity level.
 * 	This level is already predefined by enumeration type value from HAPTIC _FEEDBACK_LEVEL_1 to HAPTIC _FEEDBACK_LEVEL_5.
 * 	If you want to use the value selected by the user in the Setting application menu, just set HAPTIC_FEEDBACK_LEVEL_AUTO value.
 * 	(But the application must have a main loop to use the HAPTIC_FEEDBACK_LEVEL_AUTO value ) \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback play pattern and the intensity depends on the target hardware.
 * @param[in] dev_handle set received  dev_handle value from device_haptic_open()
 * @param[in] file_name set file name with path
 * @param[in] priority_level set priority value
 * @param[in] iteration set iteration count
 * @param[in] feedback_level set feed_back level value ( it is dependent on target's hardware )
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_play_file(), device_haptic_play_monotone(), device_haptic_stop_play()
 * @par Example
 * @code
 *      ...
 *      ret_val = device_haptic_play_file_with_priority(dev_handle, "test.ivt", HAPTIC_PRIORITY_LEVEL_MAX_DEV, HAPTIC_TEST_ITERATION , HAPTIC_FEEDBACK_LEVEL_3);
 *      if(ret_val !=0)
 *      	return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_file_with_priority(int device_handle, const char *file_name,
					int priority, int iteration, int feedback_level) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_buffer(int dev_handle, const unsigned char *vibe_buffer, int iteration , int feedback_level  )
 * @par Description:
 * 	This API plays a predefined rhythmic haptic-vibration pattern file (only supports .ivt type file, Immersion VibeTonz).\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter vibe_buffer sets rhythmic vibration pattern buffer.
 * 	The third in parameter iteration sets the number of iterations to be played.
 * 	This should be less than the maximum iteration range set for the device (currently its 255).
 * 	If you want to play indefinitely, use HAPTIC_INFINITE_ITERATION defined value. But it depends on the target hardware.\n
 * 	The fourth in parameter is the vibration feedback intensity level.
 * 	This level is already predefined by enumeration type value from HAPTIC _FEEDBACK_LEVEL_1 to HAPTIC _FEEDBACK_LEVEL_5.
 * 	If you want to use the value selected by the user in the Setting application menu, just set HAPTIC_FEEDBACK_LEVEL_AUTO value.
 * 	(But the application must have a main loop to use the HAPTIC_FEEDBACK_LEVEL_AUTO value ) \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback play pattern and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived dev_handle value from device_haptic_open()
 * @param[in] buffer set buffer to be played
 * @param[in] iteration set iteration count
 * @param[in] feedback_level set feed_back level value ( it is dependent on target's hardware )
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_play_pattern(), device_haptic_play_monotone(), device_haptic_stop_play()
 * @par Example
 * @code
 *      ...
 *      ret_val = device_haptic_play_buffer(dev_handle, buffer, HAPTIC_TEST_ITERATION , HAPTIC_FEEDBACK_LEVEL_3);
 *      if(ret_val !=0)
 *      	return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_buffer(int dev_handle, const unsigned char *vibe_buffer,
				    int iteration, int feedback_level) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_monotone(int dev_handle, int duration)
 * @par Description:
 * 	This API plays a monotonous haptic-vibration pattern with a constant intensity. \n
 * 	In this monotone play, the intensity used is the value that the user has selected in the Setting application menu.\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter duration defines the length of time this vibration should be played. 
 * 	This duration is in milliseconds.  \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback played and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] duration set duration times (ms)
 * @return if it succeed, it return zero value, otherwise negative value return
 * @see device_haptic_play_pattern(), device_haptic_play_file(), device_haptic_stop_play()
 * @par Example
 * @code
 *      ...
 *      //Play a monotone pattern for 1s == 1000ms
 *      ret_val = device_haptic_play_monotone(dev_handle, 1000);
 *      if(ret_val !=0)
 *              return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_monotone(int dev_handle, int duration) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_monotone_with_feedback_level(int dev_handle, int duration, int feedback_level)
 * @par Description:
 * 	This API plays a monotonous haptic-vibration pattern with a given feedback intensity. \n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter duration defines the length of time this vibration should be played.
 * 	This duration is in milliseconds.  \n
 * 	The fourth in parameter is the vibration feedback intensity level.
 * 	This level is already predefined by enumeration type value from HAPTIC _FEEDBACK_LEVEL_1 to HAPTIC _FEEDBACK_LEVEL_5.
 * 	If you want to use the value selected by the user in the Setting application menu, just set HAPTIC_FEEDBACK_LEVEL_AUTO value.
 * 	(But the application must have a main loop to use the HAPTIC_FEEDBACK_LEVEL_AUTO value ) \n
 * 	The third in paramater feedback_level defines the strenth of vibration.
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback played and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] duration set duration times (ms)
 * @param[in] feedback_level set feedback level
 * @return if it succeed, it return zero value, otherwise negative value return
 * @see device_haptic_play_pattern(), device_haptic_play_file(), device_haptic_stop_play()
 * @par Example
 * @code
 *      ...
 *      //Play a monotone pattern for 1s == 1000ms
 *      ret_val = device_haptic_play_monotone_with_feedback_level(dev_handle, 1000, HAPTIC_FEEDBACK_LEVEL_3);
 *      if(ret_val !=0)
 *              return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_monotone_with_feedback_level(int dev_handle, int duration, int feedback_level) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_play_monotone_with_detail_feedback_level(int dev_handle, int duration, int detail_feedback_level)
 * @par Description:
 *  This API plays a monotonous haptic-vibration pattern with a given feedback intensity. \n
 *  The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 *  The second in parameter duration defines the length of time this vibration should be played.
 *  This duration is in milliseconds.  \n
 *  The fourth in parameter is the vibration feedback intensity level.
 *  This level is integer type value from 0 to 100.
 *  If you want to use the value selected by the user in the Setting application menu, just set 0 value.
 *  On success it returns a zero value. In case of failure it returns a negative value. \n
 *  Note: The actual behavior of the feedback played and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] duration set duration times (ms)
 * @param[in] detail_feedback_level set feedback level (0 - 100)
 * @return if it succeed, it return zero value, otherwise negative value return
 * @see device_haptic_play_pattern(), device_haptic_play_file(), device_haptic_stop_play()
 * @par Example
 * @code
 *      ...
 *      //Play a monotone pattern for 1s == 1000ms
 *      ret_val = device_haptic_play_monotone_with_detail_feedback_level(dev_handle, 1000, 100);
 *      if(ret_val !=0)
 *              return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_monotone_with_detail_feedback_level(int dev_handle, int duration, int detail_feedback_level) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_stop_play(int dev_handle)
 * @par Description:
 * 	This API stops the current vibration being played.\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	On success it returns a zero value. In case of failure it returns a negative value.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_play_pattern(), device_haptic_play_file(), device_haptic_play_monotone()
 * @par Example
 * @code
 *      ...
 *      //Stop the pattern immediately
 *      ret_val = device_haptic_stop_play(dev_handle);
 *      if(ret_val !=0)
 *      	return -1;
 *      ...
 * @endcode
 */

	int device_haptic_stop_play(int dev_handle);

/**
 * @fn int device_haptic_get_buffer_duration(int dev_handle, const unsigned char* vibe_buffer , int* duration)
 * @par Description:
 * 	This API gets a duration time value from a predefined rhythmic vibration pattern.\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter vibe_buffer sets rhythmic vibration pattern buffer.
 * 	The application can get a duration time value from the third out parameter duration when this API succeeds.
 * 	The unit of duration is ms (millisecond) \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback played and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived dev_handle value from device_haptic_open()
 * @param[in] vibe_buffer set vibe pattern buffer to get duration
 * @param[out] duration get duration times (ms)
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_get_file_duration()
 */

	int device_haptic_get_buffer_duration(int dev_handle, const unsigned char *vibe_buffer,
					       int *duration) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_get_pattern_duration(int dev_handle, int pattern , int* duration)
 * @par Description:
 * 	This API gets a duration time value from a predefined rhythmic vibration pattern.\n
 * 	The first in parameter dev_handle should be from the return value of device_haptic_open().\n
 * 	The second in parameter pattern should be from a predefined pattern list
 * 	which is available in an enumeration (effectvibe_pattern_list). \n
 * 	The application can get a duration time value from the third out parameter duration when this API succeeds.
 * 	The unit of duration is ms (millisecond) \n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback played and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] pattern set predefined pattern enum value from <effectvibe_pattern_list>
 * @param[out] duration get duration times (ms)
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_get_file_duration()
 */

	int device_haptic_get_pattern_duration(int dev_handle, int pattern,
					       int *duration) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_get_file_duration(int dev_handle, const char *file_name , int* duration)
 * @par Description:
 * 	This API gets a duration time value from a predefined rhythmic vibration pattern file (only supports .ivt type file).\n
 * 	The first in parameter ?dev_handle? should be from the return value of device_haptic_open().\n
 * 	The second in parameter ?file_name? sets rhythmic vibration pattern file with path. It only supports *.ivt type pattern file.\n
 * 	The application can get a duration time value from the third out parameter duration when this API succeeds.
 * 	The unit of duration is ms (millisecond)\n
 * 	On success it returns a zero value. In case of failure it returns a negative value. \n
 * 	Note: The actual behavior of the feedback played and the intensity depends on the target hardware.
 * @param[in] dev_handle set recived  dev_handle value from device_haptic_open()
 * @param[in] file_name set file name with path
 * @param[out] duration get duration times (ms)
 * @return if it succeed, it return zero value , otherwise negative value return
 * @see device_haptic_get_pattern_duration()
 */

	int device_haptic_get_file_duration(int dev_handle,
					    const char *file_name,
					    int *duration) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_get_device_count(void)
 * @par Description:
 * 	This API gets a numer of devices.\n
 * 	The application can get a number of devices from the return value this API succeeds.
 * 	On success it returns a number of devices. In case of failure it returns a negative value. \n
 * @return if it succeed, it return number of devices, otherwise negative value return
 */
	int device_haptic_get_device_count(void) __attribute__ ((deprecated));

/**
 * @fn int device_haptic_convert_to_binary(const char *haptic_name)
 * @par Description:
 *  This API convert ivt file to binary file.\n
 *  If the haptic file is not located predefined position, this api can't work and returns a negative value.\n
 *  And also binary file is located predefined position and name according to rules.\n
 *  The first in parameter sets source file name which should be ".ivt" file.\n
 *  On success it returns 0. In case of failure it returns a negative value.\n
 * @return if it succeed, it return 0, otherwise negative value return
 */
	int device_haptic_convert_to_binary(const char *haptic_name) __attribute__ ((deprecated));

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif		/* __DEVMAN_HAPTIC_H__ */
