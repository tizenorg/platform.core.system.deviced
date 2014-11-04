/*
 *  haptic
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jiyoung Yun <jy910.yun@samsung.com>
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


#ifndef __HAPTIC_H__
#define __HAPTIC_H__

#include <tizen_error.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        haptic.h
 * @brief       This file contains the prototypes of the haptic API
 */

/**
 * @addtogroup CAPI_SYSTEM_HAPTIC_MODULE
 * @{
 */

/**
 * @brief The handle of haptic device
 */
typedef void* haptic_device_h;

/**
 * @brief The handle of haptic effect
 */
typedef void* haptic_effect_h;

/**
 * @brief Enumerations of device id for the Haptic API.
 * @details We support two motors now.
 */
typedef enum {
	HAPTIC_DEVICE_0 = 0x0,	     		/**< 1st motor */
	HAPTIC_DEVICE_1 = 0x1,        		/**< 2nd motor */
	HAPTIC_DEVICE_ALL = 0x4,    		/**< both of them */
} haptic_device_e;

/**
 * @brief Enumerations of priority level for the Haptic API.
 */
typedef enum
{
	HAPTIC_PRIORITY_MIN = 0,		/**< Minimum effect priority for developers (default) */
	HAPTIC_PRIORITY_MIDDLE,			/**< Maximum effect priority for developers */
	HAPTIC_PRIORITY_HIGH,  			/**< Maximum effect priority for OEMs */
} haptic_priority_e;

/**
 * @brief Enumerations of feedback level for the Haptic API.
 * @details Haptic level means vibration power (intensity).
 */
typedef enum
{
	HAPTIC_FEEDBACK_0 = 0,  			/**< feedback level 0 */
	HAPTIC_FEEDBACK_1 = 20, 			/**< feedback level 1 */
	HAPTIC_FEEDBACK_2 = 40, 			/**< feedback level 2 */
	HAPTIC_FEEDBACK_3 = 60, 			/**< feedback level 3 */
	HAPTIC_FEEDBACK_4 = 80, 			/**< feedback level 4 */
	HAPTIC_FEEDBACK_5 = 100,			/**< feedback level 5 */
	HAPTIC_FEEDBACK_AUTO,   			/**< feedback level auto */
} haptic_feedback_e;

/**
 * @brief Enumerations of iteration count for the Haptic API.
 */
typedef enum
{
	HAPTIC_ITERATION_ONCE = 1,
	HAPTIC_ITERATION_INFINITE = 256,
} haptic_iteration_e;

/**
 * @brief Enumerations of effect or device state for the Haptic API.
 */
typedef enum
{
	HAPTIC_STATE_STOP = 0,
	HAPTIC_STATE_PLAYING,
} haptic_state_e;

/**
 * @brief Enumerations of error codes for the Haptic API.
 */
typedef enum
{
	HAPTIC_ERROR_NONE                 = TIZEN_ERROR_NONE,               	/**< Successful */
	HAPTIC_ERROR_INVALID_PARAMETER    = TIZEN_ERROR_INVALID_PARAMETER,  	/**< Invalid parameter */
	HAPTIC_ERROR_FILE_EXISTS          = TIZEN_ERROR_FILE_EXISTS,        	/**< File exists */
	HAPTIC_ERROR_NOT_INITIALIZED      = TIZEN_ERROR_SYSTEM_CLASS | 0x26,	/**< Not initialized */
	HAPTIC_ERROR_OPERATION_FAILED     = TIZEN_ERROR_SYSTEM_CLASS | 0x28,	/**< Operation failed */
	HAPTIC_ERROR_NOT_SUPPORTED_DEVICE = TIZEN_ERROR_SYSTEM_CLASS | 0x30,	/**< Not supported device */
} haptic_error_e;

/**
 * @brief Gets the number of the vibrators.
 *
 * @remarks The index HAPTIC_DEVICE_ALL is reserved meaning for all vibrators at a time.
 *
 * @param[out] vibrator_number A number of vibrators
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	 Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 */
int haptic_get_count(int *device_number);

/**
 * @brief Opens a haptic-vibration device.
 *
 * @details Internally, it makes a connection to the vibrator.
 *
 * @remarks If this function is not called in advance, other functions will return #HAPTIC_ERROR_NOT_INITIALIZED.
 * @remarks Haptic API must be closed by haptic_close().
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_close()
 */
