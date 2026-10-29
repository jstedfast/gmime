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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>

#include "gmime-anonymizer.h"
#include "gmime-common.h"
#include "gmime-error.h"
#include "gmime-filter-anonymize.h"
#include "gmime-internal.h"
#include "gmime-message-part.h"
#include "gmime-message-delivery-status.h"
#include "gmime-message-disposition-notification.h"
#include "gmime-stream-filter.h"
#include "gmime-table-private.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-anonymizer
 * @title: GMimeAnonymizer
 * @short_description: Structure-preserving MIME anonymization
 *
 * Masks header values without decoding their raw bytes. Address delimiters,
 * encoded-word charset/encoding labels, Received keywords and valid dates,
 * content types and selected MIME parameters remain visible, matching MimeKit.
 * Date, MIME-Version and Content-Transfer-Encoding values are preserved.
 *
 * This is a diagnostic transformation, not a guarantee of removing all
 * sensitive information. Preserved fields, dates, structural syntax, escape
 * sequences, byte counts and whitespace can reveal information. Message and
 * entity bodies are masked after transfer encoding, preserving their wire
 * whitespace rather than decoding sensitive content into the output.
 **/

G_DEFINE_FINAL_TYPE (GMimeAnonymizer, g_mime_anonymizer, G_TYPE_OBJECT)

static void
g_mime_anonymizer_finalize (GObject *object)
{
	GMimeAnonymizer *anonymizer = GMIME_ANONYMIZER (object);

	g_hash_table_unref (anonymizer->preserve_headers);
	G_OBJECT_CLASS (g_mime_anonymizer_parent_class)->finalize (object);
}

static void
g_mime_anonymizer_class_init (GMimeAnonymizerClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_anonymizer_finalize;
}

static void
g_mime_anonymizer_init (GMimeAnonymizer *anonymizer)
{
	anonymizer->preserve_headers = g_hash_table_new_full (g_mime_strcase_hash, g_mime_strcase_equal, g_free, NULL);
}

/**
 * g_mime_anonymizer_new:
 *
 * Returns: (transfer full): a new anonymizer with no extra preserved fields.
 **/
GMimeAnonymizer *
g_mime_anonymizer_new (void)
{
	return g_object_new (GMIME_TYPE_ANONYMIZER, NULL);
}

/**
 * g_mime_anonymizer_add_preserved_header:
 * @anonymizer: a #GMimeAnonymizer
 * @name: a field name to preserve
 *
 * Copies @name without trimming. Field matching ignores ASCII case.
 * Adding an existing name retains its original spelling.
 *
 * Returns: whether a name was added.
 **/
gboolean
g_mime_anonymizer_add_preserved_header (GMimeAnonymizer *anonymizer, const char *name)
{
	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), FALSE);
	g_return_val_if_fail (name != NULL && g_utf8_validate (name, -1, NULL), FALSE);

	if (g_hash_table_contains (anonymizer->preserve_headers, name))
		return FALSE;

	g_hash_table_add (anonymizer->preserve_headers, g_strdup (name));

	return TRUE;
}

/**
 * g_mime_anonymizer_remove_preserved_header:
 * @anonymizer: a #GMimeAnonymizer
 * @name: a field name
 *
 * Returns: whether a name was removed, ignoring ASCII case.
 **/
gboolean
g_mime_anonymizer_remove_preserved_header (GMimeAnonymizer *anonymizer, const char *name)
{
	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return g_hash_table_remove (anonymizer->preserve_headers, name);
}

/**
 * g_mime_anonymizer_contains_preserved_header:
 * @anonymizer: a #GMimeAnonymizer
 * @name: a field name
 *
 * Tests only the configured set, not the fields preserved by default.
 *
 * Returns: whether @name is in the configured set, ignoring ASCII case.
 **/
gboolean
g_mime_anonymizer_contains_preserved_header (GMimeAnonymizer *anonymizer, const char *name)
{
	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return g_hash_table_contains (anonymizer->preserve_headers, name);
}

/**
 * g_mime_anonymizer_clear_preserved_headers:
 * @anonymizer: a #GMimeAnonymizer
 *
 * Clears the configured set without changing the default preserved fields.
 **/
void
g_mime_anonymizer_clear_preserved_headers (GMimeAnonymizer *anonymizer)
{
	g_return_if_fail (GMIME_IS_ANONYMIZER (anonymizer));

	g_hash_table_remove_all (anonymizer->preserve_headers);
}

/**
 * g_mime_anonymizer_get_preserved_headers:
 * @anonymizer: a #GMimeAnonymizer
 *
 * Returns an independent snapshot in unspecified order.
 *
 * Returns: (array zero-terminated=1) (transfer full): the configured names.
 * Free with g_strfreev().
 **/
