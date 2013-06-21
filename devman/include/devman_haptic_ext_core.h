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


#ifndef __DEVMAN_HAPTIC_EXT_CORE_H__
#define __DEVMAN_HAPTIC_EXT_CORE_H__

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        devman_haptic_ext_core.h
 * @ingroup     DEVICE_MANAGER
 * @brief       This file contains the data type, structure and definitions of the haptic extra API
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
 *      Data types level for haptic extra functions.
 */
#define HAPTIC_MAX_MAGNITUDE                          10000 /*!< Maximum Force Magnitude */
#define HAPTIC_MIN_MAGNITUDE                          0     /*!< Minimum Force Magnitude */

/**
 * @par Description:
 *      Data types level for haptic extra functions.
 */
#define HAPTIC_MAX_EFFECT_NAME_LENGTH                 128   /*!< Maximum effect name length */
#define HAPTIC_INVALID_INDEX                          -1    /*!< Invalid Index */

/* DevicePropertyType */
#define HAPTIC_DEVPROPTYPE_PRIORITY                   1 /*!< Property type constant to set device priority */
#define HAPTIC_DEVPROPTYPE_DISABLE_EFFECTS            2 /*!< Property type constant to enable/disable effects on a device */
#define HAPTIC_DEVPROPTYPE_STRENGTH                   3 /*!< Property type constant to set the strength (volume) on a device */
#define HAPTIC_DEVPROPTYPE_MASTERSTRENGTH             4 /*!< Property type constant to set the strength (volume) on ALL devices */

/* Device type returned by device_haptic_get_device_capability_int32 in 'xxxx' field of
   'yyyy' struct for 'deviceCapabilityType' equal to
   HAPTIC_DEVCAPTYPE_DEVICE_CATEGORY */
#define HAPTIC_DEVICECATEGORY_IFC                     0 /*!< Device category constant for IFC Devices */
#define HAPTIC_DEVICECATEGORY_IMPULSE                 1 /*!< Device category constant for Impulse Devices */
#define HAPTIC_DEVICECATEGORY_VIRTUAL                 2 /*!< Device category constant for Virtual Devices */
#define HAPTIC_DEVICECATEGORY_EMBEDDED                3 /*!< Device category constant for Embedded Devices */
#define HAPTIC_DEVICECATEGORY_TETHERED                4 /*!< Device category constant for Tethered Devices */
#define HAPTIC_DEVICECATEGORY_IMMERSION_USB           5 /*!< Device category constant for Immersion USB Devices */
#define HAPTIC_DEVICECATEGORY_COMPOSITE               6 /*!< Device category constant for Composite Devices */

/* Effect type returned by device_haptic_get_IVT_effect_type */
#define HAPTIC_EFFECT_TYPE_PERIODIC                   0 /*!< Periodic Effect type constant */
#define HAPTIC_EFFECT_TYPE_MAGSWEEP                   1 /*!< Magsweep Effect type constant */
#define HAPTIC_EFFECT_TYPE_TIMELINE                   2 /*!< Timeline Effect type constant */
#define HAPTIC_EFFECT_TYPE_STREAMING                  3 /*!< Streaming Effect type constant */
#define HAPTIC_EFFECT_TYPE_WAVEFORM                   4 /*!< Waveform Effect type constant */

/* Device capability type passed as input 'deviceCapabilityType' argument to device_haptic_get_device_capability_... */
#define HAPTIC_DEVCAPTYPE_DEVICE_CATEGORY             0	/*!< Use device_haptic_get_device_capability_int32 >*/
#define HAPTIC_DEVCAPTYPE_MAX_NESTED_REPEATS          1	/*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_NUM_ACTUATORS               2	/*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_ACTUATOR_TYPE               3	/*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_NUM_EFFECT_SLOTS            4	/*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_SUPPORTED_STYLES            5	/*!< Use device_haptic_get_device_capability_int32 */
/* HAPTIC_DEVCAPTYPE_SUPPORTED_CONTROL_MODES is deprecated and will not be an available constant
** in future versions of this software.  Please use HAPTIC_DEVCAPTYPE_SUPPORTED_STYLES instead. */
#define HAPTIC_DEVCAPTYPE_SUPPORTED_CONTROL_MODES     HAPTIC_DEVCAPTYPE_SUPPORTED_STYLES
#define HAPTIC_DEVCAPTYPE_MIN_PERIOD                  6  /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_MAX_PERIOD                  7  /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_MAX_EFFECT_DURATION         8  /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_SUPPORTED_EFFECTS           9  /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_DEVICE_NAME                 10 /*!< Use device_haptic_get_device_capability_string */
#define HAPTIC_DEVCAPTYPE_MAX_ENVELOPE_TIME           11 /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_APIVERSIONNUMBER            12 /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_MAX_IVT_SIZE_TETHERED       13 /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_MAX_IVT_SIZE                14 /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_EDITION_LEVEL               15 /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_SUPPORTED_WAVE_TYPES        16 /*!< Use device_haptic_get_device_capability_int32 */
#define HAPTIC_DEVCAPTYPE_HANDSET_INDEX               17 /*!< Use device_haptic_get_device_capability_int32 */

