/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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

#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-common.h"
#include "gmime-header.h"
#include "gmime-events.h"
#include "gmime-utils.h"

#include "list.h"


/**
 * SECTION: gmime-header
 * @title: GMimeHeader
 * @short_description: Message and MIME part headers
 * @see_also: #GMimeObject
 *
 * A #GMimeHeader is a collection of rfc822 header fields and their
 * values.
 **/

struct _GMimeHeader {
	GMimeHeaderList *list;
	gint64 offset;
	char *name;
	char *value;
	char *raw_value;
};

struct _GMimeHeaderList {
	GMimeParserOptions *options;
	GHashTable *writers;
	GMimeEvent *changed;
	GHashTable *hash;
	GPtrArray *list;
};


/**
 * g_mime_header_new:
 * @name: header name
 * @value: header value
 * @raw_value: raw header value
 * @offset: file/stream offset for the start of the header (or %-1 if unknown)
 *
 * Creates a new #GMimeHeader.
 *
 * Returns: a new #GMimeHeader with the specified values.
 **/
static GMimeHeader *
g_mime_header_new (GMimeHeaderList *headers, const char *name, const char *value, const char *raw_value, gint64 offset)
{
	GMimeHeader *header;
	
	header = g_slice_new (GMimeHeader);
	header->list = headers;
	header->name = g_strdup (name);
	header->value = g_strdup (value);
	header->raw_value = raw_value ? g_strdup (raw_value) : NULL;
	header->offset = offset;
	
	return header;
}


/**
 * g_mime_header_free:
 * @header: a #GMimeHeader
 *
 * Frees a single #GMimeHeader node.
 **/
static void
g_mime_header_free (GMimeHeader *header)
{
	g_free (header->name);
	g_free (header->value);
	g_free (header->raw_value);
	
	g_slice_free (GMimeHeader, header);
}


/**
 * g_mime_header_get_name:
 * @header: a #GMimeHeader
 *
 * Gets the header's name.
 *
 * Returns: the header name or %NULL if invalid.
 **/
const char *
g_mime_header_get_name (GMimeHeader *header)
{
	g_return_val_if_fail (header != NULL, NULL);
	
	return header->name;
}


/**
 * g_mime_header_get_value:
 * @header: a #GMimeHeader
 *
 * Gets the header's value.
 *
 * Returns: the header's raw, unprocessed value or %NULL if invalid.
 *
 * Note: The returned value should be decoded with a function such as
 * g_mime_utils_header_decode_text() before displaying to the user.
 **/
const char *
g_mime_header_get_value (GMimeHeader *header)
{
	g_return_val_if_fail (header != NULL, NULL);
	
	return header->value;
}


/**
 * g_mime_header_set_value:
 * @header: a #GMimeHeader
 * @value: the new header value
 *
 * Sets the header's value.
 *
 * Note: @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
 **/
void
g_mime_header_set_value (GMimeHeader *header, const char *value)
{
	g_return_if_fail (header != NULL);
	g_return_if_fail (value != NULL);
	
	g_free (header->raw_value);
	g_free (header->value);
	
	header->value = g_strdup (value);
	
	g_mime_event_emit (header->list->changed, NULL);
}


/**
 * g_mime_header_get_raw_value:
 * @header: a #GMimeHeader
 *
 * Gets the header's raw (folded) value.
 *
 * Returns: the header value or %NULL if invalid.
 **/
const char *
_g_mime_header_get_raw_value (GMimeHeader *header)
{
	g_return_val_if_fail (header != NULL, NULL);
	
	return header->raw_value;
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
	g_return_val_if_fail (header != NULL, -1);
	
	return header->offset;
}


void
_g_mime_header_set_offset (GMimeHeader *header, gint64 offset)
{
	header->offset = offset;
}

