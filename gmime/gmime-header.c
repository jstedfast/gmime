/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "gmime-stream-fs.h"
#include "gmime-stream-filter.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-stream-mem.h"
#include "gmime-internet-address.h"
#include "gmime-message-id-list.h"
#include "gmime-internal.h"
#include "gmime-common.h"
#include "gmime-header.h"
#include "gmime-parser.h"
#include "gmime-events.h"
#include "gmime-error.h"
#include "gmime-utils.h"


/**
 * SECTION: gmime-header
 * @title: GMimeHeaderList
 * @short_description: Message and MIME part headers
 * @see_also: #GMimeEntity
 *
 * A #GMimeHeader is a collection of rfc822 header fields and their
 * values.
 **/

static const char * const header_names[] = {
	"Accept-Language",
	"Ad-Hoc",
	"Alternate-Recipient",
	"Apparently-To",
	"Approved",
	"ARC-Authentication-Results",
	"ARC-Message-Signature",
	"ARC-Seal",
	"Archive",
	"Archived-At",
	"Article",
	"Authentication-Results",
	"Autocrypt",
	"Autocrypt-Gossip",
	"Autocrypt-Setup-Message",
	"Autoforwarded",
	"Auto-Submitted",
	"Autosubmitted",
	"Base",
	"Bcc",
	"Body",
	"Bytes",
	"Cc",
	"Comments",
	"Content-Alternative",
	"Content-Base",
	"Content-Class",
	"Content-Description",
	"Content-Disposition",
	"Content-Duration",
	"Content-Features",
	"Content-Id",
	"Content-Identifier",
	"Content-Language",
	"Content-Length",
	"Content-Location",
	"Content-Md5",
	"Content-Return",
	"Content-Transfer-Encoding",
	"Content-Translation-Type",
	"Content-Type",
	"Control",
	"Conversion",
	"Conversion-With-Loss",
	"Date",
	"Date-Received",
	"Deferred-Delivery",
	"Delivery-Date",
	"Disclose-Recipients",
	"Disposition-Notification-Options",
	"Disposition-Notification-To",
	"Distribution",
	"DKIM-Signature",
	"DomainKey-Signature",
	"Encoding",
	"Encrypted",
	"Expires",
	"Expiry-Date",
	"Followup-To",
	"From",
	"Generate-Delivery-Report",
	"Importance",
	"Injection-Date",
	"Injection-Info",
	"In-Reply-To",
	"Keywords",
	"Language",
	"Latest-Delivery-Time",
	"Lines",
	"List-Archive",
	"List-Help",
	"List-Id",
	"List-Owner",
	"List-Post",
	"List-Subscribe",
	"List-Unsubscribe",
	"List-Unsubscribe-Post",
	"Message-Id",
	"MIME-Version",
	"Newsgroups",
	"Nntp-Posting-Host",
	"Organization",
	"Original-From",
	"Original-Message-Id",
	"Original-Recipient",
	"Original-Return-Address",
	"Original-Subject",
	"Path",
	"Precedence",
	"Prevent-NonDelivery-Report",
	"Priority",
	"Received",
	"Received-SPF",
	"References",
	"Relay-Version",
	"Reply-By",
	"Reply-To",
	"Require-Recipient-Valid-Since",
	"Resent-Bcc",
	"Resent-Cc",
	"Resent-Date",
	"Resent-From",
	"Resent-Message-Id",
	"Resent-Reply-To",
	"Resent-Sender",
	"Resent-To",
	"Return-Path",
	"Return-Receipt-To",
	"See-Also",
	"Sender",
	"Sensitivity",
	"Solicitation",
	"Status",
	"Subject",
	"Summary",
	"Supersedes",
	"TLS-Required",
	"To",
	"User-Agent",
	"X400-Content-Identifier",
	"X400-Content-Return",
	"X400-Content-Type",
	"X400-MTS-Identifier",
	"X400-Originator",
	"X400-Received",
	"X400-Recipients",
	"X400-Trace",
	"X-Mailer",
	"X-MSMail-Priority",
	"X-Priority",
	"X-Status"
};

G_STATIC_ASSERT (G_N_ELEMENTS (header_names) == GMIME_HEADER_X_STATUS + 1);


/**
 * g_mime_header_id_to_name:
 * @id: a #GMimeHeaderId
 *
 * Gets the canonical field name for @id. Like MimeKit's ToHeaderName(),
 * unknown and out-of-range identifiers return "Unknown".
 *
 * Returns: (transfer none): the canonical field name.
 **/
const char *
g_mime_header_id_to_name (GMimeHeaderId id)
{
	if (id >= 0 && (guint) id < G_N_ELEMENTS (header_names))
		return header_names[id];

	return "Unknown";
}


/**
 * g_mime_header_id_from_name:
 * @name: a header field name
 *
 * Looks up a common header field, ignoring ASCII case. The entire name
 * must match; whitespace and colons are not removed. Unrecognized names,
 * including extension fields, map to %GMIME_HEADER_UNKNOWN.
 *
 * Returns: the header identifier.
 **/
GMimeHeaderId
g_mime_header_id_from_name (const char *name)
{
	guint i;

	g_return_val_if_fail (name != NULL, GMIME_HEADER_UNKNOWN);

	for (i = 0; i < G_N_ELEMENTS (header_names); i++) {
		if (!g_ascii_strcasecmp (name, header_names[i]))
			return (GMimeHeaderId) i;
	}

	return GMIME_HEADER_UNKNOWN;
}


static struct {
	const char *name;
	GMimeHeaderRawValueFormatter formatter;
} formatters[] = {
	{ "Received",                    g_mime_header_format_received            },
	{ "Sender",                      g_mime_header_format_addrlist            },
	{ "From",                        g_mime_header_format_addrlist            },
	{ "Reply-To",                    g_mime_header_format_addrlist            },
	{ "To",                          g_mime_header_format_addrlist            },
	{ "Cc",                          g_mime_header_format_addrlist            },
	{ "Bcc",                         g_mime_header_format_addrlist            },
	{ "Message-Id",                  g_mime_header_format_message_id          },
	{ "In-Reply-To",                 g_mime_header_format_message_id          },
	{ "Original-Message-Id",         g_mime_header_format_message_id          },
	{ "References",                  g_mime_header_format_references          },
	{ "Resent-Sender",               g_mime_header_format_addrlist            },
	{ "Resent-From",                 g_mime_header_format_addrlist            },
	{ "Resent-Reply-To",             g_mime_header_format_addrlist            },
	{ "Resent-To",                   g_mime_header_format_addrlist            },
	{ "Resent-Cc",                   g_mime_header_format_addrlist            },
	{ "Resent-Bcc",                  g_mime_header_format_addrlist            },
	{ "Resent-Message-Id",           g_mime_header_format_message_id          },
	{ "Content-Type",                g_mime_header_format_content_type        },
	{ "Content-Disposition",         g_mime_header_format_content_disposition },
	{ "Content-Id",                  g_mime_header_format_message_id          },
	{ "Disposition-Notification-To", g_mime_header_format_addrlist            },
	{ "Disposition-Notification-Options", g_mime_header_format_disposition_notification_options },
	{ "DKIM-Signature",              g_mime_header_format_dkim_signature      },
	{ "ARC-Message-Signature",       g_mime_header_format_dkim_signature      },
	{ "ARC-Seal",                    g_mime_header_format_dkim_signature      },
	{ "List-Archive",                g_mime_header_format_mailing_list_command },
	{ "List-Help",                   g_mime_header_format_mailing_list_command },
	{ "List-Owner",                  g_mime_header_format_mailing_list_command },
	{ "List-Post",                   g_mime_header_format_mailing_list_command },
	{ "List-Subscribe",              g_mime_header_format_mailing_list_command },
	{ "List-Unsubscribe",            g_mime_header_format_mailing_list_command },
	{ "Newsgroups",                  g_mime_header_format_newsgroups          },
};


static void g_mime_header_class_init (GMimeHeaderClass *klass);
static void g_mime_header_init (GMimeHeader *header, GMimeHeaderClass *klass);
static void g_mime_header_finalize (GObject *object);
static char *reformat_international_header (GMimeHeader *header, GMimeFormatOptions *options);
static char *fold_international_text (GMimeFormatOptions *options, const char *field, const char *value);
static char *format_mailing_list_command (GMimeFormatOptions *options, const char *field,
					const char *value, const char *charset);

static GObjectClass *parent_class = NULL;


GType
g_mime_header_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeHeaderClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_header_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeHeader),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_header_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeHeader", &info, 0);
	}
	
	return type;
}

static void
g_mime_header_class_init (GMimeHeaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_header_finalize;
}

static void
g_mime_header_init (GMimeHeader *header, GMimeHeaderClass *klass)
{
	header->changed = g_mime_event_new (header);
	header->formatter = NULL;
	header->options = NULL;
	header->owns_options = FALSE;
	header->reformat = FALSE;
	header->explicit_raw_value = FALSE;
	header->raw_value = NULL;
	header->raw_name = NULL;
	header->charset = NULL;
	header->value = NULL;
	header->name = NULL;
	header->offset = -1;
	header->id = GMIME_HEADER_UNKNOWN;
}

