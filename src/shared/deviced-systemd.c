/*
 * deviced
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dbus/dbus.h>
#include <gio/gio.h>

#include "common.h"
#include "dbus.h"

#include "core/log.h"


#define DBUS_IFACE_DBUS_PROPERTIES	DBUS_SERVICE_DBUS ".Properties"

#define SYSTEMD_DBUS_DEST		"org.freedesktop.systemd1"
#define SYSTEMD_DBUS_IFACE_MANAGER	SYSTEMD_DBUS_DEST ".Manager"
#define SYSTEMD_DBUS_IFACE_UNIT		SYSTEMD_DBUS_DEST ".Unit"
#define SYSTEMD_DBUS_IFACE_SERVICE	SYSTEMD_DBUS_DEST ".Service"
#define SYSTEMD_DBUS_IFACE_TARGET	SYSTEMD_DBUS_DEST ".Target"

#define SYSTEMD_DBUS_PATH		"/org/freedesktop/systemd1"
#define SYSTEMD_DBUS_UNIT_PATH		SYSTEMD_DBUS_PATH "/unit/"

#define SYSTEMD_UNIT_ESCAPE_CHAR	".-"

typedef unsigned int uint;
typedef long long int64;
typedef unsigned long long uint64;

#define g_variant_type_int32		G_VARIANT_TYPE_INT32
#define g_variant_type_int64		G_VARIANT_TYPE_INT64
#define g_variant_type_uint32		G_VARIANT_TYPE_UINT32
#define g_variant_type_uint64		G_VARIANT_TYPE_UINT64
#define g_variant_type_string		G_VARIANT_TYPE_STRING

#define g_variant_get_function_int32(v)		g_variant_get_int32(v)
#define g_variant_get_function_int64(v)		g_variant_get_int64(v)
#define g_variant_get_function_uint32(v)	g_variant_get_uint32(v)
#define g_variant_get_function_uint64(v)	g_variant_get_uint64(v)
#define g_variant_get_function_string(v)	g_variant_dup_string(v, NULL)


static int deviced_systemd_proxy_call_sync(const char *name,
					   const char *path,
					   const char *iface,
					   const char *method,
					   GVariant *variant,
					   GVariant **reply)
{
	GVariant *gvar;
	GError *error;
	GDBusProxy *proxy;
	int ret = 0;

#if (GLIB_MAJOR_VERSION <= 2 && GLIB_MINOR_VERSION < 36)
	g_type_init();
#endif

	error = NULL;
	proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
					      G_DBUS_PROXY_FLAGS_NONE,
					      NULL, /* GDBusInterfaceInfo */
					      name,
					      path,
					      iface,
					      NULL, /* GCancellable */
					      &error);

	if (proxy == NULL) {
		_E("Error creating proxy: %s", error->message);
		ret = error->code;
		g_error_free(error);
		return ret;
	}

	error = NULL;
	gvar = g_dbus_proxy_call_sync(proxy,
				      method,
				      variant,
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL, /* GCancellable */
				      &error);

	if (error) {
		_E("Error proxy call sync: %s", error->message);
		ret = error->code;
		g_error_free(error);
		return -ret;
	}

	g_assert(gvar != NULL);
	*reply = gvar;
	g_clear_error(&error);
	g_object_unref(proxy);

	return 0;
}

static int deviced_systemd_start_or_stop_unit(char *method, char *name)
{
	int ret;
	GVariant *reply = NULL;

	_I("Starting: %s %s", method, name);
	ret = deviced_systemd_proxy_call_sync(SYSTEMD_DBUS_DEST,
					      SYSTEMD_DBUS_PATH,
					      SYSTEMD_DBUS_IFACE_MANAGER,
					      method,
					      g_variant_new("(ss)",
							    name,
							    "replace"),
					      &reply);
	if (ret < 0)
		goto finish;

	if (!g_variant_is_of_type(reply, G_VARIANT_TYPE("(o)"))) {
		ret = -EBADMSG;
		goto finish;
	}

	ret = 0;
finish:
	_I("Finished(%d): %s %s", ret, method, name);

	if (reply)
		g_variant_unref(reply);

	return ret;
}

int deviced_systemd_start_unit(char *name)
{
	assert(name);

	return deviced_systemd_start_or_stop_unit("StartUnit", name);
}

