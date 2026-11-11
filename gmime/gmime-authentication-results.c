/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-authentication-results.h"
#include "gmime-object-private.h"
#include "gmime-parse-utils.h"
#include "gmime-table-private.h"

/**
 * SECTION: gmime-authentication-results
 * @title: Authentication-Results
 * @short_description: Authentication result header models
 *
 * Parsed Authentication-Results and ARC-Authentication-Results values, with
 * MimeKit-compatible recovery for Office365 and dotted authentication methods.
 * Collections retain references, preserve order and allow duplicate objects.
 * The models describe reported results; they neither authenticate a message
 * nor establish whether a header or its authentication service is trustworthy.
 *
 * Constructors and setters store UTF-8 strings without syntax validation.
 * Comments retain their inner comment syntax, including escapes and nesting.
 * Use #GMimeHeader to fold serialized values for transport.
 * g_mime_authentication_results_to_string() produces unfolded model text,
 * not a complete header.
 **/

G_DEFINE_FINAL_TYPE (GMimeAuthenticationResults, g_mime_authentication_results, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GMimeAuthenticationMethodResult, g_mime_authentication_method_result, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GMimeAuthenticationMethodProperty, g_mime_authentication_method_property, G_TYPE_OBJECT)

static void
authentication_results_finalize (GObject *object)
{
	GMimeAuthenticationResults *results = GMIME_AUTHENTICATION_RESULTS (object);

	g_free (results->authserv_id);
	g_ptr_array_unref (results->results);

	G_OBJECT_CLASS (g_mime_authentication_results_parent_class)->finalize (object);
}

static void
g_mime_authentication_results_class_init (GMimeAuthenticationResultsClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = authentication_results_finalize;
}

static void
g_mime_authentication_results_init (GMimeAuthenticationResults *results)
{
	results->results = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
authentication_method_result_finalize (GObject *object)
{
	GMimeAuthenticationMethodResult *result = GMIME_AUTHENTICATION_METHOD_RESULT (object);

	g_free (result->method);
	g_free (result->result);
	g_free (result->authserv_id);
	g_free (result->comment);
	g_free (result->reason);
	g_free (result->action);
	g_ptr_array_unref (result->properties);

	G_OBJECT_CLASS (g_mime_authentication_method_result_parent_class)->finalize (object);
}

static void
g_mime_authentication_method_result_class_init (GMimeAuthenticationMethodResultClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = authentication_method_result_finalize;
}

static void
g_mime_authentication_method_result_init (GMimeAuthenticationMethodResult *result)
{
	result->properties = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
authentication_method_property_finalize (GObject *object)
{
	GMimeAuthenticationMethodProperty *property = GMIME_AUTHENTICATION_METHOD_PROPERTY (object);

	g_free (property->ptype);
	g_free (property->property);
	g_free (property->value);

	G_OBJECT_CLASS (g_mime_authentication_method_property_parent_class)->finalize (object);
}

static void
g_mime_authentication_method_property_class_init (GMimeAuthenticationMethodPropertyClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = authentication_method_property_finalize;
}

static void
g_mime_authentication_method_property_init (GMimeAuthenticationMethodProperty *property)
{
	property->quoted = -1;
}

/**
 * g_mime_authentication_results_new:
 * @authserv_id: the authentication service identifier
 *
 * Creates a model, copying @authserv_id without validating its syntax.
 *
 * Returns: (transfer full): an empty result collection with no version or ARC instance.
 **/
GMimeAuthenticationResults *
g_mime_authentication_results_new (const char *authserv_id)
{
	GMimeAuthenticationResults *results;

	g_return_val_if_fail (authserv_id != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (authserv_id, -1, NULL), NULL);

	results = g_object_new (GMIME_TYPE_AUTHENTICATION_RESULTS, NULL);
	results->authserv_id = g_strdup (authserv_id);

	return results;
}

/**
 * g_mime_authentication_method_result_new:
 * @method: the authentication method
 * @result: the reported result
 *
 * Creates a method result, copying both strings without syntax validation.
 *
 * Returns: (transfer full): a method result with an empty property collection.
 **/
GMimeAuthenticationMethodResult *
g_mime_authentication_method_result_new (const char *method, const char *result)
{
	GMimeAuthenticationMethodResult *info;

	g_return_val_if_fail (method != NULL && result != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (method, -1, NULL), NULL);
	g_return_val_if_fail (g_utf8_validate (result, -1, NULL), NULL);

	info = g_object_new (GMIME_TYPE_AUTHENTICATION_METHOD_RESULT, NULL);
	info->method = g_strdup (method);
	info->result = g_strdup (result);

	return info;
}

/**
 * g_mime_authentication_method_property_new:
 * @ptype: the property type, such as "header" or "smtp"
 * @property: the property name
 * @value: the property value
 *
 * Empty values and values containing MIME token specials are quoted when
 * serialized. Parsed properties instead preserve their original quoting,
 * except that empty values are always quoted.
 *
 * Returns: (transfer full): an immutable property.
 **/
GMimeAuthenticationMethodProperty *
g_mime_authentication_method_property_new (const char *ptype, const char *property, const char *value)
{
	GMimeAuthenticationMethodProperty *prop;

	g_return_val_if_fail (ptype != NULL && property != NULL && value != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (ptype, -1, NULL), NULL);
	g_return_val_if_fail (g_utf8_validate (property, -1, NULL), NULL);
	g_return_val_if_fail (g_utf8_validate (value, -1, NULL), NULL);

	prop = g_object_new (GMIME_TYPE_AUTHENTICATION_METHOD_PROPERTY, NULL);
	prop->ptype = g_strdup (ptype);
	prop->property = g_strdup (property);
	prop->value = g_strdup (value);

	return prop;
}

static char *
quote_value (const char *value)
{
	GString *str = g_string_new ("\"");
	const char *p;

	for (p = value; *p; p++) {
		if (*p == '"' || *p == '\\')
			g_string_append_c (str, '\\');

		g_string_append_c (str, *p);
	}

	g_string_append_c (str, '"');

	return g_string_free (str, FALSE);
}

static char *
format_service (const char *value)
{
	const unsigned char *p;

	if (!*value)
		return quote_value (value);

	for (p = (const unsigned char *) value; *p; p++) {
		if (*p > 127 || !is_ttoken (*p))
			return quote_value (value);
	}

	return g_strdup (value);
}

static char *
format_property_value (GMimeAuthenticationMethodProperty *property)
{
	gboolean quoted = property->quoted == 1 || !*property->value;
	const unsigned char *p;

	if (property->quoted == -1) {
		for (p = (const unsigned char *) property->value; *p && !quoted; p++)
			quoted = *p <= 32 || *p == 127 || strchr (CHARS_TSPECIAL, *p) != NULL;
	}

	return quoted ? quote_value (property->value) : g_strdup (property->value);
}

/**
 * g_mime_authentication_method_property_to_string:
 * @property: an authentication property
 *
 * Formats the type, name and value as a property assignment.
 *
 * Returns: (transfer full): the serialized property, without leading whitespace.
 **/
char *
g_mime_authentication_method_property_to_string (GMimeAuthenticationMethodProperty *property)
{
	char *value, *text;

	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property), NULL);

	value = format_property_value (property);
	text = g_strdup_printf ("%s.%s=%s", property->ptype, property->property, value);
	g_free (value);

	return text;
}