static void
g_mime_header_finalize (GObject *object)
{
	GMimeHeader *header = (GMimeHeader *) object;
	
	g_mime_event_free (header->changed);
	if (header->owns_options)
		g_mime_parser_options_free (header->options);
	g_clear_pointer (&header->raw_value, g_bytes_unref);
	g_clear_pointer (&header->raw_name, g_bytes_unref);
	g_free (header->charset);
	g_free (header->value);
	g_free (header->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GBytes *
header_bytes_take_string (char *text)
{
	return text ? g_bytes_new_take (text, strlen (text)) : NULL;
}

static char *
header_bytes_dup_text (GBytes *bytes)
{
	gsize length;
	const char *data;

	if (!bytes)
		return NULL;

	data = g_bytes_get_data (bytes, &length);
	return length ? g_strndup (data, length) : g_strdup ("");
}

char *
_g_mime_header_dup_raw_value (GMimeHeader *header)
{
	/* Legacy text parsers require a temporary NUL-terminated prefix. The
	 * authoritative raw bytes are never changed by this conversion. */
	return header_bytes_dup_text (header->raw_value);
}


/**
 * header_new:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @name: header name
 * @value: header value
 * @raw_value: raw header value
 * @charset: a charset
 * @offset: file/stream offset for the start of the header (or %-1 if unknown)
 *
 * Creates a new #GMimeHeader.
 *
 * Returns: a new #GMimeHeader with the specified values.
 **/
static GMimeHeader *
header_new (GMimeParserOptions *options, const char *name, const char *value,
		   const char *raw_name, const char *raw_value, const char *charset,
		   gint64 offset)
{
	GMimeHeaderRawValueFormatter formatter;
	GMimeHeader *header;
	guint i;
	
	header = g_object_new (GMIME_TYPE_HEADER, NULL);
	header->raw_value = raw_value ? g_bytes_new (raw_value, strlen (raw_value)) : NULL;
	header->charset = charset ? g_strdup (charset) : NULL;
	header->value = value ? g_strdup (value) : NULL;
	header->raw_name = g_bytes_new (raw_name, strlen (raw_name));
	header->name = g_strdup (name);
	header->id = g_mime_header_id_from_name (name);
	header->reformat = !raw_value;
	header->options = options;
	header->offset = offset;
	
	formatter = g_mime_header_format_default;
	for (i = 0; i < G_N_ELEMENTS (formatters); i++) {
		if (!g_ascii_strcasecmp (formatters[i].name, name)) {
			formatter = header->formatter = formatters[i].formatter;
			break;
		}
	}
	
	if (!raw_value && value)
		header->raw_value = header_bytes_take_string (formatter (header, NULL, header->value, charset));
	
	return header;
}

GMimeHeader *
_g_mime_header_new_from_reader (GMimeParserOptions *options, const guint8 *buffer, gsize length,
			      gsize name_length, gsize field_length, gboolean invalid, gint64 offset)
{
	gsize value_offset = invalid ? length : field_length + 1;
	GMimeHeader *header;
	GString *name;
	gsize i;

	/* MimeKit keeps the original field and value as byte arrays so invalid
	 * fields and embedded NULs survive serialization and cloning without
	 * acquiring a synthetic colon. */
	name = g_string_sized_new (name_length);

	for (i = 0; i < name_length; i++) {
		if (buffer[i] == 0)
			g_string_append (name, "\\0");
		else
			g_string_append_unichar (name, buffer[i]);
	}

	header = header_new (g_mime_parser_options_clone (options), name->str, NULL,
			     name->str, NULL, NULL, offset);
	header->owns_options = TRUE;
	header->invalid = invalid;
	header->reformat = FALSE;
	g_bytes_unref (header->raw_name);
	header->raw_name = g_bytes_new (buffer, field_length);
	header->raw_value = g_bytes_new (buffer + value_offset, length - value_offset);
	g_string_free (name, TRUE);

	return header;
}


/**
 * g_mime_header_parse:
 * @options: (nullable): parser options, or %NULL for the defaults
 * @buffer: (array length=length) (element-type guint8) (nullable): input bytes
 * @length: the number of bytes to parse
 * @error: (nullable): return location for a #GError
 *
 * Parses a standalone header from the entire supplied byte span. The field
 * consists of printable ASCII other than ':', followed by optional spaces
 * or tabs and a colon. Like MimeKit's Header.TryParse(), this accepts an
 * empty field name; it is not a complete RFC header validator.
 *
 * All bytes after the colon belong to the value, including embedded NULs,
 * folded lines and any subsequent header-like lines. Existing LF or CRLF
 * endings are preserved; if the input does not end in LF, the default
 * newline sequence is appended to the raw value.
 *
 * The header owns copies of the input and parser options, has an unknown
 * source offset (-1), and decodes its value lazily. Missing field separators
 * return %NULL with %GMIME_ERROR_PARSE_ERROR and a relative byte offset.
 * @buffer may be %NULL only when @length is zero.
 *
 * Returns: (nullable) (transfer full): the parsed header, or %NULL on error.
 **/
GMimeHeader *
g_mime_header_parse (GMimeParserOptions *options, const guint8 *buffer, gsize length, GError **error)
{
	gsize name_length = 0, field_length, value_offset, value_length, newline_length;
	const char *newline;
	GMimeHeader *header;
	guint8 *value;

	g_return_val_if_fail (buffer != NULL || length == 0, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* find the end of the field name */
	while (name_length < length && buffer[name_length] >= 33 &&
	       buffer[name_length] < 127 && buffer[name_length] != ':')
		name_length++;

	field_length = name_length;
	while (field_length < length && (buffer[field_length] == ' ' || buffer[field_length] == '\t'))
		field_length++;

	if (field_length == length || buffer[field_length] != ':') {
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Expected header field separator at byte offset %" G_GSIZE_FORMAT, field_length);
		return NULL;
	}

	/* skip over the ':' */
	value_offset = field_length + 1;
	value_length = length - value_offset;
	newline = g_mime_format_options_get_newline (NULL);
	newline_length = buffer[length - 1] != '\n' ? strlen (newline) : 0;
	if (value_length > G_MAXSIZE - newline_length) {
		g_set_error_literal (error, GMIME_ERROR, EOVERFLOW, "Header value is too large");
		return NULL;
	}

	header = _g_mime_header_new_from_reader (options, buffer, length, name_length, field_length, FALSE, -1);

	/* When called from the public parsing APIs, force the value to be
	 * canonicalized by ending with a new-line sequence.
	 * See https://github.com/jstedfast/MimeKit/issues/695 for more information. */
	if (newline_length) {
		value = g_malloc (value_length + newline_length);
		memcpy (value, buffer + value_offset, value_length);
		memcpy (value + value_length, newline, newline_length);
		g_bytes_unref (header->raw_value);
		header->raw_value = g_bytes_new_take (value, value_length + newline_length);
	}

	return header;
}


/**
 * g_mime_header_parse_text:
 * @options: (nullable): parser options, or %NULL for the defaults
 * @text: a NUL-terminated UTF-8 header
 * @error: (nullable): return location for a #GError
 *
 * Parses a standalone header using g_mime_header_parse(). Use the byte-span
 * API for input containing embedded NULs or not terminated by a NUL.
 *
 * Returns: (nullable) (transfer full): the parsed header, or %NULL on error.
 **/
GMimeHeader *
g_mime_header_parse_text (GMimeParserOptions *options, const char *text, GError **error)
{
	g_return_val_if_fail (text != NULL, NULL);

	return g_mime_header_parse (options, (const guint8 *) text, strlen (text), error);
}


/**
 * g_mime_header_unfold:
 * @text: (nullable): a NUL-terminated UTF-8 header value
 *
 * Unfolds decoded header text for display. Leading and trailing linear
 * whitespace (SP, HTAB, CR and LF) is removed; other whitespace is retained.
 * All CR and LF characters are removed, including internal ones.
 * A %NULL input produces an empty string.
 *
 * This does not decode RFC 2047 encoded words. Use the header value getters
 * to decode raw header bytes before unfolding.
 *
 * Returns: (transfer full): newly allocated unfolded UTF-8 text.
 **/
char *
g_mime_header_unfold (const char *text)
{
	const char *inptr, *start, *end;
	char *outptr, *unfolded;

	if (!text)
		return g_strdup ("");

	g_return_val_if_fail (g_utf8_validate (text, -1, NULL), NULL);

	start = text;
	skip_lwsp (&start);

	end = start + strlen (start);
	while (end > start && is_lwsp (end[-1]))
		end--;

	outptr = unfolded = g_malloc (end - start + 1);

	for (inptr = start; inptr < end; inptr++) {
		if (*inptr != '\r' && *inptr != '\n')
			*outptr++ = *inptr;
	}

	*outptr = '\0';

	return unfolded;
}


/**
 * g_mime_header_new:
 * @name: a nonempty ASCII header field name without whitespace or a colon
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 *
 * Creates a standalone header, preserving field-name case. Leading and
 * trailing linear whitespace is trimmed and CR/LF characters are removed
 * from @value before formatting. The header owns its strings and an
 * independent copy of the default parser options and has no stream offset.
 *
 * Invalid names, %NULL values, and invalid UTF-8 are programming errors
 * reported through GLib's argument guards.
 *
 * Returns: (transfer full) (nullable): a new #GMimeHeader, or %NULL on failure.
 **/
GMimeHeader *
g_mime_header_new (const char *name, const char *value, const char *charset)
{
	const char *inptr;
	GMimeHeader *header;
	char *unfolded;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (*name != '\0', NULL);
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (value, -1, NULL), NULL);

	for (inptr = name; *inptr; inptr++)
		g_return_val_if_fail (*inptr >= 33 && *inptr <= 126 && *inptr != ':', NULL);

	unfolded = g_mime_header_unfold (value);
	header = header_new (g_mime_parser_options_clone (NULL), name, unfolded,
			     name, NULL, charset ? charset : "utf-8", -1);
	header->owns_options = TRUE;
	g_free (unfolded);

	if (!header->raw_value) {
		g_object_unref (header);
		return NULL;
	}

	return header;
}


/**
 * g_mime_header_new_full:
 * @name: a nonempty ASCII header field name without whitespace or a colon
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Creates a standalone header using g_mime_header_set_value_full().
 * Invalid UTF-8 and unsupported charsets produce %G_CONVERT_ERROR.
 * Invalid Content-Type and Content-Disposition values produce a
 * %GMIME_ERROR parse error instead of using legacy fallback values.
 * Invalid names and %NULL values are programming errors.
 *
 * Returns: (transfer full) (nullable): a new header, or %NULL on failure.
 **/
GMimeHeader *
g_mime_header_new_full (const char *name, const char *value, const char *charset, GError **error)
{
	GMimeHeader *header;
	const char *inptr;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (*name != '\0', NULL);
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	for (inptr = name; *inptr; inptr++)
		g_return_val_if_fail (*inptr >= 33 && *inptr <= 126 && *inptr != ':', NULL);

	header = header_new (g_mime_parser_options_clone (NULL), name, NULL, name, NULL, NULL, -1);
	header->owns_options = TRUE;

	if (!g_mime_header_set_value_full (header, NULL, value, charset, error)) {
		g_object_unref (header);
		return NULL;
	}

	return header;
}


/**
 * g_mime_header_new_with_id:
 * @id: a known #GMimeHeaderId
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 *
 * Creates a standalone header using the canonical field name for @id and
 * g_mime_header_new(). The header owns its strings and an independent copy
 * of the default parser options. Unknown and out-of-range IDs are
 * programming errors.
 *
 * Use g_mime_header_new_with_id_full() for checked structured values and
 * charset conversion with error reporting.
 *
 * Returns: (transfer full) (nullable): a new header, or %NULL on failure.
 **/
GMimeHeader *
g_mime_header_new_with_id (GMimeHeaderId id, const char *value, const char *charset)
{
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, NULL);

	return g_mime_header_new (g_mime_header_id_to_name (id), value, charset);
}


/**
 * g_mime_header_new_with_id_full:
 * @id: a known #GMimeHeaderId
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Creates a standalone header using the canonical field name for @id and
 * g_mime_header_new_full(), including its value normalization, independent
 * default parser options, formatting and error reporting. Unknown and
 * out-of-range IDs are programming errors.
 *
 * Returns: (transfer full) (nullable): a new header, or %NULL on failure.
 **/
GMimeHeader *
g_mime_header_new_with_id_full (GMimeHeaderId id, const char *value, const char *charset, GError **error)
{
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, NULL);

	return g_mime_header_new_full (g_mime_header_id_to_name (id), value, charset, error);
}


/**
 * g_mime_header_clone:
 * @header: a #GMimeHeader
 *
 * Clones the header, preserving its name, raw name and value, cached decoded
 * value, charset, and formatting behavior without decoding or reformatting it.
 * The clone has no change listeners and its stream offset is unknown (-1).
 *
 * The clone shares immutable raw bytes and owns copies of its decoded strings
 * and parser options. It can outlive the original header and its containing
 * list. As with
 * g_mime_parser_options_clone(), warning callback user data remains borrowed.
 *
 * Returns: (transfer full): an independent copy of @header.
 **/
GMimeHeader *
g_mime_header_clone (GMimeHeader *header)
{
	GMimeHeader *clone;

	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);

	clone = g_object_new (GMIME_TYPE_HEADER, NULL);
	clone->formatter = header->formatter;
	clone->options = g_mime_parser_options_clone (header->options);
	clone->owns_options = TRUE;
	clone->reformat = header->reformat;
	clone->explicit_raw_value = header->explicit_raw_value;
	clone->raw_value = header->raw_value ? g_bytes_ref (header->raw_value) : NULL;
	clone->raw_name = g_bytes_ref (header->raw_name);
	clone->charset = g_strdup (header->charset);
	clone->value = g_strdup (header->value);
	clone->name = g_strdup (header->name);
	clone->invalid = header->invalid;
	clone->id = header->id;

	return clone;
}


/**
 * g_mime_header_get_id:
 * @header: a #GMimeHeader
 *
 * Gets the header identifier, determined from its field name without
 * changing the original spelling or raw bytes. Extension and unrecognized
 * fields return %GMIME_HEADER_UNKNOWN. Changing the value does not change
 * the identifier.
 *
 * Returns: the header identifier.
 **/
GMimeHeaderId
g_mime_header_get_id (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), GMIME_HEADER_UNKNOWN);

	return header->id;
}


/**
 * g_mime_header_get_name:
 * @header: a #GMimeHeader
 *
 * Gets the header's name. Embedded NUL bytes in an invalid field are
 * represented as "\\0" in this display/lookup string; the original bytes
 * are preserved when writing the header.
 *
 * Returns: the header name or %NULL if invalid.
 **/
const char *
g_mime_header_get_name (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	return header->name;
}


/**
 * g_mime_header_get_value:
 * @header: a #GMimeHeader
 *
 * Gets the header's unfolded value, trimming leading and trailing linear
 * whitespace as in g_mime_header_unfold(). Literal and decoded NUL characters
 * are replaced with the Unicode replacement character U+FFFD.
 *
 * Returns: the header's decoded value or %NULL if invalid.
 **/
const char *
g_mime_header_get_value (GMimeHeader *header)
{
	const char *raw;
	gsize length;
	char *buf;
	
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	if (!header->value && header->raw_value) {
		raw = g_bytes_get_data (header->raw_value, &length);
		buf = _g_mime_utils_header_decode_text (header->options, raw, length, NULL, header->offset);
		header->value = g_mime_header_unfold (buf);
		g_free (buf);
	}
	
	return header->value;
}

