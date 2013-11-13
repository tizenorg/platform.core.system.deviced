/*
 *  devman
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "devlog.h"
#include <devman_haptic.h>
#include <devman_haptic_ext.h>
#include <devman_haptic_ext_core.h>

#ifndef EXTAPI
#define EXTAPI __attribute__ ((visibility("default")))
#endif /* EXTAPI */

#ifdef PERFORM_CHECK
static long long ms = 0;

#define MICROSECONDS(tv)        ((tv.tv_sec * 1000000ll) + tv.tv_usec)

#define ESTIMATE_PERFORMANCE() \
	do { \
		struct timeval tv; \
		if (ms == 0) { \
			gettimeofday(&tv, NULL); \
			ms = MICROSECONDS(tv); \
			fDBG(stderr, "%s start time : %lld\n", __func__, ms); \
		} else { \
			gettimeofday(&tv, NULL); \
			fDBG(stderr, "%s elapsed time : %lld\n", __func__, MICROSECONDS(tv) - ms); \
			ms = 0; \
		} \
	} while(0)
#else
#define ESTIMATE_PERFORMANCE()
#endif

#define MAX_FILE_PATH       256		/* Maximum file path length */
#define SAMPLE_INTERVAL 	10.0	/* Sample calculation interval in milliseconds */
#define IVT_BUFFER_SIZE     4096	/* IVT buffer size */
#define BASE    			5
#define FOLDER_MASK			664

typedef enum {
    WAVETYPE_SQUARE = 1,
    WAVETYPE_TRIANGLE,
    WAVETYPE_SINE,
    WAVETYPE_SAWTOOTHUP,
    WAVETYPE_SAWTOOTHDOWN
} effect_wave_type;

/* This functions loads the IVT file into memory */
static unsigned char *_load_ivt_file(const char *filepath)
{
    FILE *pFile;
    long cbyFileSize;
    unsigned char *p_ivt_data = NULL;
    /* open the IVT file */
    /* IMPORTANT: open the IVT file as a binary file to avoid translation */
    pFile = fopen(filepath, "rb");
    if (!pFile)
    {
        /* handle error, application-specific */
        return p_ivt_data;
    }
    /* determine the file size */
    /* fseek returns zero on success, non-zero on failure */
    if (fseek(pFile, 0, SEEK_END))
    {
        /* handle error, application-specific */
        fclose(pFile);
        return p_ivt_data;
    }
    cbyFileSize = ftell(pFile);
    if (fseek(pFile, 0, SEEK_SET))
    {
        /* handle error, application-specific */
        fclose(pFile);
        return p_ivt_data;
    }
    /* allocate a buffer for the IVT data */
    p_ivt_data = (unsigned char *)malloc(cbyFileSize);
    if (!p_ivt_data)
    {
        /* handle error, application-specific */
        fclose(pFile);
        return p_ivt_data;
    }
    /* read the IVT data from the IVT file */
    if (fread(p_ivt_data, 1, cbyFileSize, pFile) != cbyFileSize)
    {
        /* handle error, application-specific */
        free(p_ivt_data);
        p_ivt_data = 0;
        fclose(pFile);
        return p_ivt_data;
    }
    /* close the IVT file */
    if (0 != fclose(pFile))
    {
        /* handle error, application-specific */
        return p_ivt_data;
    }
    /* you can now play effects from the IVT data that was loaded into g_pIVTData */
    return p_ivt_data;
}

/* converts effect type to string*/
static char *_convert_effect_type_to_string(int effect_type)
{
    switch (effect_type) {
        case HAPTIC_EFFECT_TYPE_PERIODIC:
            return "HAPTIC_EFFECT_TYPE_PERIODIC";
        case HAPTIC_EFFECT_TYPE_MAGSWEEP:
            return "HAPTIC_EFFECT_TYPE_MAGSWEEP";
        case HAPTIC_EFFECT_TYPE_TIMELINE:
            return "HAPTIC_EFFECT_TYPE_TIMELINE";
        case HAPTIC_EFFECT_TYPE_STREAMING:
            return "HAPTIC_EFFECT_TYPE_STREAMING";
        case HAPTIC_EFFECT_TYPE_WAVEFORM:
            return "HAPTIC_EFFECT_TYPE_WAVEFORM";
    }
	return NULL;
}

