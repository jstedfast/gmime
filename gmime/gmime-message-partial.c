/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
#include "gmime-stream-cat.h"
#include "gmime-stream-mem.h"
#include "gmime-parser.h"


/**
 * SECTION: gmime-message-partial
 * @title: GMimeMessagePartial
 * @short_description: Partial MIME parts
 * @se_also:
 *
 * A #GMimeMessagePartial represents the message/partial MIME part.
 **/


/* GObject class methods */
static void g_mime_message_partial_class_init (GMimeMessagePartialClass *klass);
static void g_mime_message_partial_init (GMimeMessagePartial *catpart, GMimeMessagePartialClass *klass);
static void g_mime_message_partial_finalize (GObject *object);

/* GMimeObject class methods */
static void message_partial_prepend_header (GMimeObject *object, const char *header, const char *value);
static void message_partial_append_header (GMimeObject *object, const char *header, const char *value);
static void message_partial_set_header (GMimeObject *object, const char *header, const char *value);
static const char *message_partial_get_header (GMimeObject *object, const char *header);
static gboolean message_partial_remove_header (GMimeObject *object, const char *header);
static void message_partial_set_content_type (GMimeObject *object, GMimeContentType *content_type);


static GMimePartClass *parent_class = NULL;


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
			(GInstanceInitFunc) g_mime_message_partial_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_PART, "GMimeMessagePartial", &info, 0);
	}
	
	return type;
}


static void
g_mime_message_partial_class_init (GMimeMessagePartialClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_PART);
	
	gobject_class->finalize = g_mime_message_partial_finalize;
	
	object_class->prepend_header = message_partial_prepend_header;
	object_class->append_header = message_partial_append_header;
	object_class->remove_header = message_partial_remove_header;
	object_class->set_header = message_partial_set_header;
	object_class->get_header = message_partial_get_header;
	object_class->set_content_type = message_partial_set_content_type;
}

static void
g_mime_message_partial_init (GMimeMessagePartial *partial, GMimeMessagePartialClass *klass)
{
	partial->id = NULL;
	partial->number = -1;
	partial->total = -1;
}

static void
g_mime_message_partial_finalize (GObject *object)
{
	GMimeMessagePartial *partial = (GMimeMessagePartial *) object;
	
	g_free (partial->id);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
message_partial_prepend_header (GMimeObject *object, const char *header, const char *value)
{
	/* RFC 1864 states that you cannot set a Content-MD5 on a message part */
	if (!g_ascii_strcasecmp ("Content-MD5", header))
		return;
	
	GMIME_OBJECT_CLASS (parent_class)->prepend_header (object, header, value);
}

static void
message_partial_append_header (GMimeObject *object, const char *header, const char *value)
{
	/* RFC 1864 states that you cannot set a Content-MD5 on a message part */
	if (!g_ascii_strcasecmp ("Content-MD5", header))
		return;
	
	GMIME_OBJECT_CLASS (parent_class)->append_header (object, header, value);
}

static void
message_partial_set_header (GMimeObject *object, const char *header, const char *value)
{
	/* RFC 1864 states that you cannot set a Content-MD5 on a message part */
	if (!g_ascii_strcasecmp ("Content-MD5", header))
		return;
	
	GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
}

static const char *
message_partial_get_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
}

static gboolean
message_partial_remove_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
message_partial_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMimeMessagePartial *partial = (GMimeMessagePartial *) object;
	const char *value;
	
	value = g_mime_content_type_get_parameter (content_type, "id");
	g_free (partial->id);
	partial->id = g_strdup (value);
	
	value = g_mime_content_type_get_parameter (content_type, "number");
	partial->number = value ? strtol (value, NULL, 10) : -1;
	
	value = g_mime_content_type_get_parameter (content_type, "total");
	partial->total = value ? strtol (value, NULL, 10) : -1;
	
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}


/**
 * g_mime_message_partial_new:
 * @id: message/partial part id
 * @number: message/partial part number
 * @total: total number of message/partial parts
 *
 * Creates a new MIME message/partial object.
 *
 * Returns: an empty MIME message/partial object.
 **/
GMimeMessagePartial *
g_mime_message_partial_new (const char *id, int number, int total)
{
	GMimeContentType *content_type;
	GMimeMessagePartial *partial;
	char *num;
	
	partial = g_object_newv (GMIME_TYPE_MESSAGE_PARTIAL, 0, NULL);
	
	content_type = g_mime_content_type_new ("message", "partial");
	
	partial->id = g_strdup (id);
	g_mime_content_type_set_parameter (content_type, "id", id);
	
	partial->number = number;
	num = g_strdup_printf ("%d", number);
	g_mime_content_type_set_parameter (content_type, "number", num);
	g_free (num);
	
	partial->total = total;
	num = g_strdup_printf ("%d", total);
	g_mime_content_type_set_parameter (content_type, "total", num);
	g_free (num);
	
	g_mime_object_set_content_type (GMIME_OBJECT (partial), content_type);
	g_object_unref (content_type);
	
	return partial;
}


/**
 * g_mime_message_partial_get_id:
 * @partial: message/partial object
 *
 * Gets the message/partial id parameter value.
 *
 * Returns: the message/partial id or %NULL on fail.
 **/
const char *
g_mime_message_partial_get_id (GMimeMessagePartial *partial)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), NULL);
	
	return partial->id;
}


/**
 * g_mime_message_partial_get_number:
 * @partial: message/partial object
 *
 * Gets the message/partial part number.
 *
 * Returns: the message/partial part number or %-1 on fail.
 **/
int
g_mime_message_partial_get_number (GMimeMessagePartial *partial)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), -1);
	
	return partial->number;
}