/* Handles */
#define HAPTIC_INVALID_EFFECT_HANDLE_VALUE            -1 /*!< Invalid Effect Handle */
#define HAPTIC_INVALID_DEVICE_HANDLE_VALUE            -1 /*!< Invalid Device Handle */

/* Periodic, MagSweep effect Styles are only 4 bits and may be combined with other flags */
#define HAPTIC_STYLE_MASK                             0x0F    /*!< Style mask */

/* Periodic, MagSweep effect Styles */
#define HAPTIC_STYLE_SMOOTH                           0   /*!< "Smooth" style */
#define HAPTIC_STYLE_STRONG                           1   /*!< "Strong" style */
#define HAPTIC_STYLE_SHARP                            2   /*!< "Sharp" style  */

#define HAPTIC_DEFAULT_STYLE                          HAPTIC_STYLE_STRONG

/* HAPTIC_CONTROLMODE_ constants are deprecated and will not be available
** in future versions of this software.  Please use the HAPTIC_STYLE_ constants instead. */
#define HAPTIC_CONTROLMODE_SMOOTH                     HAPTIC_STYLE_SMOOTH
#define HAPTIC_CONTROLMODE_STRONG                     HAPTIC_STYLE_STRONG
#define HAPTIC_CONTROLMODE_SHARP                      HAPTIC_STYLE_SHARP

#define HAPTIC_DEFAULT_CONTROLMODE                    HAPTIC_DEFAULT_STYLE

/* Effect period resolution */
#define HAPTIC_PERIOD_RESOLUTION_MICROSECOND          0x80000000

/* Periodic effect Wave Types are only 4 bits and may be combined with other flags */
#define HAPTIC_WAVETYPE_SHIFT                         4       /*!< Wave type shift */
#define HAPTIC_WAVETYPE_MASK                          0xF0    /*!< Wave type mask */

/* Periodic effect Wave Types */
#define HAPTIC_WAVETYPE_SQUARE                        (1 << HAPTIC_WAVETYPE_SHIFT)  /*!< "Square" wave type */
#define HAPTIC_WAVETYPE_TRIANGLE                      (2 << HAPTIC_WAVETYPE_SHIFT)  /*!< "Triangle" wave type */
#define HAPTIC_WAVETYPE_SINE                          (3 << HAPTIC_WAVETYPE_SHIFT)  /*!< "Sine" wave type */
#define HAPTIC_WAVETYPE_SAWTOOTHUP                    (4 << HAPTIC_WAVETYPE_SHIFT)  /*!< "Sawtooth Up" wave type */
#define HAPTIC_WAVETYPE_SAWTOOTHDOWN                  (5 << HAPTIC_WAVETYPE_SHIFT)  /*!< "Sawtooth Down" wave type */

#define HAPTIC_DEFAULT_WAVETYPE                       HAPTIC_WAVETYPE_SQUARE

/* String length constants */
#define HAPTIC_MAX_DEVICE_NAME_LENGTH                 64 /*!<Maximum device name length */
#define HAPTIC_MAX_CAPABILITY_STRING_LENGTH           64 /*!<Maximum string length use by device_haptic_get_device_capability_string/ device_haptic_set_device_capability_string*/
#define HAPTIC_MAX_PROPERTY_STRING_LENGTH             64 /*!<Maximum string length use by device_haptic_get_device_property_string/ device_haptic_set_device_property_string */