/**
 * g_mime_authentication_method_result_to_string:
 * @result: a method result
 *
 * Formats the method, result and associated details without folding.
 *
 * Returns: (transfer full): the unfolded method result, including its properties.
 **/
char *
g_mime_authentication_method_result_to_string (GMimeAuthenticationMethodResult *result)
{
	GString *str;
	char *value;
	guint i;

	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	str = g_string_new (NULL);

	if (result->authserv_id)
		g_string_append_printf (str, "%s; ", result->authserv_id);

	g_string_append (str, result->method);

	if (result->has_version)
		g_string_append_printf (str, "/%d", result->version);

	g_string_append_printf (str, "=%s", result->result);

	if (result->comment && *result->comment)
		g_string_append_printf (str, " (%s)", result->comment);

	if (result->reason && *result->reason) {
		value = quote_value (result->reason);
		g_string_append_printf (str, " reason=%s", value);
		g_free (value);
	} else if (result->action && *result->action) {
		value = quote_value (result->action);
		g_string_append_printf (str, " action=%s", value);
		g_free (value);
	}

	for (i = 0; i < result->properties->len; i++) {
		value = g_mime_authentication_method_property_to_string (result->properties->pdata[i]);
		g_string_append_c (str, ' ');
		g_string_append (str, value);
		g_free (value);
	}

	return g_string_free (str, FALSE);
}

/**
 * g_mime_authentication_results_to_string:
 * @results: authentication results
 *
 * Formats the service, optional numbers and ordered method results.
 *
 * Returns: (transfer full): the unfolded value, without a field name or final newline.
 **/
char *
g_mime_authentication_results_to_string (GMimeAuthenticationResults *results)
{
	GString *str;
	char *value;
	guint i;

	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), NULL);

	str = g_string_new (NULL);

	if (results->has_instance)
		g_string_append_printf (str, "i=%d; ", results->instance);

	if (results->authserv_id) {
		value = format_service (results->authserv_id);
		g_string_append (str, value);
		g_free (value);

		if (results->has_version)
			g_string_append_printf (str, " %d", results->version);

		g_string_append (str, "; ");
	}

	for (i = 0; i < results->results->len; i++) {
		if (i > 0)
			g_string_append (str, "; ");

		value = g_mime_authentication_method_result_to_string (results->results->pdata[i]);
		g_string_append (str, value);
		g_free (value);
	}

	if (!results->results->len)
		g_string_append (str, "none");

	return g_string_free (str, FALSE);
}

static char *
parse_string (const char *start, const char *end, gboolean quoted)
{
	char *value = g_utf8_make_valid (start, end - start);
	gboolean escaped = FALSE;
	char *in, *out;

	if (!quoted)
		return value;

	for (in = out = value; *in; in++) {
		switch (*in) {
		case '\r': case '\n':
			escaped = FALSE;
			break;
		case '\t':
			*out++ = ' ';
			escaped = FALSE;
			break;
		case '\\':
			if (escaped)
				*out++ = '\\';
			escaped = !escaped;
			break;
		case '"':
			if (escaped)
				*out++ = '"';
			escaped = FALSE;
			break;
		default:
			*out++ = *in;
			escaped = FALSE;
			break;
		}
	}

	*out = '\0';

	return value;
}

