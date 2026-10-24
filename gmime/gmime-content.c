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

#include <errno.h>

#include "gmime-content.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-internal.h"


/**
 * SECTION: gmime-content
 * @title: GMimeContent
 * @short_description: Content objects
 * @see_also: #GMimePart
 *
 * A #GMimeContent is a wrapper object for a #GMimePart's body,
 * allowing clients to read the content from the backing stream
 * without having to know whether it is encoded/compressed/etc and not
 * neding to know how to undo said encoding(s).
 **/


static void g_mime_content_class_init (GMimeContentClass *klass);
static void g_mime_content_init (GMimeContent *content, GMimeContentClass *klass);
static void g_mime_content_finalize (GObject *object);

static ssize_t write_to_stream (GMimeContent *content, GMimeStream *stream);
static ssize_t decode_to_stream (GMimeContent *content, GMimeStream *stream);


static GObject *parent_class = NULL;


GType
g_mime_content_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeContentClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_content_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeContent),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_content_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeContent", &info, 0);
	}
	
	return type;
}


static void
g_mime_content_class_init (GMimeContentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_content_finalize;
	
	klass->write_to_stream = write_to_stream;
	klass->decode_to_stream = decode_to_stream;
}

static void
g_mime_content_init (GMimeContent *content, GMimeContentClass *klass)
{
	content->encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED;
	content->stream = NULL;
}

