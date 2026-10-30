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
#include <fcntl.h>
#include <string.h>

#include "gmime-attachment-collection.h"
#include "gmime-error.h"
#include "gmime-filter-best.h"
#include "gmime-internal.h"
#include "gmime-message-part.h"
#include "gmime-parser.h"
#include "gmime-stream-fs.h"
#include "gmime-stream-mem.h"
#include "gmime-text-part.h"
#include "gmime-types.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-attachment-collection
 * @title: GMimeAttachmentCollection
 * @short_description: Constructing attachments and linked resources
 * @see_also: #GMimePart, #GMimeMessagePart
 *
 * Ports MimeKit's AttachmentCollection. Collections retain entities by reference
 * and allow duplicate entries. Adding an existing entity does not change its
 * headers. The data, stream and file helpers construct and append an attachment
 * only after loading succeeds, and return a separate owned reference.
 *
 * Text attachments use #GMimeFilterBest with the 7bit constraint. Other ordinary
 * parts use base64. A message/rfc822 attachment is parsed into #GMimeMessagePart.
 * Linked collections give newly constructed attachments an inline disposition
 * and a Content-Location containing the file name.
 *
 * Clearing and removing entries releases references; retained entities remain
 * usable. There is no force-dispose operation. The synchronous APIs do not yet
 * implement MimeKit's cancellation or asynchronous overloads.
 **/

static void collection_finalize (GObject *object);

G_DEFINE_FINAL_TYPE (GMimeAttachmentCollection, g_mime_attachment_collection, G_TYPE_OBJECT)

static void
g_mime_attachment_collection_class_init (GMimeAttachmentCollectionClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = collection_finalize;
}

static void
g_mime_attachment_collection_init (GMimeAttachmentCollection *collection)
{
	collection->array = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
collection_finalize (GObject *object)
{
	GMimeAttachmentCollection *collection = GMIME_ATTACHMENT_COLLECTION (object);

	g_ptr_array_unref (collection->array);
	G_OBJECT_CLASS (g_mime_attachment_collection_parent_class)->finalize (object);
}

static void
set_io_error (GError **error, const char *operation)
{
	int errsv = errno;

	g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
		     "Failed to %s attachment content: %s", operation,
		     errsv ? g_strerror (errsv) : "unknown I/O error");
}

static gboolean
load_content (GMimePart *part, GMimeStream *stream, gboolean copy_stream, GError **error)
{
	GMimeStream *content = copy_stream ? g_mime_stream_mem_new () : NULL;
	GMimeFilter *filter = NULL;
	GMimeContent *wrapper;
	char buffer[4096], *output;
	size_t output_length, prespace;
	ssize_t nread;

	if (g_mime_content_type_is_type (g_mime_entity_get_content_type ((GMimeEntity *) part), "text", "*"))
		filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);

	if (filter || copy_stream) {
		while (TRUE) {
			errno = 0;
			if ((nread = g_mime_stream_read (stream, buffer, sizeof (buffer))) == -1) {
				set_io_error (error, "read");
				goto fail;
			}
			if (nread == 0)
				break;

			if (filter)
				g_mime_filter_filter (filter, buffer, nread, 0, &output, &output_length, &prespace);

			if (content) {
				GByteArray *array = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (content));

				/* GMimeStreamMem uses a GByteArray, whose length is limited to guint. */
				if ((gsize) nread > G_MAXUINT - array->len) {
					errno = EOVERFLOW;
					set_io_error (error, "buffer");
					goto fail;
				}
				errno = 0;
				if (_g_mime_stream_write_all (content, buffer, nread) == -1) {
					set_io_error (error, "copy");
					goto fail;
				}
			}
		}
	}

	if (filter) {
		g_mime_filter_complete (filter, buffer, 0, 0, &output, &output_length, &prespace);
		g_mime_part_set_content_encoding (part, g_mime_filter_best_encoding (GMIME_FILTER_BEST (filter),
										 GMIME_ENCODING_CONSTRAINT_7BIT));
	} else {
		g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_BASE64);
	}

	errno = 0;
	if (g_mime_stream_reset (content ? content : stream) == -1) {
		set_io_error (error, "rewind");
		goto fail;
	}
	wrapper = g_mime_content_new_with_stream (content ? content : stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content (part, wrapper);
	g_object_unref (wrapper);
	g_clear_object (&content);
	g_clear_object (&filter);
	return TRUE;

 fail:
	g_clear_object (&content);
	g_clear_object (&filter);
	return FALSE;
}

