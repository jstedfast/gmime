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


#ifndef __GMIME_FILTER_MD5_H__
#define __GMIME_FILTER_MD5_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_CHECKSUM            (g_mime_filter_checksum_get_type ())
#define GMIME_FILTER_CHECKSUM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_CHECKSUM, GMimeFilterChecksum))
#define GMIME_FILTER_CHECKSUM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_CHECKSUM, GMimeFilterChecksumClass))
#define GMIME_IS_FILTER_CHECKSUM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_CHECKSUM))
#define GMIME_IS_FILTER_CHECKSUM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_CHECKSUM))
#define GMIME_FILTER_CHECKSUM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_CHECKSUM, GMimeFilterChecksumClass))

typedef struct _GMimeFilterChecksum GMimeFilterChecksum;
typedef struct _GMimeFilterChecksumClass GMimeFilterChecksumClass;

/**
 * GMimeFilterChecksum:
 * @parent_object: parent #GMimeFilter
 * @checksum: The checksum context
 *
 * A filter for calculating the checksum of a stream.
 **/
struct _GMimeFilterChecksum {
	GMimeFilter parent_object;
	
	GChecksum *checksum;
};

struct _GMimeFilterChecksumClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_checksum_get_type (void);

GMimeFilter *g_mime_filter_checksum_new (GChecksumType type);

size_t g_mime_filter_checksum_get_digest (GMimeFilterChecksum *checksum, unsigned char *digest, size_t len);
gchar *g_mime_filter_checksum_get_string (GMimeFilterChecksum *checksum);

G_END_DECLS

#endif /* __GMIME_FILTER_CHECKSUM_H__ */
