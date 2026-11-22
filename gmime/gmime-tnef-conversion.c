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

/**
 * SECTION: gmime-tnef-conversion
 * @title: TNEF to MIME conversion
 * @section_id: gmime-gmime-tnef-conversion
 * @short_description: Convert a TNEF message and inspect conversion losses
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-tnef-conversion.h"
#include "gmime-tnef-private.h"
#include "gmime-tnef-calendar.h"
#include "gmime-tnef-part.h"
#include "gmime-text-part.h"
#include "gmime-message-part.h"
#include "gmime-multipart.h"
#include "gmime-multipart-related.h"
#include "gmime-multipart-alternative.h"
#include "gmime-filter-charset.h"
#include "gmime-internal.h"
#include "gmime-parser.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-rtf-compressed.h"
#include "gmime-html-tokenizer.h"
#include "gmime-rtf-to-html.h"
#include "gmime-rtf-to-text.h"
#include "gmime-rtf-interpreter-private.h"
#include "gmime-utils.h"
#include "gmime-parse-utils.h"

typedef struct {
	gint refs;
	gpointer data;
	GDestroyNotify destroy;
} Closure;

struct _GMimeTnefConversionOptions {
	gint refs;
	gboolean embedded, calendar;
	guint exceptions;
	guint64 exceptions_size;
	GMimeTnefOleObjectConverter ole;
	GMimeTnefAttachmentPlaceholderCallback placeholder;
	Closure *ole_data, *placeholder_data;
};

struct _GMimeTnefConversionResult {
	gint refs;
	GMimeMessage *message;
	GPtrArray *losses;
};

G_DEFINE_BOXED_TYPE (GMimeTnefConversionOptions, g_mime_tnef_conversion_options,
		     g_mime_tnef_conversion_options_ref, g_mime_tnef_conversion_options_unref)
G_DEFINE_BOXED_TYPE (GMimeTnefConversionResult, g_mime_tnef_conversion_result,
		     g_mime_tnef_conversion_result_ref, g_mime_tnef_conversion_result_unref)

static Closure *
closure_new (gpointer data, GDestroyNotify destroy)
{
	Closure *closure = g_new (Closure, 1);
	closure->refs = 1;
	closure->data = data;
	closure->destroy = destroy;
	return closure;
}

static void
closure_unref (Closure *closure)
{
	if (closure && g_atomic_int_dec_and_test (&closure->refs)) {
		if (closure->destroy)
			closure->destroy (closure->data);
		g_free (closure);
	}
}

/**
 * g_mime_tnef_conversion_options_new:
 *
 * Returns: (transfer full): new options with calendar generation enabled,
 * limited to 1024 exceptions and 16 MiB of exception data.
 **/
GMimeTnefConversionOptions *
g_mime_tnef_conversion_options_new (void)
{
	GMimeTnefConversionOptions *options = g_new0 (GMimeTnefConversionOptions, 1);

	options->refs = 1;
	options->calendar = TRUE;
	options->exceptions = 1024;
	options->exceptions_size = 16 * 1024 * 1024;

	return options;
}

/**
 * g_mime_tnef_conversion_options_ref:
 * @options: conversion settings
 *
 * Returns: (transfer full): @options with another reference.
 **/
GMimeTnefConversionOptions *
g_mime_tnef_conversion_options_ref (GMimeTnefConversionOptions *options)
{
	g_return_val_if_fail (options != NULL, NULL);

	g_atomic_int_inc (&options->refs);

	return options;
}

/**
 * g_mime_tnef_conversion_options_unref:
 * @options: conversion settings
 *
 * Releases one reference.
 **/
void
g_mime_tnef_conversion_options_unref (GMimeTnefConversionOptions *options)
{
	g_return_if_fail (options != NULL);

	if (g_atomic_int_dec_and_test (&options->refs)) {
		closure_unref (options->ole_data);
		closure_unref (options->placeholder_data);
		g_free (options);
	}
}

/**
 * g_mime_tnef_conversion_options_clone:
 * @options: conversion settings
 *
 * Returns: (transfer full): an independent copy. Callback data is shared
 * until the last copy releases the callback.
 **/
GMimeTnefConversionOptions *
g_mime_tnef_conversion_options_clone (const GMimeTnefConversionOptions *options)
{
	GMimeTnefConversionOptions *copy;

	g_return_val_if_fail (options != NULL, NULL);

	copy = g_new (GMimeTnefConversionOptions, 1);
	*copy = *options;
	copy->refs = 1;

	if (copy->ole_data)
		g_atomic_int_inc (&copy->ole_data->refs);

	if (copy->placeholder_data)
		g_atomic_int_inc (&copy->placeholder_data->refs);

	return copy;
}

/**
 * g_mime_tnef_conversion_options_get_convert_embedded_messages:
 * @options: conversion settings
 *
 * Returns: whether nested TNEF is converted to message/rfc822.
 **/
gboolean
g_mime_tnef_conversion_options_get_convert_embedded_messages (const GMimeTnefConversionOptions *options)
{
	g_return_val_if_fail (options != NULL, FALSE);

	return options->embedded;
}

/**
 * g_mime_tnef_conversion_options_set_convert_embedded_messages:
 * @options: conversion settings
 * @enable: whether to convert nested messages rather than preserve TNEF
 **/
void
g_mime_tnef_conversion_options_set_convert_embedded_messages (GMimeTnefConversionOptions *options, gboolean enable)
{
	g_return_if_fail (options != NULL);

	options->embedded = !!enable;
}

/**
 * g_mime_tnef_conversion_options_get_generate_calendar:
 * @options: conversion settings
 *
 * Returns: whether appointments get a text/calendar alternative.
 **/
gboolean
g_mime_tnef_conversion_options_get_generate_calendar (const GMimeTnefConversionOptions *options)
{
	g_return_val_if_fail (options != NULL, FALSE);

	return options->calendar;
}

/**
 * g_mime_tnef_conversion_options_set_generate_calendar:
 * @options: conversion settings
 * @enable: whether to generate calendar content
 **/
void
g_mime_tnef_conversion_options_set_generate_calendar (GMimeTnefConversionOptions *options, gboolean enable)
{
	g_return_if_fail (options != NULL);

	options->calendar = !!enable;
}

/**
 * g_mime_tnef_conversion_options_get_max_calendar_exceptions:
 * @options: conversion settings
 *
 * Returns: maximum modified occurrences to export.
 **/
guint
g_mime_tnef_conversion_options_get_max_calendar_exceptions (const GMimeTnefConversionOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->exceptions;
}

/**
 * g_mime_tnef_conversion_options_set_max_calendar_exceptions:
 * @options: conversion settings
 * @maximum: maximum modified occurrences to export
 **/
void
g_mime_tnef_conversion_options_set_max_calendar_exceptions (GMimeTnefConversionOptions *options, guint maximum)
{
	g_return_if_fail (options != NULL);

	options->exceptions = maximum;
}

/**
 * g_mime_tnef_conversion_options_get_max_calendar_exceptions_size:
 * @options: conversion settings
 *
 * Returns: maximum combined size of calendar exceptions, in bytes.
 **/
guint64
g_mime_tnef_conversion_options_get_max_calendar_exceptions_size (const GMimeTnefConversionOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->exceptions_size;
}

/**
 * g_mime_tnef_conversion_options_set_max_calendar_exceptions_size:
 * @options: conversion settings
 * @maximum: maximum combined size of exceptions, in bytes
 **/
void
g_mime_tnef_conversion_options_set_max_calendar_exceptions_size (GMimeTnefConversionOptions *options, guint64 maximum)
{
	g_return_if_fail (options != NULL);

	options->exceptions_size = maximum;
}

/**
 * g_mime_tnef_conversion_options_set_ole_object_converter:
 * @options: conversion settings
 * @callback: (scope notified) (nullable): renderer, or %NULL to disable
 * @user_data: (nullable): renderer data
 * @destroy: (nullable): releases the data
 **/
void
g_mime_tnef_conversion_options_set_ole_object_converter (GMimeTnefConversionOptions *options,
	GMimeTnefOleObjectConverter callback, gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (options != NULL);

	closure_unref (options->ole_data);

	options->ole = callback;
	options->ole_data = closure_new (user_data, destroy);
}

/**
 * g_mime_tnef_conversion_options_get_ole_object_converter: (skip)
 * @options: conversion settings
 * @user_data: (out) (optional) (transfer none): renderer data
 *
 * Returns: (nullable): registered OLE renderer.
 **/
GMimeTnefOleObjectConverter
g_mime_tnef_conversion_options_get_ole_object_converter (const GMimeTnefConversionOptions *options, gpointer *user_data)
{
	g_return_val_if_fail (options != NULL, NULL);

	if (user_data)
		*user_data = options->ole_data ? options->ole_data->data : NULL;

	return options->ole;
}

/**
 * g_mime_tnef_conversion_options_set_attachment_placeholder_callback:
 * @options: conversion settings
 * @callback: (scope notified) (nullable): placeholder callback, or %NULL
 * @user_data: (nullable): callback data
 * @destroy: (nullable): releases the data
 **/
