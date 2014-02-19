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

#include "gmime-data-wrapper.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"


/**
 * SECTION: gmime-data-wrapper
 * @title: GMimeDataWrapper
 * @short_description: Content objects
 * @see_also: #GMimePart
 *
 * A #GMimeDataWrapper is a wrapper object for a #GMimePart's body,
 * allowing clients to read the content from the backing stream
 * without having to know whether it is encoded/compressed/etc and not
 * neding to know how to undo said encoding(s).
 **/


static void g_mime_data_wrapper_class_init (GMimeDataWrapperClass *klass);
static void g_mime_data_wrapper_init (GMimeDataWrapper *wrapper, GMimeDataWrapperClass *klass);
static void g_mime_data_wrapper_finalize (GObject *object);

static ssize_t write_to_stream (GMimeDataWrapper *wrapper, GMimeStream *stream);


static GObject *parent_class = NULL;


GType
g_mime_data_wrapper_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeDataWrapperClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_data_wrapper_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeDataWrapper),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_data_wrapper_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeDataWrapper", &info, 0);
	}
	
	return type;
}


static void
g_mime_data_wrapper_class_init (GMimeDataWrapperClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_data_wrapper_finalize;
	
	klass->write_to_stream = write_to_stream;
}

static void
g_mime_data_wrapper_init (GMimeDataWrapper *wrapper, GMimeDataWrapperClass *klass)
{
	wrapper->encoding = GMIME_CONTENT_ENCODING_DEFAULT;
	wrapper->stream = NULL;
}

static void
g_mime_data_wrapper_finalize (GObject *object)
{
	GMimeDataWrapper *wrapper = (GMimeDataWrapper *) object;
	
	if (wrapper->stream)
		g_object_unref (wrapper->stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_data_wrapper_new:
 *
 * Creates a new #GMimeDataWrapper object.
 *
 * Returns: a new data wrapper object.
 **/
GMimeDataWrapper *
g_mime_data_wrapper_new (void)
{
	return g_object_newv (GMIME_TYPE_DATA_WRAPPER, 0, NULL);
}


/**
 * g_mime_data_wrapper_new_with_stream:
 * @stream: a #GMimeStream
 * @encoding: stream's encoding
 *
 * Creates a new #GMimeDataWrapper object around @stream.
 *
 * Returns: a data wrapper around @stream. Since the wrapper owns its
 * own reference on the stream, caller is responsible for unrefing
 * its own copy.
 **/
GMimeDataWrapper *
g_mime_data_wrapper_new_with_stream (GMimeStream *stream, GMimeContentEncoding encoding)
{
	GMimeDataWrapper *wrapper;
	
	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	
	wrapper = g_mime_data_wrapper_new ();
	wrapper->encoding = encoding;
	wrapper->stream = stream;
	
	if (stream)
		g_object_ref (stream);
	
	return wrapper;
}


/**
 * g_mime_data_wrapper_set_stream:
 * @wrapper: a #GMimeDataWrapper
 * @stream: a #GMimeStream
 *
 * Replaces the wrapper's internal stream with @stream. Don't forget,
 * if @stream is not of the same encoding as the old stream, you'll
 * want to call g_mime_data_wrapper_set_encoding() as well.
 *
 * Note: caller is responsible for its own reference on
 * @stream.
 **/
void
g_mime_data_wrapper_set_stream (GMimeDataWrapper *wrapper, GMimeStream *stream)
{
	g_return_if_fail (GMIME_IS_DATA_WRAPPER (wrapper));
	g_return_if_fail (GMIME_IS_STREAM (stream));
	
	if (stream)
		g_object_ref (stream);
	
	if (wrapper->stream)
		g_object_unref (wrapper->stream);
	
	wrapper->stream = stream;
}


/**
 * g_mime_data_wrapper_get_stream:
 * @wrapper: a #GMimeDataWrapper
 *
 * Gets a reference to the stream wrapped by @wrapper.
 *
 * Returns: (transfer none): a reference to the internal stream.
 **/
GMimeStream *
g_mime_data_wrapper_get_stream (GMimeDataWrapper *wrapper)
{
	g_return_val_if_fail (GMIME_IS_DATA_WRAPPER (wrapper), NULL);
	
	return wrapper->stream;
}


/**
 * g_mime_data_wrapper_set_encoding:
 * @wrapper: a #GMimeDataWrapper
 * @encoding: encoding
 *
 * Sets the encoding type of the internal stream.
 **/
void
g_mime_data_wrapper_set_encoding (GMimeDataWrapper *wrapper, GMimeContentEncoding encoding)
{
	g_return_if_fail (GMIME_IS_DATA_WRAPPER (wrapper));
	
	wrapper->encoding = encoding;
}


/**
 * g_mime_data_wrapper_get_encoding:
 * @wrapper: a #GMimeDataWrapper
 *
 * Gets the encoding type of the stream wrapped by @wrapper.
 *
 * Returns: the encoding type of the internal stream.
 **/
GMimeContentEncoding
g_mime_data_wrapper_get_encoding (GMimeDataWrapper *wrapper)
{
	g_return_val_if_fail (GMIME_IS_DATA_WRAPPER (wrapper), GMIME_CONTENT_ENCODING_DEFAULT);
	
	return wrapper->encoding;
}


static ssize_t
write_to_stream (GMimeDataWrapper *wrapper, GMimeStream *stream)
{
	GMimeStream *filtered_stream;
	GMimeFilter *filter;
	ssize_t written;
	
	g_mime_stream_reset (wrapper->stream);
	
	switch (wrapper->encoding) {
	case GMIME_CONTENT_ENCODING_BASE64:
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
	case GMIME_CONTENT_ENCODING_UUENCODE:
		filter = g_mime_filter_basic_new (wrapper->encoding, FALSE);
		filtered_stream = g_mime_stream_filter_new (wrapper->stream);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		g_object_unref (filter);
		break;
	default:
		filtered_stream = wrapper->stream;
		g_object_ref (wrapper->stream);
		break;
	}
	
	written = g_mime_stream_write_to_stream (filtered_stream, stream);
	g_object_unref (filtered_stream);
	
	g_mime_stream_reset (wrapper->stream);
	
	return written;
}


/**
 * g_mime_data_wrapper_write_to_stream:
 * @wrapper: a #GMimeDataWrapper
 * @stream: output stream
 *
 * Writes the raw (decoded) data to the output stream.
 *
 * Returns: the number of bytes written or %-1 on failure.
 **/
ssize_t
g_mime_data_wrapper_write_to_stream (GMimeDataWrapper *wrapper, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_DATA_WRAPPER (wrapper), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	g_return_val_if_fail (wrapper->stream != NULL, -1);
	
	return GMIME_DATA_WRAPPER_GET_CLASS (wrapper)->write_to_stream (wrapper, stream);
}
