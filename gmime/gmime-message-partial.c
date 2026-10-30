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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "gmime-message-partial.h"
#include "gmime-visitor.h"
#include "gmime-stream-cat.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-parse-utils.h"
#include "gmime-parser.h"
#include "gmime-utils.h"
#include "gmime-error.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-message-partial
 * @title: GMimeMessagePartial
 * @short_description: Partial MIME parts
 * @see_also:
 *
 * A #GMimeMessagePartial represents the message/partial MIME part.
 **/

/* GObject class methods */
static void g_mime_message_partial_class_init (GMimeMessagePartialClass *klass);

static void
message_partial_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_message_partial (visitor, (GMimeMessagePartial *) object);
}


G_DEFINE_TYPE (GMimeMessagePartial, g_mime_message_partial, GMIME_TYPE_PART)

static void
g_mime_message_partial_init (GMimeMessagePartial *partial)
{
}


static void
g_mime_message_partial_class_init (GMimeMessagePartialClass *klass)
{
	GMimeEntityClass *object_class = GMIME_ENTITY_CLASS (klass);

	object_class->accept = message_partial_accept;
}

static int
message_partial_get_integer (GMimeMessagePartial *partial, const char *name)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private ((GMimeEntity *) partial);
	GMimeContentType *content_type = priv->content_type;
	const char *value;
	gint32 number;

	if (!content_type || !(value = g_mime_content_type_get_parameter (content_type, name)))
		return -1;

	skip_lwsp (&value);

	if (!_g_mime_parse_int32 (&value, &number))
		return -1;

	skip_lwsp (&value);

	return *value ? -1 : number;
}


/**
 * g_mime_message_partial_new:
 * @id: message/partial part id
 * @number: message/partial part number, starting at 1
 * @total: total number of message/partial parts, at least @number
 *
 * Creates a new MIME message/partial object.
 * The @id may be empty, but must be a non-%NULL UTF-8 string.
 *
 * Returns: an empty MIME message/partial object.
 **/
GMimeMessagePartial *
g_mime_message_partial_new (const char *id, int number, int total)
{
	GMimeContentType *content_type;
	GMimeMessagePartial *partial;
	char *num;

	g_return_val_if_fail (id != NULL && g_utf8_validate (id, -1, NULL), NULL);
	g_return_val_if_fail (number >= 1, NULL);
	g_return_val_if_fail (total >= number, NULL);
	
	partial = g_object_new (GMIME_TYPE_MESSAGE_PARTIAL, NULL);
	
	content_type = g_mime_content_type_new ("message", "partial");
	
	g_mime_content_type_set_parameter (content_type, "id", id);
	
	num = g_strdup_printf ("%d", number);
	g_mime_content_type_set_parameter (content_type, "number", num);
	g_free (num);
	
	num = g_strdup_printf ("%d", total);
	g_mime_content_type_set_parameter (content_type, "total", num);
	g_free (num);
	
	g_mime_entity_set_content_type ((GMimeEntity *) partial, content_type);
	g_object_unref (content_type);
	
	return partial;
}


/**
 * g_mime_message_partial_get_id:
 * @partial: message/partial object
 *
 * Gets the message/partial id parameter value.
 *
 * Returns: (nullable): the borrowed message/partial id, or %NULL if absent.
 * The value is valid until the Content-Type or its parameters change.
 **/
const char *
g_mime_message_partial_get_id (GMimeMessagePartial *partial)
{
	GMimeEntity *entity = (GMimeEntity *) partial;
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), NULL);
	
	priv = _g_mime_entity_get_private (entity);

	return priv->content_type ? g_mime_content_type_get_parameter (priv->content_type, "id") : NULL;
}


/**
 * g_mime_message_partial_get_number:
 * @partial: message/partial object
 *
 * Gets the message/partial part number.
 *
 * Returns: the message/partial part number or %-1 if absent, malformed,
 * or outside the nonnegative 32-bit integer range. Surrounding ASCII
 * linear whitespace is allowed, but signs and trailing non-whitespace are not.
 **/
int
g_mime_message_partial_get_number (GMimeMessagePartial *partial)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), -1);
	
	return message_partial_get_integer (partial, "number");
}


/**
 * g_mime_message_partial_get_total:
 * @partial: message/partial object
 *
 * Gets the total number of message/partial parts needed to
 * reconstruct the original message.
 *
 * Returns: the total number of message/partial parts needed to
 * reconstruct the original message or %-1 if absent, malformed, or outside
 * the nonnegative 32-bit integer range. Surrounding ASCII linear whitespace is
 * allowed, but signs and trailing non-whitespace are not.
 **/
int
g_mime_message_partial_get_total (GMimeMessagePartial *partial)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), -1);
	
	return message_partial_get_integer (partial, "total");
}