void
g_mime_tnef_conversion_options_set_attachment_placeholder_callback (GMimeTnefConversionOptions *options,
	GMimeTnefAttachmentPlaceholderCallback callback, gpointer user_data, GDestroyNotify destroy)
{
	g_return_if_fail (options != NULL);

	closure_unref (options->placeholder_data);

	options->placeholder = callback;
	options->placeholder_data = closure_new (user_data, destroy);
}

/**
 * g_mime_tnef_conversion_options_get_attachment_placeholder_callback: (skip)
 * @options: conversion settings
 * @user_data: (out) (optional) (transfer none): callback data
 *
 * Returns: (nullable): registered placeholder callback.
 **/
GMimeTnefAttachmentPlaceholderCallback
g_mime_tnef_conversion_options_get_attachment_placeholder_callback (const GMimeTnefConversionOptions *options, gpointer *user_data)
{
	g_return_val_if_fail (options != NULL, NULL);

	if (user_data)
		*user_data = options->placeholder_data ? options->placeholder_data->data : NULL;

	return options->placeholder;
}

/**
 * g_mime_tnef_conversion_result_ref:
 * @result: conversion result
 *
 * Returns: (transfer full): @result with another reference.
 **/
GMimeTnefConversionResult *
g_mime_tnef_conversion_result_ref (GMimeTnefConversionResult *result)
{
	g_return_val_if_fail (result != NULL, NULL);

	g_atomic_int_inc (&result->refs);

	return result;
}

/**
 * g_mime_tnef_conversion_result_unref:
 * @result: conversion result
 *
 * Releases one reference.
 **/
void
g_mime_tnef_conversion_result_unref (GMimeTnefConversionResult *result)
{
	g_return_if_fail (result != NULL);

	if (g_atomic_int_dec_and_test (&result->refs)) {
		g_clear_object (&result->message);
		g_ptr_array_unref (result->losses);
		g_free (result);
	}
}

/**
 * g_mime_tnef_conversion_result_get_message:
 * @result: conversion result
 *
 * Returns: (transfer none): the converted message. Take a reference to
 * retain the message and all its content beyond the lifetime of @result.
 **/
GMimeMessage *
g_mime_tnef_conversion_result_get_message (GMimeTnefConversionResult *result)
{
	g_return_val_if_fail (result != NULL, NULL);

	return result->message;
}

/**
 * g_mime_tnef_conversion_result_get_loss_count:
 * @result: conversion result
 *
 * Returns: number of recoverable losses across the message tree.
 **/
guint
g_mime_tnef_conversion_result_get_loss_count (const GMimeTnefConversionResult *result)
{
	g_return_val_if_fail (result != NULL, 0);

	return result->losses->len;
}

/**
 * g_mime_tnef_conversion_result_get_loss:
 * @result: conversion result
 * @index: zero-based loss index
 *
 * Returns: (transfer none): loss owned by @result.
 **/
const GMimeTnefConversionLoss *
g_mime_tnef_conversion_result_get_loss (const GMimeTnefConversionResult *result, guint index)
{
	g_return_val_if_fail (result != NULL && index < result->losses->len, NULL);

	return g_ptr_array_index (result->losses, index);
}

typedef struct {
	GMimeTnefMessage *tnef;
	GMimeTnefPropertySet *props;
	GMimeTnefConversionOptions *options;
	GPtrArray *losses;
	GMimeStream *rtf;
	char *charset;
	gboolean embedded;
} Conversion;

static GMimeMessage *convert_message (GMimeTnefMessage *, GMimeTnefConversionOptions *, GPtrArray *, gboolean, GError **);

static void
loss_free (gpointer data)
{
	GMimeTnefConversionLoss *loss = data;

	g_free ((char *) loss->description);
	g_free (loss);
}

static void
add_loss (GMimeTnefConversionLossKind kind, const char *description, gpointer data)
{
	GMimeTnefConversionLoss *loss = g_new (GMimeTnefConversionLoss, 1);
	Conversion *ctx = data;

	loss->kind = kind;
	loss->description = g_strdup (description);

	g_ptr_array_add (ctx->losses, loss);
}

#define TAG(name) GMIME_TNEF_PROPERTY_TAG_##name
#define LOSS(name) GMIME_TNEF_CONVERSION_LOSS_##name

static const GValue *
get_value (GMimeTnefPropertySet *props, GMimeTnefPropertyTag tag)
{
	GMimeTnefProperty *prop = g_mime_tnef_property_set_lookup (props, tag);

	return prop && g_mime_tnef_property_get_count (prop) == 1 ? g_mime_tnef_property_get_value (prop, 0) : NULL;
}

static const char *
string_prop (GMimeTnefPropertySet *props, GMimeTnefPropertyTag tag)
{
	const GValue *value = get_value (props, tag);

	return value && G_VALUE_HOLDS_STRING (value) ? g_value_get_string (value) : NULL;
}

static int
int_prop (GMimeTnefPropertySet *props, GMimeTnefPropertyTag tag, int fallback)
{
	const GValue *value = get_value (props, tag);

	if (value && G_VALUE_HOLDS_INT (value))
		return g_value_get_int (value);

	return value && G_VALUE_HOLDS_BOOLEAN (value) ? g_value_get_boolean (value) : fallback;
}

static gpointer
boxed_prop (GMimeTnefPropertySet *props, GMimeTnefPropertyTag tag, GType type)
{
	const GValue *value = get_value (props, tag);

	return value && G_VALUE_HOLDS (value, type) ? g_value_get_boxed (value) : NULL;
}

static GMimeStream *
from_bytes (GBytes *bytes)
{
	gsize length;
	const char *data = g_bytes_get_data (bytes, &length);

	return g_mime_stream_mem_new_with_buffer (data ? data : "", length);
}

/* Consume the input, including a nonseekable callback stream. */
static GMimeStream *
copy_stream (GMimeStream *input, GError **error)
{
	GMimeStream *copy = g_mime_stream_mem_new ();
	char buffer[8192];
	ssize_t n = -1;

	while (input && (n = g_mime_stream_read (input, buffer, sizeof buffer)) > 0) {
		if (!_g_mime_tnef_write_all (copy, (const guint8 *) buffer, n, error)) {
			g_clear_object (&copy);
			break;
		}
	}

	if (n < 0) {
		_g_mime_tnef_io_error (error);
		g_clear_object (&copy);
	} else if (copy) {
		g_mime_stream_reset (copy);
	}

	g_clear_object (&input);

	return copy;
}

static void
set_content (GMimePart *part, GMimeStream *stream)
{
	GMimeContent *content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);

	g_mime_part_set_content (part, content);
	g_object_unref (content);
}

static char *
normalize_id (const char *id)
{
	char *text;
	gsize n;

	if (!id)
		return NULL;

	text = g_strdup (id);
	g_strstrip (text);
	n = strlen (text);

	if (n > 1 && text[0] == '<' && text[n - 1] == '>') {
		memmove (text, text + 1, n - 2);
		text[n - 2] = 0;
		g_strstrip (text);
	}

	if (!*text)
		g_clear_pointer (&text, g_free);

	return text;
}

static char *
binary_string (GMimeTnefPropertySet *props, GMimeTnefPropertyTag tag)
{
	GBytes *bytes = boxed_prop (props, tag, G_TYPE_BYTES);
	const char *data;
	gsize n;

	if (!bytes)
		return g_strdup (string_prop (props, tag));

	data = g_bytes_get_data (bytes, &n);

	while (n && data[n - 1] == '\0')
		n--;

	return n && g_utf8_validate (data, n, NULL) ? g_strndup (data, n) : NULL;
}

static GMimeInternetAddress *
mailbox (Conversion *ctx, GMimeTnefPropertySet *props, const char *name, const char *type,
	 const char *email, const char *smtp, GMimeTnefPropertyTag search_tag)
{
	gboolean internet = !type || !*type || !g_ascii_strcasecmp (type, "SMTP");
	const char *addr = internet && email && *email ? email : smtp;
	char *key = binary_string (props, search_tag);
	GMimeInternetAddress *ia = NULL;

	if ((!addr || !*addr) && internet && key && !g_ascii_strncasecmp (key, "SMTP:", 5))
		addr = key + 5;

	if (addr && *addr && !strpbrk (addr, "\r\n"))
		ia = g_mime_mailbox_address_parse (NULL, addr, -1, NULL);

	if (ia && name && *name && !strpbrk (name, "\r\n"))
		g_mime_internet_address_set_name (ia, name);

	if (!ia && (email || smtp || name || key)) {
		char *detail = g_strdup_printf ("No Internet mailbox could be recovered for %s.",
			name ? name : email ? email : smtp ? smtp : key);

		add_loss (LOSS (UNPARSABLE_RECIPIENT), detail, ctx);
		g_free (detail);
	}

	g_free (key);

	return ia;
}