char **
g_mime_anonymizer_get_preserved_headers (GMimeAnonymizer *anonymizer)
{
	GHashTableIter iter;
	gpointer key;
	char **names;
	guint i = 0;

	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), NULL);

	names = g_new0 (char *, (gsize) g_hash_table_size (anonymizer->preserve_headers) + 1);

	g_hash_table_iter_init (&iter, anonymizer->preserve_headers);

	while (g_hash_table_iter_next (&iter, &key, NULL))
		names[i++] = g_strdup (key);

	return names;
}

static gboolean
is_preserved_byte (guint8 c, const char *specials)
{
	return c != 0 && strchr (specials, c) != NULL;
}

static gboolean
is_received_keyword (const guint8 *raw, gsize length, gsize index, gsize *word_length)
{
	char word[5];
	gsize n = 0;

	/* look for: from, by, via, with, id, for */
	while (index < length && n < 4 && !is_lwsp (raw[index])) {
		if (!g_ascii_isalpha (raw[index]))
			return FALSE;

		word[n++] = g_ascii_tolower (raw[index++]);
	}

	if (index >= length || !is_lwsp (raw[index]))
		return FALSE;

	word[n] = 0;
	*word_length = n;

	return !strcmp (word, "from") || !strcmp (word, "by") || !strcmp (word, "via") ||
		!strcmp (word, "with") || !strcmp (word, "id") || !strcmp (word, "for");
}

static void
anonymize_received (const guint8 *raw, guint8 *output, gsize length)
{
	gsize index = 0, word_length;
	GDateTime *date;

	while (index < length) {
		while (index < length && is_lwsp (raw[index])) {
			output[index] = raw[index];
			index++;
		}

		if (index >= length)
			break;

		if (raw[index] == '(') {
			/* comment */
			gsize depth = 1;
			gboolean escaped = FALSE;

			/* consume the '(' */
			output[index] = raw[index];
			index++;

			while (index < length && depth > 0) {
				if (raw[index] == '\\') {
					output[index] = raw[index];
					escaped = !escaped;
				} else if (!escaped) {
					if (raw[index] == '(') {
						output[index] = raw[index];
						depth++;
					} else if (raw[index] == ')') {
						output[index] = raw[index];
						depth--;
					} else if (is_lwsp (raw[index]) || is_preserved_byte (raw[index], "<>[]:@,.")) {
						output[index] = raw[index];
					} else {
						/* anonymize everything else in the comment */
						output[index] = 'x';
					}
				} else {
					/* escaped */
					output[index] = 'x';
					escaped = FALSE;
				}

				index++;
			}
		} else if (raw[index] == ';') {
			output[index] = raw[index];
			index++;

			while (index < length && is_lwsp (raw[index])) {
				output[index] = raw[index];
				index++;
			}

			/* This might be a date... */
			if ((date = _g_mime_utils_header_decode_date ((const char *) raw + index, length - index))) {
				/* don't anonymize the date */
				memcpy (output + index, raw + index, length - index);
				g_date_time_unref (date);
				break;
			}
		} else if (is_received_keyword (raw, length, index, &word_length)) {
			memcpy (output + index, raw + index, word_length);
			index += word_length;
		} else {
			while (index < length) {
				if (is_lwsp (raw[index]) || raw[index] == ';' || raw[index] == '(')
					break;
				output[index] = is_preserved_byte (raw[index], "<>[]:@,.") ? raw[index] : 'x';
				index++;
			}
		}
	}
}

typedef enum {
	WORD_NONE,
	WORD_EQUALS,
	WORD_EQUALS_QUESTION,
	WORD_CHARSET,
	WORD_CHARSET_QUESTION,
	WORD_ENCODING,
	WORD_ENCODING_QUESTION,
	WORD_PAYLOAD,
	WORD_PAYLOAD_QUESTION
} EncodedWordState;

