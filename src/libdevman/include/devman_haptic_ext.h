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


#ifndef __DEVMAN_HAPTIC_EXT_H__
#define __DEVMAN_HAPTIC_EXT_H__

#include "devman_haptic_ext_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        devman_haptic_ext.h
 * @ingroup     DEVICE_MANAGER
 * @brief       This file contains the prototypes of the haptic extended API
 * @author      SLP2.0
 * @date        2010-01-24
 * @version     0.1
 */

/**
 * @addtogroup DEVICE_MANAGER
 * @{
 */

/**
 * @fn int device_haptic_get_device_state(int device_index, int *state)
 * @par Description:
 * 	This API gets the status bits of an available device that is supported by the TouchSense Player API.\n
 * @param[in] dev_idx set a device index (predefined enum value by haptic_dev_idx)
 * @param[out] state is a pointer to the variable that will receive the status bits of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int state
 *
 * 	//Get the haptic device state
 * 	status = device_haptic_get_device_state(DEV_IDX_0, &state);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_state(int device_index, int *state);

/**
 * @fn int device_haptic_get_device_capability_bool(int device_index, int device_cap_type, unsigned char *device_cap_value)
 * @par Description:
 * 	This API gets a Boolean capability of an available device that is supported by the TouchSense Player API.\n
 * @param[in] device_index set a device index (predefined enum value by haptic_dev_idx)
 * @param[in] device_cap_type set capability type of the Boolean capability to get
 * @param[out] device_cap_value is a pointer to the variable that will receive the requested Boolean capability value of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char device_cap_value;
 *
 * 	//Get haptic device capability
 * 	status = device_haptic_get_device_capability_bool(DEV_IDX_0, NOT_AVAILABLE_CAPABILITY, &device_cap_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_capability_bool(int device_index, int device_cap_type, unsigned char *device_cap_value);

/**
 * @fn int device_haptic_get_device_capability_int32(int device_index, int device_cap_type, int *device_cap_value)
 * @par Description:
 * 	This API gets a 32-bit integer capability of an available device that is supported by the TouchSense Player API.\n
 * @param[in] device_index set a device index (predefined enum value by haptic_dev_idx)
 * @param[in] device_cap_type set Capability type of the Boolean capability to get
 * @param[out] device_cap_value is a pointer to the variable that will receive the requested 32-bit integer capability value of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_cap_value;
 *
 * 	//Get haptic device capability
 * 	status = device_haptic_get_device_capability_int32(DEV_IDX_0, HAPTIC_DEVCAPTYPE_ACTUATOR_TYPE, &device_cap_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_capability_int32(int device_index, int device_cap_type, int *device_cap_value);

/**
 * @fn int device_haptic_get_device_capability_string(int device_index, int device_cap_type, int size, char *device_cap_value)
 * @par Description:
 * 	This API gets a string capability of an available device that is supported by the TouchSense Player API.\n
 * @param[in] device_index set a device index (predefined enum value by haptic_dev_idx)
 * @param[in] device_cap_type set Capability type of the Boolean capability to get
 * @param[out] device_cap_value is a pointer to the variable that will receive the requested 32-bit integer capability value of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int size = 1024;
 *      char device_cap_value[size];
 *
 * 	//Get haptic device capability
 * 	status = device_haptic_get_device_capability_string(DEV_IDX_0, HAPTIC_DEVCAPTYPE_DEVICE_NAME, size, device_cap_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_capability_string(int device_index, int device_cap_type, int size, char *device_cap_value);

/**
 * @fn int device_haptic_get_device_property_bool(int device_handle, int device_prop_type, unsigned char *device_prop_value)
 * @par Description:
 * 	This API gets a Boolean property of an open device.\n
 * @param[in] device_handle is a handle to the device for which to get a Boolean property
 * @param[in] device_prop_type set property type of the Boolean property to get
 * @param[out] device_prop_value is a pointer to the variable that will receive the requested Boolean property value of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_handle;
 *      unsigned char device_prop_value;
 *
 * 	device_handle = device_haptic_open_device(DEV_IDX0, 0);
 *      ...
 *
 * 	//Get haptic device property
 * 	status = device_haptic_get_device_property_bool(device_handle, HAPTIC_DEVPROPTYPE_DISABLE_EFFECTS, &device_prop_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_property_bool(int device_handle, int device_prop_type, unsigned char *device_prop_value);

/**
 * @fn int device_haptic_set_device_property_bool(int device_handle, int device_prop_type, unsigned char device_prop_value)
 * @par Description:
 * 	This API sets a Boolean property of an open device.\n
 * @param[in] device_handle is a handle to the device for which to set a Boolean property
 * @param[in] device_prop_type set property type of the Boolean property to set
 * @param[in] device_prop_value is a value of the Boolean property to set
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_handle;
 *      unsigned char device_prop_value = HAPTIC_FALSE;
 *
 * 	device_handle = device_haptic_open_device(DEV_IDX0, 0);
 *      ...
 *
 * 	//Get haptic device property
 * 	status = device_haptic_set_device_property_bool(device_handle, HAPTIC_DEVPROPTYPE_DISABLE_EFFECTS, device_prop_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_set_device_property_bool(int device_handle, int device_prop_type, unsigned char device_prop_value);

/**
 * @fn int device_haptic_get_device_property_int32(int device_handle, int device_prop_type, int *device_prop_value)
 * @par Description:
 * 	This API gets a 32-bit integer property of an open device.\n
 * @param[in] device_handle is a handle to the device for which to get a 32-bit integer property
 * @param[in] device_prop_type set property type of the Boolean property to get
 * @param[in] device_prop_value is a pointer to the variable that will receive the requested 32-bit integer property value of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_handle;
 *      int device_prop_value;
 *
 * 	device_handle = device_haptic_open_device(DEV_IDX0, 0);
 *      ...
 *
 * 	//Get haptic device property
 * 	status = device_haptic_get_device_property_int32(device_handle, HAPTIC_DEVPROPTYPE_STRENGTH, &device_prop_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_property_int32(int device_handle, int device_prop_type, int *device_prop_value);

/**
 * @fn int device_haptic_set_device_property_int32(int device_handle, int device_prop_type, int device_prop_value)
 * @par Description:
 * 	This API sets a 32-bit integer property of an open device.\n
 * @param[in] device_handle is a handle to the device for which to set a 32-bit integer property
 * @param[in] device_prop_type set property type of the Boolean property to set
 * @param[in] device_prop_value is a value of the 32-bit integer property to set.
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_handle;
 *      int device_prop_value = 5000;
 *
 * 	device_handle = device_haptic_open_device(DEV_IDX0, 0);
 *      ...
 *
 * 	//Get haptic device property
 * 	status = device_haptic_get_device_property_int32(device_handle, HAPTIC_DEVPROPTYPE_STRENGTH, device_prop_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_set_device_property_int32(int device_handle, int device_prop_type, int device_prop_value);

/**
 * @fn int device_haptic_get_device_property_string(int device_handle, int device_prop_type, int size, char *device_prop_value)
 * @par Description:
 * 	This API gets a string property of an open device.\n
 * @param[in] device_handle is a handle to the device for which to get a string property
 * @param[in] device_prop_type set property type of the string property to get
 * @param[in] size is a size of the buffer, in bytes, pointed to by the device_prop_value parameter.
 * @param[out] device_prop_value is a pointer to the variable that will receive the requested string property value of the device
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_handle;
 *      int size = 1024;
 *      char device_prop_value[size];
 *
 * 	device_handle = device_haptic_open_device(DEV_IDX0, 0);
 *      ...
 *
 * 	//Get haptic device property
 * 	status = device_haptic_get_device_property_string(device_handle, HAPTIC_DEVPROPTYPE_STRENGTH, size, device_prop_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_device_property_string(int device_handle, int device_prop_type, int size, char *device_prop_value);

/**
 * @fn int device_haptic_set_device_property_string(int device_handle, int device_prop_type, char *device_prop_value)
 * @par Description:
 * 	This API sets a string property of an open device.\n
 * @param[in] device_handle is a handle to the device for which to get a string property
 * @param[in] device_prop_type set property type of the string property to set
 * @param[in] device_prop_value pointer to the character buffer containing the string property value to set.
 * @return if it succeed, it return HAPTIC_S_SUCCESS , otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int device_handle;
 *      char *device_prop_value = "DMZI13021KAIA112123";
 *
 * 	device_handle = device_haptic_open_device(DEV_IDX0, 0);
 *      ...
 *
 * 	//Get haptic device property
 * 	status = device_haptic_set_device_property_string(device_handle, HAPTIC_DEVPROPTYPE_LICENSE_KEY, device_prop_value);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_set_device_property_string(int device_handle, int device_prop_type, const char *device_prop_value);

/**
 * @fn int device_haptic_get_effect_count(const unsigned char *ivt_buffer)
 * @par Description:
 * 	This API gets number of effects defined in IVT data.\n
 * @param[in] ivt_buffer is a pointer to IVT data
 * @return if it succeed, it returns number of effects defined in the specified IVT data, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int effect_count;
 *      unsigned char ivt_buffer = { .... };
 *
 * 	//Open the haptic device
 * 	effect_count = device_haptic_get_effect_count(ivt_buffer);
 * 	if(effect_count < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_count(const unsigned char *ivt_buffer);

/**
 * @fn int device_haptic_get_effect_name(const unsigned char *ivt_buffer, int effect_index, int size, char *effect_name)
 * @par Description:
 * 	This API Gets the name of an effect defined in IVT data.\n
 * @param[in] ivt_buffer is a pointer to IVT data containing the effect for which to get the name
 * @param[in] effect_index is index of the effect for which to get the name
 * @param[in] size is a size of the buffer, in bytes, pointed by the effect_name parameter
 * @param[out] effect_name is a pointer to the character buffer that will receive the name of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int size = 1024;
 *	char effect_name[size];
 *
 * 	//Get IVT effect duration
 * 	status = device_haptic_get_effect_name(ivt_buffer, 0, size, effect_name);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_name(const unsigned char *ivt_buffer, int effect_index, int size, char *effect_name);

/**
 * @fn int device_haptic_get_effect_name_u(const unsigned char *ivt_buffer, int effect_index, int size, unsigned short *effect_name)
 * @par Description:
 * 	This API gets the name of an effect defined in IVT data as a string of VibeWChar in UCS-2 format.\n
 * @param[in] ivt_buffer is a pointer to IVT data containing the effect for which to get the name
 * @param[in] effect_index is index of the effect for which to get the name
 * @param[in] size is a size of the buffer, in bytes, pointed by the effect_name parameter
 * @param[out] effect_name is a pointer to the unsigned short buffer that will receive the name of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int size = 1024;
 *	unsigned short effect_name[size];
 *
 * 	//Get IVT effect duration
 * 	status = device_haptic_get_effect_name_u(ivt_buffer, 0, size, effect_name);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_name_u(const unsigned char *ivt_buffer, int effect_index, int size, unsigned short *effect_name);

/**
 * @fn int device_haptic_get_effect_index_from_name(const unsigned char *ivt_buffer, const char *effect_name, int *effect_index)
 * @par Description:
 * 	This API gets the index of an effect defined in IVT data given the name of the effect.\n
 * @param[in] ivt_buffer is a pointer to IVT data
 * @param[in] effect_name is pointer to the character buffer containing the name of the effect for which to get the index
 * @param[out] effect_index is a pointer to the variable that will receive the index of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int effect_index;
 *	char *effect_name = "PERIODIC_EFFECT";
 *
 * 	//Get IVT effect duration
 * 	status = device_haptic_get_effect_index_from_name(ivt_buffer, effect_name, &effect_index);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_index_from_name(const unsigned char *ivt_buffer, const char *effect_name, int *effect_index);

/**
 * @fn int device_haptic_get_effect_index_from_name_u(const unsigned char *ivt_buffer, const unsigned short *effect_name, int *effect_index)
 * @par Description:
 * 	This API Gets the index of an effect defined in IVT data given the name of the effect as a string of VibeWChar in UCS-2 format.\n
 * @param[in] ivt_buffer is a pointer to IVT data
 * @param[in] effect_name is a pointer to the unsigned short buffer containing the UCS-2 formatted name of the effect for which to get the index
 * @param[out] effect_index is a pointer to the variable that will receive the index of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int effect_index;
 *	unsigned short *effect_name = "PERIODIC_EFFECT";
 *
 * 	//Get IVT effect duration
 * 	status = device_haptic_get_effect_index_from_name_u(ivt_buffer, effect_name, &effect_index);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_index_from_name_u(const unsigned char *ivt_buffer, const unsigned short *effect_name, int *effect_index);

/**
 * @fn int device_haptic_play_effect(int device_handle, const unsigned char *ivt_buffer, int effect_index, int *effect_handle)
 * @par Description:
 * 	This API Pauses a playing effect.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @param[in] ivt_buffer is a pointer to IVT data containing the definition of the effect to play
 * @param[in] effect_index is an index of the effect to play
 * @param[out] effect_handle is a pointer to the handle to the variable that will receive a handle to the playing effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int effect_handle;
 *
 *      status = device_haptic_play_effect(device_handle, ivt_buffer, 0, &effect_handle);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_play_effect(int device_handle, const unsigned char *ivt_buffer, int effect_index, int *effect_handle);

/**
 * @fn int device_haptic_play_effect_repeat(int device_handle, const unsigned char *ivt_buffer, int effect_index, unsigned char repeat, int *effect_handle)
 * @par Description:
 * 	This API Pauses a playing effect.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @param[in] ivt_buffer is a pointer to IVT data containing the definition of the effect to play
 * @param[in] effect_index is an index of the effect to play
 * @param[in] repeat is a number of time to repeat the effect
 * @param[out] effect_handle is a pointer to the handle to the variable that will receive a handle to the playing effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @see device_haptic_close()
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int effect_handle;
 *
 *      status = device_haptic_play_effect_repeat(device_handle, ivt_buffer, 0, 3, &effect_handle);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_play_effect_repeat(int device_handle, const unsigned char *ivt_buffer, int effect_index, unsigned char repeat, int *effect_handle);

/**
 * @fn int device_haptic_stop_playing_effects(int device_handle, int effect_handle)
 * @par Description:
 * 	This API stops playing effect.\n
 * @param[in] device_handle is a handle to the device on which to stop the playing effect
 * @param[in] effect_handle is a handle to the playing effect to stop
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 * 	status = device_haptic_stop_playing_effects(device_handle, effect_handle);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_stop_playing_effect(int device_handle, int effect_handle);

/**
 * @fn int device_haptic_get_effect_type(const unsigned char *ivt_buffer, int effect_index, int *effect_type)
 * @par Description:
 * 	This API gets the type of an effect defined in IVT data.\n
 * @param[in] ivt_buffer is a pointer to IVT data containing the effect for which to get the type
 * @param[in] effect_index is index of the effect for which to get the type
 * @param[out] effect_type is a pointer to the variable that will receive the type of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int effect_type;
 *
 * 	//Get IVT effect type
 * 	status = device_haptic_get_effect_type(ivt_buffer, 0, &effect_type);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_type(const unsigned char *ivt_buffer, int effect_index, int *effect_type);

/**
 * @fn int device_haptic_get_magsweep_effect_definition(const unsigned char *ivt_buffer, int effect_index, int *duration, int *magnitude, int *style, int *attacktime, int *attacklevel, int *fadetime, int *fadelevel);
 * @par Description:
 * 	This API gets the parameters of a MagSweep effect defined in IVT data.\n
 * @param[in] ivt_buffer is a pointer to IVT data containing the effect for which to get the type
 * @param[in] effect_index is index of the effect for which to get the type
 * @param[out] duration is a pointer to the variable that will receive the duration of the effect in milliseconds
 * @param[out] magnitude is a pointer to the variable that will receive the magnitude of the effect
 * @param[out] style is a pointer to the variable that will receive the style of the effect
 * @param[out] attacktime is a pointer to the variable that will receive the attack time of the effect in milliseconds
 * @param[out] attacklevel is a pointer to the variable that will receive the attack level of the effect
 * @param[out] fadetime is a pointer to the variable that will receive the fade time of the effect in milliseconds
 * @param[out] fadelevel is a pointer to the variable that will receive the fade level of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int duration;
 *      int magnitude;
 *      int style;
 *      int attacktime;
 *      int attacklevel;
 *      int fadetime;
 *      int fadelevel;
 *
 *      status = device_haptic_get_magsweep_effect_definition(ivt_buffer, 1,
 *              &duration, &magnitude, &style, &attacktime, &attacklevel, &fadetime, &fadelevel);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_get_magsweep_effect_definition(const unsigned char *ivt_buffer, int effect_index, int *duration, int *magnitude, int *style, int *attacktime, int *attacklevel, int *fadetime, int *fadelevel);

/**
 * @fn int device_haptic_get_periodic_effect_definition(const unsigned char *ivt_buffer, int effect_index, int *duration, int *magnitude, int *period, int *style_and_wave_type, int *attacktime, int *attacklevel, int *fadetime, int *fadelevel);
 * @par Description:
 * 	This API gets the parameters of a Periodic effect defined in IVT data.\n
 * @param[in] ivt_buffer is a pointer to IVT data containing the effect for which to get the type
 * @param[in] effect_index is index of the effect for which to get the type
 * @param[out] duration is a pointer to the variable that will receive the duration of the effect in milliseconds
 * @param[out] magnitude is a pointer to the variable that will receive the magnitude of the effect
 * @param[out] period is a pointer to the variable that will receive the period of the effect in milliseconds
 * @param[out] style_and_wave_type is a pointer to the variable that will receive the style and wave type of the effect
 * @param[out] attacktime is a pointer to the variable that will receive the attack time of the effect in milliseconds
 * @param[out] attacklevel is a pointer to the variable that will receive the attack level of the effect
 * @param[out] fadetime is a pointer to the variable that will receive the fade time of the effect in milliseconds
 * @param[out] fadelevel is a pointer to the variable that will receive the fade level of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int duration;
 *      int magnitude;
 *      int period;
 *      int style;
 *      int attacktime;
 *      int attacklevel;
 *      int fadetime;
 *      int fadelevel;
 *
 *      status = device_haptic_get_periodic_effect_definition(ivt_buffer, 1,
 *              &duration, &magnitude, &period, &style, &attacktime, &attacklevel, &fadetime, &fadelevel);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_get_periodic_effect_definition(const unsigned char *ivt_buffer, int effect_index, int *duration, int *magnitude, int *period, int *style_and_wave_type, int *attacktime, int *attacklevel, int *fadetime, int *fadelevel);

/**
 * @fn int device_haptic_get_effect_duration(const unsigned char *ivt_buffer, int effect_index, int *effect_duration)
 * @par Description:
 * 	This API gets the duration of an effect defined in IVT data.\n
 * @param[in] ivt_buffer is a pointer to IVT data containing the effect for which to get the Duration
 * @param[in] effect_index is a index of the effect for which to get the Duration
 * @param[out] effect_duration is a pointer to the variable that will receive the Duration of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int effect_duration
 *
 * 	//Get IVT effect duration
 * 	status = device_haptic_get_effect_duration(ivt_buffer, 0, &effect_duration);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_duration(const unsigned char *ivt_buffer, int effect_index, int *effect_duration);

/**
 * @fn int device_haptic_play_magsweep_effect(int device_handle, int duration, int magnitude, int style, int attacktime, int attacklevel, int fadetime, int fadelevel, int *effect_handle);
 * @par Description:
 * 	This API play Mag Sweep effect.\n
 * @param[in] device_handle is a handle to the device on which to modify the playing effect
 * @param[in] duration is a duration of the effect in milliseconds
 * @param[in] magnitude is a magnitude of the effect
 * @param[in] style is a style of the effect
 * @param[in] attacktime is a attack time of the effect in milliseconds
 * @param[in] attacklevel is a attack level of the effect
 * @param[in] fadetime is a fade time of the effect in milliseconds
 * @param[in] fadelevel is a fade level of the effect
 * @param[out] effect_handle is a pointer to the variable that will receive a handle to the playing effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int duration       = 2900;
 *      int magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      int style          = HAPTIC_STYLE_SMOOTH;
 *      int attackTime     = 2483;
 *      int attackLevel    = 0;
 *      int fadeTime       = 0;
 *      int fadeLevel      = HAPTIC_MAX_MAGNITUDE;
 *      int effect_handle;
 *
 *      status = device_haptic_play_magsweep_effect(device_handle,
 *              duration, magnitude, style, attacktime, attacklevel, fadetime, fadelevel, &effect_handle);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_play_magsweep_effect(int device_handle, int duration, int magnitude, int style, int attacktime, int attacklevel, int fadetime, int fadelevel, int *effect_handle);

/**
 * @fn int device_haptic_play_periodic_effect(int device_handle, int duration, int magnitude, int period, int style_and_wave_type, int attacktime, int attacklevel, int fadetime, int fadelevel, int *effect_handle);
 * @par Description:
 * 	This API play Periodic effect.\n
 * @param[in] device_handle is a handle to the device on which to modify the playing effect
 * @param[in] duration is a duration of the effect in milliseconds
 * @param[in] magnitude is a magnitude of the effect
 * @param[in] style is a style of the effect
 * @param[in] attacktime is a attack time of the effect in milliseconds
 * @param[in] attacklevel is a attack level of the effect
 * @param[in] fadetime is a fade time of the effect in milliseconds
 * @param[in] fadelevel is a fade level of the effect
 * @param[out] effect_handle is a pointer to the variable that will receive a handle to the playing effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int duration       = 2900;
 *      int magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      int period         = 100;
 *      int style          = HAPTIC_STYLE_SMOOTH;
 *      int attackTime     = 2483;
 *      int attackLevel    = 0;
 *      int fadeTime       = 0;
 *      int fadeLevel      = HAPTIC_MAX_MAGNITUDE;
 *      int effect_handle;
 *
 *      status = device_haptic_play_periodic_effect(device_handle,
 *              duration, magnitude, period, style, attacktime, attacklevel, fadetime, fadelevel, &effect_handle);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_play_periodic_effect(int device_handle, int duration, int magnitude, int period, int style_and_wave_type, int attacktime, int attacklevel, int fadetime, int fadelevel, int *effect_handle);

/**
 * @fn int device_haptic_modify_playing_magsweep_effect(int device_handle, int effect_handle,	int duration, int magnitude, int style, int attacktime, int attacklevel, int fadetime, int fadelevel);
 * @par Description:
 * 	This API Modifies a playing Mag Sweep effect.\n
 * @param[in] device_handle is a handle to the device on which to modify the playing effect
 * @param[in] effect_handle is a handle to the playing MagSweep effect to modify
 * @param[in] duration is a duration of the effect in milliseconds
 * @param[in] magnitude is a magnitude of the effect
 * @param[in] style is a style of the effect
 * @param[in] attacktime is a attack time of the effect in milliseconds
 * @param[in] attacklevel is a attack level of the effect
 * @param[in] fadetime is a fade time of the effect in milliseconds
 * @param[in] fadelevel is a fade level of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int duration       = 2900;
 *      int magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      int style          = HAPTIC_STYLE_SMOOTH;
 *      int attackTime     = 2483;
 *      int attackLevel    = 0;
 *      int fadeTime       = 0;
 *      int fadeLevel      = HAPTIC_MAX_MAGNITUDE;
 *
 *      status = device_haptic_modify_playing_magsweep_effect(device_handle, effect_handle,
 *              duration, magnitude, style, attacktime, attacklevel, fadetime, fadelevel);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_modify_playing_magsweep_effect(int device_handle, int effect_handle,	int duration, int magnitude, int style, int attacktime, int attacklevel, int fadetime, int fadelevel);

/**
 * @fn int device_haptic_modify_playing_periodic_effect(int device_handle, int effect_handle, int duration, int magnitude, int period, int style_and_wave_type, int attacktime, int attacklevel, int fadetime, int fadelevel);
 * @par Description:
 * 	This API modifies a playing Periodic effect.\n
 * @param[in] device_handle is a handle to the device on which to modify the playing effect
 * @param[in] effect_handle is a handle to the playing MagSweep effect to modify
 * @param[in] duration is a duration of the effect in milliseconds
 * @param[in] magnitude is a magnitude of the effect
 * @param[in] period is a period of the effect
 * @param[in] style is a style of the effect
 * @param[in] attacktime is a attack time of the effect in milliseconds
 * @param[in] attacklevel is a attack level of the effect
 * @param[in] fadetime is a fade time of the effect in milliseconds
 * @param[in] fadelevel is a fade level of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int duration       = 2900;
 *      int magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      int period	   = 100;
 *      int style          = HAPTIC_STYLE_SMOOTH;
 *      int attackTime     = 2483;
 *      int attackLevel    = 0;
 *      int fadeTime       = 0;
 *      int fadeLevel      = HAPTIC_MAX_MAGNITUDE;
 *
 *      status = device_haptic_modify_playing_periodic_effect(device_handle, effect_handle,
 *              duration, magnitude, period, style, attacktime, attacklevel, fadetime, fadelevel);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_modify_playing_periodic_effect(int device_handle, int effect_handle, int duration, int magnitude, int period, int style_and_wave_type, int attacktime, int attacklevel, int fadetime, int fadelevel);

/**
 * @fn int device_haptic_create_streaming_effect(int device_handle, int *effect_handle)
 * @par Description:
 * 	This API creates a Streaming effect.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @param[out] effect_handle is a pointer to the variable that will receive a handle to the Streaming effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int effect_handle;
 *
 * 	status = device_haptic_create_streaming_effect(device_handle, &effect_handle);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_create_streaming_effect(int device_handle, int *effect_handle);

/**
 * @fn int device_haptic_destroy_streaming_effect(int device_handle, int effect_handle)
 * @par Description:
 * 	This API creates a Streaming effect.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @param[in] effect_handle is a handle to the Streaming effect to destroy
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int effect_handle;
 *
 * 	status = device_haptic_create_streaming_effect(device_handle, &effect_handle);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_destroy_streaming_effect(int device_handle, int effect_handle);

/**
 * @fn int device_haptic_play_streaming_sample(int device_handle, int effect_handle, const unsigned char *streaming_sample, int size)
 * @par Description:
 * 	This API plays a Streaming Sample given the parameters defining the effect.\n
 * @param[in] device_handle is a handle to the device on which to play the effect
 * @param[in] effect_handle is a hndle to the Streaming effect to play
 * @param[in] streaming_sameple is a pointer to Streaming Sample data containing the definition of the effect to play
 * @param[in] size is a size of the buffer, in bytes, pointed to by streaming_sample parameter
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 * 	status = device_haptic_play_streaming_sample(device_handle, effect_handle, *streaming_sample, size);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_streaming_sample(int device_handle, int effect_handle, const unsigned char *streaming_sample, int size);

/**
 * @fn int device_haptic_play_streaming_sample_with_offset(int device_handle, int effect_handle, const unsigned char *streaming_sample, int size, int offset_time)
 * @par Description:
 * 	This API plays a Streaming Sample with a time offset given the parameters defining the effect.\n
 * @param[in] device_handle is a handle to the device on which to play the effect
 * @param[in] effect_handle is a hndle to the Streaming effect to play
 * @param[in] streaming_sameple is a pointer to Streaming Sample data containing the definition of the effect to play
 * @param[in] size is a size of the buffer, in bytes, pointed to by streaming_sample parameter
 * @param[in] offset_time is set offet time to play the sample
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 * 	status = device_haptic_play_streaming_sample_with_offset(device_handle, effect_handle, *streaming_sample, size, 100);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_play_streaming_sample_with_offset(int device_handle, int effect_handle, const unsigned char *streaming_sample, int size, int offset_time);



/**
 * @fn int device_haptic_stop_all_playing_effects(int device_handle)
 * @par Description:
 * 	This API stops all playing and paused effects on a device.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 * 	status = device_haptic_stop_all_playing_effects(device_handle);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_stop_all_playing_effects(int device_handle);

/**
 * @fn int device_haptic_save_file(const unsigned char *ivt_buffer, int max_bufsize, const char *path_name)
 * @par Description:
 * 	This API saves an IVT file to persistent storage.\n
 * @param[in] ivt_buffer is a pointer to IVT data
 * @param[in] max_bufsize is a max size of ivt_buffer
 * @param[in] path_name is a pointer to the character buffer containing the path name of the file to save
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      char *path_name = "test.ivt";
 *      int size = MAX_IVT_SIZE;
 *
 * 	status = device_haptic_save_file(ivt_buffer, size, path_name);
 * 	if(staus < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_save_file(const unsigned char *ivt_buffer, int max_bufsize, const char *path_name);

/**
 * @fn int device_haptic_delete_file(const char *path_name)
 * @par Description:
 * 	This API removes an IVT file from persistent storage.\n
 * @param[in] path_name is a pointer to the character buffer containing the path name of the file to remove.\n
 * @return if it succeed, it return HAPTIC_S_SUCCESSe , otherwise negative value return
 * @see device_haptic_save_file()
 * @par Example:
 * @code
 *      ...
 *      char *path_name = "test.ivt";
 *      int = status;
 *
 * 	//Remove an IVT file
 * 	status = device_haptic_delete_file(path_name);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_delete_file(const char *path_name);

/**
 * @fn int device_haptic_pause_playing_effect(int device_handle, int effect_handle)
 * @par Description:
 * 	This API Pauses a playing effect.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @param[in] effect_handle is a handle to the playing effect to pause
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 *      status = device_haptic_pause_playing_effect(handle, effect_handle);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_pause_playing_effect(int device_handle, int effect_handle);

/**
 * @fn int device_haptic_resume_paused_effect(int device_handle, int effect_handle)
 * @par Description:
 * 	This API resumes a paused effect from the point where the effect was paused.\n
 * @param[in] device_handle is a handle to the device associated to the effect
 * @param[in] effect_handle is a handle to the playing effect to pause
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 *      status = device_haptic_resume_paused_effect(handle, effect_handle);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_resume_paused_effect(int device_handle, int effect_handle);

/**
 * @fn int device_haptic_get_effect_state(int device_handle, int effect_handle, int *effect_state)
 * @par Description:
 * 	This API retrieves the status of an effect (playing, not playing, paused).\n
 * @param[in] device_handle ia a handle to the device associated to the effect
 * @param[in] effect_handle ia a handle to the effect which must have been obtained by calling playing APIs
 * @param[out] state ia a pointer to the variable that will receive the status bits of the effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *
 * 	//Open the haptic device
 * 	status = device_haptic_get_effect_state(device_handle, device_effect, &state);
 * 	if(status < 0)
 * 		return -1;
 *      ...
 * @endcode
 */

	int device_haptic_get_effect_state(int device_handle, int effect_handle, int *effect_state);

