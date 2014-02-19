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


#ifndef __GMIME_FILTER_CRLF_H__
#define __GMIME_FILTER_CRLF_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_CRLF            (g_mime_filter_crlf_get_type ())
#define GMIME_FILTER_CRLF(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_CRLF, GMimeFilterCRLF))
#define GMIME_FILTER_CRLF_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_CRLF, GMimeFilterCRLFClass))
#define GMIME_IS_FILTER_CRLF(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_CRLF))
#define GMIME_IS_FILTER_CRLF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_CRLF))
#define GMIME_FILTER_CRLF_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_CRLF, GMimeFilterCRLFClass))

typedef struct _GMimeFilterCRLF GMimeFilterCRLF;
typedef struct _GMimeFilterCRLFClass GMimeFilterCRLFClass;

/**
 * GMimeFilterCRLF:
 * @parent_object: parent #GMimeFilter
 * @encode: encoding vs decoding line endings/dots
 * @saw_cr: previous char was a CR
 * @saw_lf: previous char was a LF
 * @saw_dot: previous char was a period
 *
 * A filter to convert between line-ending formats and encode/decode
 * lines beginning with a '.'.
 **/
struct _GMimeFilterCRLF {
	GMimeFilter parent_object;
	
	gboolean encode;
	gboolean dots;
	
	gboolean saw_cr;
	gboolean saw_lf;
	gboolean saw_dot;
};

struct _GMimeFilterCRLFClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_crlf_get_type (void);

GMimeFilter *g_mime_filter_crlf_new (gboolean encode, gboolean dots);

G_END_DECLS

#endif /* __GMIME_FILTER_CRLF_H__ */