static ssize_t
default_writer (GMimeParserOptions *options, GMimeStream *stream, const char *name, const char *value)
{
	ssize_t nwritten;
	char *val;
	
	val = g_mime_utils_header_printf (options, "%s: %s\n", name, value);
	nwritten = g_mime_stream_write_string (stream, val);
	g_free (val);
	
	return nwritten;
}


/**
 * g_mime_header_write_to_stream:
 * @header: a #GMimeHeader
 * @stream: a #GMimeStream
 *
 * Write the header to the specified stream.
 *
 * Returns: the number of bytes written, or %-1 on fail.
 **/
ssize_t
g_mime_header_write_to_stream (GMimeHeader *header, GMimeStream *stream)
{
	ssize_t nwritten, total = 0;
	GMimeHeaderWriter writer;
	char *val;
	
	g_return_val_if_fail (header != NULL, -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	if (header->raw_value) {
		val = g_strdup_printf ("%s:%s", header->name, header->raw_value);
		nwritten = g_mime_stream_write_string (stream, val);
		g_free (val);
		
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
	} else if (header->value) {
		if (!(writer = g_hash_table_lookup (header->list->writers, header->name)))
			writer = default_writer;
		
		if ((nwritten = writer (header->list->options, stream, header->name, header->value)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	return total;
}


/**
 * g_mime_header_list_new:
 * @options: a #GMimeParserOptions
 *
 * Creates a new #GMimeHeaderList object.
 *
 * Returns: a new header list object.
 **/
GMimeHeaderList *
g_mime_header_list_new (GMimeParserOptions *options)
{
	GMimeHeaderList *headers;
	
	g_return_val_if_fail (options != NULL, NULL);
	
	headers = g_slice_new (GMimeHeaderList);
	headers->options = _g_mime_parser_options_clone (options);
	headers->writers = g_hash_table_new_full (g_mime_strcase_hash,
						  g_mime_strcase_equal,
						  g_free, NULL);
	headers->hash = g_hash_table_new (g_mime_strcase_hash,
					  g_mime_strcase_equal);
	headers->changed = g_mime_event_new (headers);
	headers->list = g_ptr_array_new ();
	
	return headers;
}


/**
 * g_mime_header_list_destroy:
 * @headers: a #GMimeHeaderList
 *
 * Destroy the header list.
 **/
void
g_mime_header_list_destroy (GMimeHeaderList *headers)
{
	guint i;
	
	if (!headers)
		return;
	
	for (i = 0; i < headers->list->len; i++)
		g_mime_header_free (headers->list->pdata[i]);
	
	g_ptr_array_free (headers->list, TRUE);
	
	g_mime_parser_options_free (headers->options);
	g_hash_table_destroy (headers->writers);
	g_hash_table_destroy (headers->hash);
	
	g_mime_event_destroy (headers->changed);
	
	g_slice_free (GMimeHeaderList, headers);
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
	guint i;
	
	g_return_if_fail (headers != NULL);
	
	for (i = 0; i < headers->list->len; i++)
		g_mime_header_free (headers->list->pdata[i]);
	
	g_hash_table_remove_all (headers->hash);
	
	g_ptr_array_set_size (headers->list, 0);
	
	g_mime_event_emit (headers->changed, NULL);
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
	headers->options = _g_mime_parser_options_clone (options);
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
	g_return_val_if_fail (headers != NULL, -1);
	
	return headers->list->len;
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
	
	g_return_val_if_fail (headers != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	return TRUE;
}


gboolean
_g_mime_header_list_has_raw_value (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	return header->raw_value != NULL;
}


void
_g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value, const char *raw_value, gint64 offset)
{
	unsigned char *dest, *src;
	GMimeHeader *header;
	guint n;
	
	header = g_mime_header_new (headers, name, value, raw_value, offset);
	g_hash_table_replace (headers->hash, header->name, header);
	
	if (headers->list->len > 0) {
		g_ptr_array_set_size (headers->list, headers->list->len + 1);
		
		dest = ((unsigned char *) headers->list->pdata) + sizeof (void *);
		src = (unsigned char *) headers->list->pdata;
		n = headers->list->len - 1;
		
		g_memmove (dest, src, (sizeof (void *) * n));
		headers->list->pdata[0] = header;
	} else {
		g_ptr_array_add (headers->list, header);
	}
	
	g_mime_event_emit (headers->changed, NULL);
}


/**
 * g_mime_header_list_prepend:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 *
 * Prepends a header. If @value is %NULL, a space will be set aside
 * for it (useful for setting the order of headers before values can
 * be obtained for them) otherwise the header will be unset.
 *
 * Note: @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
 **/
void
g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value)
{
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	_g_mime_header_list_prepend (headers, name, value, NULL, -1);
}


void
_g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value, const char *raw_value, gint64 offset)
{
	GMimeHeader *header;
	
	header = g_mime_header_new (headers, name, value, raw_value, offset);
	g_ptr_array_add (headers->list, header);
	
	if (!g_hash_table_lookup (headers->hash, name))
		g_hash_table_insert (headers->hash, header->name, header);
	
	g_mime_event_emit (headers->changed, NULL);
}


/**
 * g_mime_header_list_append:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 *
 * Appends a header. If @value is %NULL, a space will be set aside for it
 * (useful for setting the order of headers before values can be
 * obtained for them) otherwise the header will be unset.
 *
 * Note: @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
 **/
void
g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value)
{
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	_g_mime_header_list_append (headers, name, value, NULL, -1);
}


/**
 * g_mime_header_list_get_value:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Gets the value of the first header with the specified name.
 *
 * Returns: the value of the header requested.
 *
 * Note: The returned value should be decoded with a function such as
 * g_mime_utils_header_decode_text() before displaying to the user.
 **/
const char *
g_mime_header_list_get_value (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	
	g_return_val_if_fail (headers != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return NULL;
	
	return header->value;
}


void
_g_mime_header_list_set_value (GMimeHeaderList *headers, const char *name, const char *value, const char *raw_value, gint64 offset)
{
	GMimeHeader *header, *hdr;
	guint i;
	
	if ((header = g_hash_table_lookup (headers->hash, name))) {
		g_free (header->raw_value);
		header->raw_value = raw_value ? g_strdup (raw_value) : NULL;
		
		g_free (header->value);
		header->value = g_strdup (value);
		
		header->offset = offset;
		
		for (i = headers->list->len - 1; i > 0; i--) {
			hdr = (GMimeHeader *) headers->list->pdata[i];
			
			if (hdr == header)
				break;
			
			if (g_ascii_strcasecmp (header->name, hdr->name) != 0)
				continue;
			
			g_ptr_array_remove_index (headers->list, i);
			g_mime_header_free (hdr);
		}
		
		g_mime_event_emit (headers->changed, NULL);
	} else {
		_g_mime_header_list_append (headers, name, value, raw_value, offset);
	}
}


/**
 * g_mime_header_list_set_value:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 *
 * Set the value of the specified header. If @value is %NULL and the
 * header, @name, had not been previously set, a space will be set
 * aside for it (useful for setting the order of headers before values
 * can be obtained for them) otherwise the header will be unset.
 *
 * Note: If there are multiple headers with the specified field name,
 * the first instance of the header will be replaced and further
 * instances will be removed.
 *
 * Additionally, @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
 **/
void
g_mime_header_list_set_value (GMimeHeaderList *headers, const char *name, const char *value)
{
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	_g_mime_header_list_set_value (headers, name, value, NULL, -1);
}


/**
 * g_mime_header_list_get_header:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based index of the header
 *
 * Gets the header at the specified @index within the list.
 *
 * Returns: (transfer none): the header at position @index.
 **/
GMimeHeader *
g_mime_header_list_get_header (GMimeHeaderList *headers, int index)
{
	g_return_val_if_fail (headers != NULL, NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= headers->list->len)
		return NULL;
	
	return (GMimeHeader *) headers->list->pdata[index];
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
	GMimeHeader *header;
	guint i;
	
	g_return_val_if_fail (headers != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	/* get the index of the header */
	for (i = 0; i < headers->list->len; i++) {
		if (headers->list->pdata[i] == header)
			break;
	}
	
	g_ptr_array_remove_index (headers->list, i);
	g_hash_table_remove (headers->hash, name);
	g_mime_header_free (header);
	
	/* look for another header with the same name... */
	while (i < headers->list->len) {
		header = (GMimeHeader *) headers->list->pdata[i];
		
		if (!g_ascii_strcasecmp (header->name, name)) {
			/* enter this node into the lookup table */
			g_hash_table_insert (headers->hash, header->name, header);
			break;
		}
		
		i++;
	}
	
	g_mime_event_emit (headers->changed, NULL);
	
	return TRUE;
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
	GMimeHeader *header, *hdr;
	guint i;
	
	g_return_if_fail (headers != NULL);
	g_return_if_fail (index >= 0);
	
	if ((guint) index >= headers->list->len)
		return;
	
	header = (GMimeHeader *) headers->list->pdata[index];
	g_ptr_array_remove_index (headers->list, index);
	
	/* if this is the first instance of a header with this name, then we'll
	 * need to update the hash table to point to the next instance... */
	if ((hdr = g_hash_table_lookup (headers->hash, header->name)) == header) {
		g_hash_table_remove (headers->hash, header->name);
		
		for (i = (guint) index; i < headers->list->len; i++) {
			hdr = (GMimeHeader *) headers->list->pdata[i];
			
			if (!g_ascii_strcasecmp (header->name, hdr->name)) {
				g_hash_table_insert (headers->hash, hdr->name, hdr);
				break;
			}
		}
	}
	
	g_mime_header_free (header);
	
	g_mime_event_emit (headers->changed, NULL);
}


/**
 * g_mime_header_list_write_to_stream:
 * @headers: a #GMimeHeaderList
 * @stream: output stream
 *
 * Write the headers to a stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeStream *stream)
{
	ssize_t nwritten, total = 0;
	GMimeHeader *header;
	guint i;
	
	g_return_val_if_fail (headers != NULL, -1);
	g_return_val_if_fail (stream != NULL, -1);
	
	for (i = 0; i < headers->list->len; i++) {
		header = (GMimeHeader *) headers->list->pdata[i];
		
		if ((nwritten = g_mime_header_write_to_stream (header, stream)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	return total;
}


/**
 * g_mime_header_list_to_string:
 * @headers: a #GMimeHeaderList
 *
 * Allocates a string buffer containing the raw rfc822 headers
 * contained in @headers.
 *
 * Returns: a string containing the header block.
 **/
char *
g_mime_header_list_to_string (GMimeHeaderList *headers)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (headers != NULL, NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), array);
	g_mime_header_list_write_to_stream (headers, stream);
	g_object_unref (stream);
	
	g_byte_array_append (array, (unsigned char *) "", 1);
	str = (char *) array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}


/**
 * g_mime_header_list_register_writer:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @writer: writer function
 *
 * Changes the function used to write @name headers to @writer (or the
 * default if @writer is %NULL). This is useful if you want to change
 * the default header folding style for a particular header.
 **/
void
g_mime_header_list_register_writer (GMimeHeaderList *headers, const char *name, GMimeHeaderWriter writer)
{
	gpointer okey, oval;
	
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	g_hash_table_remove (headers->writers, name);
	
	if (writer)
		g_hash_table_insert (headers->writers, g_strdup (name), writer);
}


GMimeEvent *
_g_mime_header_list_get_changed_event (GMimeHeaderList *headers)
{
	g_return_val_if_fail (headers != NULL, NULL);
	
	return headers->changed;
}