/**
 * @fn int device_haptic_get_size(const unsigned char *ivt_buffer, int size)
 * @par Description:
 * 	This API sets the size of IVT data.\n
 * @param[in] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @return if it succeed, it return the size of the IVT data, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      unsigned char *ivt_buffer = { ... };
 *      int IVT_size;
 *
 *      IVT_size = device_haptic_get_size(ivt_buffer, sizeof(ivt_buffer));
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_get_size(const unsigned char *ivt_buffer, int size);

/**
 * @fn int device_haptic_initialize_buffer(unsigned char *ivt_buffer, int size)
 * @par Description:
 * 	This API initializes an IVT buffer. Any data currently in the buffer will be destroyed.\n
 * @param[in/out] ivt_buffer is a pointer to a buffer to initialize
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      int size = 1024;
 *      unsigned char ivt_buffer[size];
 *
 *      status = device_haptic_initialize_buffer(ivt_buffer, size);
 *      if (status < 0) {
 *              return status;
 *      }
 *      ...
 * @endcode
 */

	int device_haptic_initialize_buffer(unsigned char *ivt_buffer, int size);

/**
 * @fn int device_haptic_insert_element(unsigned char *ivt_buffer, int size, int timeline_index, const HapticElement *element)
 * @par Description:
 * 	This API inserts an element into a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect in which to insert the element
 * @param[in] element is a pointer to an HapticElement structure containing the parameters of a Periodic effect, MagSweep effect, or Repeat event to insert into the Timeline effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement elem1;
 *      elem1.elementtype    = HAPTIC_ELEMTYPE_MAGSWEEP;
 *      elem1.time           = 0;
 *      elem1.TypeSpecific.magsweep.duration       = 2900;
 *      elem1.TypeSpecific.magsweep.magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      elem1.TypeSpecific.magsweep.style          = HAPTIC_STYLE_SMOOTH;
 *      elem1.TypeSpecific.magsweep.attacktime     = 2483;
 *      elem1.TypeSpecific.magsweep.attacklevel    = 0;
 *      elem1.TypeSpecific.magsweep.fadetime       = 0;
 *      elem1.TypeSpecific.magsweep.fadelevel      = HAPTIC_MAX_MAGNITUDE;
 *
 *      status = device_haptic_insert_element(temp_buffer, temp_size, timeline_index, &elem1);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */

	int device_haptic_insert_element(unsigned char *ivt_buffer, int size, int timeline_index, const HapticElement *element);