static int
partial_compare (const void *v1, const void *v2)
{
	GMimeMessagePartial **partial1 = (GMimeMessagePartial **) v1;
	GMimeMessagePartial **partial2 = (GMimeMessagePartial **) v2;
	int num1, num2;
	
	/* Note: join() has already validated that every partial has a number. */
	num1 = g_mime_message_partial_get_number (*partial1);
	num2 = g_mime_message_partial_get_number (*partial2);
	
	return num1 - num2;
}

static gboolean
partial_preserves_header (GMimeHeaderId id)
{
	return id == GMIME_HEADER_SUBJECT || id == GMIME_HEADER_MESSAGE_ID ||
		id == GMIME_HEADER_ENCRYPTED || id == GMIME_HEADER_MIME_VERSION;
}

static void
combine_headers (GMimeMessage *message, GMimeMessage *joined)
{
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) message);
	GMimeHeaderList *outer = entity_priv->headers;
	GMimeEntityPrivate *joined_entity_priv = _g_mime_entity_get_private ((GMimeEntity *) joined);
	GMimeMessagePrivate *joined_priv = _g_mime_message_get_private (joined);
	GMimeHeaderList *inner = joined_entity_priv->headers;
	GPtrArray *headers = g_ptr_array_new_with_free_func (g_object_unref);
	GMimeHeader *header, *original, *clone;
	int i = 0, index;
	guint j;

	/* RFC2046: Any header fields in the enclosed message which do not start with "Content-"
	 * (except for the "Subject", "Message-ID", "Encrypted", and "MIME-Version" fields) will
	 * be ignored and dropped. */
	while (i < g_mime_header_list_get_count (inner)) {
		header = g_mime_header_list_get_header_at (inner, i);

		if (partial_preserves_header (g_mime_header_get_id (header))) {
			g_ptr_array_add (headers, g_object_ref (header));
			_g_mime_header_set_offset (header, -1);
			i++;
		} else {
			g_mime_header_list_remove_at (inner, i);
		}
	}

	/* RFC2046: All the header fields from the initial enclosing message, except
	 * those that start with "Content-" and the specific header fields "Subject",
	 * "Message-ID", "Encrypted", and "MIME-Version", must be copied, in order,
	 * to the new message. */
	index = 0;

	for (i = 0; i < g_mime_header_list_get_count (outer); i++) {
		header = g_mime_header_list_get_header_at (outer, i);

		if (partial_preserves_header (g_mime_header_get_id (header))) {
			for (j = 0; j < headers->len; j++) {
				original = headers->pdata[j];

				if (g_mime_header_get_id (original) == g_mime_header_get_id (header)) {
					g_mime_header_list_remove_header (inner, original);
					g_mime_header_list_insert_header (inner, index++, original);
					g_ptr_array_remove_index (headers, j);
					break;
				}
			}
		} else {
			clone = g_mime_header_clone (header);
			_g_mime_header_set_offset (clone, -1);
			g_mime_header_list_insert_header (inner, index++, clone);
			g_object_unref (clone);
		}
	}

	g_ptr_array_unref (headers);

	if (joined_priv->mime_part) {
		GMimeEntityPrivate *part_priv = _g_mime_entity_get_private (joined_priv->mime_part);

		inner = part_priv->headers;

		for (i = 0; i < g_mime_header_list_get_count (inner); i++)
			_g_mime_header_set_offset (g_mime_header_list_get_header_at (inner, i), -1);
	}
}


/**
 * g_mime_message_partial_join:
 * @options: (nullable): parser options, or %NULL for the defaults
 * @message: the message containing the first numbered fragment
 * @partials: (nullable) (array length=num) (element-type GMimeMessagePartial): an array of message/partial mime parts,
 *   or %NULL when @num is zero
 * @num: the number of elements in @partials
 * @error: (nullable): return location for a #GError
 *
 * Reconstructs the GMimeMessage from the given message/partial parts
 * in @partials without changing their order in the caller's array.
 * Each fragment's content is decoded before concatenation; fragments
 * without content contribute no bytes. Only the last fragment is
 * required to specify the total number of parts. Parsed body content is
 * stored in memory rather than as substreams of the decoded readers.
 * Envelope headers are combined with those from @message using RFC 2046
 * rules. The source message and fragments are not changed.
 * Matching absent fragment ids are permitted.
 *
 * Returns: (nullable) (transfer full): the joined message, or %NULL for an
 * empty array or on failure. Invalid fragment metadata and I/O or parsing
 * failures set @error; an empty array does not.
 **/