static void
push_encoded_word_byte (EncodedWordState *state, guint8 c, gsize *index, guint8 *output, const char *specials)
{
	if (c == '=') {
		switch (*state) {
		case WORD_NONE:
			*state = WORD_EQUALS;
			output[*index] = c;
			break;
		case WORD_PAYLOAD_QUESTION:
			*state = WORD_NONE;
			output[*index] = c;
			break;
		case WORD_PAYLOAD:
			/* anonymize '=' in the payload */
			output[*index] = 'x';
			break;
		default:
			/* break out of rfc2047 encoded-word mode */
			*state = WORD_NONE;
			output[*index] = c;
			break;
		}
	} else if (c == '?') {
		output[*index] = c;

		switch (*state) {
		case WORD_EQUALS: *state = WORD_EQUALS_QUESTION; break;
		case WORD_CHARSET: *state = WORD_CHARSET_QUESTION; break;
		case WORD_ENCODING: *state = WORD_ENCODING_QUESTION; break;
		case WORD_PAYLOAD: *state = WORD_PAYLOAD_QUESTION; break;
		case WORD_NONE:
			/* just a normal question mark */
			break;
		default:
			/* break out of rfc2047 encoded-word mode */
			*state = WORD_NONE;
			break;
		}
	} else {
		switch (*state) {
		case WORD_EQUALS_QUESTION:
		case WORD_CHARSET:
			/* allow charset name to pass through... */
			*state = WORD_CHARSET;
			output[*index] = c;
			break;
		case WORD_CHARSET_QUESTION:
		case WORD_ENCODING:
			/* allow encoding name to pass through... */
			*state = WORD_ENCODING;
			output[*index] = c;
			break;
		case WORD_ENCODING_QUESTION:
		case WORD_PAYLOAD:
			/* anonymize everything but whitespace in the rfc2047 encoded-word payload
			 * mostly in case of folding whitespace but also because it can screw up
			 * tokenization in MIME parsers and we want to be able to see that. */
			*state = WORD_PAYLOAD;
			output[*index] = is_lwsp (c) ? c : 'x';
			break;
		case WORD_NONE:
			output[*index] = is_preserved_byte (c, specials) ? c : 'x';
			break;
		default:
			/* break out of rfc2047 encoded-word mode */
			*state = WORD_NONE;
			/* rewind 1 character */
			(*index)--;
			break;
		}
	}
}

static void
anonymize_text (const guint8 *raw, guint8 *output, gsize length, gboolean address)
{
	gboolean escaped = FALSE, quoted = FALSE;
	EncodedWordState state = WORD_NONE;
	guint8 c;
	gsize i;

	for (i = 0; i < length; i++) {
		c = raw[i];

		if (address && c == '\\') {
			output[i] = c;
			escaped = !escaped;
		} else if (address && c == '"') {
			output[i] = c;

			if (escaped)
				escaped = FALSE;
			else
				quoted = !quoted;
		} else if (address && escaped) {
			output[i] = c;
			escaped = FALSE;
		} else if (address && quoted) {
			/* anonymize everything but folding whitespace within a quoted string */
			output[i] = c == '\r' || c == '\n' ? c : 'x';
		} else {
			push_encoded_word_byte (&state, c, &i, output, address ? " \t\r\n()<>[]:;@,." : " \t\r\n");
		}
	}
}

typedef enum {
	PARAM_SEMICOLON,
	PARAM_NAME,
	PARAM_NAME_STAR,
	PARAM_VALUE
} ParameterState;

static gboolean
is_safe_parameter_name (GString *name)
{
	return (name->len == 8 && !g_ascii_strncasecmp (name->str, "boundary", 8)) ||
		(name->len == 7 && !g_ascii_strncasecmp (name->str, "charset", 7)) ||
		(name->len == 5 && !g_ascii_strncasecmp (name->str, "delsp", 5)) ||
		(name->len == 6 && !g_ascii_strncasecmp (name->str, "format", 6));
}

static void
anonymize_parameters (const guint8 *raw, guint8 *output, gsize length)
{
	gboolean escaped = FALSE, quoted = FALSE, safe = FALSE;
	ParameterState state = PARAM_SEMICOLON;
	GString *name = g_string_new (NULL);
	gsize index = 0;

	/* don't anonymize the mime-type or the "attachment" or "inline" part */
	while (index < length && raw[index] != ';') {
		output[index] = raw[index];
		index++;
	}

	if (index < length) {
		output[index] = raw[index];
		index++;
	}

	while (index < length) {
		switch (state) {
		case PARAM_SEMICOLON:
			if (raw[index] == ';') {
				/* multiple semicolons in a row */
				output[index] = raw[index];
				break;
			}
			if (is_lwsp (raw[index])) {
				/* whitespace character */
				output[index] = raw[index];
				break;
			}
			state = PARAM_NAME;
			G_GNUC_FALLTHROUGH;
		case PARAM_NAME:
			output[index] = raw[index];

			if (raw[index] == '=') {
				safe = is_safe_parameter_name (name);
				state = PARAM_VALUE;
			} else if (raw[index] == ';') {
				/* shouldn't happen... */
				state = PARAM_SEMICOLON;
				g_string_truncate (name, 0);
			} else if (raw[index] == '*') {
				state = PARAM_NAME_STAR;
			} else {
				g_string_append_c (name, raw[index]);
			}
			break;
		case PARAM_NAME_STAR:
			output[index] = raw[index];

			if (raw[index] == '=') {
				safe = is_safe_parameter_name (name);
				state = PARAM_VALUE;
			} else if (raw[index] == ';') {
				/* parameter seems to be incomplete? */
				state = PARAM_SEMICOLON;
			}
			break;
		case PARAM_VALUE:
			if (raw[index] == '"') {
				output[index] = raw[index];
				if (escaped)
					escaped = FALSE;
				else
					quoted = !quoted;
			} else if (quoted) {
				if (raw[index] == '\\') {
					output[index] = raw[index];
					escaped = !escaped;
				} else if (raw[index] == '\r' || raw[index] == '\n') {
					/* don't anonymize folding whitespace within a quoted value */
					output[index] = raw[index];
					escaped = FALSE;
				} else {
					output[index] = safe ? raw[index] : 'x';
					escaped = FALSE;
				}
			} else if (raw[index] == ';') {
				output[index] = raw[index];
				state = PARAM_SEMICOLON;
				g_string_truncate (name, 0);
			} else {
				output[index] = is_lwsp (raw[index]) || safe ? raw[index] : 'x';
			}
			break;
		}

		index++;
	}

	g_string_free (name, TRUE);
}

