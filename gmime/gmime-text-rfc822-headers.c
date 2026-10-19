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
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-text-rfc822-headers.h"
#include "gmime-visitor.h"

/**
 * SECTION: gmime-text-rfc822-headers
 * @title: GMimeTextRfc822Headers
 * @short_description: Encapsulated message headers
 * @see_also: #GMimeMessagePart
 *
 * A text/rfc822-headers entity. Access the encapsulated #GMimeMessage
 * using the inherited #GMimeMessagePart accessors. Parsing and writing
 * preserve an optional embedded message body, rather than stripping it.
 **/

G_DEFINE_TYPE (GMimeTextRfc822Headers, g_mime_text_rfc822_headers, GMIME_TYPE_MESSAGE_PART)

static void
text_rfc822_headers_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_text_rfc822_headers (visitor, (GMimeTextRfc822Headers *) object);
}

static void
g_mime_text_rfc822_headers_class_init (GMimeTextRfc822HeadersClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = text_rfc822_headers_accept;
}

static void
g_mime_text_rfc822_headers_init (GMimeTextRfc822Headers *headers)
{
}

/**
 * g_mime_text_rfc822_headers_new:
 *
 * Creates an empty text/rfc822-headers entity.
 *
 * Returns: (transfer full): a new #GMimeTextRfc822Headers.
 **/
GMimeTextRfc822Headers *
g_mime_text_rfc822_headers_new (void)
{
	GMimeTextRfc822Headers *headers;
	GMimeContentType *content_type;

	headers = g_object_new (GMIME_TYPE_TEXT_RFC822_HEADERS, NULL);
	content_type = g_mime_content_type_new ("text", "rfc822-headers");
	g_mime_entity_set_content_type (GMIME_ENTITY (headers), content_type);
	g_object_unref (content_type);
	return headers;
}