/* Effect type support bit masks */
#define HAPTIC_PERIODIC_EFFECT_SUPPORT                (1 << HAPTIC_EFFECT_TYPE_PERIODIC)  /*!< Bitmask for Periodic effect support */
#define HAPTIC_MAGSWEEP_EFFECT_SUPPORT                (1 << HAPTIC_EFFECT_TYPE_MAGSWEEP)  /*!< Bitmask for Magsweep effect support */
#define HAPTIC_TIMELINE_EFFECT_SUPPORT                (1 << HAPTIC_EFFECT_TYPE_TIMELINE)  /*!< Bitmask for Timeline effect support */
#define HAPTIC_STREAMING_EFFECT_SUPPORT               (1 << HAPTIC_EFFECT_TYPE_STREAMING) /*!< Bitmask for Streaming effect support */
#define HAPTIC_WAVEFORM_EFFECT_SUPPORT                (1 << HAPTIC_EFFECT_TYPE_WAVEFORM)  /*!< Bitmask for Waveform effect support */

/* Effect Style support bit masks */
#define HAPTIC_STYLE_SUPPORT_MASK                     0x0000FFFF              /*!< Effect style support mask */
#define HAPTIC_STYLE_SMOOTH_SUPPORT                   (1 << HAPTIC_STYLE_SMOOTH) /*!< Bitmask for "Smooth" style support */
#define HAPTIC_STYLE_STRONG_SUPPORT                   (1 << HAPTIC_STYLE_STRONG) /*!< Bitmask for "Strong" style support */
#define HAPTIC_STYLE_SHARP_SUPPORT                    (1 << HAPTIC_STYLE_SHARP)  /*!< Bitmask for "Sharp" style support  */

/* Wave type support bit masks */
/* Starts at 0x10000 to allow combining the flag with the supported style (nControlMode) 32 bits flag */
#define HAPTIC_WAVETYPE_SUPPORT_MASK                  0xFFFF0000                                                       /*!< Wave type support mask */
#define HAPTIC_WAVETYPE_SQUARE_SUPPORT                (0x10000 << (HAPTIC_WAVETYPE_SQUARE >> HAPTIC_WAVETYPE_SHIFT))       /*!< Bitmask for "Square" wave type support */
#define HAPTIC_WAVETYPE_TRIANGLE_SUPPORT              (0x10000 << (HAPTIC_WAVETYPE_TRIANGLE >> HAPTIC_WAVETYPE_SHIFT))     /*!< Bitmask for "Triangle" wave type support */
#define HAPTIC_WAVETYPE_SINE_SUPPORT                  (0x10000 << (HAPTIC_WAVETYPE_SINE >> HAPTIC_WAVETYPE_SHIFT))         /*!< Bitmask for "Sine" wave type support  */
#define HAPTIC_WAVETYPE_SAWTOOTHUP_SUPPORT            (0x10000 << (HAPTIC_WAVETYPE_SAWTOOTHUP >> HAPTIC_WAVETYPE_SHIFT))   /*!< Bitmask for "Saw tooth up" wave type support  */
#define HAPTIC_WAVETYPE_SAWTOOTHDOWN_SUPPORT          (0x10000 << (HAPTIC_WAVETYPE_SAWTOOTHDOWN >> HAPTIC_WAVETYPE_SHIFT)) /*!< Bitmask for "Saw tooth down" wave type support  */

/* HAPTIC_CONTROLMODE_*_SUPPORT constants are deprecated and will not be available
** in future versions of this software.  Please use the HAPTIC_STYLE_*_SUPPORT constants instead. */
#define HAPTIC_CONTROLMODE_SMOOTH_SUPPORT             HAPTIC_STYLE_SMOOTH_SUPPORT
#define HAPTIC_CONTROLMODE_STRONG_SUPPORT             HAPTIC_STYLE_STRONG_SUPPORT
#define HAPTIC_CONTROLMODE_SHARP_SUPPORT              HAPTIC_STYLE_SHARP_SUPPORT

/* Device State constants */
#define HAPTIC_DEVICESTATE_ATTACHED                   (1 << 0) /*!< Device is attached to the system */
#define HAPTIC_DEVICESTATE_BUSY                       (1 << 1) /*!< Device is busy (playing effects) */