static void
add_addresses (Conversion *ctx, GMimeMessage *message)
{
	guint i, n = g_mime_tnef_message_get_recipient_count (ctx->tnef);
	GMimeTnefPropertySet *props = ctx->props;
	GMimeInternetAddress *from, *sender;

#define STR(name) string_prop (props, TAG (name))
	from = mailbox (ctx, props, STR (SENT_REPRESENTING_NAME_W), STR (SENT_REPRESENTING_ADDRTYPE_W),
			STR (SENT_REPRESENTING_EMAIL_ADDRESS_W), NULL, TAG (SENT_REPRESENTING_SEARCH_KEY));
	sender = mailbox (ctx, props, STR (SENDER_NAME_W), STR (SENDER_ADDRTYPE_W), STR (SENDER_EMAIL_ADDRESS_W),
			  STR (SENDER_SMTP_ADDRESS_W), TAG (SENDER_SEARCH_KEY));

	if (from || sender)
		g_mime_internet_address_list_add (g_mime_message_get_from (message), from ? from : sender);

	if (from && sender && g_ascii_strcasecmp (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (from)),
						   g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (sender))))
		g_mime_internet_address_list_add (g_mime_message_get_sender (message), sender);

	g_clear_object (&from);
	g_clear_object (&sender);

	for (i = 0; i < n; i++) {
		GMimeTnefRecipient *recipient = g_mime_tnef_message_get_recipient (ctx->tnef, i);
		GMimeInternetAddress *ia;
		GMimeAddressType dest;

		switch (g_mime_tnef_recipient_get_recipient_type (recipient)) {
		case GMIME_TNEF_RECIPIENT_TYPE_TO: dest = GMIME_ADDRESS_TYPE_TO; break;
		case GMIME_TNEF_RECIPIENT_TYPE_CC: dest = GMIME_ADDRESS_TYPE_CC; break;
		case GMIME_TNEF_RECIPIENT_TYPE_BCC: dest = GMIME_ADDRESS_TYPE_BCC; break;
		default: continue;
		}

		props = g_mime_tnef_recipient_get_properties (recipient);
		ia = mailbox (ctx, props, g_mime_tnef_recipient_get_display_name (recipient), g_mime_tnef_recipient_get_address_type (recipient),
			      STR (EMAIL_ADDRESS_W), STR (SMTP_ADDRESS_W), TAG (SEARCH_KEY));

		if (ia) {
			g_mime_internet_address_list_add (g_mime_message_get_addresses (message, dest), ia);
			g_object_unref (ia);
		}
	}

	if (!n) {
		GMimeInternetAddress *ia = mailbox (ctx, props, STR (RECEIVED_BY_NAME_W), STR (RECEIVED_BY_ADDRTYPE_W),
						    STR (RECEIVED_BY_EMAIL_ADDRESS_W), NULL, TAG (RECEIVED_BY_SEARCH_KEY));

		if (ia) {
			g_mime_internet_address_list_add (g_mime_message_get_to (message), ia);
			g_object_unref (ia);
		}
	}
}

static void
editable_headers (Conversion *ctx, GMimeMessage *message)
{
	static const char *levels[] = { "low", "normal", "high" };
	GMimeTnefNameId *name = g_mime_tnef_name_id_new_with_id (GMIME_TNEF_PROPERTY_SET_COMMON, 0x8530);
	GMimeTnefProperty *prop = g_mime_tnef_property_set_lookup_name (ctx->props, name);
	int importance = int_prop (ctx->props, TAG (IMPORTANCE), -1);

	if (importance >= 0 && importance <= 2)
		g_mime_entity_set_header (GMIME_ENTITY (message), "Importance", levels[importance], NULL);

	if (prop && g_mime_tnef_property_get_count (prop) == 1) {
		const GValue *value = g_mime_tnef_property_get_value (prop, 0);

		if (G_VALUE_HOLDS_STRING (value)) {
			const char *flag = g_value_get_string (value);

			if (flag && *flag && !strpbrk (flag, "\r\n"))
				g_mime_entity_set_header (GMIME_ENTITY (message), "X-Message-Flag", flag, NULL);
		}
	}

	g_mime_tnef_name_id_unref (name);
}

static void
internet_headers (Conversion *ctx, GMimeHeaderList *headers)
{
	static const char *prefixes[] = { "X-Microsoft-Exchange-Organization", "X-Microsoft-Exchange-Forest",
					  "X-MS-Exchange-Organization-", "X-MS-Exchange-Forest-" };
	guint i, j;

	for (i = 0; i < g_mime_tnef_property_set_get_count (ctx->props); i++) {
		GMimeTnefProperty *prop = g_mime_tnef_property_set_get_property (ctx->props, i);
		GMimeTnefNameId *name = g_mime_tnef_property_get_name (prop);
		const char *field;
		const char *c;

		if (!name || g_mime_tnef_name_id_get_kind (name) != GMIME_TNEF_NAME_ID_KIND_NAME ||
		    g_ascii_strcasecmp (g_mime_tnef_name_id_get_property_set (name), GMIME_TNEF_PROPERTY_SET_INTERNET_HEADERS))
			continue;

		field = g_mime_tnef_name_id_get_name (name);

		if (!field || !*field || !g_ascii_strncasecmp (field, "Content-", 8) ||
		    !g_ascii_strcasecmp (field, "MIME-Version") || g_mime_header_list_get_header (headers, field))
			continue;

		for (c = field; *c; c++) {
			if (*c < 33 || *c > 126 || *c == ':')
				break;
		}

		if (*c) {
			add_loss (LOSS (INVALID_HEADER), "Invalid Internet header field name.", ctx);
			continue;
		}

		if (!ctx->embedded) {
			for (j = 0; j < G_N_ELEMENTS (prefixes); j++) {
				if (!g_ascii_strncasecmp (field, prefixes[j], strlen (prefixes[j])))
					break;
			}

			if (j < G_N_ELEMENTS (prefixes))
				continue;
		}

		for (j = 0; j < g_mime_tnef_property_get_count (prop); j++) {
			const GValue *value = g_mime_tnef_property_get_value (prop, j);
			const char *text = G_VALUE_HOLDS_STRING (value) ? g_value_get_string (value) : NULL;
			GMimeHeader *header = text && !strpbrk (text, "\r\n") ? g_mime_header_new_full (field, text, NULL, NULL) : NULL;

			if (!header) {
				char *detail = g_strdup_printf ("Invalid Internet header '%s' was dropped.", field);

				add_loss (LOSS (INVALID_HEADER), detail, ctx);
				g_free (detail);
			} else {
				g_mime_header_list_add (headers, header);
				g_object_unref (header);
			}
		}
	}
}

static void
create_headers (Conversion *ctx, GMimeMessage *message)
{
	static const char *sensitivity[] = { NULL, "Personal", "Private", "Company-Confidential" };
	static const char *priority[] = { "non-urgent", "normal", "urgent" };
	GMimeHeaderList *headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	GDateTime *date = boxed_prop (ctx->props, TAG (CLIENT_SUBMIT_TIME), G_TYPE_DATE_TIME);
	GMimeTnefPropertySet *props = ctx->props;
	const char *str = STR (TRANSPORT_MESSAGE_HEADERS_W), *subject;
	char *id = NULL;
	int n;

	if (str)
		str += strspn (str, "\r\n");

	if (str && *str) {
		char *text = g_strconcat (str, "\r\n\r\n", NULL);
		GMimeStream *stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		GMimeParser *parser = g_mime_parser_new_with_stream (stream);
		GMimeHeaderList *parsed = g_mime_parser_construct_headers (parser, NULL, NULL);

		if (parsed) {
			for (n = 0; n < g_mime_header_list_get_count (parsed); n++) {
				GMimeHeader *header = g_mime_header_list_get_header_at (parsed, n);

				if (!g_ascii_strcasecmp (g_mime_header_get_name (header), "Received"))
					g_mime_header_list_add (headers, header);
			}

			g_object_unref (parsed);
		} else {
			add_loss (LOSS (INVALID_HEADER), "The transport headers could not be parsed.", ctx);
		}

		g_object_unref (parser);
		g_object_unref (stream);
		g_free (text);
	}

	add_addresses (ctx, message);

	if (date)
		g_mime_message_set_date (message, date);

	subject = g_mime_tnef_message_get_subject (ctx->tnef);

	if (subject) {
		g_mime_message_set_subject (message, subject, NULL);
	} else if ((str = STR (NORMALIZED_SUBJECT_W))) {
		const char *prefix = STR (SUBJECT_PREFIX_W);
		char *joined = g_strconcat (prefix ? prefix : "", str, NULL);

		g_mime_message_set_subject (message, joined, NULL);
		g_free (joined);
	}

	if ((str = g_mime_tnef_message_get_internet_message_id (ctx->tnef)) && !(id = _g_mime_parse_message_id_value (str)))
		add_loss (LOSS (INVALID_MESSAGE_ID), "Invalid Internet Message-Id.", ctx);

	if (!id) {
		char *key = binary_string (props, TAG (TNEF_CORRELATION_KEY));

		if (key && key[0] == '<' && strchr (key, '@') && g_str_has_suffix (key, ">"))
			id = _g_mime_parse_message_id_value (key);

		g_free (key);
	}

	if (id) {
		g_mime_message_set_message_id (message, id);
		g_free (id);
	}

	if ((str = STR (IN_REPLY_TO_ID_W))) {
		if ((id = _g_mime_parse_message_id_value (str))) {
			g_mime_message_set_in_reply_to (message, id);
			g_free (id);
		} else {
			add_loss (LOSS (INVALID_MESSAGE_ID), "Invalid In-Reply-To identifier.", ctx);
		}
	}

	if ((str = STR (INTERNET_REFERENCES_W))) {
		GPtrArray *refs = g_ptr_array_new_with_free_func (g_free);
		GMimeTnefProperty *prop = g_mime_tnef_property_set_lookup (props, TAG (INTERNET_REFERENCES_W));
		const char *end = str + prop->string_length;
		gboolean nul;

		while ((id = _g_mime_parse_reference_full (&str, end, &nul))) {
			char *normalized = nul ? NULL : _g_mime_parse_message_id_value (id);

			if (normalized)
				g_ptr_array_add (refs, normalized);
			else
				add_loss (LOSS (INVALID_MESSAGE_ID), "Invalid identifier in References.", ctx);

			g_free (id);
		}

		if (refs->len)
			g_mime_message_id_list_add_range (g_mime_message_get_references (message),
							  (const char * const *) refs->pdata, refs->len);

		g_ptr_array_unref (refs);
	}

	n = int_prop (props, TAG (PRIORITY), 99);

	if (n >= -1 && n <= 1)
		g_mime_entity_set_header (GMIME_ENTITY (message), "Priority", priority[n + 1], NULL);

	n = int_prop (props, TAG (SENSITIVITY), 0);

	if (n >= 1 && n <= 3)
		g_mime_entity_set_header (GMIME_ENTITY (message), "Sensitivity", sensitivity[n], NULL);

	editable_headers (ctx, message);
	internet_headers (ctx, headers);
}