/**
 * g_mime_message_partial_get_total:
 * @partial: message/partial object
 *
 * Gets the total number of message/partial parts needed to
 * reconstruct the original message.
 *
 * Returns: the total number of message/partial parts needed to
 * reconstruct the original message or -1 on fail.
 **/
int
g_mime_message_partial_get_total (GMimeMessagePartial *partial)
{
	g_return_val_if_fail (GMIME_IS_MESSAGE_PARTIAL (partial), -1);
	
	return partial->total;
}


static int
partial_compare (const void *v1, const void *v2)
{
	GMimeMessagePartial **partial1 = (GMimeMessagePartial **) v1;
	GMimeMessagePartial **partial2 = (GMimeMessagePartial **) v2;
	int num1, num2;
	
	num1 = g_mime_message_partial_get_number (*partial1);
	num2 = g_mime_message_partial_get_number (*partial2);
	
	return num1 - num2;
}


/**
 * g_mime_message_partial_reconstruct_message:
 * @partials: an array of message/partial mime parts
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
	GMimeDataWrapper *wrapper;
	GMimeStream *cat, *stream;
	GMimeMessage *message;
	GMimeParser *parser;
	int total, number;
	const char *id;
	size_t i;
	
	if (num == 0 || !(id = g_mime_message_partial_get_id (partials[0])))
		return NULL;
	
	/* get them into the correct order... */
	qsort ((void *) partials, num, sizeof (gpointer), partial_compare);
	
	/* only the last message/partial part is REQUIRED to have the total parameter */
	if ((total = g_mime_message_partial_get_total (partials[num - 1])) == -1)
		return NULL;
	
	if (num != (size_t) total)
		return NULL;
	
	cat = g_mime_stream_cat_new ();
	
	for (i = 0; i < num; i++) {
		const char *partial_id;
		
		partial = partials[i];
		
		/* sanity check to make sure this part belongs... */
		partial_id = g_mime_message_partial_get_id (partial);
		if (!partial_id || strcmp (id, partial_id))
			goto exception;
		
		/* sanity check to make sure we aren't missing any parts */
		if ((number = g_mime_message_partial_get_number (partial)) == -1)
			goto exception;
		
		if ((size_t) number != i + 1)
			goto exception;
		
		wrapper = g_mime_part_get_content_object (GMIME_PART (partial));
		stream = g_mime_data_wrapper_get_stream (wrapper);
		
		g_mime_stream_reset (stream);
		g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), stream);
	}
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, cat);
	g_object_unref (cat);
	
	message = g_mime_parser_construct_message (parser);
	g_object_unref (parser);
	
	return message;
	
 exception:
	
	g_object_unref (cat);
	
	return NULL;
}


static GMimeMessage *
message_partial_message_new (GMimeMessage *base)
{
	const char *name, *value;
	GMimeMessage *message;
	GMimeHeaderList *list;
	GMimeHeaderIter iter;
	
	message = g_mime_message_new (FALSE);
	
	list = ((GMimeObject *) base)->headers;
	
	if (g_mime_header_list_get_iter (list, &iter)) {
		do {
			name = g_mime_header_iter_get_name (&iter);
			value = g_mime_header_iter_get_value (&iter);
			g_mime_object_append_header ((GMimeObject *) message, name, value);
		} while (g_mime_header_iter_next (&iter));
	}
	
	return message;
}


/**
 * g_mime_message_partial_split_message:
 * @message: message object
 * @max_size: max size
 * @nparts: number of parts
 *
 * Splits @message into an array of #GMimeMessage objects each
 * containing a single #GMimeMessagePartial object containing
 * @max_size bytes or fewer. @nparts is set to the number of
 * #GMimeMessagePartial objects created.
 *
 * Returns: (transfer full): an array of #GMimeMessage objects and
 * sets @nparts to the number of messages returned or %NULL on fail.
 **/
GMimeMessage **
g_mime_message_partial_split_message (GMimeMessage *message, size_t max_size, size_t *nparts)
{
	GMimeMessage **messages;
	GMimeMessagePartial *partial;
	GMimeStream *stream, *substream;
	GMimeDataWrapper *wrapper;
	const unsigned char *buf;
	GPtrArray *parts;
	gint64 len, end;
	const char *id;
	gint64 start;
	guint i;
	
	*nparts = 0;
	
	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);
	
	stream = g_mime_stream_mem_new ();
	if (g_mime_object_write_to_stream (GMIME_OBJECT (message), stream) == -1) {
		g_object_unref (stream);
		return NULL;
	}
	
	g_mime_stream_reset (stream);
	
	len = g_mime_stream_length (stream);
	
	/* optimization */
	if (len <= max_size) {
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
	
	for (i = 0; i < parts->len; i++) {
		partial = g_mime_message_partial_new (id, i + 1, parts->len);
		wrapper = g_mime_data_wrapper_new_with_stream (GMIME_STREAM (parts->pdata[i]),
							       GMIME_CONTENT_ENCODING_DEFAULT);
		g_object_unref (parts->pdata[i]);
		g_mime_part_set_content_object (GMIME_PART (partial), wrapper);
		g_object_unref (wrapper);
		
		parts->pdata[i] = message_partial_message_new (message);
		g_mime_message_set_mime_part (GMIME_MESSAGE (parts->pdata[i]), GMIME_OBJECT (partial));
		g_object_unref (partial);
	}
	
	g_object_unref (stream);
	
	messages = (GMimeMessage **) parts->pdata;
	*nparts = parts->len;
	
	g_ptr_array_free (parts, FALSE);
	
	return messages;
}
