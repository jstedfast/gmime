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


typedef struct _GMimeHeader GMimeHeader;

struct _GMimeHeaderList {
	GHashTable *writers;
	GHashTable *hash;
	List iters;
	List list;
	char *raw;
};

/**
 * GMimeHeader:
 *
 * A message/rfc822 header.
 **/
struct _GMimeHeader {
	struct _GMimeHeader *next;
	struct _GMimeHeader *prev;
	gint64 offset;
	char *name;
	char *value;
};

struct _GMimeHeaderIter {
	struct _GMimeHeaderIter *next;
	struct _GMimeHeaderIter *prev;
	GMimeHeaderList *headers;
	GMimeHeader *cursor;
};

static void g_mime_header_list_invalidate_iters (GMimeHeaderList *headers, GMimeHeader *header);
static GMimeHeader *g_mime_header_new (const char *name, const char *value, gint64 offset);
static void g_mime_header_free (GMimeHeader *header);


/**
 * g_mime_header_new:
 * @name: header name
 * @value: header value
 * @offset: file/stream offset for the start of the header (or %-1 if unknown)
 *
 * Creates a new #GMimeHeader.
 *
 * Returns: a new #GMimeHeader with the specified values.
 **/
static GMimeHeader *
g_mime_header_new (const char *name, const char *value, gint64 offset)
{
	GMimeHeader *header;
	
	header = g_new (GMimeHeader, 1);
	header->name = g_strdup (name);
	header->value = g_strdup (value);
	header->offset = offset;
	header->next = NULL;
	header->prev = NULL;
	
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
	g_free (header);
}


/**
 * g_mime_header_iter_copy:
 * @iter: a #GMimeHeaderIter
 *
 * Copies a header iterator.
 *
 * Returns: a new #GMimeHeaderIter which matches @iter's state.
 **/
GMimeHeaderIter *
g_mime_header_iter_copy (GMimeHeaderIter *iter)
{
	GMimeHeaderIter *copy;
	
	g_return_val_if_fail (iter != NULL, NULL);
	
	copy = g_new (GMimeHeaderIter, 1);
	copy->headers = iter->headers;
	copy->cursor = iter->cursor;
	
	if (iter->headers)
		list_append (&iter->headers->iters, (ListNode *) copy);
	
	return copy;
}


/**
 * g_mime_header_iter_free:
 * @iter: a #GMimeHeaderIter
 *
 * Frees a #GMimeHeaderIter.
 **/
void
g_mime_header_iter_free (GMimeHeaderIter *iter)
{
	g_return_if_fail (iter != NULL);
	
	if (iter->headers)
		list_unlink ((ListNode *) iter);
	
	g_free (iter);
}


/**
 * g_mime_header_iter_equal:
 * @iter1: a #GMimeHeaderIter
 * @iter2: a #GMimeHeaderIter
 *
 * Check that @iter1 and @iter2 reference the same header.
 *
 * Returns: %TRUE if @iter1 and @iter2 refer to the same header or
 * %FALSE otherwise.
 **/
gboolean
g_mime_header_iter_equal (GMimeHeaderIter *iter1, GMimeHeaderIter *iter2)
{
	g_return_val_if_fail (iter1 != NULL, FALSE);
	g_return_val_if_fail (iter2 != NULL, FALSE);
	
	return iter1->cursor == iter2->cursor;
}


/**
 * g_mime_header_iter_is_valid:
 * @iter: a #GMimeHeaderIter
 *
 * Checks if a #GMimeHeaderIter is valid. An iterator may become
 * invalid if the #GMimeHeaderList that the iterator refers to changes
 * or is destroyed.
 *
 * Returns: %TRUE if @iter is still valid or %FALSE otherwise.
 **/
gboolean
g_mime_header_iter_is_valid (GMimeHeaderIter *iter)
{
	g_return_val_if_fail (iter != NULL, FALSE);
	
	return iter->cursor && iter->cursor->next;
}


/**
 * g_mime_header_iter_first:
 * @iter: a #GMimeHeaderIter
 *
 * Updates @iter to point to the first header.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_header_iter_first (GMimeHeaderIter *iter)
{
	GMimeHeader *first;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	/* make sure we can actually do as requested */
	if (!iter->headers)
		return FALSE;
	
	first = (GMimeHeader *) iter->headers->list.head;
	if (!first->next)
		return FALSE;
	
	iter->cursor = first;
	
	return TRUE;
}