/**
 * g_mime_header_get_value_with_charset:
 * @header: a #GMimeHeader
 * @charset: a nonempty fallback charset name
 * @error: return location for an error, or %NULL
 *
 * Decodes and unfolds the stored raw value using a copy of the header's
 * parser options with @charset as the fallback for undeclared 8-bit text.
 * Valid UTF-8 takes priority, and supported RFC 2047 declarations are
 * honored. Leading and trailing linear whitespace is removed.
 *
 * The result is newly allocated, even if the decoded value was already
 * cached. The header's options, cache, raw bytes and formatting state are
 * unchanged, and no change event is emitted. An unset legacy placeholder
 * returns an empty string after validating @charset.
 *
 * Empty, invalid or unsupported charsets report %G_CONVERT_ERROR.
 * Generic UTF-16/32 aliases use little-endian bytes. The historical
 * MimeKit alias "unicode" selects UTF-8, not UTF-16.
 * Literal and decoded NUL characters are replaced with the Unicode
 * replacement character U+FFFD. The stored raw bytes remain unchanged.
 * No failure returns partial text.
 *
 * Returns: (transfer full) (nullable): freshly decoded UTF-8 text, or %NULL
 * on failure.
 **/
char *
g_mime_header_get_value_with_charset (GMimeHeader *header, const char *charset, GError **error)
{
	GMimeParserOptions *options;
	const char *bytes = NULL, *encoding;
	char *decoded, *value;
	GIConv decoder;
	gsize length = 0;

	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	g_return_val_if_fail (charset != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if ((decoder = _g_mime_charset_open_text_decoder (charset, &encoding, error)) == (GIConv) -1)
		return NULL;
	g_iconv_close (decoder);

	options = g_mime_parser_options_clone (header->options);
	if (!g_mime_parser_options_set_charset_encoding (options, encoding, error)) {
		g_mime_parser_options_free (options);
		return NULL;
	}

	if (header->raw_value) {
		bytes = g_bytes_get_data (header->raw_value, &length);
	}

	decoded = _g_mime_utils_header_decode_text_full (options, bytes, length, header->offset);
	g_mime_parser_options_free (options);
	if (!decoded)
		return NULL;

	value = g_mime_header_unfold (decoded);
	g_free (decoded);

	return value;
}


GMimeContentEncoding
_g_mime_header_get_content_encoding (GMimeHeader *header)
{
	if (header->raw_value) {
		gsize length;
		const char *bytes = g_bytes_get_data (header->raw_value, &length);

		if (length && memchr (bytes, 0, length)) {
			GMimeContentEncoding encoding;
			char *text;

			/* Preserve token delimiters before literal NULs instead of
			 * removing them when unfolding the decoded value. */
			text = _g_mime_utils_header_decode_text (header->options, bytes, length, NULL, header->offset);
			encoding = _g_mime_content_encoding_parse (text, strlen (text));
			g_free (text);

			return encoding;
		}
	}

	return g_mime_content_encoding_from_string (g_mime_header_get_value (header));
}


/**
 * g_mime_header_set_value:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: the new header value
 * @charset: (nullable): a charset
 *
 * Sets the header's decoded value.
 **/
void
g_mime_header_set_value (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeHeaderRawValueFormatter formatter;
	GBytes *previous;
	char *buf, *raw_value, *encoding;
	
	g_return_if_fail (GMIME_IS_HEADER (header));
	g_return_if_fail (value != NULL);
	
	formatter = header->formatter ? header->formatter : g_mime_header_format_default;
	buf = g_mime_strdup_trim (value);
	raw_value = formatter (header, options, buf, charset);

	if (!raw_value) {
		g_free (buf);
		return;
	}

	encoding = g_strdup (charset);

	previous = header->raw_value;
	g_free (header->charset);
	g_free (header->value);
	
	header->raw_value = header_bytes_take_string (raw_value);
	header->charset = encoding;
	header->reformat = TRUE;
	header->explicit_raw_value = FALSE;
	header->value = buf;
	if (previous)
		g_bytes_unref (previous);
	
	g_mime_event_emit (header->changed, NULL);
}


static char *
header_format_value_full (GMimeHeader *header, GMimeFormatOptions *options, const char *value,
			  const char *charset, GError **error)
{
	GMimeHeaderRawValueFormatter formatter = header->formatter;
	GMimeContentDisposition *disposition = NULL;
	GMimeContentType *content_type = NULL;
	GMimeParamList *parameters;
	GMimeParam *parameter;
	char *raw_value;
	int i, errsv;

	if (formatter == g_mime_header_format_content_type) {
		content_type = g_mime_content_type_parse_full (header->options, value, -1, error);
		if (!content_type)
			return NULL;
		parameters = g_mime_content_type_get_parameters (content_type);
	} else if (formatter == g_mime_header_format_content_disposition) {
		disposition = g_mime_content_disposition_parse_full (header->options, value, -1, error);
		if (!disposition)
			return NULL;
		parameters = g_mime_content_disposition_get_parameters (disposition);
	} else {
		if (!formatter)
			formatter = g_mime_header_format_default;
		errno = 0;
		raw_value = formatter (header, options, value, charset);
		errsv = errno;
		if (!raw_value)
			g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
				     "Cannot format header '%s': %s", header->name,
				     errsv ? g_strerror (errsv) : "formatter returned no value");
		return raw_value;
	}

	for (i = 0; i < g_mime_param_list_length (parameters); i++) {
		parameter = g_mime_param_list_get_parameter_at (parameters, i);
		if (!g_mime_param_get_charset (parameter))
			g_mime_param_set_charset (parameter, charset);
	}

	if (content_type) {
		raw_value = g_mime_content_type_encode (content_type, options);
		g_object_unref (content_type);
	} else {
		raw_value = g_mime_content_disposition_encode (disposition, options);
		g_object_unref (disposition);
	}

	return raw_value;
}


/**
 * g_mime_header_set_value_full:
 * @header: a #GMimeHeader
 * @options: (nullable): formatting options, or %NULL for the defaults
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Trims leading and trailing linear whitespace, removes CR/LF characters,
 * and formats @value before replacing the header's state. Invalid UTF-8
 * and unsupported charsets produce %G_CONVERT_ERROR. Content-Type and
 * Content-Disposition use the checked parsers and report %GMIME_ERROR
 * parse errors, retaining those parsers' configured recovery behavior.
 * Other fields use the existing formatters, including UTF-8 fallback for
 * text that cannot be represented in a supported charset.
 *
 * On failure no header state is changed and no change event is emitted.
 * On success the formatted raw value is cached using @options, rather than
 * being reformatted on every write. International output can still reformat
 * it as UTF-8; this is not an explicit raw-value assignment.
 * Invalid parsed field names are not repaired by assigning a value.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_set_value_full (GMimeHeader *header, GMimeFormatOptions *options, const char *value,
			    const char *charset, GError **error)
{
	char *probe, *encoding, *text, *raw_value;
	GBytes *previous;
	gsize length;

	g_return_val_if_fail (GMIME_IS_HEADER (header), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!g_utf8_validate (value, -1, NULL)) {
		g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
				     "Header value is not valid UTF-8");
		return FALSE;
	}

	probe = _g_mime_charset_encode_text (charset ? charset : "utf-8", "", &length, &encoding, error);
	if (!probe)
		return FALSE;
	g_free (probe);

	text = g_mime_header_unfold (value);
	raw_value = header_format_value_full (header, options, text, encoding, error);
	if (!raw_value) {
		g_free (encoding);
		g_free (text);
		return FALSE;
	}

	previous = header->raw_value;
	g_free (header->charset);
	g_free (header->value);
	header->raw_value = header_bytes_take_string (raw_value);
	header->charset = encoding;
	header->value = text;
	/* Cache the formatting until reassignment, except for International output. */
	header->reformat = FALSE;
	header->explicit_raw_value = FALSE;
	if (previous)
		g_bytes_unref (previous);

	g_mime_event_emit (header->changed, NULL);

	return TRUE;
}


/**
 * g_mime_header_get_raw_name:
 * @header: a #GMimeHeader
 *
 * Gets the header's raw name. The raw header name is the complete byte sequence up to
 * (but not including) the ':' separating the header's name from its value. This
 * byte sequence may be different from the value returned by g_mime_header_get_name()
 * if the parsed message's header contained trailing whitespace after the header
 * name, such as: "Subject : this is the subject\r\n".
 * Embedded NULs in invalid fields are retained. The immutable bytes remain
 * valid after the header is changed or destroyed.
 *
 * Returns: (transfer full): the complete raw header name.
 **/
GBytes *
g_mime_header_get_raw_name (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	return g_bytes_ref (header->raw_name);
}


/**
 * g_mime_header_get_raw_value:
 * @header: a #GMimeHeader
 *
 * Gets the complete raw value before decoding or charset conversion,
 * including any embedded NUL bytes. The returned immutable bytes remain
 * valid after the header is changed or destroyed. An unset legacy
 * placeholder returns %NULL.
 *
 * Returns: (transfer full) (nullable): the raw bytes, or %NULL if unset.
 **/
GBytes *
g_mime_header_get_raw_value (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);

	return header->raw_value ? g_bytes_ref (header->raw_value) : NULL;
}


static void
header_set_raw_value (GMimeHeader *header, GBytes *raw_value)
{
	GBytes *previous = header->raw_value;

	g_free (header->value);

	header->reformat = FALSE;
	header->explicit_raw_value = TRUE;
	header->raw_value = raw_value;
	header->value = NULL;

	if (previous)
		g_bytes_unref (previous);

	g_mime_event_emit (header->changed, NULL);
}


/**
 * g_mime_header_set_raw_value:
 * @header: a #GMimeHeader
 * @raw_value: the complete raw value, ending in LF
 * @error: return location for an error, or %NULL
 *
 * Sets the raw value without encoding, folding or charset validation.
 * Empty values and values whose final byte is not LF report
 * %GMIME_ERROR_PARSE_ERROR without changing the header or notifying owners.
 * LF and CRLF endings are both accepted. No leading whitespace or other
 * header syntax is required or added.
 *
 * The header retains a reference to the immutable bytes, clears its decoded
 * cache and emits a change event, even when assigning the current bytes.
 * Its field name, source offset, charset and parser options are preserved.
 * Explicit raw values are preserved with International formatting until
 * replaced by a decoded-value assignment.
 *
 * Embedded NUL bytes are retained for serialization and byte-based access. Use
 * g_mime_header_get_value_with_charset() to reject NUL-containing text.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_set_raw_value (GMimeHeader *header, GBytes *raw_value, GError **error)
{
	const char *bytes;
	gsize length;

	g_return_val_if_fail (GMIME_IS_HEADER (header), FALSE);
	g_return_val_if_fail (raw_value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	bytes = g_bytes_get_data (raw_value, &length);
	if (!length || bytes[length - 1] != '\n') {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "Raw header value must end with a newline character");
		return FALSE;
	}

	header_set_raw_value (header, g_bytes_ref (raw_value));

	return TRUE;
}


/**
 * g_mime_header_get_offset:
 * @header: a #GMimeHeader
 *
 * Gets the header's stream offset if known.
 *
 * Returns: the header offset or %-1 if unknown.
 **/
gint64
g_mime_header_get_offset (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), -1);
	
	return header->offset;
}


void
_g_mime_header_set_offset (GMimeHeader *header, gint64 offset)
{
	header->offset = offset;
}


/**
 * g_mime_header_to_string:
 * @header: a #GMimeHeader
 *
 * Formats the header for display using its name and decoded value, separated
 * by a colon and a space. The raw name's trailing whitespace is not included,
 * and no encoding, folding, or terminating newline is added.
 * Invalid parsed fields return their display name alone, without adding a
 * colon or a value.
 *
 * This is not a transport serialization API. Use
 * g_mime_header_write_to_stream() to write the encoded header.
 *
 * Returns: (transfer full): a newly allocated display string.
 **/
char *
g_mime_header_to_string (GMimeHeader *header)
{
	const char *value;

	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	g_return_val_if_fail (header->name != NULL, NULL);

	if (header->invalid)
		return g_strdup (header->name);

	value = g_mime_header_get_value (header);

	return g_strdup_printf ("%s: %s", header->name, value ? value : "");
}


GBytes *
_g_mime_header_get_formatted_value (GMimeHeader *header, GMimeFormatOptions *options)
{
	GMimeHeaderRawValueFormatter formatter;
	gboolean reformat;
	char *raw_value = NULL;
	const char *bytes;
	gsize length;

	reformat = header->reformat || (!header->explicit_raw_value &&
				       g_mime_format_options_get_allow_international (options));

	bytes = g_bytes_get_data (header->raw_value, &length);

	/* The legacy decoded-value API is NUL-terminated. Never let an
	 * International formatting request truncate a binary raw value. */
	if (length && memchr (bytes, 0, length))
		reformat = FALSE;

	if (reformat) {
		if (header->reformat) {
			formatter = header->formatter ? header->formatter : g_mime_header_format_default;
			raw_value = formatter (header, options, header->value, header->charset);
		} else {
			raw_value = reformat_international_header (header, options);
		}

		if (!raw_value) {
			errno = EINVAL;
			return NULL;
		}

		return header_bytes_take_string (raw_value);
	}

	return g_bytes_ref (header->raw_value);
}


