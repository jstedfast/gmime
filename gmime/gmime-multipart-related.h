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

#ifndef __GMIME_MULTIPART_RELATED_H__
#define __GMIME_MULTIPART_RELATED_H__

#include <gmime/gmime-multipart.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_RELATED            (g_mime_multipart_related_get_type ())


/**
 * GMimeMultipartRelated:
 *
 * A multipart/related whose root document references its other parts.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMultipartRelated, g_mime_multipart_related, GMIME, MULTIPART_RELATED, GMimeMultipart)

struct _GMimeMultipartRelatedClass {
	GMimeMultipartClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeMultipartRelated *g_mime_multipart_related_new (void);

GMimeEntity *g_mime_multipart_related_get_root (GMimeMultipartRelated *related);
void g_mime_multipart_related_set_root (GMimeMultipartRelated *related, GMimeEntity *root);

int g_mime_multipart_related_index_of_uri (GMimeMultipartRelated *related, const char *uri, GError **error);
gboolean g_mime_multipart_related_contains_uri (GMimeMultipartRelated *related, const char *uri, GError **error);
GMimeStream *g_mime_multipart_related_open (GMimeMultipartRelated *related, const char *uri,
					  char **mime_type, char **charset, GError **error);

G_END_DECLS

#endif /* __GMIME_MULTIPART_RELATED_H__ */
