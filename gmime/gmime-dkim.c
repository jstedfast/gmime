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

#include "gmime-dkim-private.h"
#include "gmime-dkim-backend-private.h"
#include "gmime-internal.h"
#include "gmime-object-private.h"
#include "gmime-stream-filter.h"
#include "gmime-stream-null.h"

static gboolean
invalid (GError **error, const char *why)
{
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, why);
	return FALSE;
}

static gboolean
wsp (char c)
{
	return c == ' ' || c == '\t';
}

static gboolean
field_name (const char *s, gsize n)
{
	if (!n)
		return FALSE;

	for (gsize i = 0; i < n; i++) {
		if ((guchar) s[i] < 33 || (guchar) s[i] > 126 || s[i] == ':')
			return FALSE;
	}

	return TRUE;
}

/* obs-FWS immediately before the colon is not part of the name. */
static gsize
name_length (const char *raw, const char *colon)
{
	while (colon > raw && wsp (colon[-1]))
		colon--;

	return colon - raw;
}

static gboolean
header_is (const char *raw, const char *name)
{
	const gsize n = name_length (raw, strchr (raw, ':'));

	return strlen (name) == n && !g_ascii_strncasecmp (raw, name, n);
}

GPtrArray *
_g_mime_dkim_get_headers (GMimeMessage *message, GError **error)
{
	GPtrArray *headers = _g_mime_message_merge_headers (message);
	GPtrArray *fields = g_ptr_array_new_with_free_func (g_free);
	GMimeFormatOptions *options = g_mime_format_options_new ();

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	for (guint i = 0; i < headers->len; i++) {
		GMimeHeader *header = headers->pdata[i];
		const char *name, *data;
		GString *field;
		GBytes *value;
		gsize n, len;

		if (!header->raw_value && !header->invalid)
			continue; /* An unset header isn't emitted by the writer. */

		name = g_bytes_get_data (header->raw_name, &n);

		if (header->invalid || !n || !field_name (name, name_length (name, name + n)) ||
		    !(value = _g_mime_header_get_formatted_value (header, options)))
			goto malformed;

		data = g_bytes_get_data (value, &len);
		field = g_string_new_len (name, n);
		g_string_append_c (field, ':');

		if (len && data[len - 1] == '\n') {
			len--;

			if (len && data[len - 1] == '\r')
				len--;
		}

		for (gsize j = 0; j < len; j++) {
			if (data[j] == '\r' && j + 1 < len && data[j + 1] == '\n')
				j++;

			if (!data[j] || data[j] == '\r' ||
			    (data[j] == '\n' && (j + 1 == len || !wsp (data[j + 1])))) {
				g_string_free (field, TRUE);
				g_bytes_unref (value);
				goto malformed;
			}

			if (data[j] == '\n')
				g_string_append_c (field, '\r');

			g_string_append_c (field, data[j]);
		}

		g_ptr_array_add (fields, g_string_free (field, FALSE));
		g_bytes_unref (value);
	}

	goto done;

malformed:
	invalid (error, "Malformed message header");
	g_clear_pointer (&fields, g_ptr_array_unref);

done:
	g_ptr_array_unref (headers);
	g_mime_format_options_unref (options);

	return fields;
}

GMimeHeader *
_g_mime_dkim_create_header (const char *field, GError **error)
{
	const char *colon = strchr (field, ':');
	char *name = g_strndup (field, colon - field);
	GMimeHeader *header = g_mime_header_new (name, "", NULL);
	char *text = g_strconcat (colon + 1, "\r\n", NULL);
	GBytes *value = g_bytes_new_take (text, strlen (text));

	if (!g_mime_header_set_raw_value (header, value, error))
		g_clear_object (&header);

	g_bytes_unref (value);
	g_free (name);

	return header;
}

/* ARC's instance and chain-status productions extend the DKIM grammar
 * with RFC 5322 comments. Skip nested comments iteratively; a quoted
 * parenthesis or semicolon inside one is not a tag delimiter. */
static gboolean
skip_cfws (const char **cursor)
{
	const char *inptr = *cursor;

	for (;;) {
		gsize depth = 1;

		inptr += strspn (inptr, " \t\r\n");

		if (*inptr != '(')
			break;

		inptr++;

		while (depth && *inptr) {
			if (*inptr == '\\') {
				if (!inptr[1] || inptr[1] == '\r' || inptr[1] == '\n')
					return FALSE;

				inptr++;
			} else if (*inptr == '(') {
				depth++;
			} else if (*inptr == ')') {
				depth--;
			}

			inptr++;
		}

		if (depth)
			return FALSE;
	}

	*cursor = inptr;

	return TRUE;
}

/* Keep the original value offsets as well as a trimmed, unfolded copy.
 * The former are needed to erase *only* b=, including surrounding FWS. */