/**
 * g_mime_header_write_to_stream:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @stream: a #GMimeStream
 *
 * Writes the header to the specified stream. International formatting
 * reformats parsed text, address, and content headers as UTF-8 without
 * changing their stored raw values. Explicit raw assignments and parsed
 * trace, identifier, authentication, and signature fields are preserved.
 *
 * Returns: the number of bytes written, or %-1 on fail.
 **/
ssize_t
g_mime_header_write_to_stream (GMimeHeader *header, GMimeFormatOptions *options, GMimeStream *stream)
{
	ssize_t nwritten, total;
	const char *bytes, *field;
	gsize length, field_length;
	GBytes *value;
	int errsv;

	g_return_val_if_fail (GMIME_IS_HEADER (header), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);

	if (header->invalid) {
		bytes = g_bytes_get_data (header->raw_name, &length);
		return _g_mime_stream_write_all (stream, bytes, length);
	}

	if (!header->raw_value)
		return 0;

	if (!(value = _g_mime_header_get_formatted_value (header, options)))
		return -1;

	bytes = g_bytes_get_data (value, &length);
	field = g_bytes_get_data (header->raw_name, &field_length);

	if ((total = _g_mime_stream_write_all (stream, field, field_length)) < 0 ||
	    _g_mime_stream_write_all (stream, ":", 1) < 0 ||
	    (nwritten = _g_mime_stream_write_all (stream, bytes, length)) < 0) {
		errsv = errno;
		g_bytes_unref (value);
		errno = errsv;
		return -1;
	}

	total += 1 + nwritten;
	g_bytes_unref (value);

	return total;
}


/**
 * g_mime_header_format_content_disposition:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Content-Disposition header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_content_disposition (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeContentDisposition *disposition;
	GString *str;
	guint n;
	
	str = g_string_new_len (g_bytes_get_data (header->raw_name, NULL), g_bytes_get_size (header->raw_name));
	g_string_append_c (str, ':');
	n = str->len;
	
	g_string_append_c (str, ' ');
	
	disposition = g_mime_content_disposition_parse (header->options, value);
	g_string_append (str, disposition->disposition);
	
	g_mime_param_list_encode (disposition->params, options, TRUE, str);
	g_object_unref (disposition);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_content_type:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Content-Type header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_content_type (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeContentType *content_type;
	GString *str;
	guint n;
	
	str = g_string_new_len (g_bytes_get_data (header->raw_name, NULL), g_bytes_get_size (header->raw_name));
	g_string_append_c (str, ':');
	n = str->len;
	
	content_type = g_mime_content_type_parse (header->options, value);
	
	g_string_append_c (str, ' ');
	g_string_append (str, content_type->type ? content_type->type : "text");
	g_string_append_c (str, '/');
	g_string_append (str, content_type->subtype ? content_type->subtype : "plain");
	
	g_mime_param_list_encode (content_type->params, options, TRUE, str);
	g_object_unref (content_type);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_message_id:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a message-id header value, including In-Reply-To
 * @charset: a charset (note: unused)
 *
 * Unfolds @value and applies the configured newline format without introducing
 * new folds or parsing its message-id tokens.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_message_id (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	const char *newline = g_mime_format_options_get_newline (options);
	char *unfolded = g_mime_utils_header_unfold (value);
	char *raw_value = g_strdup_printf (" %s%s", unfolded, newline);

	g_free (unfolded);
	return raw_value;
}


/**
 * g_mime_header_format_references:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a References header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 * Folds only between message-ids, never before the first one.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_references (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	const char *newline, *msgid;
	GMimeMessageIdList *refs;
	guint cur, len, n, maxline;
	GString *str;
	int count, i;
	
	/* Note: we don't want to break in the middle of msgid tokens as
	   it seems to break a lot of clients (and servers) 
	   We also don't want to break before the first msgid because
	   that would violate RFC5536 */
	
	newline = g_mime_format_options_get_newline (options);
	maxline = g_mime_format_options_get_max_line_length (options);
	refs = g_mime_message_id_list_parse (header->options, value);
	str = g_string_new_len (g_bytes_get_data (header->raw_name, NULL), g_bytes_get_size (header->raw_name));
	g_string_append_c (str, ':');
	cur = n = str->len;
	
	count = g_mime_message_id_list_length (refs);
	for (i = 0; i < count; i++) {
		msgid = g_mime_message_id_list_get_message_id (refs, i);
		len = strlen (msgid);

		if (i > 0 && cur + len + 3 > maxline) {
			g_string_append (str, newline);
			g_string_append_c (str, '\t');
			cur = 1;
		} else {
			g_string_append_c (str, ' ');
			cur++;
		}
		
		g_string_append_c (str, '<');
		g_string_append_len (str, msgid, len);
		g_string_append_c (str, '>');
		cur += len + 2;
	}
	
	g_object_unref (refs);
	
	g_string_append (str, newline);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


static char *
format_address_list (const char *field, GMimeFormatOptions *options, GMimeInternetAddressList *addrlist)
{
	GString *str;
	guint n;

	str = g_string_new (field);
	g_string_append_c (str, ':');
	n = str->len;

	g_string_append_c (str, ' ');

	if (addrlist)
		g_mime_internet_address_list_encode (addrlist, options, str);

	memmove (str->str, str->str + n, (str->len - n) + 1);

	return g_string_free (str, FALSE);
}

/**
 * g_mime_header_format_addrlist:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Sender, From, Reply-To, To, Cc, or Bcc header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_addrlist (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeInternetAddressList *addrlist = NULL;
	char *raw_value, *field;

	if (value)
		addrlist = _g_mime_internet_address_list_parse (header->options, value, -1);
	field = header_bytes_dup_text (header->raw_name);
	raw_value = format_address_list (field, options, addrlist);
	g_free (field);
	g_clear_object (&addrlist);

	return raw_value;
}


static char *
reformat_international_header (GMimeHeader *header, GMimeFormatOptions *options)
{
	GMimeHeaderRawValueFormatter formatter = header->formatter;
	GMimeContentDisposition *disposition;
	GMimeInternetAddressList *addresses;
	GMimeContentType *content_type;
	char *raw_value, *text = _g_mime_header_dup_raw_value (header);

	if (formatter == g_mime_header_format_addrlist) {
		if (!(addresses = _g_mime_internet_address_list_parse (header->options, text, -1)))
			return text;
		raw_value = format_address_list (header->name, options, addresses);
		g_object_unref (addresses);
		g_free (text);
		return raw_value;
	}

	if (formatter == g_mime_header_format_content_type) {
		if (!(content_type = g_mime_content_type_parse_full (header->options, text, -1, NULL)))
			return text;
		raw_value = g_mime_content_type_encode (content_type, options);
		g_object_unref (content_type);
		g_free (text);
		return raw_value;
	}

	if (formatter == g_mime_header_format_content_disposition) {
		if (!(disposition = g_mime_content_disposition_parse_full (header->options, text, -1, NULL)))
			return text;
		raw_value = g_mime_content_disposition_encode (disposition, options);
		g_object_unref (disposition);
		g_free (text);
		return raw_value;
	}

	/* Received headers must not be reformatted. These other structured fields
	 * have no text to internationalize, or must retain their original signature data. */
	if (formatter == g_mime_header_format_received ||
	    formatter == g_mime_header_format_message_id ||
	    formatter == g_mime_header_format_references ||
	    formatter == g_mime_header_format_disposition_notification_options ||
	    formatter == g_mime_header_format_dkim_signature ||
	    !g_ascii_strcasecmp (header->name, "Authentication-Results") ||
	    !g_ascii_strcasecmp (header->name, "ARC-Authentication-Results"))
		return text;

	g_free (text);

	if (formatter == g_mime_header_format_mailing_list_command)
		return format_mailing_list_command (options, header->name, g_mime_header_get_value (header), "utf-8");

	return fold_international_text (options, header->name, g_mime_header_get_value (header));
}


static void
received_skip_token (const char **in)
{
	while (**in && !is_lwsp (**in) && **in != '(' && **in != ';')
		(*in)++;
}

static void
received_skip_cfws (const char **in)
{
	skip_lwsp (in);
	while (**in == '(') {
		/* Unlike skip_cfws(), retain the consumed position for incomplete comments. */
		skip_comment (in);
		skip_lwsp (in);
	}
}

static void
append_received_token (GString *str, const char *start, const char *end,
		       const char *newline, size_t maxline, size_t *line)
{
	size_t length;

	while (end > start && is_lwsp (end[-1]))
		end--;

	length = end - start;
	if (str->len && *line + length + 1 > maxline) {
		g_string_append_printf (str, "%s\t", newline);
		*line = 1;
	} else if (!str->len || length != 1 || *start != ';') {
		g_string_append_c (str, ' ');
		(*line)++;
	}

	g_string_append_len (str, start, length);
	*line += length;
}


/**
 * g_mime_header_format_received:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Received header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 * Preserves clause and comment boundaries and keeps the date-time together.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_received (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	const char *inptr, *start, *newline;
	size_t line, maxline;
	char *unfolded;
	GString *str;
	
	newline = g_mime_format_options_get_newline (options);
	maxline = g_mime_format_options_get_max_line_length (options);
	line = g_bytes_get_size (header->raw_name) + 1;
	str = g_string_new (NULL);
	inptr = unfolded = g_mime_utils_header_unfold (value);

	/* Leading comments are independent tokens, not part of the first clause. */
	while (*inptr) {
		skip_lwsp (&inptr);
		if (*inptr != '(')
			break;

		start = inptr;
		skip_comment (&inptr);
		append_received_token (str, start, inptr, newline, maxline, &line);
	}

	/* Keep each clause keyword, its value and surrounding comments in one token. */
	while (*inptr && *inptr != ';') {
		start = inptr;

		received_skip_token (&inptr);
		received_skip_cfws (&inptr);

		if (*inptr && *inptr != ';') {
			received_skip_token (&inptr);
			received_skip_cfws (&inptr);
		}

		append_received_token (str, start, inptr, newline, maxline, &line);
	}

	if (*inptr == ';') {
		start = inptr++;

		append_received_token (str, start, inptr, newline, maxline, &line);
		skip_lwsp (&inptr);

		/* The date-time contains whitespace but must be folded as a single token. */
		if (*inptr)
			append_received_token (str, inptr, inptr + strlen (inptr), newline, maxline, &line);
	}

	g_string_append (str, newline);
	g_free (unfolded);

	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_newsgroups:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Newsgroups header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_newsgroups (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	/* https://www.rfc-editor.org/rfc/rfc5536#section-3.1.4
	 *
	 * The Newsgroups header field specifies the newsgroup(s) to which the
	 * article is posted.
	 *
	 * newsgroups      =  "Newsgroups:" SP newsgroup-list CRLF
	 *
	 * newsgroup-list  =  *WSP newsgroup-name
	 *                    *( [FWS] "," [FWS] newsgroup-name ) *WSP
	 *
	 * newsgroup-name  =  component *( "." component )
	 *
	 * component       =  1*component-char
	 *
	 * component-char  =  ALPHA / DIGIT / "+" / "-" / "_"
	 *
	 * Not all servers support optional FWS in the list of newsgroups.  In
	 * particular, folding the Newsgroups header field over several lines
	 * has been shown to harm propagation significantly.  Optional FWS in
	 * the <newsgroup-list> SHOULD NOT be generated, but MUST be accepted.
	 *
	 * Based on this, it seems better to avoid reformatting the value at all.
	 */
	const char *newline = g_mime_format_options_get_newline (options);
	
	return g_strdup_printf (" %s%s", value, newline);
}