/* Time in milliseconds */
#define HAPTIC_TIME_INFINITE                          LONG_MAX /*!< Infinite time */

/* Effect priority */
#define HAPTIC_MIN_DEVICE_PRIORITY                    0x0 /*!< Minimum Effect priority */
#define HAPTIC_MAX_DEV_DEVICE_PRIORITY                0x7 /*!< Maximum Effect priority for developers */
#define HAPTIC_MAX_OEM_DEVICE_PRIORITY                0xF /*!< Maximum Effect priority for OEMs */
#define HAPTIC_MAX_DEVICE_PRIORITY                    HAPTIC_MAX_OEM_DEVICE_PRIORITY    /*!< FOR BACKWARD COMPATIBILITY ONLY;
                                                                                         new applications should use HAPTIC_MAX_DEV_DEVICE_PRIORITY
                                                                                         or HAPTIC_MAX_OEM_DEVICE_PRIORITY */


/* Device Actuator Type constants */
#define HAPTIC_DEVACTUATORTYPE_ERM                    0
#define HAPTIC_DEVACTUATORTYPE_BLDC                   1
#define HAPTIC_DEVACTUATORTYPE_LRA                    2
#define HAPTIC_DEVACTUATORTYPE_PIEZO                  4
#define HAPTIC_DEVACTUATORTYPE_PIEZO_WAVE             4

/* Device Default priority value */
#define HAPTIC_DEVPRIORITY_DEFAULT                    0

/* Repeat count */
#define HAPTIC_REPEAT_COUNT_INFINITE                  255 /*!< Infinite repeat count */

/* Streaming Sample */
#define HAPTIC_MAX_STREAMING_SAMPLE_SIZE              255 /*!< Maximum size for streaming sample */

/* Effect state returned by ImmVibeGetEffectState */
#define HAPTIC_EFFECT_STATE_NOT_PLAYING               0 /*!< Not Playing and not paused */
#define HAPTIC_EFFECT_STATE_PLAYING                   1 /*!< Playing */
#define HAPTIC_EFFECT_STATE_PAUSED                    2 /*!< Paused */

/* Edition levels */
#define HAPTIC_EDITION_3000                           3000
#define HAPTIC_EDITION_4000                           4000
#define HAPTIC_EDITION_5000                           5000

/* Element type for IVTElement structure that is used by ImmVibeInsertIVTElement, ImmVibeReadIVTElement and ImmVibeRemoveIVTElement */
#define HAPTIC_ELEMTYPE_PERIODIC                      0
#define HAPTIC_ELEMTYPE_MAGSWEEP                      1
#define HAPTIC_ELEMTYPE_REPEAT                        2
/* New in API v3.4 */
#define HAPTIC_ELEMTYPE_WAVEFORM                      3

/* Composite device */
#define HAPTIC_MAX_LOGICAL_DEVICE_COUNT               16 /*!< Maximum number of device indices that can be passed to ImmVibeOpenCompositeDevice */

/****************************************************************************
 *
 *  General macros
 *
 ****************************************************************************/
#define HAPTIC_SUCCEEDED(n)                           ((n) >= 0)
#define HAPTIC_FAILED(n)                              ((n) < 0)
#define HAPTIC_IS_INVALID_DEVICE_HANDLE(n)            (((n) == 0) || ((n) == HAPTIC_INVALID_DEVICE_HANDLE_VALUE))
#define HAPTIC_IS_INVALID_EFFECT_HANDLE(n)            (((n) == 0) || ((n) == HAPTIC_INVALID_EFFECT_HANDLE_VALUE))
#define HAPTIC_IS_VALID_DEVICE_HANDLE(n)              (((n) != 0) && ((n) != HAPTIC_INVALID_DEVICE_HANDLE_VALUE))
#define HAPTIC_IS_VALID_EFFECT_HANDLE(n)              (((n) != 0) && ((n) != HAPTIC_INVALID_EFFECT_HANDLE_VALUE))

/****************************************************************************
 *
 *  Error and Return value codes.
 *
 ****************************************************************************/
