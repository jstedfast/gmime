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


#ifndef __GMIME_FILTER_DOS2UNIX_H__
#define __GMIME_FILTER_DOS2UNIX_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_DOS2UNIX            (g_mime_filter_dos2unix_get_type ())


/**
 * GMimeFilterDos2Unix:
 *
 * A filter converting CRLF pairs to LF while preserving other bytes.
 * A trailing bare CR is preserved unless ensure_newline is enabled.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterDos2Unix, g_mime_filter_dos2unix, GMIME, FILTER_DOS2UNIX, GMimeFilter)

struct _GMimeFilterDos2UnixClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_dos2unix_new (gboolean ensure_newline);

G_END_DECLS

#endif /* __GMIME_FILTER_DOS2UNIX_H__ */