static void
g_mime_content_finalize (GObject *object)
{
	GMimeContent *content = (GMimeContent *) object;
	
	if (content->stream)
		g_object_unref (content->stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_content_new:
 *
 * Creates a new #GMimeContent object.
 *
 * Returns: a new data wrapper object.
 **/
GMimeContent *
g_mime_content_new (void)
{
	return g_object_new (GMIME_TYPE_CONTENT, NULL);
}


/**
 * g_mime_content_new_with_stream:
 * @stream: a #GMimeStream
 * @encoding: stream's encoding
 *
 * Creates a new #GMimeContent object around @stream.
 *
 * Returns: a data wrapper around @stream. Since the wrapper owns its
 * own reference on the stream, caller is responsible for unrefing
 * its own copy.
 **/
GMimeContent *
g_mime_content_new_with_stream (GMimeStream *stream, GMimeContentEncoding encoding)
{
	GMimeContent *content;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	
	content = g_mime_content_new ();
	content->encoding = encoding;
	content->stream = stream;
	
	if (stream)
		g_object_ref (stream);
	
	return content;
}


/**
 * g_mime_content_set_stream:
 * @content: a #GMimeContent
 * @stream: a #GMimeStream
 *
 * Replaces the wrapper's internal stream with @stream. Don't forget,
 * if @stream is not of the same encoding as the old stream, you'll
 * want to call g_mime_content_set_encoding() as well.
 *
 * Note: caller is responsible for its own reference on
 * @stream.
 **/
void
g_mime_content_set_stream (GMimeContent *content, GMimeStream *stream)
{
	g_return_if_fail (GMIME_IS_CONTENT (content));
	g_return_if_fail (GMIME_IS_STREAM (stream));
	
	if (stream)
		g_object_ref (stream);
	
	if (content->stream)
		g_object_unref (content->stream);
	
	content->stream = stream;
	content->has_newline_format = FALSE;
}


/**
 * g_mime_content_get_stream:
 * @content: a #GMimeContent
 *
 * Gets a reference to the stream wrapped by @content.
 *
 * Returns: (transfer none): a reference to the internal stream.
 **/
GMimeStream *
g_mime_content_get_stream (GMimeContent *content)
{
	g_return_val_if_fail (GMIME_IS_CONTENT (content), NULL);
	
	return content->stream;
}


/**
 * g_mime_content_set_encoding:
 * @content: a #GMimeContent
 * @encoding: encoding
 *
 * Sets the encoding type of the internal stream.
 **/
void
g_mime_content_set_encoding (GMimeContent *content, GMimeContentEncoding encoding)
{
	g_return_if_fail (GMIME_IS_CONTENT (content));
	
	content->encoding = encoding;
}


/**
 * g_mime_content_get_encoding:
 * @content: a #GMimeContent
 *
 * Gets the encoding type of the stream wrapped by @content.
 *
 * Returns: the encoding type of the internal stream.
 **/
GMimeContentEncoding
g_mime_content_get_encoding (GMimeContent *content)
{
	g_return_val_if_fail (GMIME_IS_CONTENT (content), GMIME_CONTENT_ENCODING_UNSPECIFIED);
	
	return content->encoding;
}


/**
 * g_mime_content_get_newline_format:
 * @content: a #GMimeContent
 * @format: (out) (optional): the observed new-line format
 *
 * Gets the raw content's new-line format, if known. This is a hint for
 * verifying digital signatures, not a request to convert the content.
 * Mixed newlines are reported as %GMIME_NEWLINE_FORMAT_MIXED. Content
 * without LF or CRLF has no known format.
 *
 * Returns: %TRUE if @format is known, or %FALSE otherwise, leaving @format
 *   unchanged.
 **/
gboolean
g_mime_content_get_newline_format (GMimeContent *content, GMimeNewLineFormat *format)
{
	g_return_val_if_fail (GMIME_IS_CONTENT (content), FALSE);

	if (!content->has_newline_format)
		return FALSE;

	if (format)
		*format = content->newline_format;

	return TRUE;
}

/**
 * g_mime_content_set_newline_format:
 * @content: a #GMimeContent
 * @format: the observed new-line format
 *
 * Sets the raw content's new-line format hint without modifying its bytes.
 **/
void
g_mime_content_set_newline_format (GMimeContent *content, GMimeNewLineFormat format)
{
	g_return_if_fail (GMIME_IS_CONTENT (content));
	g_return_if_fail (format >= GMIME_NEWLINE_FORMAT_UNIX && format <= GMIME_NEWLINE_FORMAT_MIXED);

	content->newline_format = format;
	content->has_newline_format = TRUE;
}

/**
 * g_mime_content_clear_newline_format:
 * @content: a #GMimeContent
 *
 * Clears the new-line format hint without modifying the content. Replacing
 * the backing stream also clears this hint.
 **/
void
g_mime_content_clear_newline_format (GMimeContent *content)
{
	g_return_if_fail (GMIME_IS_CONTENT (content));

	content->has_newline_format = FALSE;
}

static GMimeStream *
create_decoder_stream (GMimeContent *content, GMimeStream *stream)
{
	GMimeStream *filtered_stream;
	GMimeFilter *filter;

	filtered_stream = g_mime_stream_filter_new (stream);

	switch (content->encoding) {
	case GMIME_CONTENT_ENCODING_BASE64:
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
	case GMIME_CONTENT_ENCODING_UUENCODE:
		filter = g_mime_filter_basic_new (content->encoding, FALSE);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		g_object_unref (filter);
		break;
	default:
		break;
	}

	return filtered_stream;
}

/**
 * g_mime_content_open:
 * @content: a #GMimeContent
 *
 * Rewinds the backing stream and returns a new stream which decodes the
 * content as it is read, without buffering the entire content in memory.
 * Default, 7bit, 8bit, and binary content pass through unchanged.
 *
 * The returned stream retains the backing stream but does not close it.
 * It has its own decoder state, using the encoding at the time of this
 * call. Readers share the backing stream's position, so opening another
 * reader or otherwise using that stream can interfere with existing readers.
 * Closing a reader does not rewind the backing stream.
 *
 * Returns: (nullable) (transfer full): a decoded stream, or %NULL if
 * rewinding fails, with errno set by the backing stream. Read failures are
 * reported by g_mime_stream_read() as %-1 with errno set.
 **/
GMimeStream *
g_mime_content_open (GMimeContent *content)
{
	g_return_val_if_fail (GMIME_IS_CONTENT (content), NULL);
	g_return_val_if_fail (content->stream != NULL, NULL);

	if (g_mime_stream_reset (content->stream) == -1)
		return NULL;

	return create_decoder_stream (content, content->stream);
}

static ssize_t
write_to_stream (GMimeContent *content, GMimeStream *stream)
{
	ssize_t nread, total = 0;
	char buffer[4096];

	if (g_mime_stream_reset (content->stream) == -1)
		return -1;

	while ((nread = g_mime_stream_read (content->stream, buffer, sizeof (buffer))) > 0) {
		if (nread > G_MAXSSIZE - total) {
			errno = EOVERFLOW;
			return -1;
		}

		if (_g_mime_stream_write_all (stream, buffer, nread) == -1)
			return -1;

		total += nread;
	}

	if (nread == -1 || g_mime_stream_reset (content->stream) == -1)
		return -1;

	return total;
}

static ssize_t
decode_to_stream (GMimeContent *content, GMimeStream *stream)
{
	GMimeStream *filtered_stream;
	ssize_t written;
	int errsv;

	filtered_stream = create_decoder_stream (content, stream);
	written = g_mime_content_write_to_stream (content, filtered_stream);

	if (written != -1) {
		if (g_mime_stream_flush (filtered_stream) == -1)
			written = -1;
		else
			written = _g_mime_stream_filter_get_written ((GMimeStreamFilter *) filtered_stream);
	}

	errsv = errno;
	g_object_unref (filtered_stream);

	if (written == -1)
		errno = errsv;

	return written;
}


/**
 * g_mime_content_write_to_stream:
 * @content: a #GMimeContent
 * @stream: output stream
 *
 * Copies the backing stream verbatim, without decoding its transfer
 * encoding. Rewinds the source before copying and again on success.
 * On an I/O failure, leaves the source at its current position.
 * Neither stream is closed and the output stream is not flushed.
 *
 * Use g_mime_content_decode_to_stream() for decoded bytes or
 * g_mime_content_open() for a decoded reader.
 *
 * Returns: the number of bytes written or %-1 on failure, with errno set.
 **/
ssize_t
g_mime_content_write_to_stream (GMimeContent *content, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_CONTENT (content), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (content->stream != NULL, -1);
	
	return GMIME_CONTENT_GET_CLASS (content)->write_to_stream (content, stream);
}


/**
 * g_mime_content_decode_to_stream:
 * @content: a #GMimeContent
 * @stream: output stream
 *
 * Decodes the content into @stream using the content's transfer encoding.
 * Default, 7bit, 8bit, and binary data pass through unchanged. Calls the
 * virtual g_mime_content_write_to_stream() through a decoder, rewinding the
 * source before copying and again on success. An I/O failure leaves the
 * source at its current position. Neither stream is closed or flushed;
 * only the decoder's buffered data is flushed.
 *
 * Returns: the number of decoded bytes written or %-1 on failure, with
 * errno set. A failure may leave partial output in @stream.
 **/
ssize_t
g_mime_content_decode_to_stream (GMimeContent *content, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_CONTENT (content), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (content->stream != NULL, -1);

	return GMIME_CONTENT_GET_CLASS (content)->decode_to_stream (content, stream);
}