static GHashTable *
parse_tags (const char *raw, const char **b_start, const char **b_end, GError **error)
{
	GHashTable *tags = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	const char *inptr = strchr (raw, ':');
	GString *value = NULL;
	gboolean seal, arc;
	char *key = NULL;

	if (!inptr)
		goto malformed;

	seal = header_is (raw, "ARC-Seal");
	arc = seal || header_is (raw, "ARC-Message-Signature");
	inptr++;

	do {
		gboolean first = g_hash_table_size (tags) == 0, special;
		const char *name, *end, *eq;

		value = g_string_new (NULL);
		inptr += strspn (inptr, " \t\r\n");

		if (arc && first && !skip_cfws (&inptr))
			goto malformed;

		name = inptr;

		if (!g_ascii_isalpha (*inptr))
			goto malformed;

		while (g_ascii_isalnum (*inptr) || *inptr == '_')
			inptr++;

		key = g_strndup (name, inptr - name);

		if (arc && first && strcmp (key, "i"))
			goto malformed;

		special = (arc && first) || (seal && !strcmp (key, "cv"));

		if (special && !skip_cfws (&inptr))
			goto malformed;

		inptr += strspn (inptr, " \t\r\n");

		if (*inptr++ != '=' || g_hash_table_contains (tags, key))
			goto malformed;

		eq = inptr;
		inptr += strspn (inptr, " \t\r\n");

		if (special) {
			if (!skip_cfws (&inptr))
				goto malformed;

			while (g_ascii_isalnum (*inptr))
				g_string_append_c (value, *inptr++);

			if (!skip_cfws (&inptr) || (*inptr && *inptr != ';'))
				goto malformed;
		}

		while (!special && *inptr && *inptr != ';') {
			if (*inptr == '\r' && inptr[1] == '\n' && wsp (inptr[2])) {
				inptr += 2;
				continue;
			}

			if (!wsp (*inptr) && ((guchar) *inptr < 33 || (guchar) *inptr > 126))
				goto malformed;

			g_string_append_c (value, *inptr++);
		}

		end = inptr;

		while (value->len && wsp (value->str[value->len - 1]))
			g_string_truncate (value, value->len - 1);

		if (b_start && !strcmp (key, "b")) {
			*b_start = eq;
			*b_end = end;
		}

		g_hash_table_insert (tags, key, g_string_free (value, FALSE));
		key = NULL;
		value = NULL;

		if (!*inptr)
			break;

		inptr++;
	} while (inptr[strspn (inptr, " \t\r\n")]);

	return tags;

malformed:
	g_hash_table_unref (tags);
	g_free (key);

	if (value)
		g_string_free (value, TRUE);

	invalid (error, "Malformed or duplicate signature tag");

	return NULL;
}

GHashTable *
_g_mime_dkim_parse_tags (const char *raw, GError **error)
{
	return parse_tags (raw, NULL, NULL, error);
}

char *
_g_mime_dkim_strip_b (const char *raw)
{
	const char *start = NULL, *end = NULL;
	GHashTable *tags = parse_tags (raw, &start, &end, NULL);
	GString *out = NULL;

	if (!tags)
		return NULL;

	g_hash_table_unref (tags);

	if (!start)
		return NULL;

	out = g_string_new_len (raw, start - raw);
	g_string_append (out, end);

	return g_string_free (out, FALSE);
}

GBytes *
_g_mime_dkim_decode_base64 (const char *text)
{
	guint8 *decoded = NULL;
	char *canonical = NULL;
	gboolean matches;
	GString *compact;
	gsize n;

	if (!text)
		return NULL;

	compact = g_string_new (NULL);

	for (; *text; text++) {
		if (wsp (*text) || *text == '\r' || *text == '\n')
			continue;

		g_string_append_c (compact, *text);
	}

	if (!compact->len || compact->len % 4) {
		g_string_free (compact, TRUE);
		return NULL;
	}

	decoded = g_base64_decode (compact->str, &n);
	canonical = g_base64_encode (decoded, n);
	matches = !strcmp (canonical, compact->str);
	g_free (canonical);

	g_string_free (compact, TRUE);

	if (!matches) {
		g_free (decoded);
		return NULL;
	}

	return g_bytes_new_take (decoded, n);
}

gboolean
_g_mime_dkim_valid_domain (const char *text, gboolean selector)
{
	const char *inptr = text;

	if (!inptr || strlen (inptr) > 253 || (!selector && !strchr (inptr, '.')))
		return FALSE;

	do {
		const char *label = inptr;

		if (!g_ascii_isalnum (*inptr))
			return FALSE;

		while (g_ascii_isalnum (*inptr) || *inptr == '-')
			inptr++;

		if (inptr - label > 63 || inptr[-1] == '-')
			return FALSE;

		if (!*inptr)
			return TRUE;
	} while (*inptr++ == '.');

	return FALSE;
}

