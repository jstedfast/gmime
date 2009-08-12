/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2009 Jeffrey Stedfast
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
 * GMimeFilterCRLFDirection:
 * @GMIME_FILTER_CRLF_ENCODE: Convert from Unix line endings to CRLF.
 * @GMIME_FILTER_CRLF_DECODE: Convert from CRLF to Unix line endings.
 *
 * The direction in which the CRLF filter should convert.
 **/
typedef enum {
	GMIME_FILTER_CRLF_ENCODE,
	GMIME_FILTER_CRLF_DECODE
} GMimeFilterCRLFDirection;


/**
 * GMimeFilterCRLFMode:
 * @GMIME_FILTER_CRLF_MODE_CRLF_DOTS: Escape lines beginning with a '.'
 * @GMIME_FILTER_CRLF_MODE_CRLF_ONLY: Do only LF->CRLF conversion
 *
 * The mode for the #GMimeFilterCRLF filter.
 **/
typedef enum {
	GMIME_FILTER_CRLF_MODE_CRLF_DOTS,
	GMIME_FILTER_CRLF_MODE_CRLF_ONLY
} GMimeFilterCRLFMode;


struct _GMimeFilterCRLF {
	GMimeFilter parent_object;
	
	GMimeFilterCRLFDirection direction;
	GMimeFilterCRLFMode mode;
	gboolean saw_cr;
	gboolean saw_lf;
	gboolean saw_dot;
};

struct _GMimeFilterCRLFClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_crlf_get_type (void);

GMimeFilter *g_mime_filter_crlf_new (GMimeFilterCRLFDirection direction, GMimeFilterCRLFMode mode);

G_END_DECLS

#endif /* __GMIME_FILTER_CRLF_H__ */