static gboolean
prepare_rtf (Conversion *ctx, GError **error)
{
	GMimeTnefMessageBody *body = g_mime_tnef_message_get_rtf_body (ctx->tnef);
	GMimeRtfCompressionMode mode;
	GMimeStream *raw, *filtered;
	GMimeFilter *filter;

	if (!body)
		return TRUE;

	raw = g_mime_tnef_message_body_open_read (body);
	filtered = g_mime_stream_filter_new (raw);
	g_object_unref (raw);

	filter = g_mime_filter_rtf_compressed_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);

	ctx->rtf = copy_stream (filtered, error);
	mode = g_mime_filter_rtf_compressed_get_compression_mode (GMIME_FILTER_RTF_COMPRESSED (filter));

	if (!ctx->rtf) {
		g_object_unref (filter);
		return FALSE;
	}

	if (mode != GMIME_RTF_COMPRESSION_MODE_COMPRESSED && mode != GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED) {
		add_loss (LOSS (INVALID_RTF_BODY), "Unknown compressed RTF format; the body was dropped.", ctx);
		g_clear_object (&ctx->rtf);
	} else if (mode == GMIME_RTF_COMPRESSION_MODE_COMPRESSED &&
		   !g_mime_filter_rtf_compressed_is_valid_crc32 (GMIME_FILTER_RTF_COMPRESSED (filter))) {
		add_loss (LOSS (RTF_CHECKSUM_MISMATCH), "The rich-text body's CRC does not match; the recovered text was retained.", ctx);
	}

	g_object_unref (filter);

	return TRUE;
}

static gboolean
use_rtf (Conversion *ctx)
{
	int native = int_prop (ctx->props, TAG (NATIVE_BODY), 0);

	if (!ctx->rtf)
		return FALSE;

	if (native == 2 || (!g_mime_tnef_message_get_text_body (ctx->tnef) && !g_mime_tnef_message_get_html_body (ctx->tnef)))
		return TRUE;

	return native != 1 && native != 3 && int_prop (ctx->props, TAG (RTF_IN_SYNC), FALSE);
}

static const char *
html_attr (GMimeHtmlToken *token, const char *name)
{
	const GMimeHtmlAttribute *attr = g_mime_html_token_lookup_attribute (token, name);

	return attr ? g_mime_html_attribute_get_value (attr) : NULL;
}

/* Unlike a browser's 1024-byte prescan, [MS-OXCMAIL] body conversion looks
 * throughout the head and need not see explicit html/head start tags. */
static char *
html_charset (GMimeStream *stream)
{
	GMimeHtmlTokenizer *tokens = g_mime_html_tokenizer_new_with_charset (stream, "iso-8859-1", FALSE, 4096, NULL);
	GMimeHtmlToken *token;
	char *charset = NULL;

	if (!tokens)
		return NULL;

	while (g_mime_html_tokenizer_read_next_token (tokens, &token, NULL) && token) {
		gboolean stop = FALSE;

		if (g_mime_html_token_get_kind (token) == GMIME_HTML_TOKEN_TAG) {
			GMimeHtmlTagId tag = g_mime_html_token_get_tag_id (token);
			gboolean end = g_mime_html_token_is_end_tag (token);

			stop = tag == GMIME_HTML_TAG_BODY || (end && tag == GMIME_HTML_TAG_HEAD);

			if (tag == GMIME_HTML_TAG_META && !end) {
				const char *equiv = html_attr (token, "http-equiv");
				const char *content = html_attr (token, "content");
				const char *str = html_attr (token, "charset");

				if (str) {
					charset = g_strdup (str);
					g_strstrip (charset);

					if (!*charset)
						g_clear_pointer (&charset, g_free);
				}

				if (!charset && equiv && !g_ascii_strcasecmp (equiv, "Content-Type") && content) {
					GMimeContentType *content_type = g_mime_content_type_parse_full (NULL, content, -1, NULL);

					if (content_type) {
						charset = g_strdup (g_mime_content_type_get_charset (content_type));
						g_object_unref (content_type);
					}
				}
			}
		}

		g_mime_html_token_unref (token);

		if (stop || charset)
			break;
	}

	g_object_unref (tokens);
	g_mime_stream_reset (stream);

	if (charset) {
		GByteArray *data = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
		char *check = _g_mime_iconv_convert (data->len ? (char *) data->data : "", data->len, "utf-8", charset, NULL, NULL);

		if (check)
			g_free (check);
		else
			g_clear_pointer (&charset, g_free);
	}

	return charset;
}

static gboolean
set_body_content (Conversion *ctx, GMimePart *part, GMimeTnefMessageBody *body, GError **error)
{
	GMimeTnefMessageBodyFormat format = g_mime_tnef_message_body_get_format (body);
	const char *charset = g_mime_tnef_message_body_get_charset (body);
	char *detected = NULL;
	GMimeStream *stream;

	if (format == GMIME_TNEF_MESSAGE_BODY_FORMAT_COMPRESSED_RTF) {
		set_content (part, ctx->rtf);
		return TRUE;
	}

	if (g_mime_tnef_property_tag_get_value_type (g_mime_tnef_message_body_get_tag (body)) == GMIME_TNEF_PROPERTY_TYPE_UNICODE) {
		char *text = g_mime_tnef_message_body_get_text (body, error);

		if (!text)
			return FALSE;

		stream = g_mime_stream_mem_new_with_buffer (text, strlen (text));
		g_free (text);
		charset = "utf-8";
	} else {
		stream = copy_stream (g_mime_tnef_message_body_open_read (body), error);

		if (!stream)
			return FALSE;

		if (format == GMIME_TNEF_MESSAGE_BODY_FORMAT_HTML && (detected = html_charset (stream)))
			charset = detected;
	}

	set_content (part, stream);
	g_mime_entity_set_content_type_parameter (GMIME_ENTITY (part), "charset", charset ? charset : ctx->charset);
	g_object_unref (stream);
	g_free (detected);

	return TRUE;
}

static GMimeMessage *
embedded_message (Conversion *ctx, GMimeTnefAttachment *attachment, GError **error)
{
	GError *fault = NULL;
	GMimeMessage *message;
	GMimeTnefMessage *embedded = g_mime_tnef_attachment_load_embedded_message (attachment, &fault);

	if (!embedded) {
		add_loss (LOSS (INVALID_EMBEDDED_MESSAGE), fault ? fault->message : "Invalid embedded TNEF message.", ctx);
		g_clear_error (&fault);
		return NULL;
	}

	message = convert_message (embedded, ctx->options, ctx->losses, TRUE, error);
	g_object_unref (embedded);

	return message;
}

static void
attach_loss (Conversion *ctx, GMimeTnefAttachment *attachment, guint index, GMimeTnefConversionLossKind kind, const char *reason)
{
	const char *name = g_mime_tnef_attachment_get_file_name (attachment);
	char *detail = g_strdup_printf ("Attachment #%u%s%s%s %s", index + 1, name ? " (" : "",
					name ? name : "", name ? ")" : "", reason);

	add_loss (kind, detail, ctx);
	g_free (detail);
}

