/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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

#include "gmime-common.h"
#include "gmime-header.h"
#include "gmime-utils.h"
#include "gmime-stream-mem.h"


/**
 * SECTION: gmime-header
 * @title: GMimeHeader
 * @short_description: Message and MIME part headers
 * @see_also: #GMimeObject
 *
 * A #GMimeHeader is a collection of rfc822 header fields and their
 * values.
 **/

struct raw_header {
	struct raw_header *next;
	char *name;
	char *value;
};

struct _GMimeHeader {
	GHashTable *hash;
	GHashTable *writers;
	struct raw_header *headers;
	char *raw;
};


/**
 * g_mime_header_new:
 *
 * Creates a new #GMimeHeader object.
 *
 * Returns a new header object.
 **/
GMimeHeader *
g_mime_header_new (void)
{
	GMimeHeader *new;
	
	new = g_new (GMimeHeader, 1);
	new->hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	new->writers = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	new->headers = NULL;
	new->raw = NULL;
	
	return new;
}


static void
writer_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
}


/**
 * g_mime_header_destroy:
 * @header: header object
 *
 * Destroy the header object
 **/
void
g_mime_header_destroy (GMimeHeader *header)
{
	if (header) {
		struct raw_header *h, *n;
		
		h = header->headers;
		while (h) {
			g_free (h->name);
			g_free (h->value);
			n = h->next;
			g_free (h);
			h = n;
		}
		
		g_hash_table_destroy (header->hash);
		g_hash_table_foreach (header->writers, writer_free, NULL);
		g_hash_table_destroy (header->writers);
		g_free (header->raw);
		g_free (header);
	}
}


/**
 * g_mime_header_set:
 * @header: header object
 * @name: header name
 * @value: header value
 *
 * Set the value of the specified header. If @value is %NULL and the
 * header, @name, had not been previously set, a space will be set
 * aside for it (useful for setting the order of headers before values
 * can be obtained for them) otherwise the header will be unset.
 **/
void
g_mime_header_set (GMimeHeader *header, const char *name, const char *value)
{
	struct raw_header *h, *n;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (name != NULL);
	
	if ((h = g_hash_table_lookup (header->hash, name))) {
		g_free (h->value);
		h->value = g_strdup (value);
	} else {
		n = g_new (struct raw_header, 1);
		n->next = NULL;
		n->name = g_strdup (name);
		n->value = g_strdup (value);
		
		h = header->headers;
		while (h && h->next)
			h = h->next;
		
		if (h != NULL)
			h->next = n;
		else
			header->headers = n;
		
		g_hash_table_insert (header->hash, n->name, n);
	}
	
	g_free (header->raw);
	header->raw = NULL;
}


/**
 * g_mime_header_add:
 * @header: header object
 * @name: header name
 * @value: header value
 *
 * Adds a header. If @value is %NULL, a space will be set aside for it
 * (useful for setting the order of headers before values can be
 * obtained for them) otherwise the header will be unset.
 **/
void
g_mime_header_add (GMimeHeader *header, const char *name, const char *value)
{
	struct raw_header *h, *n;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (name != NULL);
	
	n = g_new (struct raw_header, 1);
	n->next = NULL;
	n->name = g_strdup (name);
	n->value = g_strdup (value);
	
	h = header->headers;
	while (h && h->next)
		h = h->next;
	
	if (h)
		h->next = n;
	else
		header->headers = n;
	
	if (!g_hash_table_lookup (header->hash, name))
		g_hash_table_insert (header->hash, n->name, n);
	
	g_free (header->raw);
	header->raw = NULL;
}


/**
 * g_mime_header_prepend:
 * @header: header object
 * @name: header name
 * @value: header value
 *
 * Adds a header to the head of the list. If @value is %NULL, a space
 * will be set aside for it (useful for setting the order of headers
 * before values can be obtained for them) otherwise the header will
 * be unset.
 **/
void
g_mime_header_prepend (GMimeHeader *header, const char *name, const char *value)
{
	struct raw_header *n;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (name != NULL);
	
	n = g_new (struct raw_header, 1);
	n->next = header->headers;
	n->name = g_strdup (name);
	n->value = g_strdup (value);
	header->headers = n;
	
	if (!g_hash_table_lookup (header->hash, name))
		g_hash_table_insert (header->hash, n->name, n);
	
	g_free (header->raw);
	header->raw = NULL;
}


/**
 * g_mime_header_get:
 * @header: header object
 * @name: header name
 *
 * Gets the value of the header requested.
 *
 * Returns the value of the header requested.
 **/
const char *
g_mime_header_get (const GMimeHeader *header, const char *name)
{
	const struct raw_header *h;
	
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	h = g_hash_table_lookup (header->hash, name);
	
	return h ? h->value : NULL;
}


