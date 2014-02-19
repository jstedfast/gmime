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


#ifndef __GMIME_FILTER_ENRICHED_H__
#define __GMIME_FILTER_ENRICHED_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_ENRICHED            (g_mime_filter_enriched_get_type ())
#define GMIME_FILTER_ENRICHED(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_ENRICHED, GMimeFilterEnriched))
#define GMIME_FILTER_ENRICHED_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_ENRICHED, GMimeFilterEnrichedClass))
#define GMIME_IS_FILTER_ENRICHED(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_ENRICHED))
#define GMIME_IS_FILTER_ENRICHED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_ENRICHED))
#define GMIME_FILTER_ENRICHED_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_ENRICHED, GMimeFilterEnrichedClass))

typedef struct _GMimeFilterEnriched GMimeFilterEnriched;
typedef struct _GMimeFilterEnrichedClass GMimeFilterEnrichedClass;


/**
 * GMIME_FILTER_ENRICHED_IS_RICHTEXT:
 *
 * A bit flag for g_mime_filter_enriched_new() which signifies that
 * the filter should expect Rich Text (aka RTF).
 **/
#define GMIME_FILTER_ENRICHED_IS_RICHTEXT  (1 << 0)

/**
 * GMimeFilterEnriched:
 * @parent_object: parent #GMimeFilter
 * @flags: bit flags
 * @nofill: nofill tag state
 *
 * A filter for converting text/enriched or text/richtext textual
 * streams into text/html.
 **/
struct _GMimeFilterEnriched {
	GMimeFilter parent_object;
	
	guint32 flags;
	int nofill;
};

struct _GMimeFilterEnrichedClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_enriched_get_type (void);

GMimeFilter *g_mime_filter_enriched_new (guint32 flags);

G_END_DECLS

#endif /* __GMIME_FILTER_ENRICHED_H__ */