static GBytes *
anonymize_header (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options, GMimeHeader *header)
{
	GBytes *value = _g_mime_header_get_formatted_value (header, options);
	const guint8 *raw;
	guint8 *output;
	gsize length;

	if (!value)
		return NULL;

	if (g_hash_table_contains (anonymizer->preserve_headers, header->name)) {
		/* don't anonymize this header */
		return value;
	}

	switch (header->id) {
	case GMIME_HEADER_CONTENT_TRANSFER_ENCODING:
	case GMIME_HEADER_MIME_VERSION:
	case GMIME_HEADER_DATE:
		/* don't anonymize these */
		return value;
	default:
		break;
	}

	raw = g_bytes_get_data (value, &length);
	output = g_malloc0 (length);

	switch (header->id) {
	case GMIME_HEADER_DISPOSITION_NOTIFICATION_TO:
	case GMIME_HEADER_RESENT_REPLY_TO:
	case GMIME_HEADER_RESENT_SENDER:
	case GMIME_HEADER_RESENT_FROM:
	case GMIME_HEADER_RESENT_BCC:
	case GMIME_HEADER_RESENT_CC:
	case GMIME_HEADER_RESENT_TO:
	case GMIME_HEADER_REPLY_TO:
	case GMIME_HEADER_SENDER:
	case GMIME_HEADER_FROM:
	case GMIME_HEADER_BCC:
	case GMIME_HEADER_CC:
	case GMIME_HEADER_TO:
		anonymize_text (raw, output, length, TRUE);
		break;
	case GMIME_HEADER_RECEIVED:
		anonymize_received (raw, output, length);
		break;
	case GMIME_HEADER_ORIGINAL_MESSAGE_ID:
	case GMIME_HEADER_RESENT_MESSAGE_ID:
	case GMIME_HEADER_REFERENCES:
	case GMIME_HEADER_IN_REPLY_TO:
	case GMIME_HEADER_MESSAGE_ID:
	case GMIME_HEADER_CONTENT_ID:
		/* We'll treat these like address headers (for now)... */
		anonymize_text (raw, output, length, TRUE);
		break;
	case GMIME_HEADER_CONTENT_DISPOSITION:
	case GMIME_HEADER_CONTENT_TYPE:
		anonymize_parameters (raw, output, length);
		break;
	case GMIME_HEADER_ARC_AUTHENTICATION_RESULTS:
	case GMIME_HEADER_AUTHENTICATION_RESULTS:
	case GMIME_HEADER_ARC_MESSAGE_SIGNATURE:
	case GMIME_HEADER_ARC_SEAL:
	case GMIME_HEADER_DKIM_SIGNATURE:
		/* TODO: should we have custom logic for anonymizing these? */
		G_GNUC_FALLTHROUGH;
	default:
		anonymize_text (raw, output, length, FALSE);
		break;
	}

	g_bytes_unref (value);

	return g_bytes_new_take (output, length);
}

static gboolean
write_bytes (GMimeStream *stream, const char *bytes, gsize length)
{
	errno = 0;
	return _g_mime_stream_write_all (stream, bytes, length) != -1;
}