/**
 * g_mime_header_iter_last:
 * @iter: a #GMimeHeaderIter
 *
 * Updates @iter to point to the last header.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_header_iter_last (GMimeHeaderIter *iter)
{
	GMimeHeader *last;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	/* make sure we can actually do as requested */
	if (!iter->headers)
		return FALSE;
	
	last = (GMimeHeader *) iter->headers->list.tailpred;
	if (!last->next)
		return FALSE;
	
	iter->cursor = last;
	
	return TRUE;
}


/**
 * g_mime_header_iter_next:
 * @iter: a #GMimeHeaderIter
 *
 * Advances to the next header.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_header_iter_next (GMimeHeaderIter *iter)
{
	GMimeHeader *next;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	/* make sure current cursor is valid */
	if (!iter->cursor || !iter->cursor->next)
		return FALSE;
	
	/* make sure next item is valid */
	next = iter->cursor->next;
	if (!next->next)
		return FALSE;
	
	iter->cursor = next;
	
	return TRUE;
}


/**
 * g_mime_header_iter_prev:
 * @iter: a #GMimeHeaderIter
 *
 * Advances to the previous header.
 *
 * Returns: %TRUE on success or %FALSE otherwise.
 **/
gboolean
g_mime_header_iter_prev (GMimeHeaderIter *iter)
{
	GMimeHeader *prev;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	/* make sure current cursor is valid */
	if (!iter->cursor || !iter->cursor->prev)
		return FALSE;
	
	/* make sure prev item is valid */
	prev = iter->cursor->prev;
	if (!prev->prev)
		return FALSE;
	
	iter->cursor = prev;
	
	return TRUE;
}


/**
 * g_mime_header_iter_get_offset:
 * @iter: a #GMimeHeaderIter
 *
 * Gets the current header's file/stream offset.
 *
 * Returns: the file/stream offset or %-1 if unknown or invalid.
 **/
gint64
g_mime_header_iter_get_offset (GMimeHeaderIter *iter)
{
	g_return_val_if_fail (iter != NULL, -1);
	
	if (!iter->cursor || !iter->cursor->next)
		return -1;
	
	return iter->cursor->offset;
}


/**
 * g_mime_header_iter_get_name:
 * @iter: a #GMimeHeaderIter
 *
 * Gets the current header's name.
 *
 * Returns: the header name or %NULL if invalid.
 **/
const char *
g_mime_header_iter_get_name (GMimeHeaderIter *iter)
{
	g_return_val_if_fail (iter != NULL, NULL);
	
	if (!iter->cursor || !iter->cursor->next)
		return NULL;
	
	return iter->cursor->name;
}


/**
 * g_mime_header_iter_set_value:
 * @iter: a #GMimeHeaderIter
 *
 * Sets the current header's value.
 *
 * Returns: %TRUE if the value was set or %FALSE otherwise (indicates
 * invalid iter).
 **/
gboolean
g_mime_header_iter_set_value (GMimeHeaderIter *iter, const char *value)
{
	g_return_val_if_fail (iter != NULL, FALSE);
	
	if (!iter->cursor || !iter->cursor->next)
		return FALSE;
	
	g_free (iter->cursor->value);
	iter->cursor->value = g_strdup (value);
	
	g_mime_header_list_set_raw (iter->headers, NULL);
	
	return TRUE;
}


/**
 * g_mime_header_iter_get_value:
 * @iter: a #GMimeHeaderIter
 *
 * Gets the current header's name.
 *
 * Returns: the header name or %NULL if invalid.
 **/
const char *
g_mime_header_iter_get_value (GMimeHeaderIter *iter)
{
	g_return_val_if_fail (iter != NULL, NULL);
	
	if (!iter->cursor || !iter->cursor->next)
		return NULL;
	
	return iter->cursor->value;
}


/**
 * g_mime_header_iter_remove:
 * @iter: a #GMimeHeaderIter
 *
 * Removes the current header and advances to the next header.
 *
 * Returns: %TRUE on success or %FALSE otherwise
 **/