static void
hash_bytes (GChecksum *sum, const char *s, gsize n)
{
	while (n > 0) {
		gsize chunk = MIN (n, G_MAXSSIZE);

		g_checksum_update (sum, (const guchar *) s, chunk);
		s += chunk;
		n -= chunk;
	}
}

void
_g_mime_dkim_hash_header (GChecksum *sum, const char *raw, gboolean relaxed, gboolean crlf)
{
	if (relaxed) {
		/* RFC 6376 §3.4.2: lowercase only the name. Whitespace
		 * on both sides of a fold is one run, not two spaces.
		 * Omit it around the colon and at the end. */
		const char *colon = strchr (raw, ':');
		char *name = g_ascii_strdown (raw, name_length (raw, colon));
		const char *inptr = colon + 1;

		hash_bytes (sum, name, strlen (name));
		g_free (name);

		hash_bytes (sum, ":", 1);

		inptr += strspn (inptr, " \t\r\n");

		while (*inptr) {
			gsize n = strcspn (inptr, " \t\r\n");

			hash_bytes (sum, inptr, n);
			inptr += n;
			inptr += strspn (inptr, " \t\r\n");

			if (*inptr)
				hash_bytes (sum, " ", 1);
		}
	} else {
		hash_bytes (sum, raw, strlen (raw));
	}

	if (crlf)
		hash_bytes (sum, "\r\n", 2);
}

static void
finish_hash (GChecksum *sum, guint8 digest[32])
{
	gsize n = 32;

	g_checksum_get_digest (sum, digest, &n);
}

static void
hash_prefix (GChecksum *sum, const char *data, gsize len, guint64 *remaining)
{
	gsize take = MIN (len, *remaining);

	hash_bytes (sum, data, take);
	*remaining -= take;
}

/* Delay empty lines and relaxed whitespace until another nonempty run
 * commits them. A CRLF, or a run of whitespace, can span stream writes. */
typedef struct {
	GMimeStreamNull parent;
	GChecksum *sum;
	guint64 remaining, empty_lines;
	gboolean relaxed, pending_cr, pending_space, line_has_data, any_data;
} DkimBodyStream;

typedef GMimeStreamNullClass DkimBodyStreamClass;
static GType dkim_body_stream_get_type (void);
G_DEFINE_TYPE (DkimBodyStream, dkim_body_stream, GMIME_TYPE_STREAM_NULL)

static void
body_stream_content (DkimBodyStream *body, const char *data, size_t length)
{
	while (body->remaining && body->empty_lines) {
		hash_prefix (body->sum, "\r\n", 2, &body->remaining);
		body->empty_lines--;
	}

	body->empty_lines = 0;
	if (body->pending_space)
		hash_prefix (body->sum, " ", 1, &body->remaining);

	body->pending_space = FALSE;
	body->line_has_data = body->any_data = TRUE;
	hash_prefix (body->sum, data, length, &body->remaining);
}

static ssize_t
body_stream_write (GMimeStream *stream, const char *data, size_t length)
{
	DkimBodyStream *body = (DkimBodyStream *) stream;
	size_t i = 0;

	while (i < length) {
		if (data[i] == '\n') {
			/* Keep the terminator of a nonempty line as written. Deferred
			 * empty lines, if followed by content, are emitted as CRLF. */
			if (!body->line_has_data)
				body->empty_lines++;
			else if (body->pending_cr)
				hash_prefix (body->sum, "\r\n", 2, &body->remaining);
			else
				hash_prefix (body->sum, "\n", 1, &body->remaining);

			body->pending_cr = body->pending_space = FALSE;
			body->line_has_data = FALSE;
			i++;
			continue;
		}

		if (body->pending_cr) {
			body_stream_content (body, "\r", 1);
			body->pending_cr = FALSE;
		}

		if (data[i] == '\r') {
			body->pending_cr = TRUE;
			i++;
		} else if (body->relaxed && wsp (data[i])) {
			body->pending_space = TRUE;
			i++;
		} else {
			size_t start = i++;

			while (i < length && data[i] != '\r' && data[i] != '\n' && !(body->relaxed && wsp (data[i])))
				i++;

			body_stream_content (body, data + start, i - start);
		}
	}

	return GMIME_STREAM_CLASS (dkim_body_stream_parent_class)->write (stream, data, length);
}

static void
body_stream_finalize (GObject *object)
{
	DkimBodyStream *body = (DkimBodyStream *) object;

	g_checksum_free (body->sum);

	G_OBJECT_CLASS (dkim_body_stream_parent_class)->finalize (object);
}