static gboolean
anonymize_invalid_field (GMimeFormatOptions *options, GMimeStream *stream, GBytes *raw)
{
	GMimeStream *filtered = g_mime_stream_filter_new (stream);
	GMimeFilter *filter;
	const char *bytes;
	gboolean success;
	gsize length;
	int errsv;

	filter = g_mime_filter_anonymize_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	filter = g_mime_format_options_create_newline_filter (options, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	bytes = g_bytes_get_data (raw, &length);
	success = write_bytes (filtered, bytes, length);

	if (success) {
		errno = 0;
		success = g_mime_stream_flush (filtered) == 0;
	}

	if (success) {
		errno = 0;
		success = g_mime_stream_flush (stream) == 0;
	}

	errsv = errno;
	g_object_unref (filtered);
	errno = errsv;

	return success;
}

static gboolean
anonymize_headers (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
		   GPtrArray *headers, GMimeStream *stream, GError **error);

/**
 * g_mime_anonymizer_write_headers:
 * @anonymizer: a #GMimeAnonymizer
 * @options: (nullable): formatting options, or %NULL for the defaults
 * @headers: the headers to anonymize
 * @stream: the destination stream, which is not closed
 * @error: (nullable): return location for an error
 *
 * Writes every header in order, including duplicates and fields hidden by
 * ordinary serialization options. No body separator is added. Newlines use
 * @options; all other masking is byte-oriented, without decoding or changing
 * the stored headers. Explicitly preserved fields still use normal formatting
 * and newline conversion. Invalid fields are masked as unstructured bytes.
 * Unset native placeholder headers are omitted.
 *
 * Output may be partial on failure; write, formatting and flush failures are
 * reported without closing @stream or flushing after a failed write.
 *
 * Returns: whether all headers were written and the filtered stream flushed.
 **/
gboolean
g_mime_anonymizer_write_headers (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
			       GMimeHeaderList *headers, GMimeStream *stream, GError **error)
{
	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), FALSE);
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return anonymize_headers (anonymizer, options, headers->array, stream, error);
}

static gboolean
anonymize_headers (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
		   GPtrArray *headers, GMimeStream *stream, GError **error)
{
	gboolean success = FALSE, written;
	GMimeStream *filtered;
	GMimeFilter *filter;
	GMimeHeader *header;
	const char *bytes;
	GBytes *value;
	gsize length;
	guint i;
	int errsv;

	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_format_options_create_newline_filter (options, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	for (i = 0; i < headers->len; i++) {
		header = headers->pdata[i];

		if (header->invalid) {
			if (!anonymize_invalid_field (options, stream, header->raw_name))
				goto done;
			continue;
		}

		if (!header->raw_value)
			continue;

		if (!(value = anonymize_header (anonymizer, options, header)))
			goto done;

		bytes = g_bytes_get_data (header->raw_name, &length);
		written = write_bytes (filtered, bytes, length) && write_bytes (filtered, ":", 1);

		if (written) {
			bytes = g_bytes_get_data (value, &length);
			written = write_bytes (filtered, bytes, length);
		}

		errsv = errno;
		g_bytes_unref (value);
		errno = errsv;

		if (!written)
			goto done;
	}

	errno = 0;
	success = g_mime_stream_flush (filtered) == 0;

	if (success) {
		/* Unlike MimeKit's FilteredStream, native filter completion does not flush its source. */
		errno = 0;
		success = g_mime_stream_flush (stream) == 0;
	}

done:
	errsv = errno;
	if (!success)
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to anonymize headers: %s", errsv ? g_strerror (errsv) : "unknown I/O error");

	g_object_unref (filtered);
	errno = errsv;

	return success;
}

typedef struct {
	GMimeMultipart *multipart;
	GMimeFormatOptions *options;
	char *boundary;
	gboolean ensure_newline;
	guint index;
} AnonymizerFrame;

static gboolean
write_newline (GMimeFormatOptions *options, GMimeStream *stream)
{
	const char *newline = g_mime_format_options_get_newline (options);

	return write_bytes (stream, newline, strlen (newline));
}

static gboolean
write_raw (GMimeStream *stream, GBytes *bytes)
{
	gsize length;
	const char *data = g_bytes_get_data (bytes, &length);

	return write_bytes (stream, data, length);
}

static gboolean
anonymize_bytes (GMimeFormatOptions *options, GMimeStream *stream, GBytes *bytes, gboolean ensure)
{
	GMimeStream *filtered;
	GMimeFilter *filter;
	gboolean success;
	int errsv;

	if (!bytes || !g_bytes_get_size (bytes))
		return TRUE;

	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_anonymize_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	filter = g_mime_format_options_create_newline_filter (options, ensure);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	success = write_raw (filtered, bytes);

	if (success) {
		errno = 0;
		success = g_mime_stream_flush (filtered) == 0;
	}

	if (success) {
		errno = 0;
		success = g_mime_stream_flush (stream) == 0;
	}

	errsv = errno;
	g_object_unref (filtered);
	errno = errsv;

	return success;
}

/* Anonymize the message headers (merged with the headers of the body, if any)
 * and return the body that should be anonymized next and its newline policy. */
static gboolean
anonymize_message_headers (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
			  GMimeMessage *message, GMimeStream *stream, gboolean *ensure, GError **error)
{
	GMimeEntity *object = (GMimeEntity *) message;
	GPtrArray *headers;
	gboolean success;

	/* Constructed native messages request a final newline; parsed messages
	 * clear this flag, corresponding to MimeKit's strict/loose distinction. */
	GMimeEntityPrivate *object_priv = _g_mime_entity_get_private (object);
	GMimeMessagePrivate *message_priv = _g_mime_message_get_private (message);

	if (object_priv->ensure_newline && message_priv->mime_part) {
		GMimeEntityPrivate *part_priv = _g_mime_entity_get_private (message_priv->mime_part);

		if (part_priv->headers->array->len && !g_mime_header_list_contains (object_priv->headers, "MIME-Version"))
			g_mime_header_list_set (object_priv->headers, "MIME-Version", "1.0", NULL);
	}

	headers = _g_mime_message_merge_headers (message);
	success = anonymize_headers (anonymizer, options, headers, stream, error);
	g_ptr_array_unref (headers);

	if (!success)
		return FALSE;

	if (message_priv->mime_part) {
		GMimeEntityPrivate *part_priv = _g_mime_entity_get_private (message_priv->mime_part);

		*ensure = object_priv->ensure_newline || g_mime_format_options_get_ensure_newline (options);

		return (!object_priv->ensure_newline && !part_priv->headers->has_body_separator) ||
			write_newline (options, stream);
	}

	*ensure = FALSE;

	return write_newline (options, stream);
}

static gboolean
multipart_is_truncated (GMimeMultipart *multipart)
{
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);

	return (priv->raw_end_boundary && !g_bytes_get_size (priv->raw_end_boundary)) ||
		(!priv->reader_model && !priv->write_end_boundary);
}

