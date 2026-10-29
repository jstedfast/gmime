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

#ifndef __GMIME_MULTIPART_ALTERNATIVE_H__
#define __GMIME_MULTIPART_ALTERNATIVE_H__

#include <gmime/gmime-multipart.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_ALTERNATIVE            (g_mime_multipart_alternative_get_type ())


/**
 * GMimeMultipartAlternative:
 *
 * A multipart/alternative whose text-body selection searches children
 * from the most faithful alternative to the least faithful.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMultipartAlternative, g_mime_multipart_alternative, GMIME, MULTIPART_ALTERNATIVE, GMimeMultipart)

struct _GMimeMultipartAlternativeClass {
	GMimeMultipartClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeMultipartAlternative *g_mime_multipart_alternative_new (void);

G_END_DECLS

#endif /* __GMIME_MULTIPART_ALTERNATIVE_H__ */