static void
missing_loss (Conversion *ctx, GMimeTnefAttachment *attachment, guint index)
{
	int method = g_mime_tnef_attachment_get_method (attachment);

	if (method >= 2 && method <= 4)
		attach_loss (ctx, attachment, index, LOSS (UNSUPPORTED_ATTACH_METHOD), "references external content and was dropped.");
	else
		attach_loss (ctx, attachment, index, LOSS (ATTACHMENT_WITHOUT_CONTENT), "has no content and was dropped.");
}

static const char *
sniff_image (GMimeStream *stream)
{
	GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	const guint8 *s = bytes->data;
	gsize n = bytes->len;

	if (n >= 8 && !memcmp (s, "\x89PNG\r\n\x1a\n", 8))
		return "png";

	if (n >= 3 && !memcmp (s, "\xff\xd8\xff", 3))
		return "jpeg";

	if (n >= 6 && (!memcmp (s, "GIF87a", 6) || !memcmp (s, "GIF89a", 6)))
		return "gif";

	if (n >= 12 && !memcmp (s, "RIFF", 4) && !memcmp (s + 8, "WEBP", 4))
		return "webp";

	if (n >= 18 && !memcmp (s, "BM", 2)) {
		switch (_g_mime_tnef_get_uint32 (s + 14)) {
		case 12: case 40: case 52: case 56: case 64: case 108: case 124: return "bmp";
		}
	}

	return NULL;
}

static void
attachment_props (Conversion *ctx, GMimeTnefAttachment *attachment, guint index, GMimeEntity *entity)
{
	GMimeTnefPropertySet *props = g_mime_tnef_attachment_get_properties (attachment);
	const char *s = STR (ATTACH_DISPOSITION_W), *file = g_mime_tnef_attachment_get_file_name (attachment);
	GDateTime *created = boxed_prop (props, TAG (CREATION_TIME), G_TYPE_DATE_TIME);
	GDateTime *modified = boxed_prop (props, TAG (LAST_MODIFICATION_TIME), G_TYPE_DATE_TIME);
	int size = int_prop (props, TAG (ATTACH_SIZE), -1);
	GMimeContentDisposition *disp;

	if (s && (disp = g_mime_content_disposition_parse_full (NULL, s, -1, NULL))) {
		g_mime_entity_set_content_disposition (entity, disp);
		g_object_unref (disp);
	}

	if (file && *file) {
		g_mime_entity_set_content_disposition_parameter (entity, "filename", file);
		g_mime_entity_set_content_type_parameter (entity, "name", file);
	}

	if ((s = STR (DISPLAY_NAME_W)) && *s)
		g_mime_entity_set_content_type_parameter (entity, "name", s);

	if (size >= 0 || created || modified) {
		if (!g_mime_entity_get_content_disposition (entity))
			g_mime_entity_set_disposition (entity, "attachment");

		disp = g_mime_entity_get_content_disposition (entity);

		if (size >= 0)
			g_mime_content_disposition_set_size (disp, size);

		if (created)
			g_mime_content_disposition_set_creation_date (disp, created);

		if (modified)
			g_mime_content_disposition_set_modification_date (disp, modified);
	}

	if ((s = g_mime_tnef_attachment_get_content_id (attachment))) {
		char *id = _g_mime_parse_message_id_value (s);

		if (id)
			g_mime_entity_set_content_id (entity, id);
		else
			attach_loss (ctx, attachment, index, LOSS (INVALID_MESSAGE_ID), "has an invalid Content-Id.");

		g_free (id);
	}

	if ((s = STR (ATTACH_CONTENT_LOCATION_W)) && _g_mime_is_content_uri (s, FALSE))
		g_mime_entity_set_content_location (entity, s);

	if ((s = STR (ATTACH_CONTENT_BASE_W)) && _g_mime_is_content_uri (s, TRUE))
		g_mime_entity_set_content_base (entity, s);
}

static GMimeEntity *
create_attachment (Conversion *ctx, GMimeTnefAttachment *attachment, guint index, GError **error)
{
	GMimeTnefAttachMethod method = g_mime_tnef_attachment_get_method (attachment);
	GMimeContentType *content_type;
	GMimeStream *stream = NULL;
	const char *image = NULL;
	GMimePart *part = NULL;

	if (!g_mime_tnef_attachment_has_content (attachment)) {
		missing_loss (ctx, attachment, index);
		return NULL;
	}

	if (g_mime_tnef_attachment_is_embedded_message (attachment)) {
		if (ctx->options->embedded) {
			GMimeMessage *message = embedded_message (ctx, attachment, error);

			if (message) {
				GMimeMessagePart *nested = g_mime_message_part_new_with_message ("rfc822", message);
				g_object_unref (message);
				attachment_props (ctx, attachment, index, GMIME_ENTITY (nested));
				return GMIME_ENTITY (nested);
			}

			if (*error)
				return NULL;
		}

		part = GMIME_PART (g_mime_tnef_part_new ());
		g_mime_part_set_filename (part, NULL);
		g_mime_entity_remove_header (GMIME_ENTITY (part), "Content-Disposition");
		attachment_props (ctx, attachment, index, GMIME_ENTITY (part));
	} else if (method == GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE) {
		attach_loss (ctx, attachment, index, LOSS (INVALID_EMBEDDED_MESSAGE), "is not a message; the original bytes were kept.");
	} else if (method == GMIME_TNEF_ATTACH_METHOD_OLE) {
		if (ctx->options->ole) {
			GMimeStream *rendered = ctx->options->ole (attachment, ctx->options->ole_data->data);

			if (rendered) {
				stream = copy_stream (rendered, error);

				if (!stream)
					return NULL;

				if ((image = sniff_image (stream)))
					part = g_mime_part_new_with_type ("image", image);
				else
					g_clear_object (&stream);
			}
		}

		if (!part)
			attach_loss (ctx, attachment, index, LOSS (OLE_OBJECT_NOT_RENDERED), "was not rendered; the original OLE data was retained.");
	}

	if (!part) {
		const char *type = g_mime_tnef_attachment_get_mime_type (attachment);

		content_type = type ? g_mime_content_type_parse_full (NULL, type, -1, NULL) : NULL;

		if (content_type && (g_mime_content_type_is_type (content_type, "multipart", "*") || g_mime_content_type_is_type (content_type, "message", "*") ||
			   g_mime_content_type_is_type (content_type, "application", "applefile") || g_mime_content_type_is_type (content_type, "application", "mac-binhex40")))
			g_clear_object (&content_type);

		part = content_type ? g_mime_part_new_with_content_type (content_type) : g_mime_part_new_with_type ("application", "octet-stream");
		g_clear_object (&content_type);
	}

	if (!stream && !(stream = copy_stream (g_mime_tnef_attachment_open_read (attachment), error))) {
		g_object_unref (part);
		return NULL;
	}

	set_content (part, stream);
	g_object_unref (stream);

	content_type = g_mime_entity_get_content_type (GMIME_ENTITY (part));
	g_mime_part_set_content_encoding (part, g_mime_content_type_is_type (content_type, "text", "*") ?
		g_mime_part_get_best_content_encoding (part, GMIME_ENCODING_CONSTRAINT_7BIT, error) : GMIME_CONTENT_ENCODING_BASE64);
	attachment_props (ctx, attachment, index, GMIME_ENTITY (part));

	if (image) {
		const char *ext = !strcmp (image, "jpeg") ? "jpg" : image;
		const char *name = g_mime_content_type_get_name (content_type);
		char *suffix = g_strconcat (".", ext, NULL);
		char *desc;

		if (!name || !*name)
			name = "image";

		desc = strlen (name) >= strlen (suffix) && !g_ascii_strcasecmp (name + strlen (name) - strlen (suffix), suffix) ?
			g_strdup (name) : g_strconcat (name, suffix, NULL);

		g_mime_part_set_filename (part, desc);
		g_mime_part_set_content_description (part, desc);
		g_mime_content_disposition_set_size (g_mime_entity_get_content_disposition (GMIME_ENTITY (part)), -1);
		g_free (desc);
		g_free (suffix);
	}

	return GMIME_ENTITY (part);
}

typedef struct {
	guint index, position;
	GMimePart *image;
	char *text;
} Placeholder;

static gint
compare_slots (gconstpointer a, gconstpointer b)
{
	const Placeholder *x = a, *y = b;

	if (x->position != y->position)
		return x->position < y->position ? -1 : 1;

	return x->index < y->index ? -1 : x->index != y->index;
}

static gboolean
write_text_slot (guint index, GMimeStream *output, gpointer data, GError **error)
{
	GArray *slots = data;
	const char *s = index < slots->len ? g_array_index (slots, Placeholder, index).text : NULL;

	return !s || _g_mime_tnef_write_all (output, (const guint8 *) s, strlen (s), error);
}

