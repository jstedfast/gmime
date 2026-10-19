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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-multipart-alternative.h"
#include "gmime-visitor.h"
#include "gmime-internal.h"

/**
 * SECTION: gmime-multipart-alternative
 * @title: GMimeMultipartAlternative
 * @short_description: Alternative MIME representations
 * @see_also: #GMimeMultipart
 *
 * A multipart/alternative container with MimeKit-style text-body selection.
 **/

static gboolean multipart_alternative_try_get_value (GMimeMultipart *multipart, GMimeTextFormat format, GMimeTextPart **body);

G_DEFINE_TYPE (GMimeMultipartAlternative, g_mime_multipart_alternative, GMIME_TYPE_MULTIPART)

static void
multipart_alternative_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_multipart_alternative (visitor, (GMimeMultipartAlternative *) object);
}

static void
g_mime_multipart_alternative_class_init (GMimeMultipartAlternativeClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = multipart_alternative_accept;
	GMIME_MULTIPART_CLASS (klass)->try_get_value = multipart_alternative_try_get_value;
}

static void
g_mime_multipart_alternative_init (GMimeMultipartAlternative *multipart)
{
}

static gboolean
multipart_alternative_try_get_value (GMimeMultipart *multipart, GMimeTextFormat format, GMimeTextPart **body)
{
	GMimeEntity *part;
	int i;

	/* Alternatives are ordered from least to most faithful, so search backwards. */
	for (i = g_mime_multipart_get_count (multipart) - 1; i >= 0; i--) {
		part = g_mime_multipart_get_part (multipart, i);
		if (GMIME_IS_MULTIPART (part) &&
		    g_mime_multipart_try_get_value ((GMimeMultipart *) part, format, body))
			return TRUE;
		if (GMIME_IS_TEXT_PART (part) && _g_mime_text_part_is_format ((GMimeTextPart *) part, format)) {
			*body = (GMimeTextPart *) part;
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * g_mime_multipart_alternative_new:
 *
 * Creates an empty multipart/alternative container.
 *
 * Returns: (transfer full): a new #GMimeMultipartAlternative.
 **/
GMimeMultipartAlternative *
g_mime_multipart_alternative_new (void)
{
	GMimeMultipartAlternative *multipart;
	GMimeContentType *content_type;

	multipart = g_object_new (GMIME_TYPE_MULTIPART_ALTERNATIVE, NULL);
	content_type = g_mime_content_type_new ("multipart", "alternative");
	g_mime_entity_set_content_type ((GMimeEntity *) multipart, content_type);
	g_object_unref (content_type);

	g_mime_multipart_set_boundary ((GMimeMultipart *) multipart, NULL);

	return multipart;
}
