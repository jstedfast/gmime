/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#include "gmime-data-wrapper.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"

/**
 * g_mime_data_wrapper_new:
 *
 * Returns a new data wrapper object.
 **/
GMimeDataWrapper *
g_mime_data_wrapper_new (void)
{
	return g_new0 (GMimeDataWrapper, 1);
}


/**
 * g_mime_data_wrapper_new_with_stream:
 * @stream:
 * @encoding:
 *
 * Returns a data wrapper around @stream. Since the wrapper owns it's
 * own reference on the stream, caller is responsible for unrefing
 * it's own copy.
 **/
GMimeDataWrapper *
g_mime_data_wrapper_new_with_stream (GMimeStream *stream, GMimePartEncodingType encoding)
{
	GMimeDataWrapper *wrapper;
	
	wrapper = g_new (GMimeDataWrapper, 1);
	wrapper->encoding = encoding;
	wrapper->stream = stream;
	if (stream)
		g_mime_stream_ref (stream);
	
	return wrapper;
}


/**
 * g_mime_data_wrapper_destroy:
 * @wrapper:
 *
 * Destroys the data wrapper and unref's its internal stream.
 **/
void
g_mime_data_wrapper_destroy (GMimeDataWrapper *wrapper)
{
	if (wrapper) {
		if (wrapper->stream)
			g_mime_stream_unref (wrapper->stream);
		g_free (wrapper);
	}
}


/**
 * g_mime_data_wrapper_set_stream:
 * @wrapper:
 * @stream:
 *
 * Replaces the wrapper's internal stream with @stream.
 * Note: caller is responsible for it's own reference on
 * @stream.
 **/
void
g_mime_data_wrapper_set_stream (GMimeDataWrapper *wrapper, GMimeStream *stream)
{
	g_return_if_fail (wrapper != NULL);
	
	if (wrapper->stream)
		g_mime_stream_unref (wrapper->stream);
	wrapper->stream = stream;
	if (stream)
		g_mime_stream_ref (stream);
}


/**
 * g_mime_data_wrapper_get_stream:
 * @wrapper:
 *
 * Returns a reference to the internal stream. Caller is responsable
 * for unrefing it.
 **/
GMimeStream *
g_mime_data_wrapper_get_stream (GMimeDataWrapper *wrapper)
{
	g_return_val_if_fail (wrapper != NULL, NULL);
	
	if (wrapper->stream == NULL)
		return NULL;
	
	g_mime_stream_ref (wrapper->stream);
	
	return wrapper->stream;
}


/**
 * g_mime_data_wrapper_set_encoding:
 * @wrapper:
 * @encoding:
 *
 * Sets the encoding type of the internal stream.
 **/
void
g_mime_data_wrapper_set_encoding (GMimeDataWrapper *wrapper, GMimePartEncodingType encoding)
{
	g_return_if_fail (wrapper != NULL);
	
	wrapper->encoding = encoding;
}


/**
 * g_mime_data_wrapper_get_encoding:
 * @wrapper:
 *
 * Returns the encoding type of the internal stream.
 **/
GMimePartEncodingType
g_mime_data_wrapper_get_encoding (GMimeDataWrapper *wrapper)
{
	g_return_val_if_fail (wrapper != NULL, GMIME_PART_ENCODING_DEFAULT);
	
	return wrapper->encoding;
}


/**
 * g_mime_data_wrapper_write_to_stream:
 * @wrapper:
 * @stream: output stream
 *
 * Write's the raw (decoded) data to the output stream.
 *
 * Returns the number of bytes written or -1 on failure.
 **/
ssize_t
g_mime_data_wrapper_write_to_stream (GMimeDataWrapper *wrapper, GMimeStream *stream)
{
	GMimeStream *filtered_stream;
	GMimeFilter *filter;
	ssize_t written;
	
	g_return_val_if_fail (wrapper != NULL, -1);
	g_return_val_if_fail (wrapper->stream != NULL, -1);
	
	filtered_stream = g_mime_stream_filter_new_with_stream (wrapper->stream);
	switch (wrapper->encoding) {
	case GMIME_PART_ENCODING_BASE64:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_BASE64_DEC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		break;
	case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_QP_DEC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		break;
	default:
		break;
	}
	
	written = g_mime_stream_write_to_stream (filtered_stream, stream);
	g_mime_stream_unref (filtered_stream);
	
	g_mime_stream_reset (wrapper->stream);
	
	return written;
}