/**
 * g_mime_header_format_disposition_notification_options:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Disposition-Notification-Options header value
 * @charset: a charset (note: unused)
 *
 * Removes whitespace within notification parameters and folds between
 * parameters using the configured line length and newline format.
 * Individual parameters are not split, and the first parameter is never
 * moved off the initial line, even if it exceeds the line-length target.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_disposition_notification_options (GMimeHeader *header, GMimeFormatOptions *options,
						      const char *value, const char *charset)
{
	const char *newline, *inptr, *start;
	size_t line, maxline;
	GString *str, *param;
	char *unfolded;

	newline = g_mime_format_options_get_newline (options);
	maxline = g_mime_format_options_get_max_line_length (options);
	line = g_bytes_get_size (header->raw_name) + 1;
	str = g_string_new (NULL);
	param = g_string_new (NULL);
	inptr = unfolded = g_mime_utils_header_unfold (value);

	while (*inptr) {
		start = inptr;

		while (*inptr && *inptr != '=')
			inptr++;

		while (*inptr && *inptr != ';')
			inptr++;

		if (*inptr == ';')
			inptr++;

		g_string_truncate (param, 0);
		while (start < inptr) {
			if (!is_lwsp (*start))
				g_string_append_c (param, *start);
			
			start++;
		}

		if (str->len && line + param->len + 1 > maxline) {
			g_string_append_printf (str, "%s\t", newline);
			line = 1;
		} else {
			g_string_append_c (str, ' ');
			line++;
		}

		g_string_append_len (str, param->str, param->len);
		line += param->len;
	}

	g_string_append (str, newline);
	g_string_free (param, TRUE);
	g_free (unfolded);

	return g_string_free (str, FALSE);
}


static void
append_signature_long_value (GString *str, const char *value, const char *newline, size_t maxline, size_t *line)
{
	size_t columns, count;
	const char *end;

	while (*value) {
		end = value;
		columns = 0;

		while (*end) {
			count = g_utf8_get_char (end) >= 0x10000 ? 2 : 1;
			if (*line >= maxline || count > maxline - *line - columns)
				break;

			columns += count;
			end = g_utf8_next_char (end);
		}

		g_string_append_len (str, value, end - value);
		*line += columns;
		value = end;

		if (*value) {
			g_string_append_printf (str, "%s\t", newline);
			*line = 1;
		}
	}
}

static void
append_signature_header_list (GString *str, const char *value, char delimiter,
			      const char *newline, size_t maxline, size_t *line)
{
	const char *end;
	size_t length;

	do {
		if (!(end = strchr (value, delimiter)))
			end = value + strlen (value);

		length = _g_mime_count_utf16_units (value, end);
		if (*line + length + 1 > maxline) {
			g_string_append_printf (str, "%s\t", newline);
			*line = 1;
		}

		g_string_append_len (str, value, end - value);
		*line += length;

		if (*end) {
			g_string_append_c (str, delimiter);
			(*line)++;
		}

		value = end + (*end != '\0');
	} while (*end);
}


/**
 * g_mime_header_format_dkim_signature:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a DKIM-Signature, ARC-Message-Signature, or ARC-Seal header value
 * @charset: a charset (note: unused)
 *
 * Compacts tag whitespace and folds using the configured line length and
 * newline format. Signature and body-hash tags start continuation lines.
 * Long h and z tags fold between list entries; other long tags are split.
 * UTF-8 text is retained and Unicode characters are never split.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_dkim_signature (GMimeHeader *header, GMimeFormatOptions *options,
				    const char *value, const char *charset)
{
	const char *inptr, *start, *name_end, *newline;
	size_t line, maxline, length, name_length;
	gboolean signature, list;
	GString *str, *token;
	char *unfolded;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (value, -1, NULL), NULL);

	newline = g_mime_format_options_get_newline (options);
	maxline = g_mime_format_options_get_max_line_length (options);
	line = g_bytes_get_size (header->raw_name) + 1;
	str = g_string_new (NULL);
	token = g_string_new (NULL);
	inptr = unfolded = g_mime_utils_header_unfold (value);

	while (*inptr) {
		skip_lwsp (&inptr);

		start = inptr;
		while (*inptr && *inptr != '=')
			inptr++;

		name_end = inptr;
		while (*inptr && *inptr != ';')
			inptr++;

		if (*inptr == ';')
			inptr++;

		/* Recognize the original tag name, before whitespace compaction. */
		name_length = name_end - start;
		signature = (name_length == 1 && *start == 'b') || (name_length == 2 && !memcmp (start, "bh", 2));
		list = name_length == 1 && (*start == 'h' || *start == 'z');

		g_string_truncate (token, 0);

		while (start < inptr) {
			if (!is_lwsp (*start))
				g_string_append_c (token, *start);
			
			start++;
		}

		length = _g_mime_count_utf16_units (token->str, token->str + token->len);
		if (line + length + 1 > maxline || signature) {
			g_string_append_printf (str, "%s\t", newline);
			line = 1;
		} else {
			g_string_append_c (str, ' ');
			line++;
		}

		if (length > maxline) {
			if (list)
				append_signature_header_list (str, token->str, token->str[0] == 'h' ? ':' : '|',
							      newline, maxline, &line);
			else
				append_signature_long_value (str, token->str, newline, maxline, &line);
		} else {
			g_string_append_len (str, token->str, token->len);
			line += length;
		}
	}

	g_string_append (str, newline);
	g_string_free (token, TRUE);
	g_free (unfolded);

	return g_string_free (str, FALSE);
}


static void
append_mailing_list_word (GString *str, const char *start, const char *end,
			 const char *newline, size_t maxline, size_t *line)
{
	size_t length = _g_mime_count_utf16_units (start, end), columns;
	const char *inptr;

	if (*line + length + 1 <= maxline) {
		g_string_append_c (str, ' ');
		g_string_append_len (str, start, end - start);
		*line += length + 1;
	} else if (length + 1 <= maxline) {
		g_string_append_printf (str, "%s ", newline);
		g_string_append_len (str, start, end - start);
		*line = length + 1;
	} else {
		while (start < end) {
			/* An oversized field prefix or preceding URL can fill the line. */
			if (*line >= maxline) {
				g_string_append (str, newline);
				*line = 0;
			}

			g_string_append_c (str, ' ');
			(*line)++;
			inptr = start;
			columns = 0;

			while (inptr < end) {
				size_t count = g_utf8_get_char (inptr) >= 0x10000 ? 2 : 1;

				if (count > maxline - *line - columns)
					break;
				columns += count;
				inptr = g_utf8_next_char (inptr);
			}

			g_string_append_len (str, start, inptr - start);
			*line += columns;
			start = inptr;

			if (start < end) {
				g_string_append (str, newline);
				*line = 0;
			}
		}
	}
}

static void
append_mailing_list_comment (GString *str, const char *comment, const char *newline,
			    size_t maxline, size_t *line)
{
	const char *start, *inptr = comment, *end = comment + strlen (comment);

	if (_g_mime_count_utf16_units (comment, end) + 1 <= maxline) {
		append_mailing_list_word (str, comment, end, newline, maxline, line);
		return;
	}

	/* Oversized comments can be split between words, compacting their whitespace. */
	while (inptr < end) {
		start = inptr;
		while (inptr < end && !is_lwsp (*inptr))
			inptr = g_utf8_next_char (inptr);
		append_mailing_list_word (str, start, inptr, newline, maxline, line);
		skip_lwsp (&inptr);
	}
}

static char *
format_mailing_list_command (GMimeFormatOptions *options, const char *field,
			    const char *value, const char *charset)
{
	const char *newline = g_mime_format_options_get_newline (options);
	size_t maxline = g_mime_format_options_get_max_line_length (options);
	size_t line = strlen (field) + 1, length;
	const char *inptr, *start;
	char *unfolded, *comment, *text, *encoded;
	GString *str;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (value, -1, NULL), NULL);

	inptr = unfolded = g_mime_utils_header_unfold (value);
	str = g_string_new (NULL);

	while (*inptr) {
		skip_lwsp (&inptr);
		if (!*inptr)
			break;

		start = inptr;
		if (*inptr == '<') {
			while (*inptr && *inptr != '>')
				inptr++;
			if (*inptr) {
				inptr++;
				/* Do not introduce breaks inside an intact angle-bracket URL. */
				length = _g_mime_count_utf16_units (start, inptr);
				if (line + length + 1 < maxline) {
					g_string_append_c (str, ' ');
					line++;
				} else {
					g_string_append_printf (str, "%s ", newline);
					line = 1;
				}
				g_string_append_len (str, start, inptr - start);
				line += length;
				continue;
			}
		} else if (*inptr == '(') {
			if (skip_comment (&inptr)) {
				if (g_mime_format_options_get_allow_international (options)) {
					comment = g_strndup (start, inptr - start);
				} else {
					text = g_strndup (start + 1, inptr - start - 2);
					encoded = g_mime_utils_header_encode_text (options, text, charset);
					g_free (text);
					/* The encoder retains all-blank input when there are no words. */
					start = encoded;
					while (is_blank (*start))
						start++;
					comment = g_strconcat ("(", start, ")", NULL);
					g_free (encoded);
				}
				append_mailing_list_comment (str, comment, newline, maxline, &line);
				g_free (comment);
				continue;
			}
		} else if (*inptr == ',') {
			/* Collapse repeated comma separators rather than emitting empty entries. */
			while (*inptr == ',' || is_lwsp (*inptr))
				inptr++;
			g_string_append_c (str, ',');
			line++;
			continue;
		}

		while (*inptr && !is_lwsp (*inptr) && *inptr != '<' && *inptr != '(' && *inptr != ',')
			inptr = g_utf8_next_char (inptr);
		append_mailing_list_word (str, start, inptr, newline, maxline, &line);
	}

	g_string_append (str, newline);
	g_free (unfolded);

	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_mailing_list_command:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a decoded UTF-8 mailing-list command header value
 * @charset: (nullable): the charset to use when encoding comments
 *
 * Formats List-Archive, List-Help, List-Owner, List-Post, List-Subscribe and
 * List-Unsubscribe values. Keeps angle-bracket URLs intact, collapses repeated
 * commas, and folds comments and words using UTF-16 column counts without
 * splitting Unicode characters. Non-International comments use RFC 2047
 * encoding; International comments retain their UTF-8 text.
 *
 * Returns: (transfer full) (nullable): the formatted value, or %NULL for invalid UTF-8.
 **/
char *
g_mime_header_format_mailing_list_command (GMimeHeader *header, GMimeFormatOptions *options,
					 const char *value, const char *charset)
{
	char *field = header_bytes_dup_text (header->raw_name);
	char *raw_value = format_mailing_list_command (options, field, value, charset);

	g_free (field);
	return raw_value;
}


static char *
fold_international_text (GMimeFormatOptions *options, const char *field, const char *value)
{
	const char *newline = g_mime_format_options_get_newline (options);
	size_t maxline = g_mime_format_options_get_max_line_length (options);
	size_t line = strlen (field) + 2, last_lwsp = G_MAXSIZE;
	const char *inptr, *start, *end;
	GString *folded;
	char *unfolded;
	size_t columns;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (value, -1, NULL), NULL);

	inptr = unfolded = g_mime_utils_header_unfold (value);
	folded = g_string_new (" ");

	while (*inptr) {
		start = inptr;

		if (is_lwsp (*inptr)) {
			while (is_lwsp (*inptr))
				inptr++;

			columns = inptr - start;

			if (line > maxline || columns > maxline - line) {
				while (start < inptr) {
					if (line > maxline) {
						g_string_append (folded, newline);
						line = 0;
					}

					g_string_append_c (folded, *start++);
					line++;
				}
			} else {
				g_string_append_len (folded, start, inptr - start);
				line += columns;
			}

			last_lwsp = folded->len - 1;
			continue;
		}

		columns = 0;
		while (*inptr && !is_lwsp (*inptr)) {
			columns += g_utf8_get_char (inptr) >= 0x10000 ? 2 : 1;
			inptr = g_utf8_next_char (inptr);
		}

		if (last_lwsp != G_MAXSIZE && (line > maxline || columns > maxline - line)) {
			g_string_insert (folded, last_lwsp, newline);
			last_lwsp = G_MAXSIZE;
			line = 1;
		}

		if (columns > maxline) {
			while (start < inptr) {
				end = start;
				columns = 0;

				while (end < inptr) {
					size_t count = g_utf8_get_char (end) >= 0x10000 ? 2 : 1;

					if (line >= maxline || count > maxline - line - columns)
						break;

					columns += count;
					end = g_utf8_next_char (end);
				}

				if (end == start) {
					/* A full header prefix or a surrogate pair may leave no room. */
					g_string_append (folded, newline);
					g_string_append_c (folded, ' ');
					line = 1;
					continue;
				}

				g_string_append_len (folded, start, end - start);
				line += columns;
				start = end;
			}
		} else {
			g_string_append_len (folded, start, inptr - start);
			line += columns;
		}
	}

	g_string_append (folded, newline);
	g_free (unfolded);

	return g_string_free (folded, FALSE);
}


