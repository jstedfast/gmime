/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_DATA_WRAPPER_H__
#define __GMIME_DATA_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <glib-object.h>

#include "gmime-type-utils.h"
#include "gmime-content-type.h"
#include "gmime-stream.h"
#include "gmime-utils.h"

#define GMIME_TYPE_DATA_WRAPPER            (g_mime_data_wrapper_get_type ())
#define GMIME_DATA_WRAPPER(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_DATA_WRAPPER, GMimeDataWrapper))
#define GMIME_DATA_WRAPPER_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_DATA_WRAPPER, GMimeDataWrapperClass))
#define GMIME_IS_DATA_WRAPPER(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_DATA_WRAPPER))
#define GMIME_IS_DATA_WRAPPER_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_DATA_WRAPPER))
#define GMIME_DATA_WRAPPER_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_DATA_WRAPPER, GMimeDataWrapperClass))

typedef struct _GMimeDataWrapper GMimeDataWrapper;
typedef struct _GMimeDataWrapperClass GMimeDataWrapperClass;

struct _GMimeDataWrapper {
	GObject parent_object;
	
	GMimePartEncodingType encoding;
	GMimeStream *stream;
};

struct _GMimeDataWrapperClass {
	GObjectClass parent_class;
	
	ssize_t (*write_to_stream) (GMimeDataWrapper *wrapper, GMimeStream *stream);
};


GType g_mime_data_wrapper_get_type (void);

GMimeDataWrapper *g_mime_data_wrapper_new (void);
GMimeDataWrapper *g_mime_data_wrapper_new_with_stream (GMimeStream *stream, GMimePartEncodingType encoding);

void g_mime_data_wrapper_destroy (GMimeDataWrapper *wrapper);

void g_mime_data_wrapper_set_stream (GMimeDataWrapper *wrapper, GMimeStream *stream);
GMimeStream *g_mime_data_wrapper_get_stream (GMimeDataWrapper *wrapper);

void g_mime_data_wrapper_set_encoding (GMimeDataWrapper *wrapper, GMimePartEncodingType encoding);
GMimePartEncodingType g_mime_data_wrapper_get_encoding (GMimeDataWrapper *wrapper);

ssize_t g_mime_data_wrapper_write_to_stream (GMimeDataWrapper *wrapper, GMimeStream *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_DATA_WRAPPER_H__ */