static gboolean
write_html_slot (int index, GMimeHtmlWriter *writer, gpointer data, GError **error)
{
	GArray *slots = data;
	Placeholder *slot;

	if (index < 0 || (guint) index >= slots->len)
		return TRUE;

	slot = &g_array_index (slots, Placeholder, index);

	if (slot->image) {
		const char *name = g_mime_part_get_filename (slot->image);
		char *escaped = g_uri_escape_string (g_mime_part_get_content_id (slot->image), "!$&'()*+,;=:@", FALSE);
		char *url = g_strconcat ("cid:", escaped, NULL);
		gboolean ok = g_mime_html_writer_write_empty_element_tag (writer, "img", error) &&
			g_mime_html_writer_write_attribute (writer, "src", url, error) &&
			(!name || g_mime_html_writer_write_attribute (writer, "alt", name, error));

		g_free (escaped);
		g_free (url);

		return ok;
	}

	return !slot->text || g_mime_html_writer_write_text (writer, slot->text, -1, error);
}

static gboolean
count_slots (GMimeStream *rtf, GMimeRtfToHtml *html, int *count, GError **error)
{
	GMimeFilter *latin1 = g_mime_filter_charset_new ("iso-8859-1", "utf-8");
	GMimeStream *decoded;
	gboolean ok;

	g_mime_stream_reset (rtf);
	decoded = g_mime_stream_filter_new (rtf);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (decoded), latin1);

	ok = _g_mime_rtf_to_html_count_object_placeholders (html, decoded, count, error);
	g_object_unref (latin1);
	g_object_unref (decoded);

	return ok;
}

static gboolean
prepare_slots (Conversion *ctx, GMimeEntity **entities, gboolean *is_inline, GArray *slots,
	       GMimeTextConverter *plain, GMimeTextConverter *html, GError **error)
{
	GHashTable *ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	guint i, n = g_mime_tnef_message_get_attachment_count (ctx->tnef);
	gboolean ok = FALSE, any_text = FALSE, any_image = FALSE;
	int hc = 0, tc;

	if (!count_slots (ctx->rtf, GMIME_RTF_TO_HTML (html), &hc, error))
		goto done;

	for (i = 0; i < n; i++) {
		GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (ctx->tnef, i);
		GMimeTnefPropertySet *props = g_mime_tnef_attachment_get_properties (attachment);
		const char *id = entities[i] ? g_mime_entity_get_content_id (entities[i]) : NULL;
		const GValue *pos = get_value (props, TAG (RENDERING_POSITION));
		Placeholder slot = { i, G_MAXUINT32, NULL, NULL };

		if (id) {
			char *key = g_ascii_strdown (id, -1);
			guint count = GPOINTER_TO_UINT (g_hash_table_lookup (ids, key));

			g_hash_table_replace (ids, key, GUINT_TO_POINTER (count + 1));
		}

		if (int_prop (props, TAG (ATTACHMENT_HIDDEN), FALSE))
			continue;

		if (pos && G_VALUE_HOLDS_INT (pos)) {
			slot.position = (guint32) g_value_get_int (pos);

			if (slot.position == G_MAXUINT32)
				continue;
		}

		g_array_append_val (slots, slot);
	}

	/* RTF placeholders occur in rendering-position order, not necessarily
	 * in the order of the TNEF attachments. */
	g_array_sort (slots, compare_slots);

	for (i = 0; i < slots->len; i++) {
		Placeholder *slot = &g_array_index (slots, Placeholder, i);
		GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (ctx->tnef, slot->index);
		GMimeEntity *entity = entities[slot->index];
		GMimeContentType *content_type;
		const char *image;

		if (ctx->options->placeholder) {
			slot->text = ctx->options->placeholder (attachment, entity, ctx->options->placeholder_data->data);

			if (slot->text && *slot->text)
				any_text = TRUE;
		}

		if (hc == -1 || !GMIME_IS_PART (entity) || g_mime_tnef_attachment_get_method (attachment) == GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE)
			continue;

		content_type = g_mime_entity_get_content_type (entity);

		if (g_mime_content_type_is_type (content_type, "image", "*") || g_mime_content_type_is_type (content_type, "application", "octet-stream")) {
			GMimeContent *content = g_mime_part_get_content (GMIME_PART (entity));
			GMimeStream *stream = content ? g_mime_content_get_stream (content) : NULL;
			const char *cid = g_mime_entity_get_content_id (entity);
			char *key;

			if (!GMIME_IS_STREAM_MEM (stream) || !(image = sniff_image (stream)))
				continue;

			key = cid ? g_ascii_strdown (cid, -1) : NULL;

			if (!key || GPOINTER_TO_UINT (g_hash_table_lookup (ids, key)) != 1) {
				char *id;

				if (key) {
					guint count = GPOINTER_TO_UINT (g_hash_table_lookup (ids, key));
					g_hash_table_replace (ids, key, GUINT_TO_POINTER (count - 1));
				}

				do {
					id = g_mime_utils_generate_message_id (NULL);
					key = g_ascii_strdown (id, -1);

					if (!g_hash_table_contains (ids, key))
						break;

					g_free (key);
					g_free (id);
				} while (TRUE);

				g_hash_table_insert (ids, key, GUINT_TO_POINTER (1));
				g_mime_entity_set_content_id (entity, id);
				g_free (id);
			} else {
				g_free (key);
			}

			g_mime_content_type_set_media_type (content_type, "image");
			g_mime_content_type_set_media_subtype (content_type, image);

			slot->image = GMIME_PART (entity);
			any_image = TRUE;
			is_inline[slot->index] = TRUE;
		}
	}

	/* A no-op HTML callback would still open a formatted span at the placeholder. */
	if (!any_image && !any_text) {
		ok = TRUE;
		goto done;
	}

	tc = hc;

	if (any_text && hc == -1) {
		g_mime_rtf_to_html_set_extract_encapsulated_html (GMIME_RTF_TO_HTML (html), FALSE);

		if (!count_slots (ctx->rtf, GMIME_RTF_TO_HTML (html), &tc, error))
			goto done;

		g_mime_rtf_to_html_set_extract_encapsulated_html (GMIME_RTF_TO_HTML (html), TRUE);
	}

	if (any_text) {
		if (tc >= 0 && (guint) tc == slots->len) {
			g_mime_rtf_to_text_set_attachment_placeholder_callback (GMIME_RTF_TO_TEXT (plain), write_text_slot, slots, NULL);
		} else {
			GString *footer = g_string_new (NULL);

			for (i = 0; i < slots->len; i++) {
				const char *s = g_array_index (slots, Placeholder, i).text;

				if (s && *s)
					g_string_append_printf (footer, "\n%s", s);
			}

			g_string_append_c (footer, '\n');
			g_mime_text_converter_set_footer (plain, footer->str);
			g_string_free (footer, TRUE);
		}
	}

	if (hc >= 0 && (guint) hc == slots->len) {
		g_mime_rtf_to_html_set_attachment_placeholder_callback (GMIME_RTF_TO_HTML (html), write_html_slot, slots, NULL);
	} else if (hc >= 0) {
		GMimeStream *stream = g_mime_stream_mem_new ();
		GMimeHtmlWriter *writer = g_mime_html_writer_new (stream);
		GByteArray *bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));

		for (i = 0; i < slots->len; i++) {
			Placeholder *slot = &g_array_index (slots, Placeholder, i);

			if (slot->image || (slot->text && *slot->text)) {
				if (!g_mime_html_writer_write_start_tag (writer, "div", error) ||
				    !write_html_slot (i, writer, slots, error) ||
				    !g_mime_html_writer_write_end_tag (writer, "div", error))
					break;
			}
		}

		if (!*error && g_mime_html_writer_finish (writer, error) && bytes->len) {
			char *footer = g_strndup ((const char *) bytes->data, bytes->len);

			g_mime_rtf_to_html_set_footer_format (GMIME_RTF_TO_HTML (html), GMIME_HEADER_FOOTER_FORMAT_HTML);
			g_mime_text_converter_set_footer (html, footer);
			g_free (footer);
		}

		g_object_unref (writer);
		g_object_unref (stream);
	}

	ok = !*error;

done:

	g_hash_table_unref (ids);

	return ok;
}

static GMimeTextPart *
render (Conversion *ctx, GMimeTextConverter *converter, const char *subtype, GError **error)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeTextPart *part = NULL;

	g_mime_stream_reset (ctx->rtf);
	g_mime_text_converter_set_write_bom (converter, FALSE);

	if (g_mime_text_converter_set_output_charset (converter, "utf-8", error) &&
	    g_mime_text_converter_convert (converter, ctx->rtf, stream, error)) {
		g_mime_stream_reset (stream);

		part = g_mime_text_part_new_with_subtype (subtype);
		set_content (GMIME_PART (part), stream);

		g_mime_text_part_set_charset (part, "utf-8");
	}

	g_object_unref (stream);

	return part;
}

/* Restrict memory used in scanning HTML to the candidates rather than the
 * number of distinct attribute values in a potentially very large document. */
static char *
location_key (const char *base, const char *location)
{
	if (!location || !*location || !base || !*base)
		return NULL;

	return g_uri_resolve_relative (base, location, G_URI_FLAGS_NONE, NULL);
}