/*This functions gets Periodic effect details using devman API*/
static int _get_periodic_effect_details(const unsigned char *pivt_data, int index, HapticPeriodic *periodic_effect)
{
    int result = -1;
    result = device_haptic_get_periodic_effect_definition(pivt_data, index,
                        &periodic_effect->duration, &periodic_effect->magnitude,
                        &periodic_effect->period, &periodic_effect->style,
                        &periodic_effect->attacktime, &periodic_effect->attacklevel,
                        &periodic_effect->fadetime, &periodic_effect->fadelevel);
    if (result == 0) {
        DBG("device_haptic_get_periodic_effect_definition() Success");
        DBG("Duration      : %d", periodic_effect->duration);
        DBG("Magnitude     : %d", periodic_effect->magnitude);
        DBG("Period        : %d", periodic_effect->period);
        DBG("Style & Wave Type : %d", periodic_effect->style);
        DBG("Attacktime    : %d", periodic_effect->attacktime);
        DBG("Attacklevel   : %d", periodic_effect->attacklevel);
        DBG("Fadetime      : %d", periodic_effect->fadetime);
        DBG("Fadelevel     : %d", periodic_effect->fadelevel);
    }  else {
        DBG("device_haptic_get_periodic_effect_definition() failed. Reason:%d", result);
    }
    return result;
}

/*This functions gets MagSweep effect details using devman API*/
static int _get_magsweep_effect_details(const unsigned char *p_ivt_data, int index, HapticMagSweep *magsweep_effect)
{
    int result = -1;
    result = device_haptic_get_magsweep_effect_definition(p_ivt_data, index,
                        &magsweep_effect->duration, &magsweep_effect->magnitude,
                        &magsweep_effect->style,
                        &magsweep_effect->attacktime, &magsweep_effect->attacklevel,
                        &magsweep_effect->fadetime, &magsweep_effect->fadelevel);
    if (result == 0) {
        DBG("device_haptic_get_magsweep_effect_definition() Success");
        DBG("Duration      : %d", magsweep_effect->duration);
        DBG("Magnitude     : %d", magsweep_effect->magnitude);
        DBG("Style         : %d", magsweep_effect->style);
        DBG("Attacktime    : %d", magsweep_effect->attacktime);
        DBG("Attacklevel   : %d", magsweep_effect->attacklevel);
        DBG("Fadetime      : %d", magsweep_effect->fadetime);
        DBG("Fadelevel     : %d", magsweep_effect->fadelevel);
    }  else {
        DBG("device_haptic_get_magsweep_effect_definition() failed. Reason:%d", result);
    }
    return result;
}

/*This functions gets Timeline effect details using devman API*/
static int _get_timeline_effect_details(const unsigned char *p_ivt_data, int index, HapticElement *timeline_effect)
{
    unsigned char ivt_buffer[IVT_BUFFER_SIZE+1] = {0,};
    int result = -1;

	result = device_haptic_initialize_buffer(ivt_buffer, sizeof(ivt_buffer));
    if (result ==0) {
        result = device_haptic_read_element(ivt_buffer, IVT_BUFFER_SIZE, 0, 0, timeline_effect);
        if (result == 0) {
            DBG("Element type:%d", timeline_effect->elementtype);
        } else {
            DBG("device_haptic_read_element() failed. Reason :%d", result);
        }
    } else {
        DBG("device_haptic_initialize_buffer() failed. Reason:%d", result);
    }
    return result;
}

/* This function parses the Periodic effect received and generates
 * corresponding LED pattern for it */