static GBytes *
multipart_raw_text (GMimeMultipart *multipart, GMimeFormatOptions *options, gboolean epilogue)
{
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
	GBytes *raw = epilogue ? priv->raw_epilogue : priv->raw_preamble;
	const char *text = epilogue ? priv->epilogue : priv->preamble;
	char *value;

	if (raw || priv->reader_model)
		return raw ? g_bytes_ref (raw) : NULL;

	if (!text)
		return NULL;

	/* Legacy native setters store text separately from delimiter newlines.
	 * Adapt it locally without changing the multipart's raw-byte model. */
	value = epilogue ? g_strconcat (g_mime_format_options_get_newline (options), text, NULL) :
		g_strdup (text);

	return g_bytes_new_take (value, strlen (value));
}

static gboolean
write_multipart_end (AnonymizerFrame *frame, GMimeStream *stream)
{
	GMimeMultipart *multipart = frame->multipart;
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
	GMimeFormatOptions *options = frame->options;
	GBytes *epilogue;
	gboolean success;
	char *boundary;

	if (multipart_is_truncated (multipart)) {
		/* The multipart was truncated, so there is no end boundary (or epilogue) to write. */
		if (priv->raw_trailing_boundary) {
			gsize length;
			const char *data = g_bytes_get_data (priv->raw_trailing_boundary, &length);

			if (priv->children->len && !_g_mime_entity_ends_with_delimiter_newline (
			    priv->children->pdata[priv->children->len - 1], TRUE) && !write_newline (options, stream))
				return FALSE;

			if (!write_raw (stream, priv->raw_trailing_boundary))
				return FALSE;

			if (frame->ensure_newline && (!length || data[length - 1] != '\n'))
				return write_newline (options, stream);
		}

		return TRUE;
	}

	if (priv->children->len && !_g_mime_entity_ends_with_delimiter_newline (
	    priv->children->pdata[priv->children->len - 1], TRUE) && !write_newline (options, stream))
		return FALSE;

	epilogue = multipart_raw_text (multipart, options, TRUE);

	if (priv->raw_end_boundary) {
		success = write_raw (stream, priv->raw_end_boundary);
	} else {
		const char *value = g_mime_entity_get_content_type_parameter (GMIME_ENTITY (multipart), "boundary");

		boundary = g_strdup_printf ("--%s--%s", value ? value : "",
					   epilogue ? "" : g_mime_format_options_get_newline (options));
		success = write_bytes (stream, boundary, strlen (boundary));
		g_free (boundary);
	}

	if (success) {
		if (epilogue && g_bytes_get_size (epilogue))
			success = anonymize_bytes (options, stream, epilogue, frame->ensure_newline);
		else if (frame->ensure_newline && (priv->raw_end_boundary || epilogue))
			success = write_newline (options, stream);
	}

	g_clear_pointer (&epilogue, g_bytes_unref);

	return success;
}

static void
anonymizer_frame_clear (gpointer data)
{
	AnonymizerFrame *frame = data;

	g_object_unref (frame->multipart);
	g_mime_format_options_free (frame->options);
	g_free (frame->boundary);
}

static gboolean
anonymize_part (GMimePart *part, GMimeFormatOptions *options, GMimeStream *stream, gboolean ensure)
{
	GMimeStream *filtered = g_mime_stream_filter_new (stream);
	GMimeFilter *filter = g_mime_filter_anonymize_new ();
	gboolean success;
	int errsv;

	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	errno = 0;
	success = _g_mime_part_write_content (part, options, filtered, ensure) >= 0;

	if (success)
		success = g_mime_stream_flush (filtered) == 0;

	if (success)
		success = g_mime_stream_flush (stream) == 0;

	errsv = errno;
	g_object_unref (filtered);
	errno = errsv;

	return success;
}

