/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_FILTER_FROM_H__
#define __GMIME_FILTER_FROM_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-filter.h>

#define GMIME_TYPE_FILTER_FROM            (g_mime_filter_from_get_type ())
#define GMIME_FILTER_FROM(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_FILTER_FROM, GMimeFilterFrom))
#define GMIME_FILTER_FROM_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_FROM, GMimeFilterFromClass))
#define GMIME_IS_FILTER_FROM(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_FILTER_FROM))
#define GMIME_IS_FILTER_FROM_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_FROM))
#define GMIME_FILTER_FROM_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_FILTER_FROM, GMimeFilterFromClass))

typedef struct _GMimeFilterFrom GMimeFilterFrom;
typedef struct _GMimeFilterFromClass GMimeFilterFromClass;

typedef enum {
	GMIME_FILTER_FROM_MODE_DEFAULT  = 0,
	GMIME_FILTER_FROM_MODE_ESCAPE   = 0,
	GMIME_FILTER_FROM_MODE_ARMOR    = 1,
} GMimeFilterFromMode;

struct _GMimeFilterFrom {
	GMimeFilter parent_object;
	
	GMimeFilterFromMode mode;
	gboolean midline;
};

struct _GMimeFilterFromClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_from_get_type (void);

GMimeFilter *g_mime_filter_from_new (GMimeFilterFromMode mode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_FROM_H__ */