int haptic_open(haptic_device_e device, haptic_device_h *device_handle);

/**
 * @brief Closes a haptic-vibration device.
 *
 * @details Internally, it disconnects the connection to vibrator.
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_open()
 */
int haptic_close(haptic_device_h device_handle);

/**
 * @brief Vibrates during the specified time with a constant intensity.
 * @details
 * This function can be used to start monotonous vibration for specified time.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.\n
 * And default value of feedback and priority is used.\n
 * feedback level is reserved for auto chaning to save variable in the settings.\n
 * priority level uses HAPTIC_PRIORITY_MIN.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] duration      	The play duration in milliseconds
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_monotone_with_detail()
 * @see haptic_vibrate_file()
 * @see haptic_vibrate_buffer()
 * @see haptic_get_count()
 */
int haptic_vibrate_monotone(haptic_device_h device_handle, int duration, haptic_effect_h *effect_handle);

/**
 * @brief Vibrates during the specified time with a constant intensity.
 * @details
 * This function can be used to start monotonous vibration for specified time.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] duration      	The play duration in milliseconds
 * @param[in] feedback       	The amount of the intensity variation
 * @param[in] priority       	The priority from HAPTIC_PRIORITY_MIN to HAPTIC_PRIORITY_HIGH
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file_with_detail()
 * @see haptic_vibrate_buffer_with_detail()
 * @see haptic_get_count()
 */
int haptic_vibrate_monotone_with_detail(haptic_device_h device_handle,
										int duration,
										haptic_feedback_e feedback,
										haptic_priority_e priority,
										haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern file.
 * @details
 * This function can be used to play a haptic-vibration pattern file.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.\n
 * And default value of feedback and priority is used.\n
 * feedback level is reserved for auto chaning to save variable in the settings.\n
 * priority level uses HAPTIC_PRIORITY_MIN.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] file_path     	Vibration pattern file with path
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_file_with_detail()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_buffer()
 * @see haptic_get_count()
 */
int haptic_vibrate_file(haptic_device_h device_handle, const char *file_path, haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern file.
 * @details
 * This function can be used to play a haptic-vibration pattern file.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] file_path     	Vibration pattern file with path
 * @param[in] iteration     	The number of times to repeat the effect
 * @param[in] feedback       	The amount of the intensity variation
 * @param[in] priority       	The priority from HAPTIC_PRIORITY_MIN to HAPTIC_PRIORITY_HIGH
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_file()
 * @see haptic_vibrate_monotone_with_detail()
 * @see haptic_vibrate_buffer_with_detail()
 * @see haptic_get_count()
 */