static gboolean
anonymize_status (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
		  GMimeEntity *entity, GMimeStream *stream, gboolean ensure, GError **error)
{
	GMimeHeaderListCollection *groups = NULL;
	GMimeHeaderList *fields = NULL;
	GError *local_error = NULL;
	guint i;

	if (GMIME_IS_MESSAGE_DELIVERY_STATUS (entity))
		groups = g_mime_message_delivery_status_get_status_groups ((GMimeMessageDeliveryStatus *) entity, &local_error);
	else
		fields = g_mime_message_disposition_notification_get_fields ((GMimeMessageDispositionNotification *) entity, &local_error);

	if (local_error) {
		if (g_error_matches (local_error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) ||
		    g_error_matches (local_error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG)) {
			g_error_free (local_error);
			return anonymize_part ((GMimePart *) entity, options, stream, ensure);
		}

		g_propagate_error (error, local_error);
		return FALSE;
	}

	if (fields)
		return anonymize_headers (anonymizer, options, fields->array, stream, error);

	for (i = 0; i < groups->array->len; i++) {
		GMimeHeaderList *group = groups->array->pdata[i];

		if (!anonymize_headers (anonymizer, options, group->array, stream, error) ||
		    (i + 1 < groups->array->len && !write_newline (options, stream)))
			return FALSE;
	}

	return TRUE;
}

/* Anonymize the entity (and all of its descendants).
 *
 * Note: Nested multiparts and message parts are handled using an explicit stack rather than by recursion so that
 * the nesting depth of the entity cannot exhaust the call stack. Only multiparts need a frame on the stack because
 * they have more to write once a child has been written. The embedded message is the last thing that a message part
 * writes and the body is the last thing that a message writes, so the anonymizer simply continues with them instead. */
static gboolean
anonymize_entity (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options, GMimeEntity *root,
		  GMimeStream *stream, gboolean message, GError **error)
{
	GMimeEntityPrivate *root_priv = _g_mime_entity_get_private (root);
	gboolean content_only = FALSE, ensure = root_priv->ensure_newline, success = FALSE;
	GArray *frames = g_array_new (FALSE, FALSE, sizeof (AnonymizerFrame));
	GMimeEntity *entity = g_object_ref (root);
	GMimeFormatOptions *temporary = NULL;
	int errsv;

	g_array_set_clear_func (frames, anonymizer_frame_clear);
	if (message) {
		GMimeMessagePrivate *message_priv;

		if (!anonymize_message_headers (anonymizer, options, (GMimeMessage *) entity, stream, &ensure, error))
			goto done;

		message_priv = _g_mime_message_get_private (((GMimeMessage *) root));

		entity = message_priv->mime_part;
		if (entity)
			g_object_ref (entity);

		g_object_unref (root);
		content_only = TRUE;
	}

	while (TRUE) {
		GMimeMultipartPrivate *multipart_priv;

		if (entity) {
			GMimeEntity *next = NULL;

			if (!content_only) {
				GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private (entity);

				if (!anonymize_headers (anonymizer, options, entity_priv->headers->array, stream, error) ||
				    (entity_priv->headers->has_body_separator && !write_newline (options, stream)))
					goto done;
			}

			if (GMIME_IS_MESSAGE_PART (entity)) {
				GMimeMessage *embedded = g_mime_message_part_get_message ((GMimeMessagePart *) entity);

				if (embedded) {
					GMimeMessagePrivate *embedded_priv;

					/* Note: Only ensure that the embedded message ends with a new-line if this part is supposed to end with one. */
					if (g_mime_format_options_get_ensure_newline (options) != ensure) {
						GMimeFormatOptions *copy = g_mime_format_options_clone (options);

						g_mime_format_options_set_ensure_newline (copy, ensure);
						g_clear_pointer (&temporary, g_mime_format_options_free);
						options = temporary = copy;
					}

					if (!anonymize_message_headers (anonymizer, options, embedded, stream, &ensure, error))
						goto done;

					embedded_priv = _g_mime_message_get_private (embedded);

					if (embedded_priv->mime_part)
						next = g_object_ref (embedded_priv->mime_part);

					content_only = TRUE;
				}
			} else if (GMIME_IS_MULTIPART (entity)) {
				GMimeMultipart *multipart = (GMimeMultipart *) entity;
				GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
				GBytes *preamble = multipart_raw_text (multipart, options, FALSE);
				AnonymizerFrame frame;
				const char *boundary;

				success = anonymize_bytes (options, stream, preamble, priv->children->len > 0 || ensure);
				g_clear_pointer (&preamble, g_bytes_unref);

				if (!success)
					goto done;

				success = FALSE;
				frame.multipart = g_object_ref (multipart);
				frame.options = g_mime_format_options_clone (options);
				frame.ensure_newline = ensure;
				frame.index = 0;
				boundary = g_mime_entity_get_content_type_parameter (entity, "boundary");
				frame.boundary = g_strdup_printf ("--%s%s", boundary ? boundary : "",
								 g_mime_format_options_get_newline (options));
				g_array_append_val (frames, frame);
			} else if (GMIME_IS_MESSAGE_DELIVERY_STATUS (entity) ||
				   GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (entity)) {
				if (!anonymize_status (anonymizer, options, entity, stream, ensure, error))
					goto done;
			} else if (GMIME_IS_PART (entity)) {
				if (!anonymize_part ((GMimePart *) entity, options, stream, ensure))
					goto done;
			}

			g_object_unref (entity);
			entity = next;
			continue;
		}

		g_clear_pointer (&temporary, g_mime_format_options_free);

		if (!frames->len) {
			success = TRUE;
			break;
		}

		AnonymizerFrame *frame = &g_array_index (frames, AnonymizerFrame, frames->len - 1);
		GMimeMultipart *multipart = frame->multipart;
		guint index = frame->index++;

		multipart_priv = _g_mime_multipart_get_private (multipart);

		if (index < multipart_priv->children->len) {
			GBytes *boundary = multipart_priv->raw_boundaries ? multipart_priv->raw_boundaries->pdata[index] : NULL;
			GMimeEntityPrivate *entity_priv;

			options = frame->options;

			if (index > 0 && !_g_mime_entity_ends_with_delimiter_newline (multipart_priv->children->pdata[index - 1], TRUE) &&
			    !write_newline (options, stream))
				goto done;

			if (!(boundary ? write_raw (stream, boundary) :
			      write_bytes (stream, frame->boundary, strlen (frame->boundary))))
				goto done;

			entity = g_object_ref (multipart_priv->children->pdata[index]);
			entity_priv = _g_mime_entity_get_private (entity);

			ensure = entity_priv->ensure_newline || (frame->ensure_newline && index + 1 == multipart_priv->children->len &&
				multipart_is_truncated (multipart) && !multipart_priv->raw_trailing_boundary);
			content_only = FALSE;
		} else {
			if (!write_multipart_end (frame, stream))
				goto done;

			g_array_set_size (frames, frames->len - 1);
		}
	}

done:
	errsv = errno;

	if (!success && (!error || !*error))
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to anonymize MIME content: %s", errsv ? g_strerror (errsv) : "unknown I/O error");

	g_clear_object (&entity);
	g_clear_pointer (&temporary, g_mime_format_options_free);
	g_array_unref (frames);
	errno = errsv;

	return success;
}

