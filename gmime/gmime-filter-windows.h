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


#ifndef __GMIME_FILTER_WINDOWS_H__
#define __GMIME_FILTER_WINDOWS_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_WINDOWS            (g_mime_filter_windows_get_type ())


/**
 * GMimeFilterWindows:
 *
 * A filter for detecting whether or not a text stream claimed to be
 * iso-8859-X is really that charset or if it is really a
 * Windows-CP125x charset.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterWindows, g_mime_filter_windows, GMIME, FILTER_WINDOWS, GMimeFilter)

struct _GMimeFilterWindowsClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_windows_new (const char *claimed_charset);


gboolean g_mime_filter_windows_is_windows_charset (GMimeFilterWindows *filter);

const char *g_mime_filter_windows_real_charset (GMimeFilterWindows *filter);

G_END_DECLS

#endif /* __GMIME_FILTER_WINDOWS_H__ */