int deviced_systemd_stop_unit(char *name)
{
	assert(name);

	return deviced_systemd_start_or_stop_unit("StopUnit", name);
}

static char *deviced_systemd_get_unit_dbus_path(const char *unit)
{
	char *path = NULL;
	int i, escape;
	size_t p, k, prefix_len, unit_len = strlen(unit);
	int path_len, len;

	assert(unit);

	for (escape = 0, p = 0; p < unit_len; escape++) {
		k = strcspn(unit+p, SYSTEMD_UNIT_ESCAPE_CHAR);
		if (p+k >= unit_len)
			break;
		p += k+1;
	}

	prefix_len = strlen(SYSTEMD_DBUS_UNIT_PATH);
	/* assume we try to get object path of foo-bar.service then
	* the object path will be
	* "/org/freedesktop/systemd1/unit/foo_2dbar_2eservice\n". In
	* this case we can find two escape characters, one of escape
	* char('-') is changed to three of char("_2d"). So the total
	* length will be: */
	/* (PREFIX) + (unit - escape + 3*escape) + NULL */
	path_len = prefix_len + (unit_len - escape)
		+ (escape * 3 * sizeof(char)) + 1;
	path = (char *)calloc(path_len, sizeof(char));
	if (!path)
		return NULL;

	strncpy(path, SYSTEMD_DBUS_UNIT_PATH, prefix_len + 1);
	for (i = 0, p = 0; i <= escape; i++) {
		k = strcspn(unit + p, SYSTEMD_UNIT_ESCAPE_CHAR);
		strncpy(path + prefix_len, unit + p, k);
		if (k < strlen(unit + p)) {
			len = path_len - (prefix_len + k);
			snprintf(path + prefix_len + k, len,
					"_%x", *(unit + p + k) & 0xff);
			prefix_len += k+3;
			p += k+1;
		}
	}

	return path;
}

static int deviced_systemd_get_property(const char *name,
					const char *path,
					const char *iface,
					const char *method,
					const char *interface,
					const char *property,
					GVariant **reply)
{
	int ret;
	GVariant *variant;

	variant = g_variant_new("(ss)",
				interface,
				property);

	ret = deviced_systemd_proxy_call_sync(name,
					      path,
					      iface,
					      method,
					      variant,
					      reply);

	return ret;
}

int deviced_systemd_get_manager_property(const char *iface,
					 const char *property,
					 GVariant **variant)
{
	assert(iface);
	assert(property);

	return deviced_systemd_get_property(SYSTEMD_DBUS_DEST,
					    SYSTEMD_DBUS_PATH,
					    DBUS_IFACE_DBUS_PROPERTIES,
					    "Get",
					    iface,
					    property,
					    variant);
}

int deviced_systemd_get_unit_property(const char *unit,
				      const char *property,
				      GVariant **variant)
{
	int r;
	char *escaped;

	assert(unit);
	assert(property);

	escaped = deviced_systemd_get_unit_dbus_path(unit);

	r = deviced_systemd_get_property(SYSTEMD_DBUS_DEST,
					 escaped,
					 DBUS_IFACE_DBUS_PROPERTIES,
					 "Get",
					 SYSTEMD_DBUS_IFACE_UNIT,
					 property,
					 variant);
	free(escaped);
	return r;
}

int deviced_systemd_get_service_property(const char *unit,
					 const char *property,
					 GVariant **variant)
{
	int ret;
	char *escaped;

	assert(unit);
	assert(property);

	escaped = deviced_systemd_get_unit_dbus_path(unit);

	ret = deviced_systemd_get_property(SYSTEMD_DBUS_DEST,
					   escaped,
					   DBUS_IFACE_DBUS_PROPERTIES,
					   "Get",
					   SYSTEMD_DBUS_IFACE_SERVICE,
					   property,
					   variant);
	free(escaped);
	return ret;
}