static char *_parse_periodic_effect_and_generate_led_pattern(HapticPeriodic periodic, int *buffer_size)
{
	char *led_pattern = NULL;
    int unit = periodic.magnitude/10;
    int i = 0;
    int sample_index = 0;
    int value = 0;
    int style = 0;
    int wave_type = 0;
    int mid = periodic.period/2;
    int j = 0;
    int step = 0;
    int delta = 0;
    int base = BASE*unit;
    char const_value ='0';

	led_pattern = (char*)calloc(((periodic.duration/SAMPLE_INTERVAL)+1), sizeof(char));
	if (led_pattern == NULL) {
		DBG("Memory allocation failure");
		return NULL;
	}

    if (periodic.magnitude >0)
        const_value = '1';

    if (periodic.attacktime == 0 && periodic.attacklevel == 0
        && periodic.fadetime == 0 && periodic.fadelevel == 0) {
        DBG("Periodic effect");

        if (periodic.style <=0) {
            DBG("Unknown periodic effect");
            free(led_pattern);
            return NULL;
        } else {
            /* Extract Style and wave type*/
            DBG("Style and wave type: %d", periodic.style);
            style = periodic.style && HAPTIC_STYLE_SUPPORT_MASK;
            wave_type = periodic.style && HAPTIC_WAVETYPE_SUPPORT_MASK;
            DBG("Style     : %d", style);
            DBG("Wave type : %d", wave_type);

            /* Generate pattern based on Wave type. Ignore Style*/
            switch (wave_type) {
            case WAVETYPE_SQUARE:
            case WAVETYPE_SAWTOOTHDOWN:
            {
                for (i = 0; i<periodic.duration; i =i+periodic.period) {
                    for (j =SAMPLE_INTERVAL; j<=periodic.period; j = j+SAMPLE_INTERVAL) {
                        if (j <= mid)
                            led_pattern[sample_index++] = '1';
                        else
                            led_pattern[sample_index++] = '0';
                    }
                }
                break;
            }
            case WAVETYPE_TRIANGLE:
            case WAVETYPE_SINE:
            {
                step = periodic.magnitude/mid;
                delta = step * SAMPLE_INTERVAL;
                for (i = 0; i<periodic.duration; i =i+periodic.period) {
                    value = 0;
                    for (j =SAMPLE_INTERVAL; j<=periodic.period; j = j+SAMPLE_INTERVAL) {
                        if (j <= mid)
                            value = value + delta;
                        else
                            value = value - delta;
                        if (value>= base)
                            led_pattern[sample_index++] = '1';
                        else
                            led_pattern[sample_index++] = '0';
                    }
                }
                break;
            }
            case WAVETYPE_SAWTOOTHUP:
            {
                for (i = 0; i<periodic.duration; i =i+periodic.period) {
                    for (j =SAMPLE_INTERVAL; j<=periodic.period; j = j+SAMPLE_INTERVAL) {
                        if (j > mid)
                            led_pattern[sample_index++] = '1';
                        else
                            led_pattern[sample_index++] = '0';
                    }
                }
                break;
            }
            default: DBG("Unknown wave type\n");
                break;
            }
         }
    } else {
        /*TODO*/
        /* handling periodic effect if attacktime and fade time less than period*/
        /* Need to keep repeating the pattern with attack and fade effectes within period till duration is reached*/
        if (periodic.attacktime>periodic.period || periodic.fadetime>periodic.period) {
            if (periodic.attacktime >0) {
                DBG("Attack time present\n");

                if (periodic.attacklevel >periodic.magnitude) { /* Decrementing effect */
                    step = (periodic.attacklevel - periodic.magnitude)/periodic.attacktime;
                } else if (periodic.attacklevel <periodic.magnitude) { /* Incrementing effect */
                    step = ( periodic.magnitude - periodic.attacklevel)/periodic.attacktime;
                }
                delta = step * SAMPLE_INTERVAL;

                for (i=SAMPLE_INTERVAL; i<= periodic.attacktime; i = i+SAMPLE_INTERVAL) {
                    value = value+delta;
                    if (value>base)
                        led_pattern[sample_index++] = '1';
                    else
                        led_pattern[sample_index++] = '0';
                }
            }

            for (i = periodic.attacktime+SAMPLE_INTERVAL; i<= (periodic.duration-periodic.fadetime); i = i+ SAMPLE_INTERVAL) {
                led_pattern[sample_index++] = const_value;
            }
            if (periodic.fadetime >0) {
                step = (periodic.magnitude - periodic.fadelevel)/periodic.fadetime;
                delta = step* SAMPLE_INTERVAL;
                value = periodic.magnitude;
                for (i = (periodic.duration-periodic.fadetime+SAMPLE_INTERVAL); i<= periodic.duration; i = i+ SAMPLE_INTERVAL) {
                    value = value - delta;
                    if (value>base)
                        led_pattern[sample_index++] = '1';
                    else
                        led_pattern[sample_index++] = '0';
                }
            }
        }
    }
    /*To mark end of effect*/
    led_pattern[sample_index++] = '0';
    *buffer_size = sample_index;
    DBG("LED Pattern for Periodic effect: %s", led_pattern);
    return led_pattern;
}

