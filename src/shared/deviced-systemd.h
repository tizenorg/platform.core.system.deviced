/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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


#ifndef __DEVICED_SYSTEMD_H__
#define __DEVICED_SYSTEMD_H__

#include <dbus/dbus.h>
#include <gio/gio.h>

int deviced_systemd_start_unit(char *name);
int deviced_systemd_stop_unit(char *name);

int deviced_systemd_get_manager_property(const char *iface, const char *property, GVariant **variant);
int deviced_systemd_get_unit_property(const char *unit, const char *property, GVariant **variant);
int deviced_systemd_get_service_property(const char* unit, const char* property, GVariant **variant);

int deviced_systemd_get_manager_property_as_int32(const char *iface, const char *property, int *result);
int deviced_systemd_get_manager_property_as_uint32(const char *iface, const char *property, unsigned int *result);
int deviced_systemd_get_manager_property_as_int64(const char *iface, const char *property, long long *result);
int deviced_systemd_get_manager_property_as_uint64(const char *iface, const char *property, unsigned long long *result);
int deviced_systemd_get_manager_property_as_string(const char *iface, const char *property, char **result);
int deviced_systemd_get_unit_property_as_int32(const char *unit, const char *property, int *result);
int deviced_systemd_get_unit_property_as_uint32(const char *unit, const char *property, unsigned int *result);
int deviced_systemd_get_unit_property_as_int64(const char *unit, const char *property, long long *result);
int deviced_systemd_get_unit_property_as_uint64(const char *unit, const char *property, unsigned long long *result);
int deviced_systemd_get_unit_property_as_string(const char *unit, const char *property, char **result);
int deviced_systemd_get_service_property_as_int32(const char *unit, const char *property, int *result);
int deviced_systemd_get_service_property_as_uint32(const char *unit, const char *property, unsigned int *result);
int deviced_systemd_get_service_property_as_int64(const char *unit, const char *property, long long *result);
int deviced_systemd_get_service_property_as_uint64(const char *unit, const char *property, unsigned long long *result);
int deviced_systemd_get_service_property_as_string(const char *unit, const char *property, char **result);

int deviced_systemd_instance_new_from_template(const char *template, const char *instance, const char **name);

#endif
