/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_STREAM_NULL_H__
#define __GMIME_STREAM_NULL_H__

#include <glib.h>
#include <gmime/gmime-stream.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_STREAM_NULL            (g_mime_stream_null_get_type ())
#define GMIME_STREAM_NULL(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_STREAM_NULL, GMimeStreamNull))
#define GMIME_STREAM_NULL_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_STREAM_NULL, GMimeStreamNullClass))
#define GMIME_IS_STREAM_NULL(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_STREAM_NULL))
#define GMIME_IS_STREAM_NULL_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_STREAM_NULL))
#define GMIME_STREAM_NULL_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_STREAM_NULL, GMimeStreamNullClass))

typedef struct _GMimeStreamNull GMimeStreamNull;
typedef struct _GMimeStreamNullClass GMimeStreamNullClass;

struct _GMimeStreamNull {
	GMimeStream parent_object;
	
	size_t written;
};

struct _GMimeStreamNullClass {
	GMimeStreamClass parent_class;
	
};


GType g_mime_stream_null_get_type (void);

GMimeStream *g_mime_stream_null_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_NULL_H__ */
