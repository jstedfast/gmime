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


#ifndef __GMIME_FILTER_WINDOWS_H__
#define __GMIME_FILTER_WINDOWS_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_WINDOWS            (g_mime_filter_windows_get_type ())
#define GMIME_FILTER_WINDOWS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_WINDOWS, GMimeFilterWindows))
#define GMIME_FILTER_WINDOWS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_WINDOWS, GMimeFilterWindowsClass))
#define GMIME_IS_FILTER_WINDOWS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_WINDOWS))
#define GMIME_IS_FILTER_WINDOWS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_WINDOWS))
#define GMIME_FILTER_WINDOWS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_WINDOWS, GMimeFilterWindowsClass))

typedef struct _GMimeFilterWindows GMimeFilterWindows;
typedef struct _GMimeFilterWindowsClass GMimeFilterWindowsClass;

/**
 * GMimeFilterWindows:
 * @parent_object: parent #GMimeFilter
 * @is_windows: %TRUE if the stream is detected to be a windows-cp125x charset
 * @claimed_charset: charset the text stream is claimed to be
 *
 * A filter for detecting whether or not a text stream claimed to be
 * iso-8859-X is really that charset or if it is really a
 * Windows-CP125x charset.
 **/
struct _GMimeFilterWindows {
	GMimeFilter parent_object;
	
	gboolean is_windows;
	char *claimed_charset;
};

struct _GMimeFilterWindowsClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_windows_get_type (void);

GMimeFilter *g_mime_filter_windows_new (const char *claimed_charset);


gboolean g_mime_filter_windows_is_windows_charset (GMimeFilterWindows *filter);

const char *g_mime_filter_windows_real_charset (GMimeFilterWindows *filter);

G_END_DECLS

#endif /* __GMIME_FILTER_WINDOWS_H__ */