/**
 * g_mime_anonymizer_write_message:
 * @anonymizer: a #GMimeAnonymizer
 * @options: (nullable): formatting options, or %NULL for defaults
 * @message: the message to anonymize
 * @stream: the destination, which remains open
 * @error: (nullable): return location for an error
 *
 * Writes merged message/body headers and anonymizes the entire MIME tree
 * without recursion. Raw multipart delimiters remain visible and unmodified;
 * preambles, epilogues and transfer-encoded content are masked. Status and
 * notification bodies use header masking, including the preservation set.
 *
 * As in MimeKit, a constructed message with body headers acquires a missing
 * MIME-Version field. Lazy status-field access may initialize content/caches.
 * Other stored header and content bytes are not rewritten. Output may be
 * partial on error; formatting, content I/O and destination errors propagate.
 *
 * Returns: whether anonymization succeeded.
 **/
gboolean
g_mime_anonymizer_write_message (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
			       GMimeMessage *message, GMimeStream *stream, GError **error)
{
	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), FALSE);
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return anonymize_entity (anonymizer, options, (GMimeEntity *) message, stream, TRUE, error);
}

/**
 * g_mime_anonymizer_write_entity:
 * @anonymizer: a #GMimeAnonymizer
 * @options: (nullable): formatting options, or %NULL for defaults
 * @entity: the MIME entity to anonymize
 * @stream: the destination, which remains open
 * @error: (nullable): return location for an error
 *
 * Writes anonymized entity headers and content using the same iterative
 * traversal as g_mime_anonymizer_write_message(). Native #GMimeMessage
 * instances are handled as messages, including merged body headers.
 *
 * Returns: whether anonymization succeeded.
 **/
gboolean
g_mime_anonymizer_write_entity (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
			      GMimeEntity *entity, GMimeStream *stream, GError **error)
{
	g_return_val_if_fail (GMIME_IS_ANONYMIZER (anonymizer), FALSE);
	g_return_val_if_fail (GMIME_IS_ENTITY (entity), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return anonymize_entity (anonymizer, options, entity, stream, GMIME_IS_MESSAGE (entity), error);
}