/**
 * @fn int device_haptic_insert_element2(unsigned char *ivt_buffer, int size, int timeline_index, const HapticElement2 *element)
 * @par Description:
 * 	This API inserts an element into a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect in which to insert the element
 * @param[in] element is a pointer to an HapticElement2 structure containing the parameters of a Periodic effect, MagSweep effect, or Repeat event to insert into the Timeline effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement2 elem1;
 *      elem1.elementtype    = HAPTIC_ELEMTYPE_MAGSWEEP;
 *      elem1.time           = 0;
 *      elem1.TypeSpecific.magsweep.duration       = 2900;
 *      elem1.TypeSpecific.magsweep.magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      elem1.TypeSpecific.magsweep.style          = HAPTIC_STYLE_SMOOTH;
 *      elem1.TypeSpecific.magsweep.attacktime     = 2483;
 *      elem1.TypeSpecific.magsweep.attacklevel    = 0;
 *      elem1.TypeSpecific.magsweep.fadetime       = 0;
 *      elem1.TypeSpecific.magsweep.fadelevel      = HAPTIC_MAX_MAGNITUDE;
 *
 *      status = device_haptic_insert_element2(temp_buffer, temp_size, timeline_index, &elem1);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */

	int device_haptic_insert_element2(unsigned char *ivt_buffer, int size, int timeline_index, const HapticElement2 *element);