static char *
parse_comment (const char *start, const char *end)
{
	char *value = g_utf8_make_valid (start, end - start);
	char *in, *out, *unfolded;
	gsize length, slashes;

	for (in = out = value; *in; in++) {
		if (*in == '\\' && in[1]) {
			if (in[1] != '\r' && in[1] != '\n')
				*out++ = *in;
			in++;
		}

		*out++ = *in;
	}

	*out = '\0';
	unfolded = g_mime_header_unfold (value);
	g_free (value);

	length = strlen (unfolded);

	do {
		slashes = 0;

		while (slashes < length && unfolded[length - slashes - 1] == '\\')
			slashes++;

		if (!(slashes & 1))
			break;

		length--;

		while (length && is_lwsp (unfolded[length - 1]))
			length--;

		unfolded[length] = '\0';
	} while (TRUE);

	return unfolded;
}

static gboolean
skip_keyword (const char **in, const char *end)
{
	const char *start = *in;

	while (*in < end && (g_ascii_isalnum (**in) || **in == '-' || **in == '_'))
		(*in)++;

	return *in != start;
}

static gboolean
skip_value (const char **in, const char *end, gboolean *quoted)
{
	const char *start = *in;

	*quoted = *in < end && **in == '"';

	if (*quoted)
		return skip_quoted (in, end);

	while (*in < end && (guchar) **in < 128 && is_ttoken (**in))
		(*in)++;

	return *in != start;
}

static gboolean
parse_methods (GMimeAuthenticationResults *results, const char *text, const char *p,
	       const char *end, GError **error)
{
	const char *start, *method_end, *ptype, *ptype_end, *name, *name_end;
	GMimeAuthenticationMethodProperty *property;
	GMimeAuthenticationMethodResult *result;
	char *service = NULL, *method, *value;
	gboolean quoted;

	while (p < end) {
	method_token:
		if (!_g_mime_parse_cfws (text, &p, end, error))
			goto fail;

		if (p == end)
			break;

		start = p;
		if (!skip_keyword (&p, end))
			goto invalid;

		if (!service && p < end && *p == '.') {
			do {
				p++;

				if (!skip_keyword (&p, end))
					goto invalid;
			} while (p < end && *p == '.');

			method_end = p;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (p == end)
				goto invalid;

			if (*p == ';') {
				service = g_strndup (start, method_end - start);
				p++;
				goto method_token;
			}

			if (*p != '=' && *p != '/')
				goto invalid;
		} else {
			method_end = p;
		}

		if (!_g_mime_parse_cfws (text, &p, end, error))
			goto fail;

		if (p == end) {
			if (method_end - start != 4 || memcmp (start, "none", 4) || results->results->len)
				goto invalid;
			break;
		}

		method = g_strndup (start, method_end - start);
		result = g_mime_authentication_method_result_new (method, "");
		g_free (method);

		result->authserv_id = service;
		service = NULL;
		g_ptr_array_add (results->results, result);

		if (*p == '/') {
			p++;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (!_g_mime_parse_int32 (&p, end, &result->version))
				goto invalid;

			result->has_version = TRUE;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;
		}

		if (p == end || *p++ != '=')
			goto invalid;

		if (!_g_mime_parse_cfws (text, &p, end, error))
			goto fail;

		start = p;

		if (!skip_keyword (&p, end))
			goto invalid;

		g_free (result->result);
		result->result = g_strndup (start, p - start);
		skip_lwsp (&p, end);

		if (p < end && *p == '(') {
			start = p;

			if (!skip_comment (&p, end))
				goto invalid;

			result->comment = parse_comment (start + 1, p - 1);

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;
		}

		if (p == end)
			break;

		if (*p == ';') {
			p++;
			continue;
		}

		ptype = p;

		if (!skip_keyword (&p, end))
			goto invalid;

		ptype_end = p;

		if (p - ptype == 6 && (!memcmp (ptype, "reason", 6) || !memcmp (ptype, "action", 6))) {
			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (p == end || *p++ != '=')
				goto invalid;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			start = p;

			if (!skip_value (&p, end, &quoted))
				goto invalid;

			value = parse_string (start, p, quoted);

			if (*ptype == 'r')
				result->reason = value;
			else
				result->action = value;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (p == end)
				break;

			if (*p == ';') {
				p++;
				continue;
			}

			ptype = p;

			if (!skip_keyword (&p, end))
				goto invalid;

			ptype_end = p;
		}

		do {
			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (p == end || *p++ != '.')
				goto invalid;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			name = p;

			if (!skip_keyword (&p, end))
				goto invalid;

			name_end = p;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (p == end || *p++ != '=')
				goto invalid;

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			start = p;
			quoted = p < end && *p == '"';

			if (quoted) {
				if (!skip_quoted (&p, end))
					goto invalid;
			} else {
				/* Real-world values contain tspecials and may be empty. */
				while (p < end && !is_lwsp (*p) && *p != ';' && *p != '(')
					p++;
			}

			property = g_object_new (GMIME_TYPE_AUTHENTICATION_METHOD_PROPERTY, NULL);
			property->ptype = g_strndup (ptype, ptype_end - ptype);
			property->property = g_strndup (name, name_end - name);
			property->value = parse_string (start, p, quoted);
			property->quoted = quoted;
			g_ptr_array_add (result->properties, property);

			if (!_g_mime_parse_cfws (text, &p, end, error))
				goto fail;

			if (p == end || *p == ';')
				break;

			ptype = p;

			if (!skip_keyword (&p, end))
				goto invalid;

			ptype_end = p;
		} while (TRUE);

		if (p < end)
			p++;
	}

	g_free (service);

	return TRUE;

 invalid:
	_g_mime_parse_error (error, text, p, "Invalid authentication method specification");

 fail:
	g_free (service);

	return FALSE;
}