static void
dkim_body_stream_class_init (DkimBodyStreamClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = body_stream_finalize;

	GMIME_STREAM_CLASS (klass)->write = body_stream_write;
}

static void
dkim_body_stream_init (DkimBodyStream *body)
{
	body->sum = g_checksum_new (G_CHECKSUM_SHA256);

	g_mime_stream_construct (GMIME_STREAM (body), 0, -1);
}

GMimeStream *
_g_mime_dkim_body_stream_new (gboolean relaxed, guint64 remaining)
{
	DkimBodyStream *body = g_object_new (dkim_body_stream_get_type (), NULL);

	body->remaining = remaining;
	body->relaxed = relaxed;

	return GMIME_STREAM (body);
}

gboolean
_g_mime_dkim_body_stream_finish (GMimeStream *stream, gboolean limited, guint8 digest[32], GError **error)
{
	DkimBodyStream *body = (DkimBodyStream *) stream;

	if (body->pending_cr)
		body_stream_content (body, "\r", 1);

	if (body->line_has_data || (!body->relaxed && !body->any_data))
		hash_prefix (body->sum, "\r\n", 2, &body->remaining);

	if (limited && body->remaining)
		return invalid (error, "Signed body length exceeds canonicalized body length");

	finish_hash (body->sum, digest);

	return TRUE;
}

static gboolean
body_digest (GMimeMessage *message, gboolean relaxed, gboolean limited, guint64 remaining,
	     guint8 digest[32], GError **error)
{
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeStream *body = _g_mime_dkim_body_stream_new (relaxed, remaining);
	GMimeStream *filtered = g_mime_stream_filter_new (body);
	gboolean valid = FALSE;
	GMimeFilter *filter;

	/* Parsed MIME content may retain bare LFs. Convert to wire form
	 * before canonicalization, including across MIME writer calls. */
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	filter = g_mime_format_options_create_newline_filter (options, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	if (g_mime_entity_write_content_to_stream (GMIME_ENTITY (message), options, filtered) == -1 ||
	    g_mime_stream_flush (filtered) == -1) {
		int errsv = errno;
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv), "Cannot hash MIME content: %s", g_strerror (errsv));
	} else {
		valid = _g_mime_dkim_body_stream_finish (body, limited, digest, error);
	}

	g_mime_format_options_unref (options);
	g_object_unref (filtered);
	g_object_unref (body);

	return valid;
}

static gboolean signed_name (const char *name, gboolean ams);
static gboolean header_digest (GPtrArray *fields_raw, guint skip, const char * const *names,
			      const char *unsigned_header, gboolean relaxed, gboolean ams,
			      guint8 digest[32], GError **error);

static void
append_h (GString *raw, const char * const *names)
{
	gsize column = 3;

	g_string_append (raw, ";\r\n\th=");

	for (guint i = 0; names[i]; i++) {
		gsize n = strlen (names[i]);

		if (i > 0) {
			g_string_append_c (raw, ':');
			column++;
		}

		if (column + n > 76) {
			g_string_append (raw, "\r\n\t");
			column = 1;
		}

		g_string_append (raw, names[i]);
		column += n;
	}
}