GMimeMessage *
g_mime_message_partial_join (GMimeParserOptions *options, GMimeMessage *message,
			    GMimeMessagePartial **partials, size_t num, GError **error)
{
	GMimeMessagePartial *partial, **parts;
	GMimeStream *cat, *stream;
	GMimeContent *wrapper;
	GMimeMessage *joined;
	int total, number, errsv;
	const char *id = NULL;
	size_t i;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (partials != NULL || num == 0, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (num == 0)
		return NULL;

	if (num > G_MAXINT32) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     "The number of partials exceeds the supported count.");
		return NULL;
	}

	for (i = 0; i < num; i++) {
		const char *partial_id;

		partial = partials[i];

		g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), NULL);

		/* sanity check to make sure this part belongs... */
		partial_id = g_mime_message_partial_get_id (partial);
		if (i == 0)
			id = partial_id;

		if (g_strcmp0 (id, partial_id)) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "Partial messages have mismatching identifiers.");
			return NULL;
		}

		if (g_mime_message_partial_get_number (partial) == -1) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "One or more partial messages have missing numbers.");
			return NULL;
		}
	}
	
	parts = g_new (GMimeMessagePartial *, num);
	memcpy (parts, partials, num * sizeof (*parts));
	qsort (parts, num, sizeof (*parts), partial_compare);
	
	/* only the last message/partial part is REQUIRED to have the total parameter */
	total = g_mime_message_partial_get_total (parts[num - 1]);
	if (total == -1 || num != (size_t) total) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     total == -1 ? "The last partial does not have a total." :
				     "The number of partials does not match the expected count.");
		g_free (parts);
		return NULL;
	}
	
	cat = g_mime_stream_cat_new ();
	
	/* chain all the partial content streams... */
	for (i = 0; i < num; i++) {
		partial = parts[i];
		
		/* sanity check to make sure we aren't missing any parts */
		number = g_mime_message_partial_get_number (partial);
		if ((size_t) number != i + 1) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "One or more partials is missing.");
			goto exception;
		}

		if (!(wrapper = g_mime_part_get_content ((GMimePart *) partial)))
			continue;

		if (!g_mime_content_get_stream (wrapper)) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "Partial content has no backing stream.");
			goto exception;
		}

		errno = 0;

		if (!(stream = g_mime_content_open (wrapper))) {
			errsv = errno;
			g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
				     "Failed to open partial content: %s",
				     errsv ? g_strerror (errsv) : "unknown I/O error");
			goto exception;
		}

		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
	}

	g_free (parts);
	
	joined = g_mime_message_load (options, cat, error);
	g_object_unref (cat);

	if (joined)
		combine_headers (message, joined);
	
	return joined;
	
 exception:
	
	g_object_unref (cat);
	g_free (parts);
	
	return NULL;
}


static GMimeMessage *
message_partial_message_new (GMimeMessage *base)
{
	GMimeEntityPrivate *base_priv, *priv;
	GMimeHeaderList *headers, *cloned;
	GMimeHeader *header, *copy;
	GMimeMessage *message;
	char *id, *value;
	int count, i;
	
	message = g_mime_message_new (FALSE);
	base_priv = _g_mime_entity_get_private ((GMimeEntity *) base);

	headers = base_priv->headers;
	priv = _g_mime_entity_get_private ((GMimeEntity *) message);

	cloned = priv->headers;
	_g_mime_header_list_set_options (cloned, _g_mime_header_list_get_options (headers));
	
	count = g_mime_header_list_get_count (headers);
	
	for (i = 0; i < count; i++) {
		header = g_mime_header_list_get_header_at (headers, i);
		copy = g_mime_header_clone (header);

		g_mime_header_list_add (cloned, copy);
		g_object_unref (copy);
	}

	id = g_mime_utils_generate_message_id (NULL);
	value = g_strdup_printf ("<%s>", id);
	header = g_mime_header_new_with_id (GMIME_HEADER_MESSAGE_ID, value, NULL);

	g_mime_header_list_replace (cloned, header);
	g_object_unref (header);
	g_free (value);
	g_free (id);
	
	return message;
}


/**
 * g_mime_message_partial_split:
 * @message: message object
 * @max_size: positive target size for each fragment
 * @nparts: (out): number of parts
 * @error: (nullable): return location for a #GError
 *
 * Splits @message into an array of #GMimeMessage objects each
 * containing a single #GMimeMessagePartial object containing
 * @max_size bytes or fewer, preferring whole lines. A newline at the
 * target boundary is included in the preceding fragment, which can make
 * it one byte larger. @nparts is set to the number of messages returned.
 * If no splitting is needed, the returned array contains a reference to
 * the original message.
 *
 * Each fragment message has a new Message-Id, distinct from the original
 * and from the id shared by the message/partial bodies. The original
 * headers are otherwise cloned without changing the source message.
 * The serialized fragment payload excludes envelope headers, including
 * unrecognized extension fields; join() restores them from the first
 * enclosing message.
 *
 * Returns: (nullable) (transfer full) (array length=nparts): an array of #GMimeMessage objects and
 * sets @nparts to the number of messages returned or %NULL on fail.
 **/
