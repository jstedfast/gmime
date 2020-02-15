/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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


#ifndef __GMIME_FILTER_UNIX2DOS_H__
#define __GMIME_FILTER_UNIX2DOS_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_UNIX2DOS            (g_mime_filter_unix2dos_get_type ())
#define GMIME_FILTER_UNIX2DOS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_UNIX2DOS, GMimeFilterUnix2Dos))
#define GMIME_FILTER_UNIX2DOS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_UNIX2DOS, GMimeFilterUnix2DosClass))
#define GMIME_IS_FILTER_UNIX2DOS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_UNIX2DOS))
#define GMIME_IS_FILTER_UNIX2DOS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_UNIX2DOS))
#define GMIME_FILTER_UNIX2DOS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_UNIX2DOS, GMimeFilterUnix2DosClass))

typedef struct _GMimeFilterUnix2Dos GMimeFilterUnix2Dos;
typedef struct _GMimeFilterUnix2DosClass GMimeFilterUnix2DosClass;

/**
 * GMimeFilterUnix2Dos:
 * @parent_object: parent #GMimeFilter
 * @ensure_newline: %TRUE if the filter should ensure that the stream ends with a new line
 * @pc: the previous character encountered
 *
 * A filter to convert a stream from Windows/DOS line endings to Unix line endings.
 **/
struct _GMimeFilterUnix2Dos {
	GMimeFilter parent_object;
	
	gboolean ensure_newline;
	char pc;
};

struct _GMimeFilterUnix2DosClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_unix2dos_get_type (void);

GMimeFilter *g_mime_filter_unix2dos_new (gboolean ensure_newline);

G_END_DECLS

#endif /* __GMIME_FILTER_UNIX2DOS_H__ */