#define DEFINE_SYSTEMD_GET_PROPERTY(iface, type, value)			\
	int deviced_systemd_get_##iface##_property_as_##type(		\
			const char *target,				\
			const char *property,				\
			value*result)					\
	{								\
									\
		GVariant *var;						\
		GVariant *inner;					\
		int r;							\
									\
		r = deviced_systemd_get_##iface##_property(target,	\
							   property,	\
							   &var);	\
		if (r < 0) {						\
			_E("Failed to get property:\n"			\
			   "  target: %s\n"				\
			   "  property: %s",				\
			   target, property);				\
			return r;					\
		}							\
									\
		if (!g_variant_is_of_type(var,				\
					  G_VARIANT_TYPE("(v)")))	\
			return -EBADMSG;				\
									\
		g_variant_get(var, "(v)", &inner);			\
		if (!g_variant_is_of_type(inner,			\
					  g_variant_type_##type))	\
			return -EBADMSG;				\
									\
		*result = g_variant_get_function_##type(inner);		\
		g_variant_unref(var);					\
									\
		return 0;						\
	}

/* int deviced_systemd_get_manager_property_as_int32(const char *iface, const char *property, int *result); */
DEFINE_SYSTEMD_GET_PROPERTY(manager, int32, int)
/* int deviced_systemd_get_manager_property_as_uint32(const char *iface, const char *property, unsigned int *result); */
DEFINE_SYSTEMD_GET_PROPERTY(manager, uint32, uint)
/* int deviced_systemd_get_manager_property_as_int64(const char *iface, const char *property, long long *result); */
DEFINE_SYSTEMD_GET_PROPERTY(manager, int64, long long)
/* int deviced_systemd_get_manager_property_as_uint64(const char *iface, const char *property, unsigned long long *result); */
DEFINE_SYSTEMD_GET_PROPERTY(manager, uint64, unsigned long long)
/* int deviced_systemd_get_manager_property_as_string(const char *iface, const char *property, char **result); */
DEFINE_SYSTEMD_GET_PROPERTY(manager, string, char*)
/* int deviced_systemd_get_unit_property_as_int32(const char *unit, const char *property, int *result); */
DEFINE_SYSTEMD_GET_PROPERTY(unit, int32, int)
/* int deviced_systemd_get_unit_property_as_uint32(const char *unit, const char *property, unsigned int *result); */
DEFINE_SYSTEMD_GET_PROPERTY(unit, uint32, uint)
/* int deviced_systemd_get_unit_property_as_int64(const char *unit, const char *property, long long *result); */
DEFINE_SYSTEMD_GET_PROPERTY(unit, int64, long long)
/* int deviced_systemd_get_unit_property_as_uint64(const char *unit, const char *property, unsigned long long *result); */
DEFINE_SYSTEMD_GET_PROPERTY(unit, uint64, unsigned long long)
/* int deviced_systemd_get_unit_property_as_string(const char *unit, const char *property, char **result); */
DEFINE_SYSTEMD_GET_PROPERTY(unit, string, char*)
/* int deviced_systemd_get_service_property_as_int32(const char *unit, const char *property, int *result); */
DEFINE_SYSTEMD_GET_PROPERTY(service, int32, int)
/* int deviced_systemd_get_service_property_as_uint32(const char *unit, const char *property, unsigned int *result); */
DEFINE_SYSTEMD_GET_PROPERTY(service, uint32, uint)
/* int deviced_systemd_get_service_property_as_int64(const char *unit, const char *property, long long *result); */
DEFINE_SYSTEMD_GET_PROPERTY(service, int64, long long)
/* int deviced_systemd_get_service_property_as_uint64(const char *unit, const char *property, unsigned long long *result); */
DEFINE_SYSTEMD_GET_PROPERTY(service, uint64, unsigned long long)
/* int deviced_systemd_get_service_property_as_string(const char *unit, const char *property, char **result); */
DEFINE_SYSTEMD_GET_PROPERTY(service, string, char*)

int deviced_systemd_instance_new_from_template(const char *template,
					       const char *instance,
					       const char **name)
{
	/* p point '@' */
	/* e point '.' */
	const char *p, *e;
	char *prefix = NULL, *n = NULL;
	int r;

	assert(template);
	p = strchr(template, '@');
	if (!p) {
		r = -EINVAL;
		goto finish;
	}

	prefix = strndup(template, p - template + 1);
	if (!prefix) {
		r = -ENOMEM;
		goto finish;
	}

	e = strrchr(p + 1, '.');
	if (!e) {
		r = -EINVAL;
		goto finish;
	}

	/* verifying template. prefix@.service */
	if (e != p + 1) {
		r = -EINVAL;
		goto finish;
	}

	r = asprintf(&n, "%s%s%s", prefix, instance, e);
	if (r < 0) {
		r = -ENOMEM;
		goto finish;
	}

	*name = n;
	r = 0;
finish:
	if (prefix)
		free(prefix);

	return r;
}