/**
 * g_mime_authentication_results_parse:
 * @text: the header value, without the field name
 * @length: the byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a parse error
 *
 * Parses a bounded value, including ARC instance numbers and common
 * Office365 deviations. Missing methods become "none". Numbers must be
 * nonnegative signed 32-bit integers. Comments and quoted strings are
 * unfolded; property quoting is retained. Invalid UTF-8 in text values is
 * replaced with U+FFFD, while embedded NUL bytes are rejected.
 *
 * Returns: (transfer full) (nullable): the parsed model, or %NULL with
 * %GMIME_ERROR_PARSE_ERROR and a relative byte offset on malformed input.
 **/
GMimeAuthenticationResults *
g_mime_authentication_results_parse (const char *text, gssize length, GError **error)
{
	GMimeAuthenticationResults *results;
	const char *p, *end, *start;
	gboolean quoted;
	char *value;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!_g_mime_parse_validate_text (text, &length, error))
		return NULL;

	results = g_object_new (GMIME_TYPE_AUTHENTICATION_RESULTS, NULL);
	p = text;
	end = text + length;

	if (!_g_mime_parse_cfws (text, &p, end, error))
		goto fail;

	do {
		start = p;

		if (!skip_value (&p, end, &quoted))
			goto invalid;

		value = parse_string (start, p, quoted);

		if (quoted) {
			results->authserv_id = value;
		} else {
			if (!_g_mime_parse_cfws (text, &p, end, error)) {
				g_free (value);
				goto fail;
			}

			if (p < end && *p == '=') {
				gboolean instance = !strcmp (value, "i");

				g_free (value);

				if (results->has_instance)
					goto invalid;

				if (!instance) {
					if (!parse_methods (results, text, text, end, error))
						goto fail;
					return results;
				}

				p++;

				if (!_g_mime_parse_cfws (text, &p, end, error))
					goto fail;

				if (!_g_mime_parse_int32 (&p, end, &results->instance))
					goto invalid;

				results->has_instance = TRUE;

				if (!_g_mime_parse_cfws (text, &p, end, error))
					goto fail;

				if (p == end || *p++ != ';')
					goto invalid;

				if (!_g_mime_parse_cfws (text, &p, end, error))
					goto fail;
			} else {
				results->authserv_id = value;
			}
		}
	} while (!results->authserv_id);

	if (!_g_mime_parse_cfws (text, &p, end, error))
		goto fail;

	if (p == end)
		return results;

	if (*p != ';') {
		if (!_g_mime_parse_int32 (&p, end, &results->version))
			goto invalid;

		results->has_version = TRUE;

		if (!_g_mime_parse_cfws (text, &p, end, error))
			goto fail;

		if (p == end)
			return results;

		if (*p != ';')
			goto invalid;
	}

	if (!parse_methods (results, text, p + 1, end, error))
		goto fail;

	return results;

 invalid:
	_g_mime_parse_error (error, text, p, "Invalid Authentication-Results value");

 fail:
	g_object_unref (results);

	return NULL;
}

static void
append_token (GString *str, GMimeFormatOptions *options, gsize *line, gboolean *space, const char *token)
{
	gsize length = strlen (token);

	if (*line + *space + length > (gsize) g_mime_format_options_get_max_line_length (options)) {
		g_string_append (str, g_mime_format_options_get_newline (options));
		g_string_append_c (str, '\t');
		*line = 1;
	} else if (*space) {
		g_string_append_c (str, ' ');
		(*line)++;
	}

	*space = FALSE;
	g_string_append (str, token);
	*line += length;
}