/**
 * g_mime_header_remove:
 * @header: header object
 * @name: header name
 *
 * Remove the specified header.
 **/
void
g_mime_header_remove (GMimeHeader *header, const char *name)
{
	struct raw_header *h, *n;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (name != NULL);
	
	if ((h = g_hash_table_lookup (header->hash, name))) {
		/* remove the header */
		g_hash_table_remove (header->hash, name);
		n = header->headers;
		
		if (h == n) {
			header->headers = h->next;
		} else {
			while (n->next != h)
				n = n->next;
			
			n->next = h->next;
		}
		
		g_free (h->name);
		g_free (h->value);
		g_free (h);
	}
	
	g_free (header->raw);
	header->raw = NULL;
}


static ssize_t
write_default (GMimeStream *stream, const char *name, const char *value)
{
	ssize_t nwritten;
	char *val;
	
	val = g_mime_utils_header_printf ("%s: %s\n", name, value);
	nwritten = g_mime_stream_write_string (stream, val);
	g_free (val);
	
	return nwritten;
}


/**
 * g_mime_header_write_to_stream:
 * @header: header object
 * @stream: output stream
 *
 * Write the headers to a stream.
 *
 * Returns the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_header_write_to_stream (const GMimeHeader *header, GMimeStream *stream)
{
	GMimeHeaderWriter header_write;
	ssize_t nwritten, total = 0;
	struct raw_header *h;
	
	g_return_val_if_fail (header != NULL, -1);
	g_return_val_if_fail (stream != NULL, -1);
	
	if (header->raw)
		return g_mime_stream_write_string (stream, header->raw);
	
	h = header->headers;
	while (h) {
		if (h->value) {
			header_write = g_hash_table_lookup (header->writers, h->name);
			if (header_write)
				nwritten = (*header_write) (stream, h->name, h->value);
			else
				nwritten = write_default (stream, h->name, h->value);
			
			if (nwritten == -1)
				return -1;
			
			total += nwritten;
		}
		
		h = h->next;
	}
	
	return total;
}


/**
 * g_mime_header_to_string:
 * @header: header object
 *
 * Allocates a string buffer containing the raw rfc822 headers
 * contained in @header.
 *
 * Returns a string containing the header block
 **/
char *
g_mime_header_to_string (const GMimeHeader *header)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (header != NULL, NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), array);
	g_mime_header_write_to_stream (header, stream);
	g_object_unref (stream);
	
	g_byte_array_append (array, (unsigned char *) "", 1);
	str = (char *) array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}


/**
 * g_mime_header_foreach:
 * @header: header object
 * @func: function to be called for each header.
 * @user_data: User data to be passed to the func.
 *
 * Calls @func for each header name/value pair.
 */
void
g_mime_header_foreach (const GMimeHeader *header, GMimeHeaderForeachFunc func, gpointer user_data)
{
	const struct raw_header *h;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (header->hash != NULL);
	g_return_if_fail (func != NULL);
	
	for (h = header->headers; h != NULL; h = h->next)
		(*func) (h->name, h->value, user_data);
}


/**
 * g_mime_header_register_writer:
 * @header: header object
 * @name: header name
 * @writer: writer function
 *
 * Changes the function used to write @name headers to @writer (or the
 * default if @writer is %NULL). This is useful if you want to change
 * the default header folding style for a particular header.
 **/
void
g_mime_header_register_writer (GMimeHeader *header, const char *name, GMimeHeaderWriter writer)
{
	gpointer okey, oval;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (name != NULL);
	
	if (g_hash_table_lookup (header->writers, name)) {
		g_hash_table_lookup_extended (header->writers, name, &okey, &oval);
		g_hash_table_remove (header->writers, name);
		g_free (okey);
	}
	
	if (writer)
		g_hash_table_insert (header->writers, g_strdup (name), writer);
}



/**
 * g_mime_header_set_raw:
 * @header: header object
 * @raw: raw mime part header
 *
 * Set the raw header.
 **/
void
g_mime_header_set_raw (GMimeHeader *header, const char *raw)
{
	g_return_if_fail (header != NULL);
	
	g_free (header->raw);
	header->raw = raw ? g_strdup (raw) : NULL;
}


/**
 * g_mime_header_has_raw:
 * @header: ehader object
 *
 * Gets whether or not a raw header has been set on @header.
 *
 * Returns %TRUE if a raw header is set or %FALSE otherwise.
 **/
gboolean
g_mime_header_has_raw (GMimeHeader *header)
{
	g_return_val_if_fail (header != NULL, FALSE);
	
	return header->raw ? TRUE : FALSE;
}
