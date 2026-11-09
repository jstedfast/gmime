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

#ifndef __GMIME_TEXT_PRIVATE_H__
#define __GMIME_TEXT_PRIVATE_H__

#include "gmime-stream.h"
#include "gmime-text-converter.h"
#include "gmime-html-tag-context.h"

typedef struct {
	GMimeStream *stream;
	char buffer[4096];
	gsize position, end, length;
	gboolean eof, skip_lf, allow_nul;
} GMimeTextReader;

G_GNUC_INTERNAL gboolean _g_mime_text_reader_read (GMimeTextReader *reader, GError **error);
G_GNUC_INTERNAL gboolean _g_mime_text_reader_read_line (GMimeTextReader *reader, GString *line, gboolean *eof,
							GError **error);
G_GNUC_INTERNAL gboolean _g_mime_text_write (GMimeStream *stream, const char *text, gsize length, GError **error);
G_GNUC_INTERNAL char *_g_mime_text_read_all (GMimeStream *stream, gsize max_chars, GError **error);
G_GNUC_INTERNAL gboolean _g_mime_text_converter_is_active (GMimeTextConverter *converter);

G_GNUC_INTERNAL GMimeStream *_g_mime_text_stream_new_decoder (GMimeStream *source, const char *charset,
							      gboolean detect_bom, gsize buffer_size, GError **error);
G_GNUC_INTERNAL GMimeStream *_g_mime_text_stream_new_encoder (GMimeStream *destination, const char *charset,
							      gboolean write_bom, gsize buffer_size, GError **error);
G_GNUC_INTERNAL gboolean _g_mime_text_stream_propagate_error (GMimeStream *stream, GError **error);

/* Internal shared implementation for plain/flowed output. LF is used for
 * generated newlines on every platform. */
G_GNUC_INTERNAL gboolean _g_mime_html_output_convert (GMimeTextConverter *converter, GMimeStream *input,
						      GMimeStream *output, gboolean flowed, gboolean delete_space,
						      gboolean fragment, GMimeHeaderFooterFormat header_format,
						      GMimeHeaderFooterFormat footer_format,
						      GMimeHtmlTagCallback callback, gpointer user_data,
						      GError **error);

G_GNUC_INTERNAL gboolean _g_mime_html_output_header_footer (GMimeHtmlWriter *writer, const char *text,
							    GMimeHeaderFooterFormat format, GError **error);

#endif