gboolean
g_mime_header_iter_remove (GMimeHeaderIter *iter)
{
	GMimeHeader *cursor, *header, *next;
	GMimeHeaderList *headers;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	if (!iter->cursor || !iter->cursor->next)
		return FALSE;
	
	/* save iter state */
	headers = iter->headers;
	cursor = iter->cursor;
	next = cursor->next;
	
	if (!(header = g_hash_table_lookup (headers->hash, cursor->name)))
		return FALSE;
	
	if (cursor == header) {
		/* update the header lookup table */
		GMimeHeader *node = next;
		
		g_hash_table_remove (headers->hash, cursor->name);
		
		while (node->next) {
			if (!g_ascii_strcasecmp (node->name, cursor->name)) {
				/* enter this node into the lookup table */
				g_hash_table_insert (headers->hash, node->name, node);
				break;
			}
			
			node = node->next;
		}
	}
	
	/* remove/free the header */
	g_mime_header_list_invalidate_iters (iter->headers, cursor);
	list_unlink ((ListNode *) cursor);
	g_mime_header_free (cursor);
	
	/* restore iter state */
	iter->headers = headers;
	iter->cursor = next;
	
	return TRUE;
}


/**
 * g_mime_header_list_invalidate_iters:
 * @headers: a #GMimeHeaderList
 * @header: a #GMimeHeader
 *
 * Invalidate all outstanding iterators that are currently referencing
 * @header. If @header is NULL, then invalidate all iterators.
 **/
static void
g_mime_header_list_invalidate_iters (GMimeHeaderList *headers, GMimeHeader *header)
{
	GMimeHeaderIter *iter, *next;
	
	/* invalidate all our outstanding iterators matching @header */
	iter = (GMimeHeaderIter *) headers->iters.head;
	while (iter->next) {
		next = iter->next;
		
		if (!header || iter->cursor == header) {
			/* invalidate this iter */
			list_unlink ((ListNode *) iter);
			iter->cursor = NULL;
			
			if (header == NULL) {
				/* invalidating because HeaderList is being destroyed */
				iter->headers = NULL;
			}
		}
		
		iter = next;
	}
}


/**
 * g_mime_header_list_new:
 *
 * Creates a new #GMimeHeaderList object.
 *
 * Returns: a new header list object.
 **/
GMimeHeaderList *
g_mime_header_list_new (void)
{
	GMimeHeaderList *headers;
	
	headers = g_new (GMimeHeaderList, 1);
	headers->writers = g_hash_table_new_full (g_mime_strcase_hash,
						  g_mime_strcase_equal,
						  g_free, NULL);
	headers->hash = g_hash_table_new (g_mime_strcase_hash,
					  g_mime_strcase_equal);
	list_init (&headers->iters);
	list_init (&headers->list);
	headers->raw = NULL;
	
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
	GMimeHeader *header, *next;
	GMimeHeaderIter *iter;
	
	if (!headers)
		return;
	
	/* invalidate all our outstanding iterators */
	g_mime_header_list_invalidate_iters (headers, NULL);
	
	header = (GMimeHeader *) headers->list.head;
	while (header->next) {
		next = header->next;
		g_mime_header_free (header);
		header = next;
	}
	
	g_hash_table_destroy (headers->writers);
	g_hash_table_destroy (headers->hash);
	g_free (headers->raw);
	g_free (headers);
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
 **/
void
g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value)
{
	GMimeHeader *header;
	
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	header = g_mime_header_new (name, value, -1);
	list_append (&headers->list, (ListNode *) header);
	g_hash_table_replace (headers->hash, header->name, header);
	
	g_free (headers->raw);
	headers->raw = NULL;
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
 **/
void
g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value)
{
	GMimeHeader *header;
	
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	header = g_mime_header_new (name, value, -1);
	list_append (&headers->list, (ListNode *) header);
	
	if (!g_hash_table_lookup (headers->hash, name))
		g_hash_table_insert (headers->hash, header->name, header);
	
	g_free (headers->raw);
	headers->raw = NULL;
}


/**
 * g_mime_header_list_get:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Gets the value of the first header with the name requested.
 *
 * Returns: the value of the header requested.
 **/
const char *
g_mime_header_list_get (const GMimeHeaderList *headers, const char *name)
{
	const GMimeHeader *header;
	
	g_return_val_if_fail (headers != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return NULL;
	
	return header->value;
}


/**
 * g_mime_header_list_set:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 *
 * Set the value of the first header with the name specified. If
 * @value is %NULL and the header, @name, had not been previously set,
 * a space will be set aside for it (useful for setting the order of
 * headers before values can be obtained for them) otherwise the
 * header will be unset.
 **/
void
g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value)
{
	GMimeHeader *header;
	
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	if ((header = g_hash_table_lookup (headers->hash, name))) {
		g_free (header->value);
		header->value = g_strdup (value);
	} else {
		header = g_mime_header_new (name, value, -1);
		list_append (&headers->list, (ListNode *) header);
		g_hash_table_insert (headers->hash, header->name, header);
	}
	
	g_free (headers->raw);
	headers->raw = NULL;
}


