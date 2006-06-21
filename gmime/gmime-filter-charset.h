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


#ifndef __GMIME_FILTER_CHARSET_H__
#define __GMIME_FILTER_CHARSET_H__

#include <iconv.h>
#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_CHARSET            (g_mime_filter_charset_get_type ())
#define GMIME_FILTER_CHARSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_CHARSET, GMimeFilterCharset))
#define GMIME_FILTER_CHARSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_CHARSET, GMimeFilterCharsetClass))
#define GMIME_IS_FILTER_CHARSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_CHARSET))
#define GMIME_IS_FILTER_CHARSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_CHARSET))
#define GMIME_FILTER_CHARSET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_CHARSET, GMimeFilterCharsetClass))

typedef struct _GMimeFilterCharset GMimeFilterCharset;
typedef struct _GMimeFilterCharsetClass GMimeFilterCharsetClass;

struct _GMimeFilterCharset {
	GMimeFilter parent_object;
	
	char *from_charset;
	char *to_charset;
	iconv_t cd;
};

struct _GMimeFilterCharsetClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_charset_get_type (void);

GMimeFilter *g_mime_filter_charset_new (const char *from_charset, const char *to_charset);

G_END_DECLS

#endif /* __GMIME_FILTER_CHARSET_H__ */