#define HAPTIC_S_SUCCESS                               0  /*!< Success */
#define HAPTIC_S_FALSE                                 0  /*!< False */
#define HAPTIC_S_TRUE                                  1  /*!< True */
#define HAPTIC_W_NOT_PLAYING                           1  /*!< Effect is not playing */
#define HAPTIC_W_INSUFFICIENT_PRIORITY                 2  /*!< Effect doesn't have enough priority to play: higher priority effect is playing on the device */
#define HAPTIC_W_EFFECTS_DISABLED                      3  /*!< Effects are disabled on the device */
#define HAPTIC_W_NOT_PAUSED                            4  /*!< The ImmVibeResumePausedEffect function cannot resume an effect that is not paused */
#define HAPTIC_E_NOT_INITIALIZED                      -2  /*!< The API is already is not initialized */
#define HAPTIC_E_INVALID_ARGUMENT                     -3  /*!< Invalid argument was used in a API function call */
#define HAPTIC_E_FAIL                                 -4  /*!< Generic error */
#define HAPTIC_E_INCOMPATIBLE_EFFECT_TYPE             -5  /*!< Incompatible Effect type has been passed into  API function call */
#define HAPTIC_E_INCOMPATIBLE_CAPABILITY_TYPE         -6  /*!< Incompatible Capability type was used into one of the following functions */
#define HAPTIC_E_INCOMPATIBLE_PROPERTY_TYPE           -7  /*!< Incompatible Property type was used into one of the following functions */
#define HAPTIC_E_DEVICE_NEEDS_LICENSE                 -8  /*!< Access to the instance of the device is locked until a valid license key is provided. */
#define HAPTIC_E_NOT_ENOUGH_MEMORY                    -9  /*!< The API function cannot allocate memory to complete the process */
#define HAPTIC_E_SERVICE_NOT_RUNNING                  -10 /*!< ImmVibeService is not running */
#define HAPTIC_E_INSUFFICIENT_PRIORITY                -11 /*!< Not enough priority to achieve the request (insufficient license key priority) */
#define HAPTIC_E_SERVICE_BUSY                         -12 /*!< ImmVibeService is busy and failed to complete the request */
#define HAPTIC_E_NOT_SUPPORTED                        -13 /*!< The API function is not supported by this version of the API */

/****************************************************************************
 *
 *  Stuctures
 *
 ****************************************************************************/
typedef struct
{
	int duration;
	int magnitude;
	int period;
	int style;
	int attacktime;
	int attacklevel;
	int fadetime;
	int fadelevel;
} HapticPeriodic;

typedef struct
{
	int duration;
	int magnitude;
	int period;
	int style;
	int attacktime;
	int attacklevel;
	int fadetime;
	int fadelevel;
	int actuatorindex;
} HapticPeriodic2;

typedef struct
{
	int duration;
	int magnitude;
	int style;
	int attacktime;
	int attacklevel;
	int fadetime;
	int fadelevel;
} HapticMagSweep;

typedef struct
{
	int duration;
	int magnitude;
	int style;
	int attacktime;
	int attacklevel;
	int fadetime;
	int fadelevel;
	int actuatorindex;
} HapticMagSweep2;

typedef struct
{
	int count;
	int duration;
} HapticRepeat;

typedef struct
{
    const unsigned char *data;
    int datasize;
    int samplingrate;
    int bitdepth;
    int magnitude;
    int actuatorindex;
} HapticWaveform;

typedef struct
{
    int elementtype;
    int time;
    union
    {
        HapticPeriodic periodic;
        HapticMagSweep magsweep;
        HapticRepeat   repeat;
    } TypeSpecific;
} HapticElement;

typedef struct
{
    int elementtype;
    int time;
    union
    {
        HapticPeriodic2	periodic;
        HapticMagSweep2	magsweep;
        HapticRepeat	repeat;
    } TypeSpecific;
} HapticElement2;

typedef struct
{
    int elementtype;
    int time;
    union
    {
        HapticPeriodic2	periodic;
        HapticMagSweep2	magsweep;
        HapticRepeat	repeat;
        HapticWaveform	waveform;
    } TypeSpecific;
} HapticElement3;

#ifdef __cplusplus
}
#endif
#endif /* __DEVMAN_HAPTIC_EXT_CORE_H__ */