/**
 * g_mime_header_list_remove:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Remove the specified header.
 *
 * Returns: %TRUE if the header was successfully removed or %FALSE if
 * the specified header could not be found.
 **/
gboolean
g_mime_header_list_remove (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header, *node;
	
	g_return_if_fail (headers != NULL);
	g_return_if_fail (name != NULL);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	/* look for another header with the same name... */
	node = header->next;
	while (node->next) {
		if (!g_ascii_strcasecmp (node->name, name)) {
			/* enter this node into the lookup table */
			g_hash_table_replace (headers->hash, node->name, node);
			break;
		}
		
		node = node->next;
	}
	
	/* invalidate all our outstanding iterators matching @header */
	g_mime_header_list_invalidate_iters (headers, header);
	
	/* remove/free the header */
	list_unlink ((ListNode *) header);
	g_mime_header_free (header);
	
	g_free (headers->raw);
	headers->raw = NULL;
}


/**
 * g_mime_header_list_get_iter:
 * @headers: a #GMimeHeaderList
 *
 * Gets a new iterator for traversing @headers.
 *
 * Returns: a new #GMimeHeaderIter which must be freed using
 * g_mime_header_iter_free() when finished with it.
 **/
GMimeHeaderIter *
g_mime_header_list_get_iter (GMimeHeaderList *headers)
{
	GMimeHeaderIter *iter;
	
	g_return_val_if_fail (headers != NULL, NULL);
	
	iter = g_new (GMimeHeaderIter, 1);
	iter->cursor = (GMimeHeader *) headers->list.head;
	iter->headers = headers;
	
	list_append (&headers->iters, (ListNode *) iter);
	
	return iter;
}


static ssize_t
default_writer (GMimeStream *stream, const char *name, const char *value)
{
	ssize_t nwritten;
	char *val;
	
	val = g_mime_utils_header_printf ("%s: %s\n", name, value);
	nwritten = g_mime_stream_write_string (stream, val);
	g_free (val);
	
	return nwritten;
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
g_mime_header_list_write_to_stream (const GMimeHeaderList *headers, GMimeStream *stream)
{
	ssize_t nwritten, total = 0;
	GMimeHeaderWriter writer;
	GHashTable *writers;
	GMimeHeader *header;
	
	g_return_val_if_fail (headers != NULL, -1);
	g_return_val_if_fail (stream != NULL, -1);
	
	if (headers->raw)
		return g_mime_stream_write_string (stream, headers->raw);
	
	header = (GMimeHeader *) headers->list.head;
	writers = headers->writers;
	
	while (header->next) {
		if (header->value) {
			if (!(writer = g_hash_table_lookup (writers, header->name)))
				writer = default_writer;
			
			if ((nwritten = (* writer) (stream, header->name, header->value)) == -1)
				return -1;
			
			total += nwritten;
		}
		
		header = header->next;
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
g_mime_header_list_to_string (const GMimeHeaderList *headers)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (headers != NULL, NULL);
	
	if (headers->raw)
		return g_strdup (headers->raw);
	
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
	
	if (g_hash_table_lookup (headers->writers, name)) {
		g_hash_table_lookup_extended (headers->writers, name, &okey, &oval);
		g_hash_table_remove (headers->writers, name);
		g_free (okey);
	}
	
	if (writer)
		g_hash_table_insert (headers->writers, g_strdup (name), writer);
}



/**
 * g_mime_header_list_set_raw:
 * @headers: a #GMimeHeaderList
 * @raw: raw mime part header
 *
 * Set the raw header.
 **/
void
g_mime_header_list_set_raw (GMimeHeaderList *headers, const char *raw)
{
	g_return_if_fail (headers != NULL);
	
	g_free (headers->raw);
	headers->raw = raw ? g_strdup (raw) : NULL;
}


/**
 * g_mime_header_list_has_raw:
 * @headers: a #GMimeHeaderList
 *
 * Gets whether or not a raw header has been set on @headers.
 *
 * Returns: %TRUE if a raw header is set or %FALSE otherwise.
 **/
gboolean
g_mime_header_list_has_raw (const GMimeHeaderList *headers)
{
	g_return_val_if_fail (headers != NULL, FALSE);
	
	return headers->raw ? TRUE : FALSE;
}
