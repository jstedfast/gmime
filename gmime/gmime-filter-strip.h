/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
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
 *
 */


#ifndef __GMIME_FILTER_STRIP_H__
#define __GMIME_FILTER_STRIP_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-filter.h>

#define GMIME_TYPE_FILTER_STRIP            (g_mime_filter_strip_get_type ())
#define GMIME_FILTER_STRIP(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_FILTER_STRIP, GMimeFilterStrip))
#define GMIME_FILTER_STRIP_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_STRIP, GMimeFilterStripClass))
#define GMIME_IS_FILTER_STRIP(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_FILTER_STRIP))
#define GMIME_IS_FILTER_STRIP_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_STRIP))
#define GMIME_FILTER_STRIP_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_FILTER_STRIP, GMimeFilterStripClass))

typedef struct _GMimeFilterStrip GMimeFilterStrip;
typedef struct _GMimeFilterStripClass GMimeFilterStripClass;

struct _GMimeFilterStrip {
	GMimeFilter parent_object;
	
};

struct _GMimeFilterStripClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_strip_get_type (void);

GMimeFilter *g_mime_filter_strip_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_STRIP_H__ */