GMimeMessage **
g_mime_message_partial_split (GMimeMessage *message, size_t max_size, size_t *nparts, GError **error)
{
	GMimeStreamMemPrivate *priv;
	GMimeMessage **messages;
	GMimeMessagePartial *partial;
	GMimeStream *stream, *substream;
	GMimeFormatOptions *options;
	GMimeContent *wrapper;
	char *generated_id = NULL;
	const unsigned char *buf;
	GMimeHeaderId header_id;
	GPtrArray *parts;
	gint64 len, end;
	const char *id;
	gint64 start;
	int errsv;
	guint i;

	g_return_val_if_fail (nparts != NULL, NULL);

	*nparts = 0;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	g_return_val_if_fail (max_size > 0, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	
	options = g_mime_format_options_clone (NULL);

	for (header_id = 0; header_id <= GMIME_HEADER_X_STATUS; header_id++) {
		if (!partial_preserves_header (header_id) &&
		    (header_id < GMIME_HEADER_CONTENT_ALTERNATIVE || header_id > GMIME_HEADER_CONTENT_TYPE))
			g_mime_format_options_add_hidden_header (options, g_mime_header_id_to_name (header_id));
	}

	/* hide all unknown headers when writing message headers. */
	_g_mime_format_options_set_hide_unknown_headers (options, TRUE);
	stream = g_mime_stream_mem_new ();
	
	errno = 0;

	if (g_mime_entity_write_to_stream ((GMimeEntity *) message, options, stream) == -1) {
		errsv = errno;
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to serialize the message: %s",
			     errsv ? g_strerror (errsv) : "unknown I/O error");
		g_mime_format_options_unref (options);
		g_object_unref (stream);
		return NULL;
	}

	g_mime_format_options_unref (options);

	g_mime_stream_reset (stream);
	
	len = g_mime_stream_length (stream);
	
	/* optimization */
	if ((guint64) len <= max_size) {
		g_object_unref (stream);
		g_object_ref (message);
		
		messages = g_malloc (sizeof (void *));
		messages[0] = message;
		*nparts = 1;
		
		return messages;
	}
	
	start = 0;
	parts = g_ptr_array_new_with_free_func (g_object_unref);
	priv = _g_mime_stream_mem_get_private ((GMimeStreamMem *) stream);

	buf = priv->buffer->data;
	
	while (start < len) {
		/* Preferably, we'd split on whole-lines if we can,
		 * but if that's not possible, split on max size */
		end = start + (gint64) MIN ((guint64) (len - start), max_size);

		if (end < len) {
			register gint64 ebx; /* end boundary */
			
			ebx = end;
			while (ebx > (start + 1) && ebx[buf] != '\n')
				ebx--;
			
			if (ebx[buf] == '\n')
				end = ebx + 1;
		}
		
		if (parts->len == G_MAXINT32) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "The number of partials exceeds the supported count.");
			g_ptr_array_unref (parts);
			g_object_unref (stream);
			return NULL;
		}

		substream = g_mime_stream_substream (stream, start, end);

		if (!substream) {
			errsv = errno;
			g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
				     "Failed to create a partial stream: %s",
				     errsv ? g_strerror (errsv) : "unknown I/O error");
			g_ptr_array_unref (parts);
			g_object_unref (stream);
			return NULL;
		}

		g_ptr_array_add (parts, substream);
		start = end;
	}

	if (!(id = g_mime_message_get_message_id (message)))
		id = generated_id = g_mime_utils_generate_message_id (NULL);
	
	for (i = 0; i < parts->len; i++) {
		partial = g_mime_message_partial_new (id, i + 1, parts->len);
		wrapper = g_mime_content_new_with_stream ((GMimeStream *) parts->pdata[i],
							       GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_object_unref (parts->pdata[i]);
		g_mime_part_set_content ((GMimePart *) partial, wrapper);
		g_object_unref (wrapper);
		
		parts->pdata[i] = message_partial_message_new (message);
		g_mime_message_set_mime_part ((GMimeMessage *) parts->pdata[i], (GMimeEntity *) partial);
		g_object_unref (partial);
	}
	
	g_free (generated_id);
	g_object_unref (stream);
	
	messages = (GMimeMessage **) parts->pdata;
	*nparts = parts->len;
	
	g_ptr_array_free (parts, FALSE);
	
	return messages;
}