char *
_g_mime_dkim_sign_header (GMimeMessage *message, GPtrArray *fields, guint instance, const char *domain,
			  const char *selector, GMimeDkimAlgorithm algorithm, GBytes *private_key,
			  GMimeDkimCanonicalization hc, GMimeDkimCanonicalization bc,
			  const char * const *headers, GError **error)
{
	static const char * const common[] = {
		"From", "To", "Cc", "Subject", "Date", "Message-ID", "Reply-To",
		"Sender", "MIME-Version", "Content-Type", "Content-Transfer-Encoding", NULL
	};
	gboolean ed = algorithm == GMIME_DKIM_ALGORITHM_ED25519_SHA256;
	char *bh, *b, *result = NULL;
	GPtrArray *defaults = NULL;
	const guint8 *sig_data;
	GBytes *sig = NULL;
	guint8 digest[32];
	gsize sig_size;
	GString *raw;

	if (!_g_mime_dkim_valid_domain (domain, FALSE) || !_g_mime_dkim_valid_domain (selector, TRUE) ||
	    strlen (domain) + strlen (selector) + strlen ("._domainkey.") > 253 ||
	    (!ed && algorithm != GMIME_DKIM_ALGORITHM_RSA_SHA256) || instance > 50 || !private_key ||
	    (hc != GMIME_DKIM_CANONICALIZATION_SIMPLE && hc != GMIME_DKIM_CANONICALIZATION_RELAXED) ||
	    (bc != GMIME_DKIM_CANONICALIZATION_SIMPLE && bc != GMIME_DKIM_CANONICALIZATION_RELAXED)) {
		invalid (error, "Invalid signing parameters");
		return NULL;
	}

	if (!headers) {
		defaults = g_ptr_array_new ();

		for (guint i = 0; common[i]; i++)
			g_ptr_array_add (defaults, (gpointer) common[i]);

		/* Every existing DKIM signature should survive ARC transit. */
		if (instance) {
			for (guint i = 0; i < fields->len; i++) {
				if (header_is (fields->pdata[i], "DKIM-Signature"))
					g_ptr_array_add (defaults, (gpointer) "DKIM-Signature");
			}
		}

		g_ptr_array_add (defaults, NULL);
		headers = (const char * const *) defaults->pdata;
	}

	raw = g_string_new (NULL);

	for (guint i = 0; headers[i]; i++) {
		if (strlen (headers[i]) > 996 || !signed_name (headers[i], instance != 0)) {
			invalid (error, "Invalid signed header name");
			goto done;
		}
	}

	if (!body_digest (message, bc, FALSE, G_MAXUINT64, digest, error))
		goto done;

	bh = g_base64_encode (digest, sizeof digest);

	if (instance)
		g_string_append_printf (raw, "ARC-Message-Signature: i=%u", instance);
	else
		g_string_append (raw, "DKIM-Signature: v=1");

	g_string_append_printf (raw, "; a=%s-sha256; c=%s/%s;\r\n\td=%s; s=%s; t=%" G_GINT64_FORMAT,
				ed ? "ed25519" : "rsa", hc ? "relaxed" : "simple", bc ? "relaxed" : "simple",
				domain, selector, (gint64) (g_get_real_time () / G_USEC_PER_SEC));
	append_h (raw, headers);

	g_string_append_printf (raw, ";\r\n\tbh=%s;\r\n\tb=", bh);
	g_free (bh);

	if (!header_digest (fields, G_MAXUINT, headers, raw->str, hc, instance != 0, digest, error))
		goto done;

	sig = _g_mime_dkim_sign_digest (ed, private_key, digest, error);

	if (!sig)
		goto done;

	sig_data = g_bytes_get_data (sig, &sig_size);
	b = g_base64_encode (sig_data, sig_size);
	g_bytes_unref (sig);

	for (gsize i = 0; b[i]; i++) {
		if (i && i % 72 == 0)
			g_string_append (raw, "\r\n\t");

		g_string_append_c (raw, b[i]);
	}

	g_free (b);
	result = g_string_free (raw, FALSE);
	raw = NULL;

done:
	if (raw)
		g_string_free (raw, TRUE);

	g_clear_pointer (&defaults, g_ptr_array_unref);

	return result;
}

/**
 * g_mime_dkim_sign:
 * @message: message to sign in place
 * @domain: signing domain (ASCII A-labels)
 * @selector: DNS selector (ASCII)
 * @algorithm: RSA-SHA256 or Ed25519-SHA256
 * @private_key: unencrypted private key in PEM format
 * @header_canon: header canonicalization
 * @body_canon: body canonicalization
 * @headers: (array zero-terminated=1) (nullable): names in signing order, or %NULL for common author, recipient and content fields
 * @error: return location for a #GError
 *
 * Signs the entire body and prepends a DKIM-Signature without modifying
 * the MIME body. Names may be repeated to sign additional occurrences
 * or prevent insertion of absent fields. From must be present and signed.
 *
 * Returns: %TRUE on success
 */
gboolean
g_mime_dkim_sign (GMimeMessage *message, const char *domain, const char *selector,
		  GMimeDkimAlgorithm algorithm, GBytes *private_key,
		  GMimeDkimCanonicalization header_canon, GMimeDkimCanonicalization body_canon,
		  const char * const *headers, GError **error)
{
	GPtrArray *fields;
	GMimeHeader *header;
	char *raw = NULL;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);

	if (!(fields = _g_mime_dkim_get_headers (message, error)))
		return FALSE;

	raw = _g_mime_dkim_sign_header (message, fields, 0, domain, selector, algorithm, private_key,
					header_canon, body_canon, headers, error);
	g_ptr_array_unref (fields);

	if (!raw)
		return FALSE;

	header = _g_mime_dkim_create_header (raw, error);
	g_free (raw);

	if (!header)
		return FALSE;

	g_mime_header_list_insert_header (g_mime_entity_get_header_list (GMIME_ENTITY (message)), 0, header);
	g_object_unref (header);

	return TRUE;
}