/**
 * g_mime_header_format_default:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a header value
 * @charset: a charset to use when encoding the @value
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 * International formatting emits UTF-8 without RFC 2047 encoding and counts
 * UTF-16 units when folding, keeping each Unicode character intact.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_default (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	char *encoded, *raw_value, *field;
	
	field = header_bytes_dup_text (header->raw_name);
	if (g_mime_format_options_get_allow_international (options)) {
		raw_value = fold_international_text (options, field, value);
		g_free (field);
		return raw_value;
	}

	encoded = g_mime_utils_header_encode_text (options, value, charset);
	raw_value = _g_mime_utils_unstructured_header_fold (header->options, options, field, encoded);
	g_free (encoded);
	g_free (field);
	
	return raw_value;
}


static void
header_changed (GMimeHeader *header, gpointer user_args, GMimeHeaderList *list)
{
	GMimeHeaderListChangedEventArgs args;
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
	args.header = header;
	
	g_mime_event_emit (list->changed, &args);
}


static void g_mime_header_list_class_init (GMimeHeaderListClass *klass);
static void g_mime_header_list_init (GMimeHeaderList *list, GMimeHeaderListClass *klass);
static void g_mime_header_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


GType
g_mime_header_list_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeHeaderListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_header_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeHeaderList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_header_list_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeHeaderList", &info, 0);
	}
	
	return type;
}


static void
g_mime_header_list_class_init (GMimeHeaderListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_header_list_finalize;
}

static void
g_mime_header_list_init (GMimeHeaderList *list, GMimeHeaderListClass *klass)
{
	/* The lookup table references only the first header of each field name. */
	list->hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	list->changed = g_mime_event_new (list);
	list->array = g_ptr_array_new ();
	list->has_body_separator = TRUE;
}

static void
g_mime_header_list_finalize (GObject *object)
{
	GMimeHeaderList *headers = (GMimeHeaderList *) object;
	GMimeHeader *header;
	guint i;
	
	for (i = 0; i < headers->array->len; i++) {
		header = (GMimeHeader *) headers->array->pdata[i];
		g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
		g_object_unref (header);
	}
	
	g_ptr_array_free (headers->array, TRUE);
	
	g_mime_parser_options_free (headers->options);
	g_hash_table_destroy (headers->hash);
	g_mime_event_free (headers->changed);
	
	G_OBJECT_CLASS (list_parent_class)->finalize (object);
}


/**
 * g_mime_header_list_new:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Creates a new #GMimeHeaderList object.
 *
 * Returns: a new header list object.
 **/
GMimeHeaderList *
g_mime_header_list_new (GMimeParserOptions *options)
{
	GMimeHeaderList *headers;
	
	headers = g_object_new (GMIME_TYPE_HEADER_LIST, NULL);
	headers->options = g_mime_parser_options_clone (options);
	
	return headers;
}


/**
 * g_mime_header_list_load:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @stream: the input stream
 * @error: return location for a #GError, or %NULL
 *
 * Loads a header block from @stream using g_mime_parser_construct_headers().
 * The list owns its parsed strings and a snapshot of @options, and remains
 * usable after @stream is released. The stream is not closed or reset.
 *
 * The temporary parser may read ahead beyond the terminating blank line.
 * Use a #GMimeParser directly when the logical input position or subsequent
 * buffered input is needed. The header/body separator is not part of the list.
 *
 * Returns: (nullable) (transfer full): the loaded headers, or %NULL on failure.
 **/
GMimeHeaderList *
g_mime_header_list_load (GMimeParserOptions *options, GMimeStream *stream, GError **error)
{
	GMimeHeaderList *headers;
	GMimeParser *parser;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	parser = g_mime_parser_new_with_stream (stream);
	headers = g_mime_parser_construct_headers (parser, options, error);
	g_object_unref (parser);

	return headers;
}


/**
 * g_mime_header_list_load_file:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @filename: (type filename): the path to the input file
 * @error: return location for a #GError, or %NULL
 *
 * Opens @filename and loads its first header block using
 * g_mime_header_list_load(). Empty files return an empty list. The file is
 * closed on both success and failure, and the returned list owns its strings
 * and a snapshot of @options.
 *
 * File open and input I/O errors use the %GMIME_ERROR domain with a positive
 * errno code. Fatal syntax errors and header length violations use the same
 * errors as g_mime_parser_construct_headers().
 *
 * Returns: (nullable) (transfer full): the loaded headers, or %NULL on failure.
 **/
GMimeHeaderList *
g_mime_header_list_load_file (GMimeParserOptions *options, const char *filename, GError **error)
{
	GMimeHeaderList *headers;
	GMimeStream *stream;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(stream = g_mime_stream_fs_open (filename, O_RDONLY, 0, error)))
		return NULL;

	headers = g_mime_header_list_load (options, stream, error);
	g_object_unref (stream);

	return headers;
}


/**
 * g_mime_header_list_clear:
 * @headers: a #GMimeHeaderList
 *
 * Removes all of the headers from the #GMimeHeaderList.
 **/
void
g_mime_header_list_clear (GMimeHeaderList *headers)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	guint i;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	
	for (i = 0; i < headers->array->len; i++) {
		header = (GMimeHeader *) headers->array->pdata[i];
		g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
		g_object_unref (header);
	}
	
	g_hash_table_remove_all (headers->hash);
	
	g_ptr_array_set_size (headers->array, 0);
	headers->has_body_separator = TRUE;
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CLEARED;
	args.header = NULL;
	
	g_mime_event_emit (headers->changed, &args);
}


GMimeParserOptions *
_g_mime_header_list_get_options (GMimeHeaderList *headers)
{
	return headers->options;
}

void
_g_mime_header_list_set_options (GMimeHeaderList *headers, GMimeParserOptions *options)
{
	g_mime_parser_options_free (headers->options);
	headers->options = g_mime_parser_options_clone (options);
}


/**
 * g_mime_header_list_get_count:
 * @headers: a #GMimeHeaderList
 *
 * Gets the number of headers contained within the header list.
 *
 * Returns: the number of headers in the header list.
 **/
int
g_mime_header_list_get_count (GMimeHeaderList *headers)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	
	return headers->array->len;
}


/**
 * g_mime_header_list_contains:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Checks whether or not a header exists.
 *
 * Returns: %TRUE if the specified header exists or %FALSE otherwise.
 **/
gboolean
g_mime_header_list_contains (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	return TRUE;
}


/**
 * g_mime_header_list_contains_id:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 *
 * Checks whether a header with the canonical name for @id exists,
 * ignoring ASCII case. Unknown and out-of-range IDs are programming errors.
 * This does not decode values or notify owners.
 *
 * Returns: %TRUE if the specified header exists, or %FALSE otherwise.
 **/
gboolean
g_mime_header_list_contains_id (GMimeHeaderList *headers, GMimeHeaderId id)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, FALSE);

	return g_mime_header_list_contains (headers, g_mime_header_id_to_name (id));
}


/**
 * g_mime_header_list_contains_header:
 * @headers: a #GMimeHeaderList
 * @header: a #GMimeHeader
 *
 * Checks for the exact header instance, not another header with the same
 * name or value.
 *
 * Returns: %TRUE if @header is in the list, or %FALSE otherwise.
 **/
gboolean
g_mime_header_list_contains_header (GMimeHeaderList *headers, GMimeHeader *header)
{
	return g_mime_header_list_index_of_header (headers, header) != -1;
}


/**
 * g_mime_header_list_index_of:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Finds the first header with the specified name, ignoring ASCII case.
 *
 * Returns: the 0-based index of the first matching header, or -1 if not found.
 **/
int
g_mime_header_list_index_of (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	guint i;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (name != NULL, -1);

	for (i = 0; i < headers->array->len; i++) {
		header = headers->array->pdata[i];
		if (!g_ascii_strcasecmp (header->name, name))
			return i;
	}

	return -1;
}


/**
 * g_mime_header_list_index_of_id:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 *
 * Finds the first header with the specified cached identifier without
 * decoding values or comparing field-name strings. Unknown and out-of-range
 * IDs are programming errors.
 *
 * Returns: the 0-based index of the first matching header, or -1 if absent.
 **/
int
g_mime_header_list_index_of_id (GMimeHeaderList *headers, GMimeHeaderId id)
{
	GMimeHeader *header;
	guint i;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, -1);

	for (i = 0; i < headers->array->len; i++) {
		header = headers->array->pdata[i];
		if (header->id == id)
			return i;
	}

	return -1;
}


/**
 * g_mime_header_list_index_of_header:
 * @headers: a #GMimeHeaderList
 * @header: a #GMimeHeader
 *
 * Finds the first occurrence of the exact header instance.
 *
 * Returns: the 0-based index of @header, or -1 if not found.
 **/
int
g_mime_header_list_index_of_header (GMimeHeaderList *headers, GMimeHeader *header)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (GMIME_IS_HEADER (header), -1);

	for (i = 0; i < headers->array->len; i++) {
		if (headers->array->pdata[i] == header)
			return i;
	}

	return -1;
}


/**
 * g_mime_header_list_last_index_of:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Finds the last header with the specified name, ignoring ASCII case.
 *
 * Returns: the 0-based index of the last matching header, or -1 if not found.
 **/
int
g_mime_header_list_last_index_of (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	guint i;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (name != NULL, -1);

	for (i = headers->array->len; i > 0; i--) {
		header = headers->array->pdata[i - 1];
		if (!g_ascii_strcasecmp (header->name, name))
			return i - 1;
	}

	return -1;
}


/**
 * g_mime_header_list_last_index_of_id:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 *
 * Finds the last header with the specified cached identifier without
 * decoding values or comparing field-name strings. Unknown and out-of-range
 * IDs are programming errors.
 *
 * Returns: the 0-based index of the last matching header, or -1 if absent.
 **/
int
g_mime_header_list_last_index_of_id (GMimeHeaderList *headers, GMimeHeaderId id)
{
	GMimeHeader *header;
	guint i;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, -1);

	for (i = headers->array->len; i > 0; i--) {
		header = headers->array->pdata[i - 1];
		if (header->id == id)
			return i - 1;
	}

	return -1;
}


static void
header_ensure_owned_options (GMimeHeader *header)
{
	if (!header->owns_options) {
		header->options = g_mime_parser_options_clone (header->options);
		header->owns_options = TRUE;
	}
}


/**
 * g_mime_header_list_add:
 * @headers: a #GMimeHeaderList
 * @header: (transfer none): a #GMimeHeader
 *
 * Appends the existing header without copying or reformatting it. The list
 * takes its own reference. A header may be shared between lists or occur
 * more than once in a list; each occurrence retains a reference.
 *
 * Returns: the index of the added header, or -1 on invalid arguments.
 **/
int
g_mime_header_list_add (GMimeHeaderList *headers, GMimeHeader *header)
{
	int index;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (GMIME_IS_HEADER (header), -1);
	g_return_val_if_fail (header->name != NULL, -1);
	g_return_val_if_fail (headers->array->len < G_MAXINT, -1);

	index = headers->array->len;
	g_mime_header_list_insert_header (headers, index, header);

	return index;
}


/**
 * g_mime_header_list_insert_header:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based insertion index, from zero through the current count
 * @header: (transfer none): a #GMimeHeader
 *
 * Inserts the existing header, retaining its identity, raw text and stream
 * offset. The list takes a reference for each occurrence. Name lookup still
 * returns the first matching header.
 *
 * If @header borrowed its parser options from a list, it takes an independent
 * snapshot before insertion so it remains usable after that list is freed.
 * Other header state is unchanged. Changes notify every containing list until
 * its last occurrence is removed from that list.
 **/
void
g_mime_header_list_insert_header (GMimeHeaderList *headers, int index, GMimeHeader *header)
{
	GMimeHeaderListChangedEventArgs args;
	int first;

	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (GMIME_IS_HEADER (header));
	g_return_if_fail (header->name != NULL);
	g_return_if_fail (index >= 0);
	g_return_if_fail ((guint) index <= headers->array->len);
	g_return_if_fail (headers->array->len < G_MAXINT);

	header_ensure_owned_options (header);

	first = g_mime_header_list_index_of (headers, header->name);
	args.action = (guint) index == headers->array->len ?
		GMIME_HEADER_LIST_CHANGED_ACTION_ADDED : GMIME_HEADER_LIST_CHANGED_ACTION_INSERTED;
	args.header = header;
	g_object_ref (header);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_insert (headers->array, index, header);
	/* Update the lookup table to keep the first matching header. */
	if (first == -1 || index <= first)
		g_hash_table_replace (headers->hash, header->name, header);

	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
}


/**
 * g_mime_header_list_insert:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based insertion index, from zero through the current count
 * @name: header name
 * @value: (nullable): header value, or %NULL to reserve an unset header
 * @charset: (nullable): a charset
 *
 * Inserts a newly created header at @index, preserving the order of existing
 * headers. Name lookup continues to return the first matching header.
 * An index equal to the current count appends the header.
 **/