static GMimeEntity *
create_attachment (GMimeAttachmentCollection *collection, const char *path, GMimeStream *stream,
		   GMimeContentType *content_type, gboolean copy_stream, GError **error)
{
	gboolean auto_detected = content_type == NULL;
	GMimeEntity *attachment = NULL;
	GMimeContentType *type;
	GMimeMessage *message;
	GMimeParser *parser;
	GError *local_error = NULL;
	const char *separator;
	gint64 position;
	char *mime_type, *file_name;

	if (content_type) {
		type = g_object_ref (content_type);
	} else {
		mime_type = g_mime_types_get_mime_type (path);
		type = g_mime_content_type_parse_full (NULL, mime_type, -1, error);
		g_free (mime_type);

		if (!type)
			return NULL;
	}

	if (g_mime_content_type_is_type (type, "message", "rfc822")) {
		position = g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_CUR);

		/* The caller may close the stream immediately after adding the attachment. */
		parser = g_mime_parser_new_with_stream_and_storage (stream, GMIME_FORMAT_MESSAGE,
								 GMIME_CONTENT_STORAGE_MEMORY, &local_error);
		if (!parser) {
			g_propagate_error (error, local_error);
			goto done;
		}

		message = g_mime_parser_construct_message (parser, NULL, &local_error);
		g_object_unref (parser);

		if (message) {
			attachment = (GMimeEntity *) g_mime_message_part_new_with_message ("rfc822", message);
			g_object_unref (message);
		} else {
			if (!local_error)
				g_set_error_literal (&local_error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
						     "No message found in attachment content");

			if (auto_detected && position != -1 &&
			    (g_error_matches (local_error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) ||
			     g_error_matches (local_error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG))) {
				/* If the content type was auto-detected and the stream is seekable,
				 * fall back to attaching this content as a generic stream. */
				g_clear_error (&local_error);
				g_object_unref (type);

				type = g_mime_content_type_new ("application", "octet-stream");

				errno = 0;
				if (g_mime_stream_seek (stream, position, GMIME_STREAM_SEEK_SET) == -1) {
					set_io_error (error, "rewind");
					goto done;
				}
			} else {
				g_propagate_error (error, local_error);
				goto done;
			}
		}
	}

	if (!attachment) {
		if (g_mime_content_type_is_type (type, "text", "*")) {
			/* TODO: should we try to auto-detect charsets if no charset parameter is specified? */
			attachment = g_object_new (GMIME_TYPE_TEXT_PART, NULL);
		} else {
			attachment = g_object_new (GMIME_TYPE_PART, NULL);
		}
		g_mime_entity_set_content_type (attachment, type);
		if (!load_content ((GMimePart *) attachment, stream, copy_stream, error)) {
			g_clear_object (&attachment);
			goto done;
		}
	}

	separator = strrchr (path, G_DIR_SEPARATOR);
	file_name = g_strdup (separator && separator > path ? separator + 1 : path);

	if (collection->linked && (g_uri_peek_scheme (file_name) || !_g_mime_is_content_uri (file_name, FALSE))) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "Attachment file name is not a valid relative Content-Location");
		g_free (file_name);
		g_clear_object (&attachment);
		goto done;
	}

	g_mime_entity_set_disposition (attachment, collection->linked ? GMIME_DISPOSITION_INLINE : GMIME_DISPOSITION_ATTACHMENT);
	g_mime_entity_set_content_disposition_parameter (attachment, "filename", file_name);
	g_mime_entity_set_content_type_parameter (attachment, "name", file_name);

	if (collection->linked)
		g_mime_entity_set_content_location (attachment, file_name);

	g_free (file_name);
	g_ptr_array_add (collection->array, g_object_ref (attachment));

 done:
	g_object_unref (type);
	return attachment;
}