int haptic_vibrate_file_with_detail(haptic_device_h device_handle,
									const char *file_path,
									haptic_iteration_e iteration,
									haptic_feedback_e feedback,
									haptic_priority_e priority,
									haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern buffer.
 * @details
 * This function can be used to play a haptic-vibration pattern buffer.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.\n
 * And default value of feedback and priority is used.\n
 * feedback level is reserved for auto chaning to save variable in the settings.\n
 * priority level uses HAPTIC_PRIORITY_MIN.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer         	Pointer to the vibration pattern
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer_with_detail()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file()
 * @see haptic_get_count()
 */
int haptic_vibrate_buffer(haptic_device_h device_handle, const unsigned char *vibe_buffer, haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern buffer.
 * @details
 * This function can be used to play a haptic-vibration pattern buffer.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer     	Pointer to the vibration pattern
 * @param[in] iteration     	The number of times to repeat the effect
 * @param[in] feedback      	The amount of the intensity variation
 * @param[in] priority      	The priority from HAPTIC_PRIORITY_MIN to HAPTIC_PRIORITY_HIGH
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer()
 * @see haptic_vibrate_monotone_with_detail()
 * @see haptic_vibrate_file_with_detail()
 * @see haptic_get_count()
 */
int haptic_vibrate_buffer_with_detail(haptic_device_h device_handle,
									  const unsigned char *vibe_buffer,
									  haptic_iteration_e iteration,
									  haptic_feedback_e feedback,
									  haptic_priority_e priority,
									  haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern buffer.
 * @details
 * This function can be used to play a haptic-vibration pattern buffer.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.\n
 * And default value of feedback and priority is used.\n
 * feedback level is reserved for auto chaning to save variable in the settings.\n
 * priority level uses HAPTIC_PRIORITY_MIN.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer         	Pointer to the vibration pattern
 * @param[in] size                	Size to the vibration pattern
 * @param[out] effect_handle	[DEPRECATED] Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer_with_detail()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file()
 * @see haptic_get_count()
 */
int haptic_vibrate_buffers(haptic_device_h device_handle,
							const unsigned char *vibe_buffer,
							int size,
							haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern buffer.
 * @details
 * This function can be used to play a haptic-vibration pattern buffer.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer     	Pointer to the vibration pattern
 * @param[in] size              Size to the vibration pattern
 * @param[in] iteration     	The number of times to repeat the effect
 * @param[in] feedback      	The amount of the intensity variation
 * @param[in] priority      	The priority from HAPTIC_PRIORITY_MIN to HAPTIC_PRIORITY_HIGH
 * @param[out] effect_handle	[DEPRECATED] Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer()
 * @see haptic_vibrate_monotone_with_detail()
 * @see haptic_vibrate_file_with_detail()
 * @see haptic_get_count()
 */
int haptic_vibrate_buffers_with_detail(haptic_device_h device_handle,
                                      const unsigned char *vibe_buffer,
									  int size,
                                      haptic_iteration_e iteration,
                                      haptic_feedback_e feedback,
                                      haptic_priority_e priority,
                                      haptic_effect_h *effect_handle);

/**
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer_with_detail()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file()
 * @see haptic_get_count()
 */
int haptic_vibrate_buffers(haptic_device_h device_handle,
							const unsigned char *vibe_buffer,
							int size,
							haptic_effect_h *effect_handle);

/**
 * @brief Vibrates a predefined rhythmic haptic-vibration pattern buffer.
 * @details
 * This function can be used to play a haptic-vibration pattern buffer.
 *
 * @remark
 * If you don't use th api regarding effect_handle, you can pass in a NULL value to last parameter.
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer     	Pointer to the vibration pattern
 * @param[in] size              Size to the vibration pattern
 * @param[in] iteration     	The number of times to repeat the effect
 * @param[in] feedback      	The amount of the intensity variation
 * @param[in] priority      	The priority from HAPTIC_PRIORITY_MIN to HAPTIC_PRIORITY_HIGH
 * @param[out] effect_handle	Pointer to the variable that will receive a handle to the playing effect
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer()
 * @see haptic_vibrate_monotone_with_detail()
 * @see haptic_vibrate_file_with_detail()
 * @see haptic_get_count()
 */
int haptic_vibrate_buffers_with_detail(haptic_device_h device_handle,
                                      const unsigned char *vibe_buffer,
									  int size,
                                      haptic_iteration_e iteration,
                                      haptic_feedback_e feedback,
                                      haptic_priority_e priority,
                                      haptic_effect_h *effect_handle);

/**
 * @brief Stops the current vibration effect which is being played.
 * @details This function can be used to stop each effect started by haptic_vibrate_xxx().
 *
 * @remark
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] effect_handle 	The effect handle from haptic_vibrate_xxx()
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file()
 * @see haptic_get_count()
 * @see haptic_stop_all_effects()
 */
int haptic_stop_effect(haptic_device_h device_handle, haptic_effect_h effect_handle);

/**
 * @brief Stops all vibration effects which are being played.
 * @details This function can be used to stop all effects started by haptic_vibrate_xxx().
 *
 * @remark
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file()
 * @see haptic_get_count()
 * @see haptic_stop_effect()
 */
int haptic_stop_all_effects(haptic_device_h device_handle);

/**
 * @brief Gets the status of the effect.
 * @details This function can be used to get the status of the effect wheter the effect are playing or not.
 *
 * @remark
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] effect_handle 	The effect handle from haptic_vibrate_xxx()
 * @param[out] state        	The pointer to variable that will receive the status of the effect.
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_vibrate_buffer()
 * @see haptic_vibrate_monotone()
 * @see haptic_vibrate_file()
 * @see haptic_get_count()
 * @see haptic_stop_effect()
 * @see haptic_stop_all_effects()
 */
int haptic_get_effect_state(haptic_device_h device_handle, haptic_effect_h effect_handle, haptic_state_e *state);

/**
 * @par Description:
 *      effect element for haptic.
 */
typedef struct {
	int haptic_duration;			/**< Duration of the effect element in millisecond */
	haptic_feedback_e haptic_level;	/**< Level of the effect element (0 ~ 100) */
} haptic_effect_element_s;

/**
 * @brief Creates an effect buffer.
 * @details This function can be used to create an effect buffer using effeclt_element variable.
 *
 * @remark
 *
 * @param[out] vibe_buffer  	Pointer to the vibration pattern
 * @param[in] max_bufsize  		The size of the buffer pointed to by vibe_buffer
 * @param[in] elem_arr     		Pointer to an haptic_effect_element_s structure
 * @param[in] max_elemcnt  		The size fo the buffer pointed to by elem_arr
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_save_effect()
 */
int haptic_create_effect(unsigned char *vibe_buffer,
						 int max_bufsize,
						 haptic_effect_element_s *elem_arr,
						 int max_elemcnt);

/**
 * @brief Save an effect buffer to the file.
 * @details This function can be used to save an effect buffer to the file using third parameter.
 *
 * @remark
 *
 * @param[in] vibe_buffer   	Pointer to the vibration pattern
 * @param[in] max_bufsize  		The size of the buffer pointed to by vibe_buffer
 * @param[in] file_path   		The pointer to the character buffer containing the path to save
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_FILE_EXISTS         	File exists
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_create_effect()
 */
int haptic_save_effect(const unsigned char *vibe_buffer,
					   int max_bufsize,
					   const char *file_path);

/**
 * @brief Gets a duration time value from file.
 * @details This function can be used to get a duration time value from the file using second parameter.
 *
 * @remark
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] file_path   		The pointer to the character buffer containing the path to save
 * @param[out] duration   		The pointer to the duration time value
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_get_buffer_duration()
 */
int haptic_get_file_duration(haptic_device_h device_handle, const char *file_path, int *duration);

/**
 * @brief Gets a duration time value from buffer.
 * @details This function can be used to get a duration time value from the buffer using second parameter.
 *
 * @remark
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer 		Pointer to the vibration pattern buffer
 * @param[out] duration   		The pointer to the duration time value
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_get_file_duration()
 */
int haptic_get_buffer_duration(haptic_device_h device_handle, const unsigned char *vibe_buffer, int *duration);

/**
 * @brief Gets a duration time value from buffer.
 * @details This function can be used to get a duration time value from the buffer using second parameter.
 *
 * @remark
 *
 * @param[in] device_handle 	The device handle from haptic_open()
 * @param[in] vibe_buffer 		Pointer to the vibration pattern buffer
 * @param[in] size 	          	Size to the vibration pattern buffer
 * @param[out] duration   		The pointer to the duration time value
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_get_file_duration()
 */
int haptic_get_buffers_duration(haptic_device_h device_handle, const unsigned char *vibe_buffer, int size, int *buffer_duration);

/**
 * @brief Save an effect buffer to the led file.
 * @details This function can be used to save an effect buffer to the led file which name is third parameter.
 *
 * @remark
 * Third parameter should be compatible with ledplayer file.
 *
 * @param[in] vibe_buffer		Pointer to the vibration pattern
 * @param[in] max_bufsize		The size of the buffer pointed to by vibe_buffer
 * @param[in] file_path  		The pointer to the character buffer containing the path to save
 *
 * @return 0 on success, otherwise a negative error value.
 * @retval #HAPTIC_ERROR_NONE                	Successful
 * @retval #HAPTIC_ERROR_INVALID_PARAMETER   	Invalid parameter
 * @retval #HAPTIC_ERROR_NOT_INITIALIZED     	Not initialized
 * @retval #HAPTIC_ERROR_FILE_EXISTS         	File exists
 * @retval #HAPTIC_ERROR_OPERATION_FAILED    	Operation failed
 * @retval #HAPTIC_ERROR_NOT_SUPPORTED_DEVICE	Not supported device
 *
 * @see haptic_save_effect()
 */
int haptic_save_led(const unsigned char *vibe_buffer, int max_bufsize, const char *file_path);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif		/* __HAPTIC_H__ */
