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


/**
 * GMimeFilterPassThrough:
 *
 * A filter that returns input unchanged without copying or buffering it.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterPassThrough, g_mime_filter_pass_through, GMIME, FILTER_PASS_THROUGH, GMimeFilter)

struct _GMimeFilterPassThroughClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_pass_through_new (void);

G_END_DECLS

#endif /* __GMIME_FILTER_PASS_THROUGH_H__ */
