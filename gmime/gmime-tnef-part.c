/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#include <errno.h>
#include "gmime-error.h"
#include "gmime-internal.h"
#include "gmime-tnef-part.h"
#include "gmime-visitor.h"

/**
 * SECTION: gmime-tnef-part
 * @title: GMimeTnefPart
 * @short_description: A TNEF attachment
 *
 * Parsing a MIME message identifies TNEF parts without interpreting their
 * payload. Use g_mime_tnef_part_load_tnef_message() to decode one explicitly.
 **/
G_DEFINE_TYPE (GMimeTnefPart, g_mime_tnef_part, GMIME_TYPE_PART)

static void
tnef_part_accept (GMimeEntity *entity, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_tnef_part (visitor, (GMimeTnefPart *) entity);
}

static void
g_mime_tnef_part_class_init (GMimeTnefPartClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = tnef_part_accept;
}

static void
g_mime_tnef_part_init (GMimeTnefPart *part)
{
}

/**
 * g_mime_tnef_part_new:
 *
 * Returns: (transfer full): an application/ms-tnef attachment named winmail.dat.
 **/
GMimeTnefPart *
g_mime_tnef_part_new (void)
{
	GMimeTnefPart *part = g_object_new (GMIME_TYPE_TNEF_PART, NULL);
	GMimeContentType *content_type = g_mime_content_type_new ("application", "ms-tnef");

	g_mime_entity_set_content_type (GMIME_ENTITY (part), content_type);
	g_object_unref (content_type);
	g_mime_part_set_filename (GMIME_PART (part), "winmail.dat");

	return part;
}

/**
 * g_mime_tnef_part_load_tnef_message:
 * @part: a TNEF part
 * @options: (nullable): TNEF loading options
 * @error: return location for a #GError, or %NULL
 *
 * Loads the transfer-decoded payload independently of the part. Without
 * explicit options, a recognized MIME charset supplies the default code page.
 *
 * Returns: (transfer full) (nullable): the TNEF message, or %NULL on error.
 **/
GMimeTnefMessage *
g_mime_tnef_part_load_tnef_message (GMimeTnefPart *part, const GMimeTnefOptions *options, GError **error)
{
	GMimeTnefOptions *defaults = NULL;
	GMimeTnefMessage *message;
	GMimeContent *content;
	GMimeStream *stream;
	const char *charset;
	int codepage;

	g_return_val_if_fail (GMIME_IS_TNEF_PART (part), NULL);

	content = g_mime_part_get_content (GMIME_PART (part));

	if (!content || !g_mime_content_get_stream (content)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION, "TNEF part has no content");
		return NULL;
	}

	errno = 0;
	if (!(stream = g_mime_content_open (content))) {
		int errsv = errno ? errno : EIO;
		g_set_error (error, GMIME_ERROR, errsv, "Cannot open TNEF content: %s", g_strerror (errsv));
		return NULL;
	}

	if (!options && (charset = g_mime_entity_get_content_type_parameter (GMIME_ENTITY (part), "charset")) &&
	    *charset && (codepage = _g_mime_charset_get_codepage (charset)) > 0) {
		defaults = g_mime_tnef_options_new ();
		g_mime_tnef_options_set_default_codepage (defaults, codepage);
		options = defaults;
	}

	message = g_mime_tnef_message_load (stream, options, error);
	g_clear_pointer (&defaults, g_mime_tnef_options_unref);
	g_object_unref (stream);

	return message;
}
