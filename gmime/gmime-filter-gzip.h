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


#ifndef __GMIME_FILTER_GZIP_H__
#define __GMIME_FILTER_GZIP_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_GZIP            (g_mime_filter_gzip_get_type ())



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
 *
 * A filter for compresing or decompressing a gzip stream.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterGZip, g_mime_filter_gzip, GMIME, FILTER_GZIP, GMimeFilter)

struct _GMimeFilterGZipClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_gzip_new (GMimeFilterGZipMode mode, int level);

const char *g_mime_filter_gzip_get_filename (GMimeFilterGZip *gzip);
void g_mime_filter_gzip_set_filename (GMimeFilterGZip *gzip, const char *filename);

const char *g_mime_filter_gzip_get_comment (GMimeFilterGZip *gzip);
void g_mime_filter_gzip_set_comment (GMimeFilterGZip *gzip, const char *comment);

G_END_DECLS

#endif /* __GMIME_FILTER_GZIP_H__ */
