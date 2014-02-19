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


#ifndef __GMIME_FILTER_GZIP_H__
#define __GMIME_FILTER_GZIP_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_GZIP            (g_mime_filter_gzip_get_type ())
#define GMIME_FILTER_GZIP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_GZIP, GMimeFilterGZip))
#define GMIME_FILTER_GZIP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_GZIP, GMimeFilterGZipClass))
#define GMIME_IS_FILTER_GZIP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_GZIP))
#define GMIME_IS_FILTER_GZIP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_GZIP))
#define GMIME_FILTER_GZIP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_GZIP, GMimeFilterGZipClass))

typedef struct _GMimeFilterGZip GMimeFilterGZip;
typedef struct _GMimeFilterGZipClass GMimeFilterGZipClass;


/**
 * GMimeFilterGZipMode:
 * @GMIME_FILTER_GZIP_MODE_ZIP: Compress (zip) mode.
 * @GMIME_FILTER_GZIP_MODE_UNZIP: Uncompress (unzip) mode.
 *
 * The mode for the #GMimeFilterGZip filter.
 **/
typedef enum {
	GMIME_FILTER_GZIP_MODE_ZIP,
	GMIME_FILTER_GZIP_MODE_UNZIP
} GMimeFilterGZipMode;


/**
 * GMimeFilterGZip:
 * @parent_object: parent #GMimeFilter
 * @priv: private state data
 * @mode: #GMimeFilterGZipMode
 * @level: compression level
 *
 * A filter for compresing or decompressing a gzip stream.
 **/
struct _GMimeFilterGZip {
	GMimeFilter parent_object;
	
	struct _GMimeFilterGZipPrivate *priv;
	
	GMimeFilterGZipMode mode;
	int level;
};

struct _GMimeFilterGZipClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_gzip_get_type (void);

GMimeFilter *g_mime_filter_gzip_new (GMimeFilterGZipMode mode, int level);

G_END_DECLS

#endif /* __GMIME_FILTER_GZIP_H__ */