static void
mark_html_references (Conversion *ctx, GMimeTextPart *html, GMimeEntity **entities, gboolean *is_inline)
{
	GHashTable *candidates = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	guint i, n = g_mime_tnef_message_get_attachment_count (ctx->tnef);
	GPtrArray *keys = g_ptr_array_new_full (0, g_free);
	GMimeHtmlTokenizer *tokenizer;
	GMimeHtmlToken *token;
	GMimeStream *stream;

	for (i = 0; i < n; i++) {
		GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (ctx->tnef, i);
		char *cid = NULL, *loc = NULL, *abs = NULL;

		if (entities[i] && !g_mime_tnef_attachment_is_embedded_message (attachment) &&
		    g_mime_tnef_attachment_get_method (attachment) != GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE &&
		    (g_mime_tnef_attachment_get_flags (attachment) & GMIME_TNEF_ATTACH_FLAGS_RENDERED_IN_BODY)) {
			const char *id = g_mime_entity_get_content_id (entities[i]);
			const char *location = g_mime_entity_get_content_location (entities[i]);

			if (id) {
				char *lower = g_ascii_strdown (id, -1);

				cid = g_strconcat ("cid:", lower, NULL);

				g_free (lower);
				g_hash_table_insert (candidates, g_strdup (cid), NULL);
			}

			if (location) {
				loc = g_strdup (location);
				abs = location_key (g_mime_entity_get_content_base (entities[i]), loc);

				g_hash_table_insert (candidates, g_strdup (loc), NULL);

				if (abs)
					g_hash_table_insert (candidates, g_strdup (abs), NULL);
			}
		}

		g_ptr_array_add (keys, cid);
		g_ptr_array_add (keys, loc);
		g_ptr_array_add (keys, abs);
	}

	if (!g_hash_table_size (candidates))
		goto done;

	stream = g_mime_content_get_stream (g_mime_part_get_content (GMIME_PART (html)));
	g_mime_stream_reset (stream);

	tokenizer = g_mime_html_tokenizer_new_with_charset (stream, g_mime_entity_get_content_type_parameter (GMIME_ENTITY (html), "charset"),
							    FALSE, 4096, NULL);

	if (tokenizer) {
		while (g_mime_html_tokenizer_read_next_token (tokenizer, &token, NULL) && token) {
			gsize j;

			for (j = 0; j < g_mime_html_token_get_n_attributes (token); j++) {
				const GMimeHtmlAttribute *attr = g_mime_html_token_get_attribute (token, j);
				const char *s = g_mime_html_attribute_get_value (attr);
				char *key;

				if (!s)
					continue;

				key = g_strdup (s);
				g_strstrip (key);

				if (!g_ascii_strncasecmp (key, "cid:", 4)) {
					char *unescaped = g_uri_unescape_string (key + 4, NULL);

					g_free (key);

					if (!unescaped)
						continue;

					key = g_ascii_strdown (unescaped, -1);
					g_free (unescaped);

					unescaped = g_strconcat ("cid:", key, NULL);
					g_free (key);

					key = unescaped;
				}

				if (g_hash_table_contains (candidates, key))
					g_hash_table_replace (candidates, key, GINT_TO_POINTER (1));
				else
					g_free (key);
			}

			g_mime_html_token_unref (token);
		}

		g_object_unref (tokenizer);
	}

	g_mime_stream_reset (stream);

	for (i = 0; i < keys->len; i++) {
		char *s = g_ptr_array_index (keys, i);

		if (s && g_hash_table_lookup (candidates, s))
			is_inline[i / 3] = TRUE;
	}

done:

	g_ptr_array_unref (keys);
	g_hash_table_unref (candidates);
}

typedef struct {
	GHashTable *ids, *consumed;
	char *body_id;
	const char *failure;
	gboolean took_text, took_html, took_rtf;
} Skeleton;

/* When the original part had no Content-Id, this header links the empty
 * skeleton part to its promoted content. It is not an output header
 * ([MS-OXCMAIL] 2.4.3.1). */
#define SKELETON_ID "X-Exchange-MIME-Skeleton-Content-Id"

static char *
take_id (GMimeEntity *entity)
{
	char *id = normalize_id (g_mime_entity_get_header (entity, SKELETON_ID));

	g_mime_header_list_remove_all (g_mime_entity_get_header_list (entity), SKELETON_ID);

	return id;
}

static gboolean
fill_leaf (Conversion *ctx, Skeleton *sk, GMimeEntity *entity, const char *id, GError **error)
{
	GMimeTnefAttachment *attachment = id ? g_hash_table_lookup (sk->ids, id) : NULL;
	GMimeTnefMessageBody *body = NULL;
	GMimeContentType *content_type;
	GMimeContent *content;
	GMimeStream *stream;

	if (attachment && !g_hash_table_add (sk->consumed, attachment))
		attachment = NULL;

	if (GMIME_IS_MESSAGE_PART (entity)) {
		GMimeMessage *embedded;

		if (!attachment)
			return TRUE;

		if (!g_mime_tnef_attachment_is_embedded_message (attachment)) {
			sk->failure = "The matching attachment is not an embedded message.";
			return FALSE;
		}

		if (!(embedded = embedded_message (ctx, attachment, error))) {
			sk->failure = "The matching embedded message could not be converted.";
			return FALSE;
		}

		g_mime_message_part_set_message (GMIME_MESSAGE_PART (entity), embedded);
		g_object_unref (embedded);

		return TRUE;
	}

	if (!GMIME_IS_PART (entity))
		return TRUE;

	content = g_mime_part_get_content (GMIME_PART (entity));
	stream = content ? g_mime_content_get_stream (content) : NULL;

	if (stream && g_mime_stream_length (stream) != 0)
		return TRUE;

	if (attachment) {
		if (!g_mime_tnef_attachment_has_content (attachment)) {
			sk->failure = "The matching attachment has no content.";
			return FALSE;
		}
		if (!(stream = copy_stream (g_mime_tnef_attachment_open_read (attachment), error)))
			return FALSE;

		set_content (GMIME_PART (entity), stream);
		g_object_unref (stream);

		return TRUE;
	}

	if (id && sk->body_id && strcmp (id, sk->body_id)) {
		sk->failure = "No promoted part matches the skeleton Content-Id.";
		return FALSE;
	}

	content_type = g_mime_entity_get_content_type (entity);

	if (!sk->took_text && g_mime_content_type_is_type (content_type, "text", "plain")) {
		body = g_mime_tnef_message_get_text_body (ctx->tnef);
		sk->took_text = TRUE;
	} else if (!sk->took_html && g_mime_content_type_is_type (content_type, "text", "html")) {
		body = g_mime_tnef_message_get_html_body (ctx->tnef);
		sk->took_html = TRUE;
	} else if (ctx->rtf && !sk->took_rtf && (g_mime_content_type_is_type (content_type, "text", "rtf") ||
						  g_mime_content_type_is_type (content_type, "application", "rtf"))) {
		body = g_mime_tnef_message_get_rtf_body (ctx->tnef);
		sk->took_rtf = TRUE;
	}

	if (!body) {
		sk->failure = "No promoted body matches the empty skeleton part.";
		return FALSE;
	}
	return set_body_content (ctx, GMIME_PART (entity), body, error);
}

static gboolean
fill_skeleton (Conversion *ctx, Skeleton *sk, GMimeEntity *entity, const char *root_id, GError **error)
{
	char *id = take_id (entity);
	gboolean ok = TRUE;

	if (GMIME_IS_MULTIPART (entity)) {
		GMimeMultipart *multi = GMIME_MULTIPART (entity);
		int i;

		for (i = 0; i < g_mime_multipart_get_count (multi) && ok; i++)
			ok = fill_skeleton (ctx, sk, g_mime_multipart_get_part (multi, i), NULL, error);
	} else {
		if (!id)
			id = root_id ? g_strdup (root_id) : normalize_id (g_mime_entity_get_content_id (entity));

		ok = fill_leaf (ctx, sk, entity, id, error);
	}

	g_free (id);

	return ok;
}

