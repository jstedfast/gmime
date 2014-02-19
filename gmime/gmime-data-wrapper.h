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


#ifndef __GMIME_DATA_WRAPPER_H__
#define __GMIME_DATA_WRAPPER_H__

#include <glib.h>
#include <glib-object.h>

#include <gmime/gmime-content-type.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-utils.h>

G_BEGIN_DECLS

#define GMIME_TYPE_DATA_WRAPPER            (g_mime_data_wrapper_get_type ())
#define GMIME_DATA_WRAPPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_DATA_WRAPPER, GMimeDataWrapper))
#define GMIME_DATA_WRAPPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_DATA_WRAPPER, GMimeDataWrapperClass))
#define GMIME_IS_DATA_WRAPPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_DATA_WRAPPER))
#define GMIME_IS_DATA_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_DATA_WRAPPER))
#define GMIME_DATA_WRAPPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_DATA_WRAPPER, GMimeDataWrapperClass))

typedef struct _GMimeDataWrapper GMimeDataWrapper;
typedef struct _GMimeDataWrapperClass GMimeDataWrapperClass;


/**
 * GMimeDataWrapper:
 * @parent_object: parent #GObject
 * @encoding: the encoding of the content
 * @stream: content stream
 *
 * A wrapper for a stream which may be encoded.
 **/
struct _GMimeDataWrapper {
	GObject parent_object;
	
	GMimeContentEncoding encoding;
	GMimeStream *stream;
};

struct _GMimeDataWrapperClass {
	GObjectClass parent_class;
	
	ssize_t (* write_to_stream) (GMimeDataWrapper *wrapper, GMimeStream *stream);
};


GType g_mime_data_wrapper_get_type (void);

GMimeDataWrapper *g_mime_data_wrapper_new (void);
GMimeDataWrapper *g_mime_data_wrapper_new_with_stream (GMimeStream *stream, GMimeContentEncoding encoding);

void g_mime_data_wrapper_set_stream (GMimeDataWrapper *wrapper, GMimeStream *stream);
GMimeStream *g_mime_data_wrapper_get_stream (GMimeDataWrapper *wrapper);

void g_mime_data_wrapper_set_encoding (GMimeDataWrapper *wrapper, GMimeContentEncoding encoding);
GMimeContentEncoding g_mime_data_wrapper_get_encoding (GMimeDataWrapper *wrapper);

ssize_t g_mime_data_wrapper_write_to_stream (GMimeDataWrapper *wrapper, GMimeStream *stream);

G_END_DECLS

#endif /* __GMIME_DATA_WRAPPER_H__ */