static void
encode_method (GMimeAuthenticationMethodResult *result, GMimeFormatOptions *options, GString *str, gsize *line)
{
	gsize max = g_mime_format_options_get_max_line_length (options);
	char *complete = g_mime_authentication_method_result_to_string (result);
	GMimeAuthenticationMethodProperty *property;
	char *method, *token, *value, version[32];
	gboolean space = TRUE;
	guint i;

	if (strlen (complete) + 1 < max) {
		append_token (str, options, line, &space, complete);
		g_free (complete);
		return;
	}

	g_free (complete);

	if (result->authserv_id) {
		append_token (str, options, line, &space, result->authserv_id);
		append_token (str, options, line, &space, ";");
		space = TRUE;
	}

	if (result->has_version)
		method = g_strdup_printf ("%s/%d", result->method, result->version);
	else
		method = g_strdup (result->method);

	if (strlen (method) + 1 + strlen (result->result) < max) {
		token = g_strdup_printf ("%s=%s", method, result->result);
		append_token (str, options, line, &space, token);
		g_free (token);
	} else {
		if (result->has_version && strlen (method) >= max) {
			append_token (str, options, line, &space, result->method);
			append_token (str, options, line, &space, "/");
			g_snprintf (version, sizeof (version), "%d", result->version);
			append_token (str, options, line, &space, version);
		} else {
			append_token (str, options, line, &space, method);
		}

		append_token (str, options, line, &space, "=");
		append_token (str, options, line, &space, result->result);
	}

	g_free (method);

	if (result->comment && *result->comment) {
		token = g_strdup_printf ("(%s)", result->comment);
		space = TRUE;
		append_token (str, options, line, &space, token);
		g_free (token);
	}

	if ((result->reason && *result->reason) || (result->action && *result->action)) {
		const char *prefix = result->reason && *result->reason ? "reason=" : "action=";

		value = quote_value (*prefix == 'r' ? result->reason : result->action);
		space = TRUE;

		if (strlen (prefix) + strlen (value) < max) {
			token = g_strconcat (prefix, value, NULL);
			append_token (str, options, line, &space, token);
			g_free (token);
		} else {
			append_token (str, options, line, &space, prefix);
			append_token (str, options, line, &space, value);
		}

		g_free (value);
	}

	for (i = 0; i < result->properties->len; i++) {
		property = result->properties->pdata[i];
		value = format_property_value (property);
		token = g_strdup_printf ("%s.%s=", property->ptype, property->property);
		space = TRUE;

		if (strlen (token) + strlen (value) < max) {
			complete = g_strconcat (token, value, NULL);
			append_token (str, options, line, &space, complete);
			g_free (complete);
		} else {
			if (strlen (token) < max) {
				append_token (str, options, line, &space, token);
			} else {
				append_token (str, options, line, &space, property->ptype);
				append_token (str, options, line, &space, ".");
				append_token (str, options, line, &space, property->property);
				append_token (str, options, line, &space, "=");
			}

			append_token (str, options, line, &space, value);
		}

		g_free (token);
		g_free (value);
	}
}

char *
_g_mime_authentication_results_encode (GMimeAuthenticationResults *results, GMimeFormatOptions *options, gsize field_length)
{
	GString *str = g_string_new (NULL);
	gsize line = field_length;
	gboolean space = TRUE;
	char *value, version[32];
	guint i;

	if (results->has_instance) {
		g_string_append_printf (str, " i=%d;", results->instance);
		line += str->len;
	}

	if (results->authserv_id) {
		value = format_service (results->authserv_id);
		append_token (str, options, &line, &space, value);
		g_free (value);

		if (results->has_version) {
			g_snprintf (version, sizeof (version), "%d", results->version);
			space = TRUE;
			append_token (str, options, &line, &space, version);
		}

		g_string_append_c (str, ';');
		line++;
	}

	for (i = 0; i < results->results->len; i++) {
		if (i) {
			g_string_append_c (str, ';');
			line++;
		}

		encode_method (results->results->pdata[i], options, str, &line);
	}

	if (!results->results->len)
		g_string_append (str, " none");

	g_string_append (str, g_mime_format_options_get_newline (options));

	return g_string_free (str, FALSE);
}

/**
 * g_mime_authentication_results_get_authentication_service_identifier:
 * @results: authentication results
 *
 * Gets the identifier of the service reporting these results.
 *
 * Returns: (transfer none) (nullable): the service identifier, or %NULL for
 * Office365 values that omit it.
 **/
const char *
g_mime_authentication_results_get_authentication_service_identifier (GMimeAuthenticationResults *results)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), NULL);

	return results->authserv_id;
}

/**
 * g_mime_authentication_results_get_instance:
 * @results: authentication results
 * @instance: (out) (optional): the instance, or zero when absent
 *
 * Gets the optional ARC instance without conflating zero with absence.
 *
 * Returns: whether an ARC instance is present.
 **/
gboolean
g_mime_authentication_results_get_instance (GMimeAuthenticationResults *results, int *instance)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), FALSE);

	if (instance)
		*instance = results->has_instance ? results->instance : 0;

	return results->has_instance;
}

/**
 * g_mime_authentication_results_set_instance:
 * @results: authentication results
 * @instance: the ARC instance
 *
 * Sets the instance without range validation, as in MimeKit.
 **/
void
g_mime_authentication_results_set_instance (GMimeAuthenticationResults *results, int instance)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));

	results->instance = instance;
	results->has_instance = TRUE;
}

/**
 * g_mime_authentication_results_clear_instance:
 * @results: authentication results
 *
 * Removes the optional ARC instance.
 **/
void
g_mime_authentication_results_clear_instance (GMimeAuthenticationResults *results)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));

	results->has_instance = FALSE;
}

