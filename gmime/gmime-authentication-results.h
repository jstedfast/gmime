/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef __GMIME_AUTHENTICATION_RESULTS_H__
#define __GMIME_AUTHENTICATION_RESULTS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GMIME_TYPE_AUTHENTICATION_RESULTS (g_mime_authentication_results_get_type ())
#define GMIME_TYPE_AUTHENTICATION_METHOD_RESULT (g_mime_authentication_method_result_get_type ())
#define GMIME_TYPE_AUTHENTICATION_METHOD_PROPERTY (g_mime_authentication_method_property_get_type ())

/**
 * GMimeAuthenticationResults:
 *
 * An Authentication-Results or ARC-Authentication-Results header value.
 **/
G_DECLARE_FINAL_TYPE (GMimeAuthenticationResults, g_mime_authentication_results, GMIME, AUTHENTICATION_RESULTS, GObject)

/**
 * GMimeAuthenticationMethodResult:
 *
 * An authentication method, its result and ordered properties.
 **/
G_DECLARE_FINAL_TYPE (GMimeAuthenticationMethodResult, g_mime_authentication_method_result, GMIME, AUTHENTICATION_METHOD_RESULT, GObject)

/**
 * GMimeAuthenticationMethodProperty:
 *
 * An immutable authentication method property.
 **/
G_DECLARE_FINAL_TYPE (GMimeAuthenticationMethodProperty, g_mime_authentication_method_property, GMIME, AUTHENTICATION_METHOD_PROPERTY, GObject)

GMimeAuthenticationResults *g_mime_authentication_results_new (const char *authserv_id);
GMimeAuthenticationResults *g_mime_authentication_results_parse (const char *text, gssize length, GError **error);
char *g_mime_authentication_results_to_string (GMimeAuthenticationResults *results);
const char *g_mime_authentication_results_get_authentication_service_identifier (GMimeAuthenticationResults *results);
gboolean g_mime_authentication_results_get_instance (GMimeAuthenticationResults *results, int *instance);
void g_mime_authentication_results_set_instance (GMimeAuthenticationResults *results, int instance);
void g_mime_authentication_results_clear_instance (GMimeAuthenticationResults *results);
gboolean g_mime_authentication_results_get_version (GMimeAuthenticationResults *results, int *version);
void g_mime_authentication_results_set_version (GMimeAuthenticationResults *results, int version);
void g_mime_authentication_results_clear_version (GMimeAuthenticationResults *results);

int g_mime_authentication_results_get_result_count (GMimeAuthenticationResults *results);
GMimeAuthenticationMethodResult *g_mime_authentication_results_get_result (GMimeAuthenticationResults *results, int index);
void g_mime_authentication_results_set_result (GMimeAuthenticationResults *results, int index, GMimeAuthenticationMethodResult *result);
void g_mime_authentication_results_add_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result);
void g_mime_authentication_results_insert_result (GMimeAuthenticationResults *results, int index, GMimeAuthenticationMethodResult *result);
int g_mime_authentication_results_index_of_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result);
gboolean g_mime_authentication_results_contains_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result);
gboolean g_mime_authentication_results_remove_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result);
void g_mime_authentication_results_remove_result_at (GMimeAuthenticationResults *results, int index);
void g_mime_authentication_results_clear_results (GMimeAuthenticationResults *results);

GMimeAuthenticationMethodResult *g_mime_authentication_method_result_new (const char *method, const char *result);
char *g_mime_authentication_method_result_to_string (GMimeAuthenticationMethodResult *result);
const char *g_mime_authentication_method_result_get_method (GMimeAuthenticationMethodResult *result);
const char *g_mime_authentication_method_result_get_result (GMimeAuthenticationMethodResult *result);
const char *g_mime_authentication_method_result_get_office365_authentication_service_identifier (GMimeAuthenticationMethodResult *result);
gboolean g_mime_authentication_method_result_get_version (GMimeAuthenticationMethodResult *result, int *version);
void g_mime_authentication_method_result_set_version (GMimeAuthenticationMethodResult *result, int version);
void g_mime_authentication_method_result_clear_version (GMimeAuthenticationMethodResult *result);
const char *g_mime_authentication_method_result_get_result_comment (GMimeAuthenticationMethodResult *result);
void g_mime_authentication_method_result_set_result_comment (GMimeAuthenticationMethodResult *result, const char *comment);
const char *g_mime_authentication_method_result_get_reason (GMimeAuthenticationMethodResult *result);
void g_mime_authentication_method_result_set_reason (GMimeAuthenticationMethodResult *result, const char *reason);
const char *g_mime_authentication_method_result_get_action (GMimeAuthenticationMethodResult *result);

int g_mime_authentication_method_result_get_property_count (GMimeAuthenticationMethodResult *result);
GMimeAuthenticationMethodProperty *g_mime_authentication_method_result_get_property (GMimeAuthenticationMethodResult *result, int index);
void g_mime_authentication_method_result_set_property (GMimeAuthenticationMethodResult *result, int index, GMimeAuthenticationMethodProperty *property);
void g_mime_authentication_method_result_add_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property);
void g_mime_authentication_method_result_insert_property (GMimeAuthenticationMethodResult *result, int index, GMimeAuthenticationMethodProperty *property);
int g_mime_authentication_method_result_index_of_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property);
gboolean g_mime_authentication_method_result_contains_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property);
gboolean g_mime_authentication_method_result_remove_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property);
void g_mime_authentication_method_result_remove_property_at (GMimeAuthenticationMethodResult *result, int index);
void g_mime_authentication_method_result_clear_properties (GMimeAuthenticationMethodResult *result);

GMimeAuthenticationMethodProperty *g_mime_authentication_method_property_new (const char *ptype, const char *property, const char *value);
const char *g_mime_authentication_method_property_get_property_type (GMimeAuthenticationMethodProperty *property);
const char *g_mime_authentication_method_property_get_property (GMimeAuthenticationMethodProperty *property);
const char *g_mime_authentication_method_property_get_value (GMimeAuthenticationMethodProperty *property);
char *g_mime_authentication_method_property_to_string (GMimeAuthenticationMethodProperty *property);

G_END_DECLS

#endif /* __GMIME_AUTHENTICATION_RESULTS_H__ */