/* This function parses the MagSweep effect received and generates
 * corresponding LED pattern for it */
static char *_parse_magsweep_effect_and_generate_led_pattern(HapticMagSweep mag_sweep, int *buffer_size)
{
    int unit = mag_sweep.magnitude/10;
    int i =0;
    int sample_index = 0;
    int step = 0;
    int delta = 0;
    int value =0;
    int base = BASE*unit;
    char const_value ='0';
    char *led_pattern = NULL;

	led_pattern = (char*)calloc(((mag_sweep.duration/SAMPLE_INTERVAL)+1), sizeof(char));
    if (led_pattern == NULL) {
        DBG("Memory allocation failure");
        return NULL;
    }

    if (mag_sweep.magnitude >0)
        const_value = '1';

    if (mag_sweep.attacktime == 0 && mag_sweep.attacklevel == 0
        && mag_sweep.fadetime == 0 && mag_sweep.fadelevel == 0) {
        /* Constant effect with maximum magnitude*/
        DBG("Constant effect");

        for (i = 0; i<=mag_sweep.duration; i=i+SAMPLE_INTERVAL) {
            led_pattern[sample_index++] = const_value;
        }
    } else {
        DBG("Varying effect");
        /* Handling Attack effect*/
        if (mag_sweep.attacktime >0) {
            DBG("Attack time present");

            if (mag_sweep.attacklevel >mag_sweep.magnitude) { /* Decrementing effect */
                step = (mag_sweep.attacklevel - mag_sweep.magnitude)/mag_sweep.attacktime;
            } else if (mag_sweep.attacklevel <mag_sweep.magnitude) { /* Incrementing effect */
                step = ( mag_sweep.magnitude - mag_sweep.attacklevel)/mag_sweep.attacktime;
            }
            delta = step * SAMPLE_INTERVAL;

            for (i=SAMPLE_INTERVAL; i<= mag_sweep.attacktime; i = i+SAMPLE_INTERVAL) {
                value = value+delta;
                if (value>base)
                    led_pattern[sample_index++] = '1';
                else
                    led_pattern[sample_index++] = '0';
            }
        }
        /* For Handling constant effect between attacktime and fade time*/
        for (i = mag_sweep.attacktime+SAMPLE_INTERVAL; i<= (mag_sweep.duration-mag_sweep.fadetime); i = i+ SAMPLE_INTERVAL) {
            led_pattern[sample_index++] = const_value;
        }
        /* Handling fading effect*/
        if (mag_sweep.fadetime >0) {
            step = (mag_sweep.magnitude - mag_sweep.fadelevel)/mag_sweep.fadetime;
            delta = step* SAMPLE_INTERVAL;
            value = mag_sweep.magnitude;
            for (i = (mag_sweep.duration-mag_sweep.fadetime+ SAMPLE_INTERVAL); i<= mag_sweep.duration; i = i+ SAMPLE_INTERVAL) {
                value = value - delta;
                if (value>base)
                    led_pattern[sample_index++] = '1';
                else
                    led_pattern[sample_index++] = '0';
            }
        }

    }
    /*To mark end of effect*/
    led_pattern[sample_index++] = '0';
    *buffer_size = sample_index;
    DBG("Appending 0 at the end");
    DBG("LED Pattern for MagSweep effect: %s", led_pattern);
    return led_pattern;
}