/**
 * g_mime_authentication_results_get_version:
 * @results: authentication results
 * @version: (out) (optional): the version, or zero when absent
 *
 * Gets the optional header version without conflating zero with absence.
 *
 * Returns: whether an Authentication-Results version is present.
 **/
gboolean
g_mime_authentication_results_get_version (GMimeAuthenticationResults *results, int *version)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), FALSE);

	if (version)
		*version = results->has_version ? results->version : 0;

	return results->has_version;
}

/**
 * g_mime_authentication_results_set_version:
 * @results: authentication results
 * @version: the version
 *
 * Sets the version without range validation, as in MimeKit.
 **/
void
g_mime_authentication_results_set_version (GMimeAuthenticationResults *results, int version)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));

	results->version = version;
	results->has_version = TRUE;
}

/**
 * g_mime_authentication_results_clear_version:
 * @results: authentication results
 *
 * Removes the optional version.
 **/
void
g_mime_authentication_results_clear_version (GMimeAuthenticationResults *results)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));

	results->has_version = FALSE;
}

/**
 * g_mime_authentication_results_get_result_count:
 * @results: authentication results
 *
 * Gets the size of the ordered method-result collection.
 *
 * Returns: the number of method results.
 **/
int
g_mime_authentication_results_get_result_count (GMimeAuthenticationResults *results)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), 0);

	return results->results->len;
}

/**
 * g_mime_authentication_results_get_result:
 * @results: authentication results
 * @index: a zero-based result index
 *
 * Gets an existing entry without adding a reference.
 *
 * Returns: (transfer none): the result owned by @results.
 **/
GMimeAuthenticationMethodResult *
g_mime_authentication_results_get_result (GMimeAuthenticationResults *results, int index)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < results->results->len, NULL);

	return results->results->pdata[index];
}

/**
 * g_mime_authentication_results_set_result:
 * @results: authentication results
 * @index: an existing result index
 * @result: (transfer none): the replacement result
 *
 * Retains the replacement before releasing the old entry. Duplicates are allowed.
 **/
void
g_mime_authentication_results_set_result (GMimeAuthenticationResults *results, int index, GMimeAuthenticationMethodResult *result)
{
	GMimeAuthenticationMethodResult *previous;

	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));
	g_return_if_fail (index >= 0 && (guint) index < results->results->len);
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));

	previous = results->results->pdata[index];
	results->results->pdata[index] = g_object_ref (result);
	g_object_unref (previous);
}

/**
 * g_mime_authentication_results_add_result:
 * @results: authentication results
 * @result: (transfer none): the result to append
 *
 * Appends a retained reference, allowing duplicates.
 **/
void
g_mime_authentication_results_add_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (results->results->len < G_MAXINT);

	g_ptr_array_add (results->results, g_object_ref (result));
}

/**
 * g_mime_authentication_results_insert_result:
 * @results: authentication results
 * @index: an index from zero through the result count
 * @result: (transfer none): the result to insert
 *
 * Inserts a retained reference at @index.
 **/
void
g_mime_authentication_results_insert_result (GMimeAuthenticationResults *results, int index, GMimeAuthenticationMethodResult *result)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));
	g_return_if_fail (index >= 0 && (guint) index <= results->results->len);
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (results->results->len < G_MAXINT);

	g_ptr_array_insert (results->results, index, g_object_ref (result));
}

/**
 * g_mime_authentication_results_index_of_result:
 * @results: authentication results
 * @result: the result to find
 *
 * Searches by object identity, not by serialized value.
 *
 * Returns: the first matching instance index, or -1 if absent.
 **/
int
g_mime_authentication_results_index_of_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result)
{
	guint index;

	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results), -1);
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), -1);

	return g_ptr_array_find (results->results, result, &index) ? (int) index : -1;
}

/**
 * g_mime_authentication_results_contains_result:
 * @results: authentication results
 * @result: the result to find
 *
 * Tests collection membership by object identity.
 *
 * Returns: whether the exact result instance belongs to the collection.
 **/
gboolean
g_mime_authentication_results_contains_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result)
{
	return g_mime_authentication_results_index_of_result (results, result) != -1;
}

/**
 * g_mime_authentication_results_remove_result:
 * @results: authentication results
 * @result: (transfer none): the result to remove
 *
 * Removes only the first occurrence, preserving the remaining order.
 *
 * Returns: whether a result was removed.
 **/
gboolean
g_mime_authentication_results_remove_result (GMimeAuthenticationResults *results, GMimeAuthenticationMethodResult *result)
{
	int index = g_mime_authentication_results_index_of_result (results, result);

	if (index == -1)
		return FALSE;

	g_ptr_array_remove_index (results->results, index);

	return TRUE;
}

/**
 * g_mime_authentication_results_remove_result_at:
 * @results: authentication results
 * @index: an existing result index
 *
 * Releases and removes the entry at @index.
 **/
void
g_mime_authentication_results_remove_result_at (GMimeAuthenticationResults *results, int index)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));
	g_return_if_fail (index >= 0 && (guint) index < results->results->len);

	g_ptr_array_remove_index (results->results, index);
}

