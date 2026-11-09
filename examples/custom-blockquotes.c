/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <gmime/gmime.h>

static gboolean
write_custom_blockquote (GMimeHtmlTagContext *context, GMimeHtmlWriter *writer, gpointer user_data, GError **error)
{
	guint *depth = user_data;
	char class_name[32];

	if (g_mime_html_tag_context_get_tag_id (context) != GMIME_HTML_TAG_BLOCK_QUOTE)
		return g_mime_html_tag_context_write_tag (context, writer, TRUE, error);

	if (g_mime_html_tag_context_get_is_end_tag (context)) {
		if (!g_mime_html_tag_context_write_tag (context, writer, FALSE, error))
			return FALSE;

		(*depth)--;
		return TRUE;
	}

	/* Request closing callbacks so the depth follows the converter's nesting. */
	g_mime_html_tag_context_set_invoke_callback_for_end_tag (context, TRUE);
	g_snprintf (class_name, sizeof (class_name), "quote level_%03u", ++(*depth));

	/* The writer leaves the start tag open for additional attributes. */
	return g_mime_html_tag_context_write_tag (context, writer, TRUE, error) &&
	       g_mime_html_writer_write_attribute (writer, "class", class_name, error);
}

int
main (void)
{
	const char *text = "Introduction\n"
			   "> First quotation & more\n"
			   ">> Nested quotation\n"
			   "> Back to the first level\n"
			   "Outside the quotation\n"
			   ">> Nested again at the end";
	GMimeTextConverter *converter;
	GError *error = NULL;
	int status = EXIT_SUCCESS;
	guint depth = 0;
	char *html;

	g_mime_init ();

	converter = g_mime_text_to_html_new ();
	g_mime_text_to_html_set_output_html_fragment (GMIME_TEXT_TO_HTML (converter), TRUE);
	g_mime_text_to_html_set_html_tag_callback (GMIME_TEXT_TO_HTML (converter), write_custom_blockquote, &depth,
						   NULL);

	html = g_mime_text_converter_convert_to_string (converter, text, -1, &error);

	if (!html) {
		g_printerr ("Conversion failed: %s\n", error->message);
		g_clear_error (&error);
		status = EXIT_FAILURE;
	} else if (puts (html) == EOF || fflush (stdout) == EOF) {
		g_printerr ("Writing HTML failed: %s\n", g_strerror (errno));
		status = EXIT_FAILURE;
	}

	g_free (html);
	g_object_unref (converter);
	g_mime_shutdown ();

	return status;
}