/**
 * g_mime_attachment_collection_new:
 * @linked_resources: whether constructed attachments are linked resources
 *
 * Returns: (transfer full): a new empty attachment collection.
 **/
GMimeAttachmentCollection *
g_mime_attachment_collection_new (gboolean linked_resources)
{
	GMimeAttachmentCollection *collection;

	collection = g_object_new (GMIME_TYPE_ATTACHMENT_COLLECTION, NULL);
	collection->linked = linked_resources;
	return collection;
}

/**
 * g_mime_attachment_collection_get_count:
 * @collection: a #GMimeAttachmentCollection
 *
 * Returns: the number of attachments, including duplicate references.
 **/
int
g_mime_attachment_collection_get_count (GMimeAttachmentCollection *collection)
{
	g_return_val_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection), 0);

	return collection->array->len;
}

/**
 * g_mime_attachment_collection_get_attachment:
 * @collection: a #GMimeAttachmentCollection
 * @index: an existing attachment index
 *
 * Returns: (transfer none): the attachment at @index.
 **/
GMimeEntity *
g_mime_attachment_collection_get_attachment (GMimeAttachmentCollection *collection, int index)
{
	g_return_val_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection), NULL);
	g_return_val_if_fail (index >= 0 && (guint) index < collection->array->len, NULL);

	return collection->array->pdata[index];
}

/**
 * g_mime_attachment_collection_set_attachment:
 * @collection: a #GMimeAttachmentCollection
 * @index: an existing attachment index
 * @attachment: (transfer none): the replacement entity
 *
 * Retains @attachment and releases the previous entry without changing headers.
 **/
void
g_mime_attachment_collection_set_attachment (GMimeAttachmentCollection *collection, int index, GMimeEntity *attachment)
{
	GMimeEntity *previous;

	g_return_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection));
	g_return_if_fail (index >= 0 && (guint) index < collection->array->len);
	g_return_if_fail (GMIME_IS_ENTITY (attachment));

	previous = collection->array->pdata[index];
	g_object_ref (attachment);
	collection->array->pdata[index] = attachment;
	g_object_unref (previous);
}

/**
 * g_mime_attachment_collection_add:
 * @collection: a #GMimeAttachmentCollection
 * @attachment: (transfer none): the entity to append
 *
 * Retains @attachment without changing its headers. Duplicate entries are allowed.
 **/
void
g_mime_attachment_collection_add (GMimeAttachmentCollection *collection, GMimeEntity *attachment)
{
	g_return_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection));
	g_return_if_fail (GMIME_IS_ENTITY (attachment));
	g_return_if_fail (collection->array->len < G_MAXINT);

	g_ptr_array_add (collection->array, g_object_ref (attachment));
}