/**
 * g_mime_authentication_results_clear_results:
 * @results: authentication results
 *
 * Releases all results without changing the service, version or instance.
 **/
void
g_mime_authentication_results_clear_results (GMimeAuthenticationResults *results)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_RESULTS (results));

	g_ptr_array_set_size (results->results, 0);
}

/**
 * g_mime_authentication_method_result_get_method:
 * @result: a method result
 *
 * Gets the authentication method, such as "dkim" or "spf".
 *
 * Returns: (transfer none): the method name, preserving case.
 **/
const char *
g_mime_authentication_method_result_get_method (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	return result->method;
}

/**
 * g_mime_authentication_method_result_get_result:
 * @result: a method result
 *
 * Gets the reported outcome, such as "pass" or "fail".
 *
 * Returns: (transfer none): the reported result, preserving case.
 **/
const char *
g_mime_authentication_method_result_get_result (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	return result->result;
}

/**
 * g_mime_authentication_method_result_get_office365_authentication_service_identifier:
 * @result: a method result
 *
 * Gets the nonstandard per-method service identifier, when present.
 *
 * Returns: (transfer none) (nullable): the Office365 method-specific service identifier.
 **/
const char *
g_mime_authentication_method_result_get_office365_authentication_service_identifier (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	return result->authserv_id;
}

/**
 * g_mime_authentication_method_result_get_version:
 * @result: a method result
 * @version: (out) (optional): the version, or zero when absent
 *
 * Gets the optional method version without conflating zero with absence.
 *
 * Returns: whether a method version is present.
 **/
gboolean
g_mime_authentication_method_result_get_version (GMimeAuthenticationMethodResult *result, int *version)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), FALSE);

	if (version)
		*version = result->has_version ? result->version : 0;

	return result->has_version;
}

/**
 * g_mime_authentication_method_result_set_version:
 * @result: a method result
 * @version: the version
 *
 * Sets the version without range validation, as in MimeKit.
 **/
void
g_mime_authentication_method_result_set_version (GMimeAuthenticationMethodResult *result, int version)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));

	result->version = version;
	result->has_version = TRUE;
}

/**
 * g_mime_authentication_method_result_clear_version:
 * @result: a method result
 *
 * Removes the optional method version.
 **/
void
g_mime_authentication_method_result_clear_version (GMimeAuthenticationMethodResult *result)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));

	result->has_version = FALSE;
}

/**
 * g_mime_authentication_method_result_get_result_comment:
 * @result: a method result
 *
 * Gets the first comment following the result, retaining inner escapes and nesting.
 *
 * Returns: (transfer none) (nullable): the inner comment syntax, without outer parentheses.
 **/
const char *
g_mime_authentication_method_result_get_result_comment (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	return result->comment;
}

/**
 * g_mime_authentication_method_result_set_result_comment:
 * @result: a method result
 * @comment: (nullable): inner comment syntax, or %NULL to clear
 *
 * Copies the comment without escaping, unfolding or syntax validation.
 **/
void
g_mime_authentication_method_result_set_result_comment (GMimeAuthenticationMethodResult *result, const char *comment)
{
	char *copy;

	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (comment == NULL || g_utf8_validate (comment, -1, NULL));

	copy = g_strdup (comment);
	g_free (result->comment);
	result->comment = copy;
}

/**
 * g_mime_authentication_method_result_get_reason:
 * @result: a method result
 *
 * Gets the explanatory reason associated with the result.
 *
 * Returns: (transfer none) (nullable): the reason text.
 **/
const char *
g_mime_authentication_method_result_get_reason (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	return result->reason;
}

/**
 * g_mime_authentication_method_result_set_reason:
 * @result: a method result
 * @reason: (nullable): the reason, or %NULL to clear
 *
 * Copies the reason. A nonempty reason takes precedence over an action
 * when serializing; an empty or absent reason allows the parsed action.
 **/
void
g_mime_authentication_method_result_set_reason (GMimeAuthenticationMethodResult *result, const char *reason)
{
	char *copy;

	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (reason == NULL || g_utf8_validate (reason, -1, NULL));

	copy = g_strdup (reason);
	g_free (result->reason);
	result->reason = copy;
}

/**
 * g_mime_authentication_method_result_get_action:
 * @result: a method result
 *
 * Gets the nonstandard action reported by the authentication service.
 *
 * Returns: (transfer none) (nullable): the parsed action text.
 **/
const char *
g_mime_authentication_method_result_get_action (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);

	return result->action;
}

/**
 * g_mime_authentication_method_result_get_property_count:
 * @result: a method result
 *
 * Gets the size of the ordered property collection.
 *
 * Returns: the number of properties.
 **/
int
g_mime_authentication_method_result_get_property_count (GMimeAuthenticationMethodResult *result)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), 0);

	return result->properties->len;
}

/**
 * g_mime_authentication_method_result_get_property:
 * @result: a method result
 * @index: a zero-based property index
 *
 * Gets an existing entry without adding a reference.
 *
 * Returns: (transfer none): the property owned by @result.
 **/
