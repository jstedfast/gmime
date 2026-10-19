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
#define GMIME_MULTIPART_ALTERNATIVE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MULTIPART_ALTERNATIVE, GMimeMultipartAlternative))
#define GMIME_MULTIPART_ALTERNATIVE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART_ALTERNATIVE, GMimeMultipartAlternativeClass))
#define GMIME_IS_MULTIPART_ALTERNATIVE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MULTIPART_ALTERNATIVE))
#define GMIME_IS_MULTIPART_ALTERNATIVE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART_ALTERNATIVE))
#define GMIME_MULTIPART_ALTERNATIVE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MULTIPART_ALTERNATIVE, GMimeMultipartAlternativeClass))

typedef struct _GMimeMultipartAlternative GMimeMultipartAlternative;
typedef struct _GMimeMultipartAlternativeClass GMimeMultipartAlternativeClass;

/**
 * GMimeMultipartAlternative:
 * @parent_object: parent #GMimeMultipart
 *
 * A multipart/alternative whose text-body selection searches children
 * from the most faithful alternative to the least faithful.
 **/
struct _GMimeMultipartAlternative {
	GMimeMultipart parent_object;
};

struct _GMimeMultipartAlternativeClass {
	GMimeMultipartClass parent_class;
};

GType g_mime_multipart_alternative_get_type (void);

GMimeMultipartAlternative *g_mime_multipart_alternative_new (void);

G_END_DECLS

#endif /* __GMIME_MULTIPART_ALTERNATIVE_H__ */