/**
 * g_mime_dkim_verify:
 * @message: message to verify, before modifying signed headers or content
 * @signature_index: zero-based DKIM-Signature occurrence, counting from the top
 * @resolver: (nullable): TXT record resolver, or %NULL for the default resolver
 * @cancellable: (nullable): optional cancellable for the synchronous DNS query
 * @error: return location for a #GError
 *
 * Verifies one signature independently of all other signatures. A valid
 * signature authenticates the signing domain, not necessarily the author.
 * With l=, only that many canonicalized body octets were signed; unsigned
 * fields and appended content are not authenticated.
 *
 * Returns: %TRUE only when the selected signature and body hash both verify
 */
gboolean
g_mime_dkim_verify (GMimeMessage *message, guint signature_index, GResolver *resolver,
		    GCancellable *cancellable, GError **error)
{
	GPtrArray *fields;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	if (!(fields = _g_mime_dkim_get_headers (message, error)))
		return FALSE;

	for (guint i = 0; i < fields->len; i++) {
		if (header_is (fields->pdata[i], "DKIM-Signature") && !signature_index--) {
			gboolean valid = _g_mime_dkim_verify_header (message, fields, i, FALSE, resolver, cancellable, error);
			g_ptr_array_unref (fields);
			return valid;
		}
	}

	g_ptr_array_unref (fields);

	return invalid (error, "DKIM signature not found");
}

static gboolean
signed_name (const char *name, gboolean ams)
{
	if (!field_name (name, strlen (name)) || strchr (name, ';'))
		return FALSE;

	/* RFC 8617 §4.1.2: an intermediary may delete Authentication-Results;
	 * the ARC-Seal, not the AMS, binds the ARC fields into a chain. */
	return !ams || (g_ascii_strcasecmp (name, "Authentication-Results") &&
			g_ascii_strcasecmp (name, "ARC-Authentication-Results") &&
			g_ascii_strcasecmp (name, "ARC-Message-Signature") &&
			g_ascii_strcasecmp (name, "ARC-Seal"));
}

/* RFC 6376 §5.4.2. Consume one occurrence per h= entry, bottommost
 * first. Oversigning a missing occurrence hashes nothing (not "Name:").
 * Exclude only the current signature; another DKIM-Signature is ordinary
 * header data, including its b= value. */
static gboolean
header_digest (GPtrArray *fields_raw, guint skip, const char * const *names,
	       const char *unsigned_header, gboolean relaxed, gboolean ams,
	       guint8 digest[32], GError **error)
{
	GHashTable *fields = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_queue_free);
	GChecksum *sum = g_checksum_new (G_CHECKSUM_SHA256);
	gboolean signed_from = FALSE;
	gboolean valid = FALSE;
	guint from_count = 0;

	for (guint i = 0; i < fields_raw->len; i++) {
		const char *raw = fields_raw->pdata[i];
		char *name = NULL;
		GQueue *stack;

		if (i == skip)
			continue;

		name = g_ascii_strdown (raw, name_length (raw, strchr (raw, ':')));
		from_count += !strcmp (name, "from");

		if (!(stack = g_hash_table_lookup (fields, name))) {
			stack = g_queue_new ();
			g_hash_table_insert (fields, name, stack);
		} else {
			g_free (name);
		}

		g_queue_push_head (stack, (gpointer) raw);
	}

	for (guint i = 0; names[i]; i++) {
		char *name = g_ascii_strdown (names[i], -1);
		const char *raw;
		GQueue *stack;

		if (!signed_name (name, ams)) {
			invalid (error, "Invalid or prohibited signed header name");
			g_free (name);
			goto done;
		}

		stack = g_hash_table_lookup (fields, name);

		if (stack && (raw = g_queue_pop_head (stack))) {
			_g_mime_dkim_hash_header (sum, raw, relaxed, TRUE);
			signed_from |= !strcmp (name, "from");
		}

		g_free (name);
	}

	/* A signature over one From in a malformed message with two does not
	 * necessarily authenticate the author shown to the reader (§8.15). */
	if (!signed_from || from_count != 1) {
		invalid (error, "Message must contain one From field and it must be signed");
		goto done;
	}

	/* The signature field terminates the hash without a final CRLF. */
	_g_mime_dkim_hash_header (sum, unsigned_header, relaxed, FALSE);
	finish_hash (sum, digest);
	valid = TRUE;

done:
	g_hash_table_unref (fields);
	g_checksum_free (sum);

	return valid;
}

static gboolean
number (const char *str, gsize max_digits, guint64 *result)
{
	guint64 n = 0;

	if (!str || !*str || strlen (str) > max_digits)
		return FALSE;

	for (; *str; str++) {
		if (!g_ascii_isdigit (*str) || n > (G_MAXUINT64 - (*str - '0')) / 10)
			return FALSE;

		n = n * 10 + (*str - '0');
	}

	*result = n;

	return TRUE;
}