GMimeAuthenticationMethodProperty *
g_mime_authentication_method_result_get_property (GMimeAuthenticationMethodResult *result, int index)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < result->properties->len, NULL);

	return result->properties->pdata[index];
}

/**
 * g_mime_authentication_method_result_set_property:
 * @result: a method result
 * @index: an existing property index
 * @property: (transfer none): the replacement property
 *
 * Retains the replacement before releasing the old entry. Duplicates are allowed.
 **/
void
g_mime_authentication_method_result_set_property (GMimeAuthenticationMethodResult *result, int index, GMimeAuthenticationMethodProperty *property)
{
	GMimeAuthenticationMethodProperty *previous;

	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (index >= 0 && (guint) index < result->properties->len);
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property));

	previous = result->properties->pdata[index];
	result->properties->pdata[index] = g_object_ref (property);
	g_object_unref (previous);
}

/**
 * g_mime_authentication_method_result_add_property:
 * @result: a method result
 * @property: (transfer none): the property to append
 *
 * Appends a retained reference, allowing duplicates.
 **/
void
g_mime_authentication_method_result_add_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property));
	g_return_if_fail (result->properties->len < G_MAXINT);

	g_ptr_array_add (result->properties, g_object_ref (property));
}

/**
 * g_mime_authentication_method_result_insert_property:
 * @result: a method result
 * @index: an index from zero through the property count
 * @property: (transfer none): the property to insert
 *
 * Inserts a retained reference at @index.
 **/
void
g_mime_authentication_method_result_insert_property (GMimeAuthenticationMethodResult *result, int index, GMimeAuthenticationMethodProperty *property)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (index >= 0 && (guint) index <= result->properties->len);
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property));
	g_return_if_fail (result->properties->len < G_MAXINT);

	g_ptr_array_insert (result->properties, index, g_object_ref (property));
}

/**
 * g_mime_authentication_method_result_index_of_property:
 * @result: a method result
 * @property: the property to find
 *
 * Searches by object identity, not by serialized value.
 *
 * Returns: the first matching instance index, or -1 if absent.
 **/
int
g_mime_authentication_method_result_index_of_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property)
{
	guint index;

	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result), -1);
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property), -1);
	return g_ptr_array_find (result->properties, property, &index) ? (int) index : -1;
}

/**
 * g_mime_authentication_method_result_contains_property:
 * @result: a method result
 * @property: the property to find
 *
 * Tests collection membership by object identity.
 *
 * Returns: whether the exact property instance belongs to the collection.
 **/
gboolean
g_mime_authentication_method_result_contains_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property)
{
	return g_mime_authentication_method_result_index_of_property (result, property) != -1;
}

/**
 * g_mime_authentication_method_result_remove_property:
 * @result: a method result
 * @property: (transfer none): the property to remove
 *
 * Removes only the first occurrence, preserving the remaining order.
 *
 * Returns: whether a property was removed.
 **/
gboolean
g_mime_authentication_method_result_remove_property (GMimeAuthenticationMethodResult *result, GMimeAuthenticationMethodProperty *property)
{
	int index = g_mime_authentication_method_result_index_of_property (result, property);

	if (index == -1)
		return FALSE;

	g_ptr_array_remove_index (result->properties, index);

	return TRUE;
}

/**
 * g_mime_authentication_method_result_remove_property_at:
 * @result: a method result
 * @index: an existing property index
 *
 * Releases and removes the entry at @index.
 **/
void
g_mime_authentication_method_result_remove_property_at (GMimeAuthenticationMethodResult *result, int index)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));
	g_return_if_fail (index >= 0 && (guint) index < result->properties->len);

	g_ptr_array_remove_index (result->properties, index);
}

/**
 * g_mime_authentication_method_result_clear_properties:
 * @result: a method result
 *
 * Releases every property without changing other result fields.
 **/
void
g_mime_authentication_method_result_clear_properties (GMimeAuthenticationMethodResult *result)
{
	g_return_if_fail (GMIME_IS_AUTHENTICATION_METHOD_RESULT (result));

	g_ptr_array_set_size (result->properties, 0);
}

/**
 * g_mime_authentication_method_property_get_property_type:
 * @property: an authentication property
 *
 * Gets the property namespace, such as "header" or "smtp".
 *
 * Returns: (transfer none): the property type.
 **/
const char *
g_mime_authentication_method_property_get_property_type (GMimeAuthenticationMethodProperty *property)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property), NULL);

	return property->ptype;
}

/**
 * g_mime_authentication_method_property_get_property:
 * @property: an authentication property
 *
 * Gets the name within the property namespace.
 *
 * Returns: (transfer none): the property name.
 **/
const char *
g_mime_authentication_method_property_get_property (GMimeAuthenticationMethodProperty *property)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property), NULL);

	return property->property;
}

/**
 * g_mime_authentication_method_property_get_value:
 * @property: an authentication property
 *
 * Gets the property value without its surrounding quoting syntax.
 *
 * Returns: (transfer none): the property value.
 **/
const char *
g_mime_authentication_method_property_get_value (GMimeAuthenticationMethodProperty *property)
{
	g_return_val_if_fail (GMIME_IS_AUTHENTICATION_METHOD_PROPERTY (property), NULL);

	return property->value;
}
