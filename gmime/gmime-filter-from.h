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


#ifndef __GMIME_FILTER_FROM_H__
#define __GMIME_FILTER_FROM_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_FROM            (g_mime_filter_from_get_type ())
#define GMIME_FILTER_FROM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_FROM, GMimeFilterFrom))
#define GMIME_FILTER_FROM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_FROM, GMimeFilterFromClass))
#define GMIME_IS_FILTER_FROM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_FROM))
#define GMIME_IS_FILTER_FROM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_FROM))
#define GMIME_FILTER_FROM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_FROM, GMimeFilterFromClass))

typedef struct _GMimeFilterFrom GMimeFilterFrom;
typedef struct _GMimeFilterFromClass GMimeFilterFromClass;


/**
 * GMimeFilterFromMode:
 * @GMIME_FILTER_FROM_MODE_DEFAULT: Default mode.
 * @GMIME_FILTER_FROM_MODE_ESCAPE: Escape 'From ' lines with a '>'
 * @GMIME_FILTER_FROM_MODE_ARMOR: QP-Encode 'From ' lines
 *
 * The mode for a #GMimeFilterFrom filter.
 **/
typedef enum {
	GMIME_FILTER_FROM_MODE_DEFAULT  = 0,
	GMIME_FILTER_FROM_MODE_ESCAPE   = 0,
	GMIME_FILTER_FROM_MODE_ARMOR    = 1
} GMimeFilterFromMode;


/**
 * GMimeFilterFrom:
 * @parent_object: parent #GMimeFilter
 * @mode: #GMimeFilterFromMode
 * @midline: %TRUE if in the middle of a line
 *
 * A filter for armoring or escaping lines beginning with "From ".
 **/
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

G_END_DECLS

#endif /* __GMIME_FILTER_FROM_H__ */