/**
 * @fn int device_haptic_insert_element3(unsigned char *ivt_buffer, int size, int timeline_index, const HapticElement3 *element)
 * @par Description:
 * 	This API inserts an element into a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect in which to insert the element
 * @param[in] element is a pointer to an HapticElement3 structure containing the parameters of a Periodic effect, MagSweep effect, or Repeat event to insert into the Timeline effect
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement3 elem1;
 *      elem1.elementtype    = HAPTIC_ELEMTYPE_MAGSWEEP;
 *      elem1.time           = 0;
 *      elem1.TypeSpecific.magsweep.duration       = 2900;
 *      elem1.TypeSpecific.magsweep.magnitude      = HAPTIC_MAX_MAGNITUDE;
 *      elem1.TypeSpecific.magsweep.style          = HAPTIC_STYLE_SMOOTH;
 *      elem1.TypeSpecific.magsweep.attacktime     = 2483;
 *      elem1.TypeSpecific.magsweep.attacklevel    = 0;
 *      elem1.TypeSpecific.magsweep.fadetime       = 0;
 *      elem1.TypeSpecific.magsweep.fadelevel      = HAPTIC_MAX_MAGNITUDE;
 *
 *      status = device_haptic_insert_element3(temp_buffer, temp_size, 0, &elem1);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */

	int device_haptic_insert_element3(unsigned char *ivt_buffer, int size, int timeline_index, const HapticElement3 *element);