static GMimeMessage *
restore_skeleton (Conversion *ctx, GBytes *bytes, GError **error)
{
	GMimeStream *stream = from_bytes (bytes);
	GMimeParser *parser = g_mime_parser_new_with_stream (stream);
	GMimeMessage *message = g_mime_parser_construct_message (parser, NULL, NULL);
	guint i, n = g_mime_tnef_message_get_attachment_count (ctx->tnef), checkpoint = ctx->losses->len;
	Skeleton sk = { 0 };
	gboolean ok = FALSE;

	g_object_unref (parser);
	g_object_unref (stream);

	if (message && g_mime_message_get_mime_part (message)) {
		char *root_id = take_id (GMIME_ENTITY (message));

		sk.ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		sk.consumed = g_hash_table_new (NULL, NULL);
		sk.body_id = normalize_id (string_prop (ctx->props, TAG (BODY_CONTENT_ID_W)));

		for (i = 0; i < n; i++) {
			GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (ctx->tnef, i);
			char *id = normalize_id (g_mime_tnef_attachment_get_content_id (attachment));

			if (id && !g_hash_table_contains (sk.ids, id))
				g_hash_table_insert (sk.ids, id, attachment);
			else
				g_free (id);
		}

		ok = fill_skeleton (ctx, &sk, g_mime_message_get_mime_part (message), root_id, error);

		for (i = 0; i < n && ok; i++) {
			GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (ctx->tnef, i);

			if (g_mime_tnef_attachment_has_content (attachment)) {
				ok = g_hash_table_contains (sk.consumed, attachment);
				if (!ok)
					sk.failure = "The MIME skeleton does not contain a promoted attachment.";
			} else
				missing_loss (ctx, attachment, i);
		}

		g_free (root_id);
		g_free (sk.body_id);
		g_hash_table_unref (sk.ids);
		g_hash_table_unref (sk.consumed);
	}

	if (ok) {
		editable_headers (ctx, message);
	} else {
		g_clear_object (&message);
		g_ptr_array_set_size (ctx->losses, checkpoint);
		add_loss (LOSS (INVALID_MIME_SKELETON), sk.failure ? sk.failure : "The MIME skeleton could not be parsed.", ctx);
	}

	return message;
}

static GMimeMessage *
convert_properties (Conversion *ctx, GError **error)
{
	guint i, n = g_mime_tnef_message_get_attachment_count (ctx->tnef);
	GMimeEntity **entities = g_new0 (GMimeEntity *, n);
	gboolean *is_inline = g_new0 (gboolean, n);
	GHashTable *consumed = g_hash_table_new (NULL, NULL);
	GMimeTextPart *parts[3] = { NULL, NULL, NULL };
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMultipart *alt = NULL, *mixed = NULL;
	GMimeMultipartRelated *related = NULL;
	GMimeEntity *body = NULL;
	GBytes *calendar = NULL;
	const char *method = NULL;

	create_headers (ctx, message);

	if (ctx->options->calendar)
		calendar = _g_mime_tnef_calendar_export (ctx->tnef, ctx->charset, ctx->options->exceptions,
							  ctx->options->exceptions_size, &method, consumed, add_loss, ctx);

	for (i = 0; i < n; i++) {
		GMimeTnefAttachment *attachment = g_mime_tnef_message_get_attachment (ctx->tnef, i);

		if (!g_hash_table_contains (consumed, attachment))
			entities[i] = create_attachment (ctx, attachment, i, error);

		if (*error)
			goto done;
	}

	if (use_rtf (ctx)) {
		GMimeTextConverter *plain = g_mime_rtf_to_text_new (), *html = g_mime_rtf_to_html_new ();
		GArray *slots = g_array_new (FALSE, FALSE, sizeof (Placeholder));

		if (prepare_slots (ctx, entities, is_inline, slots, plain, html, error)) {
			parts[0] = render (ctx, plain, "plain", error);

			if (parts[0])
				parts[1] = render (ctx, html, "html", error);
		}

		g_object_unref (plain);
		g_object_unref (html);

		for (i = 0; i < slots->len; i++)
			g_free (g_array_index (slots, Placeholder, i).text);

		g_array_unref (slots);

		if (*error)
			goto done;
	} else {
		for (i = 0; i < 2; i++) {
			GMimeTnefMessageBody *source = i ? g_mime_tnef_message_get_html_body (ctx->tnef) :
				g_mime_tnef_message_get_text_body (ctx->tnef);

			if (source) {
				parts[i] = g_mime_text_part_new_with_subtype (i ? "html" : "plain");

				if (!set_body_content (ctx, GMIME_PART (parts[i]), source, error))
					goto done;
			}
		}
	}

	if (calendar) {
		GMimeStream *stream = from_bytes (calendar);

		parts[2] = g_mime_text_part_new_with_subtype ("calendar");

		set_content (GMIME_PART (parts[2]), stream);
		g_mime_text_part_set_charset (parts[2], "utf-8");

		if (method)
			g_mime_entity_set_content_type_parameter (GMIME_ENTITY (parts[2]), "method", method);

		g_object_unref (stream);
	}

	if (parts[1])
		mark_html_references (ctx, parts[1], entities, is_inline);

	for (i = 0; i < G_N_ELEMENTS (parts); i++) {
		if (!parts[i])
			continue;

		g_mime_part_set_content_encoding (GMIME_PART (parts[i]), g_mime_part_get_best_content_encoding (
			GMIME_PART (parts[i]), GMIME_ENCODING_CONSTRAINT_7BIT, error));

		if (*error)
			goto done;

		if (!body) {
			body = GMIME_ENTITY (parts[i]);
		} else {
			if (!alt) {
				alt = GMIME_MULTIPART (g_mime_multipart_alternative_new ());
				g_mime_multipart_add (alt, body);
				body = GMIME_ENTITY (alt);
			}

			g_mime_multipart_add (alt, GMIME_ENTITY (parts[i]));
		}
	}

	for (i = 0; i < n; i++) {
		if (!entities[i])
			continue;

		if (body && is_inline[i]) {
			if (!related) {
				related = g_mime_multipart_related_new ();
				g_mime_multipart_related_set_root (related, body);
			}

			g_mime_entity_set_disposition (entities[i], "inline");
			g_mime_multipart_add (GMIME_MULTIPART (related), entities[i]);
		} else {
			if (!mixed)
				mixed = g_mime_multipart_new_with_subtype ("mixed");

			g_mime_multipart_add (mixed, entities[i]);
		}
	}

	if (related)
		body = GMIME_ENTITY (related);

	if (mixed) {
		if (body)
			g_mime_multipart_insert (mixed, 0, body);

		body = GMIME_ENTITY (mixed);
	}

	if (body)
		g_mime_message_set_mime_part (message, body);

done:

	g_clear_object (&related);
	g_clear_object (&alt);
	g_clear_object (&mixed);

	for (i = 0; i < G_N_ELEMENTS (parts); i++)
		g_clear_object (&parts[i]);

	for (i = 0; i < n; i++)
		g_clear_object (&entities[i]);

	g_clear_pointer (&calendar, g_bytes_unref);
	g_hash_table_unref (consumed);
	g_free (entities);
	g_free (is_inline);

	if (*error)
		g_clear_object (&message);

	return message;
}

static GMimeMessage *
convert_message (GMimeTnefMessage *tnef, GMimeTnefConversionOptions *options, GPtrArray *losses, gboolean embedded, GError **error)
{
	Conversion ctx = { tnef, g_mime_tnef_message_get_properties (tnef), options, losses, NULL, NULL, embedded };
	GMimeMessage *message = NULL;
	GBytes *skeleton = boxed_prop (ctx.props, TAG (MIME_SKELETON), G_TYPE_BYTES);

	ctx.charset = _g_mime_tnef_get_charset (g_mime_tnef_message_get_codepage (tnef));

	if (!ctx.charset)
		ctx.charset = g_strdup ("windows-1252");

	/* Whether the RTF is usable affects body selection and which attachments
	 * can be rendered inline. Resolve that before rebuilding the MIME tree. */
	if (prepare_rtf (&ctx, error)) {
		/* PidTagMimeSkeleton retains the original headers and part structure;
		 * the individual MAPI properties can only approximate them. */
		if (skeleton && g_bytes_get_size (skeleton))
			message = restore_skeleton (&ctx, skeleton, error);

		if (!message && !*error)
			message = convert_properties (&ctx, error);
	}

	g_clear_object (&ctx.rtf);
	g_free (ctx.charset);

	return message;
}

/**
 * g_mime_tnef_message_convert_to_mime:
 * @message: a loaded TNEF message
 * @options: (nullable): conversion options, or %NULL for defaults
 * @error: return location for a fatal conversion error, or %NULL
 *
 * Restores the MIME skeleton when all promoted parts can be matched.
 * Otherwise reconstructs the headers, bodies and attachments from message
 * properties. Recoverable losses (including those of nested messages) are
 * available in the result. All output payloads outlive the TNEF input.
 *
 * Returns: (transfer full) (nullable): result, or %NULL on fatal error.
 **/
GMimeTnefConversionResult *
g_mime_tnef_message_convert_to_mime (GMimeTnefMessage *message, const GMimeTnefConversionOptions *options, GError **error)
{
	GMimeTnefConversionOptions *settings;
	GMimeTnefConversionResult *result;
	GError *fault = NULL;

	g_return_val_if_fail (GMIME_IS_TNEF_MESSAGE (message), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	settings = options ? g_mime_tnef_conversion_options_clone (options) : g_mime_tnef_conversion_options_new ();
	result = g_new0 (GMimeTnefConversionResult, 1);
	result->refs = 1;
	result->losses = g_ptr_array_new_with_free_func (loss_free);
	result->message = convert_message (message, settings, result->losses, FALSE, &fault);

	g_mime_tnef_conversion_options_unref (settings);

	if (!result->message) {
		g_mime_tnef_conversion_result_unref (result);

		if (fault)
			g_propagate_error (error, fault);

		return NULL;
	}

	return result;
}