void
g_mime_header_list_insert (GMimeHeaderList *headers, int index, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	int first;

	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	g_return_if_fail (index >= 0);
	g_return_if_fail ((guint) index <= headers->array->len);

	if (index == 0) {
		g_mime_header_list_prepend (headers, name, value, charset);
		return;
	}

	if ((guint) index == headers->array->len) {
		g_mime_header_list_append (headers, name, value, charset);
		return;
	}

	header = header_new (headers->options, name, value, name, NULL, charset, -1);
	first = g_mime_header_list_index_of (headers, header->name);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_insert (headers->array, index, header);
	if (first == -1 || index <= first)
		g_hash_table_replace (headers->hash, header->name, header);

	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_INSERTED;
	args.header = header;
	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
}


/**
 * g_mime_header_list_insert_full:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based insertion index, from zero through the current count
 * @name: a nonempty ASCII header field name without whitespace or a colon
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs a header using g_mime_header_new_full() before inserting it.
 * The header owns an independent snapshot of the default parser options.
 * Existing headers retain their identity, values, offsets and relative order.
 * Name lookup continues to return the first matching header.
 *
 * Failure leaves the list, its body-separator state and notifications
 * unchanged. Invalid indexes, field names and %NULL values are programming
 * errors. Unlike g_mime_header_list_insert(), this does not reserve unset
 * headers or use legacy structured-value fallbacks.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_insert_full (GMimeHeaderList *headers, int index, const char *name, const char *value,
				const char *charset, GError **error)
{
	GMimeHeader *header;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (index >= 0, FALSE);
	g_return_val_if_fail ((guint) index <= headers->array->len, FALSE);
	g_return_val_if_fail (headers->array->len < G_MAXINT, FALSE);

	if (!(header = g_mime_header_new_full (name, value, charset, error)))
		return FALSE;

	g_mime_header_list_insert_header (headers, index, header);
	g_object_unref (header);

	return TRUE;
}


/**
 * g_mime_header_list_insert_id_full:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based insertion index, from zero through the current count
 * @id: a known #GMimeHeaderId
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs and inserts a checked header using the canonical field name
 * for @id and g_mime_header_list_insert_full(). Existing headers retain
 * their identity, raw bytes and relative order. Validation failure leaves
 * the list and its notifications unchanged. Unknown and out-of-range IDs
 * are programming errors.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_insert_id_full (GMimeHeaderList *headers, int index, GMimeHeaderId id,
				 const char *value, const char *charset, GError **error)
{
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, FALSE);

	return g_mime_header_list_insert_full (headers, index, g_mime_header_id_to_name (id), value, charset, error);
}


/**
 * g_mime_header_list_prepend_full:
 * @headers: a #GMimeHeaderList
 * @name: a nonempty ASCII header field name without whitespace or a colon
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs and prepends a checked header. This is equivalent to
 * g_mime_header_list_insert_full() with an index of zero, including its
 * validation, ownership and failure behavior.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_prepend_full (GMimeHeaderList *headers, const char *name, const char *value,
				 const char *charset, GError **error)
{
	return g_mime_header_list_insert_full (headers, 0, name, value, charset, error);
}


/**
 * g_mime_header_list_append_full:
 * @headers: a #GMimeHeaderList
 * @name: a nonempty ASCII header field name without whitespace or a colon
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs and appends a checked header. This is equivalent to
 * g_mime_header_list_insert_full() at the current count, including its
 * validation, ownership and failure behavior.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_append_full (GMimeHeaderList *headers, const char *name, const char *value,
				const char *charset, GError **error)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);

	return g_mime_header_list_insert_full (headers, headers->array->len, name, value, charset, error);
}


/**
 * g_mime_header_list_append_id_full:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs and appends a checked header using the canonical field name
 * for @id and g_mime_header_list_append_full(). Existing headers, including
 * duplicates, are unchanged. Unknown and out-of-range IDs are programming
 * errors. Failure leaves the list and its notifications unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_append_id_full (GMimeHeaderList *headers, GMimeHeaderId id,
				 const char *value, const char *charset, GError **error)
{
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, FALSE);

	return g_mime_header_list_append_full (headers, g_mime_header_id_to_name (id), value, charset, error);
}


/**
 * g_mime_header_list_prepend:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 * @charset: (nullable): a charset
 *
 * Prepends a header. If @value is %NULL, a space will be set aside
 * for it (useful for setting the order of headers before values can
 * be obtained for them) otherwise the header will be unset.
 **/
void
g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	unsigned char *dest, *src;
	GMimeHeader *header;
	guint n;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	header = header_new (headers->options, name, value, name, NULL, charset, -1);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_hash_table_replace (headers->hash, header->name, header);
	
	if (headers->array->len > 0) {
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_INSERTED;
		g_ptr_array_set_size (headers->array, headers->array->len + 1);
		
		dest = ((unsigned char *) headers->array->pdata) + sizeof (void *);
		src = (unsigned char *) headers->array->pdata;
		n = headers->array->len - 1;
		
		memmove (dest, src, (sizeof (void *) * n));
		headers->array->pdata[0] = header;
	} else {
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
		g_ptr_array_add (headers->array, header);
	}
	
	args.header = header;
	
	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
}


static void
header_list_append_raw (GMimeHeaderList *headers, const char *name, const char *raw_name,
			const char *raw_value, gint64 offset, gboolean explicit_raw_value)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	
	header = header_new (headers->options, name, NULL, raw_name, raw_value, NULL, offset);
	header->explicit_raw_value = explicit_raw_value;
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_add (headers->array, header);
	
	if (!g_hash_table_lookup (headers->hash, name))
		g_hash_table_insert (headers->hash, header->name, header);
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
	args.header = header;
	
	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
}

void
_g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *raw_name,
			    const char *raw_value, gint64 offset)
{
	header_list_append_raw (headers, name, raw_name, raw_value, offset, FALSE);
}


/**
 * g_mime_header_list_append:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 * @charset: (nullable): a charset
 *
 * Appends a header. If @value is %NULL, a space will be set aside for it
 * (useful for setting the order of headers before values can be
 * obtained for them) otherwise the header will be unset.
 **/
void
g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	header = header_new (headers->options, name, value, name, NULL, charset, -1);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_add (headers->array, header);
	
	if (!g_hash_table_lookup (headers->hash, name))
		g_hash_table_insert (headers->hash, header->name, header);
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
	args.header = header;
	
	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
}


/**
 * g_mime_header_list_get_header:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Gets the first header with the specified name.
 *
 * Returns: (transfer none): a #GMimeHeader for the specified @name.
 **/
GMimeHeader *
g_mime_header_list_get_header (GMimeHeaderList *headers, const char *name)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	return g_hash_table_lookup (headers->hash, name);
}


/**
 * g_mime_header_list_get_value:
 * @headers: a #GMimeHeaderList
 * @name: the case-insensitive header field name
 *
 * Gets the decoded value of the first matching header without changing
 * list order or notifying owners. Later matches are not considered, even
 * if the first is an unset legacy placeholder.
 *
 * Returns: (transfer none) (nullable): the first header's decoded value,
 * or %NULL if no match exists or the first header is unset. The value is
 * owned by the header and is invalidated when that header is changed or
 * released.
 **/
const char *
g_mime_header_list_get_value (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	header = g_hash_table_lookup (headers->hash, name);

	return header ? g_mime_header_get_value (header) : NULL;
}


/**
 * g_mime_header_list_get_value_by_id:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 *
 * Gets the decoded value of the first header with the canonical name for
 * @id, ignoring ASCII case. Like g_mime_header_list_get_value(), this does
 * not skip unset placeholders, change raw bytes or notify owners.
 * Unknown and out-of-range IDs are programming errors.
 *
 * Returns: (transfer none) (nullable): the first header's decoded value,
 * or %NULL if absent or unset. The value is owned by the header and is
 * invalidated when that header is changed or released.
 **/
const char *
g_mime_header_list_get_value_by_id (GMimeHeaderList *headers, GMimeHeaderId id)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, NULL);

	return g_mime_header_list_get_value (headers, g_mime_header_id_to_name (id));
}


/**
 * g_mime_header_list_set_value_full:
 * @headers: a #GMimeHeaderList
 * @name: the case-insensitive header field name
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Sets only the first matching header using g_mime_header_set_value_full(),
 * or appends a checked header using g_mime_header_list_append_full() if
 * absent. Existing matches retain their identity, names, parser options and
 * source offsets. Later duplicates are not removed or changed, unless they
 * are additional occurrences of the same header object.
 *
 * Existing-header changes notify all lists sharing that object and preserve
 * each list's body-separator state. Appending adds a body separator and
 * emits the normal add notification. Validation failure leaves header and
 * list state unchanged and emits no notification.
 *
 * Unlike g_mime_header_list_set(), this does not collapse duplicates.
 * An invalid parsed field name is not repaired; if no match exists, @name
 * must satisfy the checked constructor's field-name requirements.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_set_value_full (GMimeHeaderList *headers, const char *name, const char *value,
				   const char *charset, GError **error)
{
	GMimeHeader *header;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if ((header = g_hash_table_lookup (headers->hash, name)))
		return g_mime_header_set_value_full (header, NULL, value, charset, error);

	return g_mime_header_list_append_full (headers, name, value, charset, error);
}


/**
 * g_mime_header_list_set_value_by_id_full:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Sets only the first matching header, or appends a checked header with
 * the canonical field name for @id if absent. Like MimeKit's HeaderId
 * indexer, this preserves the first header's identity and field spelling
 * and does not collapse duplicates. Uses g_mime_header_list_set_value_full()
 * and its existing ownership, notification and atomic failure behavior.
 * Unknown and out-of-range IDs are programming errors.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_set_value_by_id_full (GMimeHeaderList *headers, GMimeHeaderId id,
				      const char *value, const char *charset, GError **error)
{
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, FALSE);

	return g_mime_header_list_set_value_full (headers, g_mime_header_id_to_name (id), value, charset, error);
}


static void
header_list_remove_duplicates (GMimeHeaderList *headers, GMimeHeader *header, guint first)
{
	GMimeHeader *duplicate;
	guint i;

	for (i = headers->array->len - 1; i > first; i--) {
		duplicate = headers->array->pdata[i];

		if (g_ascii_strcasecmp (header->name, duplicate->name) != 0)
			continue;

		g_mime_event_remove (duplicate->changed, (GMimeEventCallback) header_changed, headers);
		g_ptr_array_remove_index (headers->array, i);
		g_object_unref (duplicate);
		headers->has_body_separator = TRUE;
	}
}


void
_g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *raw_value)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	guint first;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	if ((header = g_hash_table_lookup (headers->hash, name))) {
		first = g_mime_header_list_index_of_header (headers, header);
		header_set_raw_value (header, g_bytes_new (raw_value, strlen (raw_value)));
		
		header_list_remove_duplicates (headers, header, first);
		
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
		args.header = header;
		
		g_mime_event_emit (headers->changed, &args);
	} else {
		header_list_append_raw (headers, name, name, raw_value, -1, TRUE);
	}
}


/**
 * g_mime_header_list_set:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 * @charset: (nullable): a charset
 *
 * Set the value of the specified header. If @value is %NULL and the
 * header, @name, had not been previously set, a space will be set
 * aside for it (useful for setting the order of headers before values
 * can be obtained for them) otherwise the header will be unset.
 *
 * Note: If there are multiple headers with the specified field name,
 * the first instance of the header will be replaced and further
 * instances will be removed.
 **/
void
g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	guint first;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	if ((header = g_hash_table_lookup (headers->hash, name))) {
		first = g_mime_header_list_index_of_header (headers, header);
		g_mime_header_set_value (header, NULL, value, charset);
		
		header_list_remove_duplicates (headers, header, first);
		
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
		args.header = header;
		
		g_mime_event_emit (headers->changed, &args);
	} else {
		g_mime_header_list_append (headers, name, value, charset);
	}
}


/**
 * g_mime_header_list_replace:
 * @headers: a #GMimeHeaderList
 * @header: (transfer none): the replacement #GMimeHeader
 *
 * Replaces all headers with the same field name, ignoring ASCII case, with
 * the exact @header instance at the first matching position. If no match
 * exists, @header is appended. Other fields retain their order.
 *
 * The list takes its own reference without copying or reformatting @header.
 * The replacement may already occur in this list or be shared with another
 * list. Borrowed parser options are snapshotted as with instance insertion.
 * Removed headers stop notifying this list; other containing lists are
 * unaffected.
 *
 * Replacing existing fields emits one normal change notification after the
 * list and name lookup have been updated. Appending emits an add notification.
 **/