/**
 * @fn int device_haptic_read_element(const unsigned char *ivt_buffer, int size, int timeline_index, int element_index, HapticElement *element);
 * @par Description:
 * 	This API retrieves the parameters of an element of a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect in which to read the element
 * @param[in] element_index is an index of the element to retrieve
 * @param[out] element is a pointer to an HapticElement structure to receive the parameters of a Periodic effect, MagSweepeffect, or Repeat event
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement elem1;
 *
 *      status = device_haptic_insert_element(ivt_buffer, size, 0, 0, &elem1);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */


	int device_haptic_read_element(const unsigned char *ivt_buffer, int size, int timeline_index, int element_index, HapticElement *element);

/**
 * @fn int device_haptic_read_element2(const unsigned char *ivt_buffer, int size, int timeline_index, int element_index, HapticElement2 *element);
 * @par Description:
 * 	This API retrieves the parameters of an element of a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect in which to read the element
 * @param[in] element_index is an index of the element to retrieve
 * @param[out] element is a pointer to an HapticElement2 structure to receive the parameters of a Periodic effect, MagSweepeffect, or Repeat event
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement2 elem1;
 *
 *      status = device_haptic_insert_element2(ivt_buffer, size, 0, 0, &elem1);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */

	int device_haptic_read_element2(const unsigned char *ivt_buffer, int size, int timeline_index, int element_index, HapticElement2 *element);