/**
 * g_mime_attachment_collection_add_data:
 * @collection: a #GMimeAttachmentCollection
 * @file_name: a nonempty UTF-8 file name or path
 * @data: (array length=length) (element-type guint8) (nullable): attachment data
 * @length: the number of bytes in @data
 * @content_type: (nullable): the content type, or %NULL to detect it by extension
 * @error: return location for a #GError, or %NULL
 *
 * Copies @data into owned storage, then constructs and appends an attachment.
 * @data may be %NULL only when @length is zero. Inputs larger than %G_MAXUINT
 * bytes fail with %GMIME_ERROR and EOVERFLOW because native memory streams use
 * #GByteArray. A supplied @content_type is retained by ordinary parts, not
 * cloned; its name parameter is updated on success.
 *
 * Automatically detected message/rfc822 data that fails to parse is attached
 * as application/octet-stream. Explicit message/rfc822 parse errors propagate.
 * Linked file names must be relative references satisfying native
 * Content-Location validation; invalid references return
 * %GMIME_ERROR_PARSE_ERROR without adding an entry.
 *
 * Returns: (nullable) (transfer full): the added entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_attachment_collection_add_data (GMimeAttachmentCollection *collection, const char *file_name,
				      const guint8 *data, gsize length, GMimeContentType *content_type, GError **error)
{
	GMimeEntity *attachment;
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection), NULL);
	g_return_val_if_fail (collection->array->len < G_MAXINT, NULL);
	g_return_val_if_fail (file_name != NULL && *file_name != '\0', NULL);
	g_return_val_if_fail (g_utf8_validate (file_name, -1, NULL), NULL);
	g_return_val_if_fail (data != NULL || length == 0, NULL);
	g_return_val_if_fail (content_type == NULL || GMIME_IS_CONTENT_TYPE (content_type), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (length > G_MAXUINT) {
		g_set_error_literal (error, GMIME_ERROR, EOVERFLOW, "Attachment data exceeds the native memory stream limit");
		return NULL;
	}

	stream = g_mime_stream_mem_new_with_buffer (data ? (const char *) data : "", length);
	attachment = create_attachment (collection, file_name, stream, content_type, FALSE, error);
	g_object_unref (stream);

	return attachment;
}

/**
 * g_mime_attachment_collection_add_stream:
 * @collection: a #GMimeAttachmentCollection
 * @file_name: a nonempty UTF-8 file name or path
 * @stream: the input stream, read from its current position
 * @content_type: (nullable): the content type, or %NULL to detect it by extension
 * @error: return location for a #GError, or %NULL
 *
 * Constructs and appends an attachment with an independent copy of the remaining
 * input. The caller's stream is consumed but not closed or reset. Parsed message
 * content is also independent of @stream. Failed additions leave the collection
 * unchanged, but may consume input.
 * Linked file names must be relative references satisfying native
 * Content-Location validation.
 *
 * Automatically detected message/rfc822 content falls back to a generic binary
 * attachment only on a format error and only if @stream can seek back to its
 * original position. I/O failures and explicit message/rfc822 format errors
 * propagate. Native parser recovery rules still apply. Ordinary part content
 * is limited to %G_MAXUINT bytes by #GMimeStreamMem; exceeding this returns
 * %GMIME_ERROR with EOVERFLOW.
 *
 * Returns: (nullable) (transfer full): the added entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_attachment_collection_add_stream (GMimeAttachmentCollection *collection, const char *file_name,
					GMimeStream *stream, GMimeContentType *content_type, GError **error)
{
	g_return_val_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection), NULL);
	g_return_val_if_fail (collection->array->len < G_MAXINT, NULL);
	g_return_val_if_fail (file_name != NULL && *file_name != '\0', NULL);
	g_return_val_if_fail (g_utf8_validate (file_name, -1, NULL), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	g_return_val_if_fail (content_type == NULL || GMIME_IS_CONTENT_TYPE (content_type), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	return create_attachment (collection, file_name, stream, content_type, TRUE, error);
}

/**
 * g_mime_attachment_collection_add_file:
 * @collection: a #GMimeAttachmentCollection
 * @file_name: (type filename): a nonempty path to the attachment file
 * @content_type: (nullable): the content type, or %NULL to detect it by extension
 * @error: return location for a #GError, or %NULL
 *
 * Opens @file_name and constructs an attachment using
 * g_mime_attachment_collection_add_stream(). The file is closed on both success
 * and failure. File open and input I/O errors use %GMIME_ERROR with errno codes.
 * File paths must also be valid UTF-8 for the attachment's file name metadata.
 *
 * Returns: (nullable) (transfer full): the added entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_attachment_collection_add_file (GMimeAttachmentCollection *collection, const char *file_name,
				      GMimeContentType *content_type, GError **error)
{
	GMimeEntity *attachment;
	GMimeStream *stream;

	g_return_val_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection), NULL);
	g_return_val_if_fail (collection->array->len < G_MAXINT, NULL);
	g_return_val_if_fail (file_name != NULL && *file_name != '\0', NULL);
	g_return_val_if_fail (g_utf8_validate (file_name, -1, NULL), NULL);
	g_return_val_if_fail (content_type == NULL || GMIME_IS_CONTENT_TYPE (content_type), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!(stream = g_mime_stream_fs_open (file_name, O_RDONLY, 0, error)))
		return NULL;

	attachment = create_attachment (collection, file_name, stream, content_type, TRUE, error);
	g_object_unref (stream);

	return attachment;
}

/**
 * g_mime_attachment_collection_clear:
 * @collection: a #GMimeAttachmentCollection
 *
 * Removes all entries, releasing each reference. Externally retained entities
 * are not disposed and remain usable.
 **/