void
g_mime_header_list_replace (GMimeHeaderList *headers, GMimeHeader *header)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *original;
	int first;

	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (GMIME_IS_HEADER (header));
	g_return_if_fail (header->name != NULL);

	if ((first = g_mime_header_list_index_of (headers, header->name)) == -1) {
		g_mime_header_list_add (headers, header);
		return;
	}

	header_ensure_owned_options (header);
	/* The replacement can be borrowed from a later duplicate being removed. */
	g_object_ref (header);
	original = headers->array->pdata[first];
	header_list_remove_duplicates (headers, header, first);
	g_mime_event_remove (original->changed, (GMimeEventCallback) header_changed, headers);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	headers->array->pdata[first] = header;
	g_hash_table_replace (headers->hash, header->name, header);

	headers->has_body_separator = TRUE;
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
	args.header = header;
	g_mime_event_emit (headers->changed, &args);
	g_object_unref (original);
}


/**
 * g_mime_header_list_replace_value_full:
 * @headers: a #GMimeHeaderList
 * @name: a nonempty ASCII header field name without whitespace or a colon
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs a header using g_mime_header_new_full() and replaces all
 * case-insensitive matches using g_mime_header_list_replace(). If no match
 * exists, the header is appended. The new header owns an independent
 * snapshot of the default parser options.
 *
 * Unlike g_mime_header_list_set(), this replaces the first matching object
 * instead of changing its value. Retained references and other lists sharing
 * removed headers are unaffected. All validation precedes mutation; failure
 * leaves the list, its body-separator state and notifications unchanged.
 * Successful replacement uses the existing single change notification,
 * after the final list and name lookup are established.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_replace_value_full (GMimeHeaderList *headers, const char *name, const char *value,
				       const char *charset, GError **error)
{
	GMimeHeader *header;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (headers->array->len < G_MAXINT || g_mime_header_list_contains (headers, name), FALSE);

	if (!(header = g_mime_header_new_full (name, value, charset, error)))
		return FALSE;

	g_mime_header_list_replace (headers, header);
	g_object_unref (header);

	return TRUE;
}


/**
 * g_mime_header_list_replace_value_by_id_full:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 * @value: the decoded UTF-8 header value
 * @charset: (nullable): the encoding charset, or %NULL for UTF-8
 * @error: return location for an error, or %NULL
 *
 * Constructs a checked header with the canonical field name for @id and
 * replaces all matching headers using g_mime_header_list_replace_value_full().
 * Unlike assignment, replacement installs a new header object and removes
 * duplicates. If absent, the new header is appended. Existing native owner
 * notifications and atomic failure behavior are preserved. Unknown and
 * out-of-range IDs are programming errors.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_header_list_replace_value_by_id_full (GMimeHeaderList *headers, GMimeHeaderId id,
					  const char *value, const char *charset, GError **error)
{
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, FALSE);

	return g_mime_header_list_replace_value_full (headers, g_mime_header_id_to_name (id), value, charset, error);
}


/**
 * g_mime_header_list_get_header_at:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based index of the header
 *
 * Gets the header at the specified @index within the list.
 *
 * Returns: (transfer none): the header at position @index.
 **/
GMimeHeader *
g_mime_header_list_get_header_at (GMimeHeaderList *headers, int index)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= headers->array->len)
		return NULL;
	
	return (GMimeHeader *) headers->array->pdata[index];
}


static void
header_list_update_lookup (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	int first;

	if ((first = g_mime_header_list_index_of (headers, name)) == -1) {
		g_hash_table_remove (headers->hash, name);
	} else {
		header = headers->array->pdata[first];
		g_hash_table_replace (headers->hash, header->name, header);
	}
}


/**
 * g_mime_header_list_set_header_at:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based index of the header to replace
 * @header: (transfer none): the replacement #GMimeHeader
 *
 * Replaces just the header at @index, without changing the list length or
 * removing duplicate fields. Assigning the instance already at @index does
 * nothing. Invalid indexes are programming errors reported by GLib's
 * argument guards without changing the list or @header.
 *
 * The list retains its own reference and preserves @header's raw text and
 * stream offset. Borrowed parser options are snapshotted as with insertion.
 * Name lookup continues to identify the first matching occurrence, even if
 * either instance occurs elsewhere in the list.
 *
 * Assigning a matching field name emits a change notification. Assigning a
 * different field name emits removal of the old header and addition of the
 * new header after the list and name lookups have been updated.
 **/
void
g_mime_header_list_set_header_at (GMimeHeaderList *headers, int index, GMimeHeader *header)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *original;
	gboolean same_name;

	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (GMIME_IS_HEADER (header));
	g_return_if_fail (header->name != NULL);
	g_return_if_fail (index >= 0);
	g_return_if_fail ((guint) index < headers->array->len);

	original = headers->array->pdata[index];
	if (original == header)
		return;

	same_name = !g_ascii_strcasecmp (original->name, header->name);
	header_ensure_owned_options (header);
	g_object_ref (header);
	g_mime_event_remove (original->changed, (GMimeEventCallback) header_changed, headers);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	headers->array->pdata[index] = header;
	header_list_update_lookup (headers, original->name);
	if (!same_name)
		header_list_update_lookup (headers, header->name);

	headers->has_body_separator = TRUE;
	if (same_name) {
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
		args.header = header;
		g_mime_event_emit (headers->changed, &args);
	} else {
		g_object_ref (header);
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED;
		args.header = original;
		g_mime_event_emit (headers->changed, &args);
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
		args.header = header;
		g_mime_event_emit (headers->changed, &args);
		g_object_unref (header);
	}
	g_object_unref (original);
}


/**
 * g_mime_header_list_remove:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Remove the first instance of the specified header.
 *
 * Returns: %TRUE if the header was successfully removed or %FALSE if
 * the specified header could not be found.
 **/
gboolean
g_mime_header_list_remove (GMimeHeaderList *headers, const char *name)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header, *hdr;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	/* get the index of the header */
	for (i = 0; i < headers->array->len; i++) {
		if (headers->array->pdata[i] == header)
			break;
	}
	
	g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_remove_index (headers->array, i);
	g_hash_table_remove (headers->hash, name);

	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED;
	args.header = header;
	
	/* look for another header with the same name... */
	while (i < headers->array->len) {
		hdr = (GMimeHeader *) headers->array->pdata[i];
		
		if (!g_ascii_strcasecmp (hdr->name, name)) {
			/* enter this node into the lookup table */
			g_hash_table_insert (headers->hash, hdr->name, hdr);
			break;
		}
		
		i++;
	}
	
	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
	g_object_unref (header);
	
	return TRUE;
}


/**
 * g_mime_header_list_remove_id:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 *
 * Removes the first header with the canonical name for @id, ignoring
 * ASCII case. Uses the normal removal and owner-notification paths and
 * preserves the other headers' order and raw bytes. Does nothing if
 * absent. Unknown and out-of-range IDs are programming errors.
 *
 * Returns: %TRUE if the first occurrence was removed, or %FALSE if absent.
 **/
gboolean
g_mime_header_list_remove_id (GMimeHeaderList *headers, GMimeHeaderId id)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS, FALSE);

	return g_mime_header_list_remove (headers, g_mime_header_id_to_name (id));
}


/**
 * g_mime_header_list_remove_header:
 * @headers: a #GMimeHeaderList
 * @header: a #GMimeHeader
 *
 * Removes the first occurrence of the exact header instance. A different
 * header with the same name and value is not removed.
 *
 * Returns: %TRUE if an occurrence was removed, or %FALSE if absent.
 **/
gboolean
g_mime_header_list_remove_header (GMimeHeaderList *headers, GMimeHeader *header)
{
	int index;

	if ((index = g_mime_header_list_index_of_header (headers, header)) == -1)
		return FALSE;

	g_mime_header_list_remove_at (headers, index);

	return TRUE;
}


/**
 * g_mime_header_list_remove_all:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Removes every header with the specified name, ignoring ASCII case.
 * Remaining headers retain their order and raw values. Each removed header
 * emits the normal removal notification and stops notifying the list.
 * Does nothing if no matching headers exist.
 **/
void
g_mime_header_list_remove_all (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	char *field;
	guint i;

	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);

	if (!g_hash_table_contains (headers->hash, name))
		return;

	/* The caller may borrow the name of a header being removed. */
	field = g_strdup (name);
	for (i = headers->array->len; i > 0; i--) {
		header = headers->array->pdata[i - 1];
		if (!g_ascii_strcasecmp (header->name, field))
			g_mime_header_list_remove_at (headers, i - 1);
	}
	g_free (field);
}


/**
 * g_mime_header_list_remove_all_id:
 * @headers: a #GMimeHeaderList
 * @id: a known #GMimeHeaderId
 *
 * Removes every header with the canonical name for @id, ignoring ASCII
 * case. Each occurrence emits the normal removal notification; surviving
 * headers retain their order and raw bytes. Does nothing if absent.
 * Unknown and out-of-range IDs are programming errors.
 **/
void
g_mime_header_list_remove_all_id (GMimeHeaderList *headers, GMimeHeaderId id)
{
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (id >= GMIME_HEADER_ACCEPT_LANGUAGE && id <= GMIME_HEADER_X_STATUS);

	g_mime_header_list_remove_all (headers, g_mime_header_id_to_name (id));
}


/**
 * g_mime_header_list_remove_at:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based index of the header to remove
 *
 * Removes the header at the specified @index from @headers.
 **/
void
g_mime_header_list_remove_at (GMimeHeaderList *headers, int index)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (index >= 0);
	
	if ((guint) index >= headers->array->len)
		return;
	
	header = (GMimeHeader *) headers->array->pdata[index];
	g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_remove_index (headers->array, index);
	
	/* The same object can occur earlier as well as later in the list. */
	if (g_hash_table_lookup (headers->hash, header->name) == header)
		header_list_update_lookup (headers, header->name);
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED;
	args.header = header;
	
	headers->has_body_separator = TRUE;
	g_mime_event_emit (headers->changed, &args);
	g_object_unref (header);
}


/**
 * g_mime_header_list_write_to_stream:
 * @headers: a #GMimeHeaderList
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @stream: output stream
 *
 * Writes the headers followed by the header/body separator. A parsed list
 * without a separator omits it until a structural edit is made. Changing a
 * contained header's value does not change this state.
 * Hidden headers are only omitted when writing merged message headers,
 * not standalone header lists or descendant MIME entity headers.
 *
 * Newline conversion is completed before the separator is written. The
 * caller's stream is not closed.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeFormatOptions *options, GMimeStream *stream)
{
	ssize_t nwritten, total;
	const char *newline;

	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);

	if ((total = _g_mime_header_list_write_to_stream (headers, options, stream)) == -1)
		return -1;

	if (headers->has_body_separator) {
		newline = g_mime_format_options_get_newline (options);
		if (strlen (newline) > (size_t) (G_MAXSSIZE - total)) {
			errno = EOVERFLOW;
			return -1;
		}
		if ((nwritten = _g_mime_stream_write_all (stream, newline, strlen (newline))) == -1)
			return -1;
		total += nwritten;
	}

	return total;
}


ssize_t
_g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeFormatOptions *options, GMimeStream *stream)
{
	ssize_t total = -1;
	GMimeStream *filtered;
	GMimeHeader *header;
	GMimeFilter *filter;
	int errsv;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_format_options_create_newline_filter (options, FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	for (i = 0; i < headers->array->len; i++) {
		header = (GMimeHeader *) headers->array->pdata[i];
		
		if (g_mime_header_write_to_stream (header, options, filtered) == -1)
			goto done;
	}
	
	if (g_mime_stream_flush (filtered) == 0)
		total = _g_mime_stream_filter_get_written ((GMimeStreamFilter *) filtered);

done:
	errsv = errno;
	g_object_unref (filtered);
	errno = errsv;
	
	return total;
}


/**
 * g_mime_header_list_to_string:
 * @headers: a #GMimeHeaderList
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Allocates a string buffer containing the raw rfc822 headers
 * contained in @headers, without the header/body separator.
 *
 * Returns: (nullable): a string containing the header block, or %NULL on failure.
 **/
char *
g_mime_header_list_to_string (GMimeHeaderList *headers, GMimeFormatOptions *options)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array ((GMimeStreamMem *) stream, array);
	if (_g_mime_header_list_write_to_stream (headers, options, stream) == -1) {
		int errsv = errno;

		g_object_unref (stream);
		g_byte_array_free (array, TRUE);
		errno = errsv;
		return NULL;
	}
	g_object_unref (stream);
	
	g_byte_array_append (array, (unsigned char *) "", 1);
	str = (char *) array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}