/**
 * @fn int device_haptic_read_element3(const unsigned char *ivt_buffer, int size, int timeline_index, int element_index, HapticElement3 *element);
 * @par Description:
 * 	This API retrieves the parameters of an element of a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect in which to read the element
 * @param[in] element_index is an index of the element to retrieve
 * @param[out] element is a pointer to an HapticElement3 structure to receive the parameters of a Periodic effect, MagSweepeffect, or Repeat event
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement3 elem1;
 *
 *      status = device_haptic_insert_element3(ivt_buffer, size, 0, 0, &elem1);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */

	int device_haptic_read_element3(const unsigned char *ivt_buffer, int size, int timeline_index, int element_index, HapticElement3 *element);

/**
 * @fn int device_haptic_remove_element(unsigned char *ivt_buffer, int size, int timeline_index, int element_index);
 * @par Description:
 * 	This API removes the element at the specified index from a Timeline effect in an IVT buffer.\n
 * @param[in/out] ivt_buffer is a pointer to an IVT buffer
 * @param[in] size is a size of the buffer pointed to by ivt_buffer
 * @param[in] timeline_index is an index of a Timeline effect to remove the element from
 * @param[in] element_index is an index of the element to remove
 * @return if it succeed, it return HAPTIC_S_SUCCESS, otherwise negative value return
 * @par Example:
 * @code
 *      ...
 *      int status;
 *      HapticElement3 elem1;
 *
 *      status = device_haptic_remove_element(ivt_buffer, size, 0, 0);
 *      if (status < 0) {
 *              return status;
 *      }

 *      ...
 * @endcode
 */

	int device_haptic_remove_element(unsigned char *ivt_buffer, int size, int timeline_index, int element_index);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif /* __DEVMAN_HAPTIC_EXT_H__ */