static int _write_pattern_to_file(const char *binary_path, char *pled_dat, int buf_size, int *opened_flag)
{
	FILE *ptr_myfile = NULL;

    DBG("LED file name:%s", binary_path);
	/* Open file for the FIRST time */
	if (*opened_flag) {
        ptr_myfile = fopen(binary_path, "wb+");
        if (!ptr_myfile) {
            DBG("Unable to open file!");
            return -1;
        }
		*opened_flag = 0;
    } else {
        ptr_myfile = fopen(binary_path, "ab+");
        if (!ptr_myfile) {
            DBG("Unable to open file!");
            return -1;
        }
    }

    DBG("Buffer: %s", pled_dat);
    if (fwrite(pled_dat, 1, buf_size, ptr_myfile) <= 0) {
        DBG("fwrite() failed");
    } else {
        DBG("fwrite() success");
    }

    fclose(ptr_myfile);
	return 0;
}

static int _convert_ivt_to_binary(const char *haptic_path, const char *binary_path)
{
	unsigned char *pivt_dat = NULL;
	char *pled_dat = NULL;
	int result = -1;
	int effect_cnt = -1;
	int effect_type = -1;
	int effect_duration = -1;
	int buf_size = -1;
	int opened_flag = 1;	// 1 : Not opened, 0 : Opened
	HapticPeriodic periodic_effect;
	HapticMagSweep magsweep_effect;
	HapticElement element;
	int i;

	/* Load IVT file into memory */
	pivt_dat = _load_ivt_file(haptic_path);
	if (!pivt_dat) {
		DBG("Loading IVT failed");
		return -1;
	}

	/* Get total number of effects in IVT file */
	effect_cnt = device_haptic_get_effect_count(pivt_dat);
	DBG("device_haptic_get_effect_count() Return:%d", effect_cnt);

    /* Parse effects in IVT */
	for (i = 0; i < effect_cnt; i++) {
		/* Get effect type*/
		result = device_haptic_get_effect_type(pivt_dat, i, &effect_type);
		if (result < 0) {
			DBG("EffectNo:%d Getting Effect Type Failed. Reason:%d", (i+1), result);
			continue;
		}

		DBG("EffectNo:%d EffectType:%s\n", (i+1), _convert_effect_type_to_string(effect_type));
		switch (effect_type) {
		case HAPTIC_EFFECT_TYPE_PERIODIC:
			memset(&periodic_effect, 0x00, sizeof(HapticPeriodic));
			if (_get_periodic_effect_details(pivt_dat, i, &periodic_effect) == 0) {
				/* Parse periodic effect type*/
				pled_dat = _parse_periodic_effect_and_generate_led_pattern(periodic_effect, &buf_size);
				if (pled_dat) {
					_write_pattern_to_file(binary_path, pled_dat, buf_size, &opened_flag);
					free(pled_dat);
				}
			}
			break;
		case HAPTIC_EFFECT_TYPE_MAGSWEEP:
			memset(&magsweep_effect, 0x00, sizeof(HapticMagSweep));
			if (_get_magsweep_effect_details(pivt_dat, i, &magsweep_effect) == 0) {
				/* Parse magsweep effect type*/
				pled_dat = _parse_magsweep_effect_and_generate_led_pattern(magsweep_effect, &buf_size);
				if (pled_dat) {
					_write_pattern_to_file(binary_path, pled_dat, buf_size, &opened_flag);
					free(pled_dat);
				}
			}
			break;
		case HAPTIC_EFFECT_TYPE_TIMELINE:
			memset(&element, 0x00, sizeof(HapticElement));
			if (_get_timeline_effect_details(pivt_dat, i, &element) > 0) {
				device_haptic_get_effect_duration(pivt_dat, i, &effect_duration);
				DBG("Timeline effect duration:%d", effect_duration);
			}
			break;
		case HAPTIC_EFFECT_TYPE_STREAMING:
		case HAPTIC_EFFECT_TYPE_WAVEFORM:
		default:
			DBG("Unsupported effect type");
			break;
		}
	}

	free(pivt_dat);
	return 0;
}

EXTAPI int device_haptic_convert_to_binary(const char *haptic_name)
{
	DBG("this api is not implementation yet");
	return -1;
}