static char *
unquote_dkim (const char *str)
{
	GString *text = g_string_new (NULL);

	for (; *str; str++) {
		int hi, lo, c;

		if (wsp (*str))
			continue;

		if (*str != '=') {
			g_string_append_c (text, *str);
			continue;
		}

		do {
			str++;
		} while (wsp (*str));

		if ((hi = g_ascii_xdigit_value (*str)) < 0 || g_ascii_islower (*str))
			goto malformed;

		do {
			 str++;
		} while (wsp (*str));

		if ((lo = g_ascii_xdigit_value (*str)) < 0 || g_ascii_islower (*str))
			goto malformed;

		c = hi * 16 + lo;

		if (!c)
			goto malformed;

		g_string_append_c (text, c);
	}

	return g_string_free (text, FALSE);

malformed:
	g_string_free (text, TRUE);

	return NULL;
}

static gboolean
valid_identity (char *identity, const char *domain, gboolean *subdomain)
{
	char *at = identity ? strrchr (identity, '@') : NULL;
	gsize n, d = strlen (domain);
	const char *inptr;

	if (!at || !_g_mime_dkim_valid_domain (at + 1, FALSE))
		return FALSE;

	n = strlen (at + 1);

	if (n < d || g_ascii_strcasecmp (at + 1 + n - d, domain) || (n > d && at[n - d] != '.'))
		return FALSE;

	*subdomain = n != d;
	*at = '\0';

	if (*identity == '"') {
		inptr = identity + 1;

		while (*inptr && *inptr != '"') {
			if (*inptr == '\\')
				inptr++;

			if ((guchar) *inptr < 32 || (guchar) *inptr > 126)
				return FALSE;

			inptr++;
		}

		return *inptr == '"' && !inptr[1];
	}

	for (inptr = identity; *inptr; inptr++) {
		if (*inptr == '.') {
			if (inptr == identity || !inptr[1] || inptr[1] == '.')
				return FALSE;
		} else if (!g_ascii_isalnum (*inptr) && !strchr ("!#$%&'*+-/=?^_`{|}~", *inptr)) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
valid_auid (const char *s, const char *domain, gboolean *subdomain)
{
	char *identity = unquote_dkim (s);
	gboolean valid = valid_identity (identity, domain, subdomain);

	g_free (identity);

	return valid;
}

static gboolean
query_supported (const char *q)
{
	char **items = g_strsplit (q, ":", -1);
	gboolean dns = FALSE;

	for (guint i = 0; items[i]; i++) {
		const char *inptr = g_strstrip (items[i]);

		dns |= !strcmp (inptr, "dns/txt");

		if (!g_ascii_isalpha (*inptr))
			goto malformed;

		while (g_ascii_isalnum (*inptr) || *inptr == '-')
			inptr++;

		if (inptr[-1] == '-' || (*inptr && *inptr != '/'))
			goto malformed;

		if (*inptr) {
			char *args = unquote_dkim (inptr + 1);
			gboolean valid = args && !strchr (inptr + 1, '|');

			g_free (args);

			if (!valid)
				goto malformed;
		}
	}

	g_strfreev (items);

	return dns;

malformed:
	g_strfreev (items);

	return FALSE;
}

static gboolean
valid_copies (const char *z)
{
	char **fields = g_strsplit (z, "|", -1);
	gboolean valid = *fields != NULL;

	for (guint i = 0; fields[i]; i++) {
		char *field = g_strstrip (fields[i]);
		char *colon = strchr (field, ':');
		char *value = NULL;

		if (!colon || !field_name (field, name_length (field, colon)) || !(value = unquote_dkim (colon + 1))) {
			valid = FALSE;
			break;
		}

		g_free (value);
	}

	g_strfreev (fields);

	return valid;
}

static gboolean
canonicalizations (const char *c, gboolean *header, gboolean *body)
{
	char **parts = g_strsplit (c ? c : "simple", "/", 2);
	gboolean valid;

	if ((valid = parts[0] && (!strcmp (parts[0], "simple") || !strcmp (parts[0], "relaxed")))) {
		*header = !strcmp (parts[0], "relaxed");
		*body = parts[1] && !strcmp (parts[1], "relaxed");
		valid = !parts[1] || *body || !strcmp (parts[1], "simple");
	}

	g_strfreev (parts);

	return valid;
}

static gboolean
verify_tags (GMimeMessage *message, GPtrArray *fields, guint index, gboolean ams, GHashTable *tags,
			    GResolver *resolver, GCancellable *cancellable, GError **error)
{
	const char *a, *h, *domain, *selector, *identity, *l, *t, *x, *q, *z;
	guint64 length = G_MAXUINT64, timestamp = 0, expires, instance;
	GBytes *signature = NULL, *bh = NULL, *key = NULL;
	const char *raw = fields->pdata[index];
	gboolean ed, hc, bc, subdomain = FALSE;
	GError *verify_error = NULL;
	gboolean valid = FALSE;
	char **names = NULL;
	char *blank = NULL;
	guint8 digest[32];

	if (!header_is (raw, ams ? "ARC-Message-Signature" : "DKIM-Signature"))
		return invalid (error, "Unexpected signature field");

	identity = g_hash_table_lookup (tags, "i");

	if (ams) {
		if (!number (identity, 2, &instance) || instance < 1 || instance > 50)
			return invalid (error, "Invalid ARC instance");
	} else if (g_strcmp0 (g_hash_table_lookup (tags, "v"), "1")) {
		return invalid (error, "Missing or unsupported DKIM version");
	}

	domain = g_hash_table_lookup (tags, "d");
	selector = g_hash_table_lookup (tags, "s");

	if (!_g_mime_dkim_valid_domain (domain, FALSE) || !_g_mime_dkim_valid_domain (selector, TRUE))
		return invalid (error, "Invalid signing domain or selector");

	if (!ams && identity && !valid_auid (identity, domain, &subdomain))
		return invalid (error, "Invalid or unaligned signing identity");

	q = g_hash_table_lookup (tags, "q");
	z = g_hash_table_lookup (tags, "z");

	if ((q && !query_supported (q)) || (z && !valid_copies (z)))
		return invalid (error, "Invalid query method or copied headers");

	a = g_hash_table_lookup (tags, "a");
	ed = !g_strcmp0 (a, "ed25519-sha256");

	if (!ed && g_strcmp0 (a, "rsa-sha256"))
		return invalid (error, "Missing or unsupported signature algorithm");

	t = g_hash_table_lookup (tags, "t");
	x = g_hash_table_lookup (tags, "x");
	l = g_hash_table_lookup (tags, "l");

	if ((t && !number (t, 12, &timestamp)) ||
	    (x && (!number (x, 12, &expires) || (t && expires <= timestamp) ||
		   expires < (guint64) (g_get_real_time () / G_USEC_PER_SEC))))
		return invalid (error, "Malformed timestamp or expired signature");

	if (l && !number (l, 76, &length))
		return invalid (error, "Invalid signed body length");

	if (!canonicalizations (g_hash_table_lookup (tags, "c"), &hc, &bc))
		return invalid (error, "Invalid canonicalization algorithm");

	bh = _g_mime_dkim_decode_base64 (g_hash_table_lookup (tags, "bh"));
	signature = _g_mime_dkim_decode_base64 (g_hash_table_lookup (tags, "b"));
	h = g_hash_table_lookup (tags, "h");

	if (!bh || g_bytes_get_size (bh) != 32 || !signature ||
	    (ed && g_bytes_get_size (signature) != 64) || !h || !*h) {
		invalid (error, "Missing or malformed bh=, b=, or h= tag");
		goto done;
	}

	names = g_strsplit (h, ":", -1);

	for (guint i = 0; names[i]; i++)
		g_strstrip (names[i]);

	blank = _g_mime_dkim_strip_b (raw);

	/* A body hash mismatch already invalidates the signature; no
	 * selector lookup or public-key operation is needed in that case. */
	if (!body_digest (message, bc, l != NULL, length, digest, error))
		goto done;

	if (memcmp (digest, g_bytes_get_data (bh, NULL), 32)) {
		invalid (error, "DKIM body hash mismatch");
		goto done;
	}

	if (!header_digest (fields, index, (const char * const *) names, blank, hc, ams, digest, error))
		goto done;

	if (!(key = _g_mime_dkim_lookup_key_full (resolver, domain, selector, ed, subdomain, cancellable, error)))
		goto done;

	if (!(valid = _g_mime_dkim_verify_digest (ed, key, digest, signature, &verify_error))) {
		if (verify_error)
			g_propagate_error (error, verify_error);
		else
			invalid (error, "DKIM signature mismatch");
	}

done:
	g_clear_pointer (&key, g_bytes_unref);
	g_clear_pointer (&bh, g_bytes_unref);
	g_clear_pointer (&signature, g_bytes_unref);
	g_free (blank);
	g_strfreev (names);

	return valid;
}

gboolean
_g_mime_dkim_verify_header (GMimeMessage *message, GPtrArray *fields, guint index, gboolean ams,
			    GResolver *resolver, GCancellable *cancellable, GError **error)
{
	GHashTable *tags;
	gboolean valid;

	if (!(tags = _g_mime_dkim_parse_tags (fields->pdata[index], error)))
		return FALSE;

	valid = verify_tags (message, fields, index, ams, tags, resolver, cancellable, error);
	g_hash_table_unref (tags);

	return valid;
}
