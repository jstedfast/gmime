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

#include "gmime-message-partial.h"
#include "gmime-visitor.h"
#include "gmime-stream-cat.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-parse-utils.h"
#include "gmime-parser.h"
#include "gmime-utils.h"


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


GType
g_mime_message_partial_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMessagePartialClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_message_partial_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMessagePartial),
			0,    /* n_preallocs */
			NULL, /* instance_init */
		};
		
		type = g_type_register_static (GMIME_TYPE_PART, "GMimeMessagePartial", &info, 0);
	}
	
	return type;
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
	GMimeContentType *content_type = ((GMimeEntity *) partial)->content_type;
	const char *value;
	gint32 number;

	if (!content_type || !(value = g_mime_content_type_get_parameter (content_type, name)))
		return -1;

	value += strspn (value, " \t\r\n\v\f");

	if (!_g_mime_parse_int32 (&value, &number))
		return -1;

	value += strspn (value, " \t\r\n\v\f");

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

	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), NULL);
	
	return entity->content_type ? g_mime_content_type_get_parameter (entity->content_type, "id") : NULL;
}


/**
 * g_mime_message_partial_get_number:
 * @partial: message/partial object
 *
 * Gets the message/partial part number.
 *
 * Returns: the message/partial part number or %-1 if absent, malformed,
 * or outside the nonnegative 32-bit integer range. Surrounding ASCII
 * whitespace is allowed, but signs and trailing non-whitespace are not.
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
 * the nonnegative 32-bit integer range. Surrounding ASCII whitespace is
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
	
	/* Note: reconstruct_message() has already validated that every partial has a number. */
	num1 = g_mime_message_partial_get_number (*partial1);
	num2 = g_mime_message_partial_get_number (*partial2);
	
	return num1 - num2;
}


/**
 * g_mime_message_partial_reconstruct_message:
 * @partials: (array length=num) (element-type GMimeMessagePartial): an array of message/partial mime parts
 * @num: the number of elements in @partials
 *
 * Reconstructs the GMimeMessage from the given message/partial parts
 * in @partials.
 *
 * Returns: (transfer full): a GMimeMessage object on success or %NULL
 * on fail.
 **/
GMimeMessage *
g_mime_message_partial_reconstruct_message (GMimeMessagePartial **partials, size_t num)
{
	GMimeMessagePartial *partial;
	GMimeStream *cat, *stream;
	GMimeContent *wrapper;
	GMimeMessage *message;
	GMimeParser *parser;
	int total, number;
	const char *id;
	size_t i;
	
	if (num == 0 || !(id = g_mime_message_partial_get_id (partials[0])))
		return NULL;

	for (i = 0; i < num; i++) {
		const char *partial_id;

		partial = partials[i];

		/* sanity check to make sure this part belongs... */
		partial_id = g_mime_message_partial_get_id (partial);
		if (!partial_id || strcmp (id, partial_id))
			return NULL;

		if (g_mime_message_partial_get_number (partial) == -1)
			return NULL;
	}
	
	/* get them into the correct order... */
	qsort ((void *) partials, num, sizeof (gpointer), partial_compare);
	
	/* only the last message/partial part is REQUIRED to have the total parameter */
	if ((total = g_mime_message_partial_get_total (partials[num - 1])) == -1)
		return NULL;
	
	if (num != (size_t) total)
		return NULL;
	
	cat = g_mime_stream_cat_new ();
	
	for (i = 0; i < num; i++) {
		partial = partials[i];
		
		/* sanity check to make sure we aren't missing any parts */
		number = g_mime_message_partial_get_number (partial);
		if ((size_t) number != i + 1)
			goto exception;
		
		wrapper = g_mime_part_get_content ((GMimePart *) partial);
		stream = g_mime_content_get_stream (wrapper);
		
		g_mime_stream_reset (stream);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
	}
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, cat);
	g_object_unref (cat);
	
	message = g_mime_parser_construct_message (parser, NULL, NULL);
	g_object_unref (parser);
	
	return message;
	
 exception:
	
	g_object_unref (cat);
	
	return NULL;
}


static GMimeMessage *
message_partial_message_new (GMimeMessage *base)
{
	GMimeHeaderList *headers;
	GMimeMessage *message;
	GMimeHeader *header, *copy;
	int count, i;
	
	message = g_mime_message_new (FALSE);
	
	headers = ((GMimeEntity *) base)->headers;
	
	count = g_mime_header_list_get_count (headers);
	
	for (i = 0; i < count; i++) {
		header = g_mime_header_list_get_header_at (headers, i);
		copy = g_mime_header_clone (header);
		copy->offset = header->offset;
		g_mime_header_list_add (((GMimeEntity *) message)->headers, copy);
		g_object_unref (copy);
	}
	
	return message;
}


/**
 * g_mime_message_partial_split_message:
 * @message: message object
 * @max_size: max size
 * @nparts: (out): number of parts
 *
 * Splits @message into an array of #GMimeMessage objects each
 * containing a single #GMimeMessagePartial object containing
 * @max_size bytes or fewer. @nparts is set to the number of
 * #GMimeMessagePartial objects created.
 *
 * Returns: (nullable) (transfer full) (array length=nparts): an array of #GMimeMessage objects and
 * sets @nparts to the number of messages returned or %NULL on fail.
 **/
GMimeMessage **
g_mime_message_partial_split_message (GMimeMessage *message, size_t max_size, size_t *nparts)
{
	GMimeMessage **messages;
	GMimeMessagePartial *partial;
	GMimeStream *stream, *substream;
	GMimeFormatOptions *options;
	GMimeContent *wrapper;
	char *generated_id = NULL;
	const unsigned char *buf;
	GPtrArray *parts;
	gint64 len, end;
	const char *id;
	gint64 start;
	guint i;
	
	*nparts = 0;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	options = g_mime_format_options_get_default ();
	stream = g_mime_stream_mem_new ();
	
	if (g_mime_entity_write_to_stream ((GMimeEntity *) message, options, stream) == -1) {
		g_object_unref (stream);
		return NULL;
	}
	
	g_mime_stream_reset (stream);
	
	len = g_mime_stream_length (stream);
	
	/* optimization */
	if (len <= (gint64) max_size) {
		g_object_unref (stream);
		g_object_ref (message);
		
		messages = g_malloc (sizeof (void *));
		messages[0] = message;
		*nparts = 1;
		
		return messages;
	}
	
	start = 0;
	parts = g_ptr_array_new ();
	buf = ((GMimeStreamMem *) stream)->buffer->data;
	
	while (start < len) {
		/* Preferably, we'd split on whole-lines if we can,
		 * but if that's not possible, split on max size */
		if ((end = MIN (len, start + max_size)) < len) {
			register gint64 ebx; /* end boundary */
			
			ebx = end;
			while (ebx > (start + 1) && ebx[buf] != '\n')
				ebx--;
			
			if (ebx[buf] == '\n')
				end = ebx + 1;
		}
		
		substream = g_mime_stream_substream (stream, start, end);
		g_ptr_array_add (parts, substream);
		start = end;
	}
	
	id = g_mime_message_get_message_id (message);
	if (!id)
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
