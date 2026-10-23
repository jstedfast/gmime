/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#ifndef __GMIME_FILTER_PASS_THROUGH_H__
#define __GMIME_FILTER_PASS_THROUGH_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_PASS_THROUGH            (g_mime_filter_pass_through_get_type ())
#define GMIME_FILTER_PASS_THROUGH(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_PASS_THROUGH, GMimeFilterPassThrough))
#define GMIME_FILTER_PASS_THROUGH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_PASS_THROUGH, GMimeFilterPassThroughClass))
#define GMIME_IS_FILTER_PASS_THROUGH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_PASS_THROUGH))
#define GMIME_IS_FILTER_PASS_THROUGH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_PASS_THROUGH))
#define GMIME_FILTER_PASS_THROUGH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_PASS_THROUGH, GMimeFilterPassThroughClass))

typedef struct _GMimeFilterPassThrough GMimeFilterPassThrough;
typedef struct _GMimeFilterPassThroughClass GMimeFilterPassThroughClass;

/**
 * GMimeFilterPassThrough:
 * @parent_object: parent #GMimeFilter
 *
 * A filter that returns input unchanged without copying or buffering it.
 **/
struct _GMimeFilterPassThrough {
	GMimeFilter parent_object;
};

struct _GMimeFilterPassThroughClass {
	GMimeFilterClass parent_class;
};

GType g_mime_filter_pass_through_get_type (void);

GMimeFilter *g_mime_filter_pass_through_new (void);

G_END_DECLS

#endif /* __GMIME_FILTER_PASS_THROUGH_H__ */
