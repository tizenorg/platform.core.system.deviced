/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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


#ifndef __DISPLAY_BACKLIGHT_SERVICE_H__
#define __DISPLAY_BACKLIGHT_SERVICE_H__

#define BACKLIGHT_MAX_BRIGHTNESS	100

enum backlight_mode {
	BACKLIGHT_MODE_MANUAL,
	BACKLIGHT_MODE_SENSOR,
};

int backlight_get_brightness(int *brightness);
int backlight_set_brightness(int brightness);
int backlight_set_mode(enum backlight_mode mode);
int backlight_service_load(void);
int backlight_service_free(void);

#endif
