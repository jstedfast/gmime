/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifndef __GMIME_FILTER_BEST_H__
#define __GMIME_FILTER_BEST_H__

#include <gmime/gmime-filter.h>
#include <gmime/gmime-charset.h>
#include <gmime/gmime-utils.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_FILTER_BEST            (g_mime_filter_best_get_type ())
#define GMIME_FILTER_BEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_BEST, GMimeFilterBest))
#define GMIME_FILTER_BEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_BEST, GMimeFilterBestClass))
#define GMIME_IS_FILTER_BEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_BEST))
#define GMIME_IS_FILTER_BEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_BEST))
#define GMIME_FILTER_BEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_BEST, GMimeFilterBestClass))

typedef struct _GMimeFilterBest GMimeFilterBest;
typedef struct _GMimeFilterBestClass GMimeFilterBestClass;

enum {
	GMIME_FILTER_BEST_CHARSET      = (1 << 0),
	GMIME_FILTER_BEST_ENCODING     = (1 << 1)
};

typedef enum {
	GMIME_BEST_ENCODING_7BIT,
	GMIME_BEST_ENCODING_8BIT,
	GMIME_BEST_ENCODING_BINARY
} GMimeBestEncoding;

struct _GMimeFilterBest {
	GMimeFilter parent_object;
	
	unsigned int flags;
	
	/* for best charset detection */
	GMimeCharset charset;
	
	/* for best encoding detection */
	unsigned int count0;      /* count of null bytes */
	unsigned int count8;      /* count of 8bit bytes */
	unsigned int total;       /* total octets */
	
	unsigned int maxline;     /* longest line length */
	unsigned int linelen;     /* current line length */
	
	unsigned char frombuf[6];
	unsigned int fromlen : 4;
	unsigned int hadfrom : 1;
	
	unsigned int startline : 1;
	unsigned int midline : 1;
};

struct _GMimeFilterBestClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_best_get_type (void);

GMimeFilter *g_mime_filter_best_new (unsigned int flags);

const char *g_mime_filter_best_charset (GMimeFilterBest *best);

GMimePartEncodingType g_mime_filter_best_encoding (GMimeFilterBest *best, GMimeBestEncoding required);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_BEST_H__ */