void
g_mime_attachment_collection_clear (GMimeAttachmentCollection *collection)
{
	g_return_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection));

	g_ptr_array_set_size (collection->array, 0);
}

/**
 * g_mime_attachment_collection_index_of:
 * @collection: a #GMimeAttachmentCollection
 * @attachment: the entity to find
 *
 * Returns: the index of the first identical instance, or -1 if absent.
 **/
int
g_mime_attachment_collection_index_of (GMimeAttachmentCollection *collection, GMimeEntity *attachment)
{
	guint index;

	g_return_val_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection), -1);
	g_return_val_if_fail (GMIME_IS_ENTITY (attachment), -1);

	return g_ptr_array_find (collection->array, attachment, &index) ? (int) index : -1;
}

/**
 * g_mime_attachment_collection_contains:
 * @collection: a #GMimeAttachmentCollection
 * @attachment: the entity to find
 *
 * Returns: whether the exact @attachment instance belongs to the collection.
 **/
gboolean
g_mime_attachment_collection_contains (GMimeAttachmentCollection *collection, GMimeEntity *attachment)
{
	return g_mime_attachment_collection_index_of (collection, attachment) != -1;
}

/**
 * g_mime_attachment_collection_insert:
 * @collection: a #GMimeAttachmentCollection
 * @index: an existing attachment index
 * @attachment: (transfer none): the entity to insert
 *
 * Retains @attachment and inserts it before @index without changing its headers.
 * Matching MimeKit, @index must be less than the count; use add() to append.
 **/
void
g_mime_attachment_collection_insert (GMimeAttachmentCollection *collection, int index, GMimeEntity *attachment)
{
	g_return_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection));
	g_return_if_fail (index >= 0 && (guint) index < collection->array->len);
	g_return_if_fail (GMIME_IS_ENTITY (attachment));
	g_return_if_fail (collection->array->len < G_MAXINT);

	g_ptr_array_insert (collection->array, index, g_object_ref (attachment));
}

/**
 * g_mime_attachment_collection_remove:
 * @collection: a #GMimeAttachmentCollection
 * @attachment: the entity to remove
 *
 * Removes only the first identical instance, releasing its reference.
 *
 * Returns: whether an entry was removed.
 **/
gboolean
g_mime_attachment_collection_remove (GMimeAttachmentCollection *collection, GMimeEntity *attachment)
{
	int index;

	if ((index = g_mime_attachment_collection_index_of (collection, attachment)) == -1)
		return FALSE;

	g_ptr_array_remove_index (collection->array, index);

	return TRUE;
}

/**
 * g_mime_attachment_collection_remove_at:
 * @collection: a #GMimeAttachmentCollection
 * @index: an existing attachment index
 *
 * Removes the entry at @index, releasing its reference.
 **/
void
g_mime_attachment_collection_remove_at (GMimeAttachmentCollection *collection, int index)
{
	g_return_if_fail (GMIME_IS_ATTACHMENT_COLLECTION (collection));
	g_return_if_fail (index >= 0 && (guint) index < collection->array->len);

	g_ptr_array_remove_index (collection->array, index);
}
