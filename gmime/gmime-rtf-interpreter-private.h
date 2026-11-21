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

#ifndef __GMIME_RTF_INTERPRETER_PRIVATE_H__
#define __GMIME_RTF_INTERPRETER_PRIVATE_H__

#include "gmime-stream.h"
#include "gmime-rtf-compliance-private.h"

G_BEGIN_DECLS

typedef enum {
	GMIME_RTF_EVENT_BEGIN,
	GMIME_RTF_EVENT_TEXT,
	GMIME_RTF_EVENT_PARAGRAPH,
	GMIME_RTF_EVENT_LINE_BREAK,
	GMIME_RTF_EVENT_CELL,
	GMIME_RTF_EVENT_ROW,
	GMIME_RTF_EVENT_HTML_TAG,
	GMIME_RTF_EVENT_OBJECT_PLACEHOLDER
} GMimeRtfEvent;

typedef enum {
	GMIME_RTF_ALIGN_LEFT,
	GMIME_RTF_ALIGN_CENTER,
	GMIME_RTF_ALIGN_RIGHT,
	GMIME_RTF_ALIGN_JUSTIFY
} GMimeRtfAlignment;

typedef enum {
	GMIME_RTF_BASELINE,
	GMIME_RTF_SUPERSCRIPT,
	GMIME_RTF_SUBSCRIPT
} GMimeRtfVerticalAlignment;

/* Borrowed for the duration of the callback. Colors are RGB or -1 for auto/undefined. */
typedef struct {
	gboolean from_html, bold, italic, underline, strike, in_table;
	GMimeRtfAlignment alignment;
	GMimeRtfVerticalAlignment vertical_alignment;
	int font, font_size, foreground, background; /* size in half-points */
	const char *hyperlink;
} GMimeRtfFormat;

/* UTF-8 data (not necessarily NUL-terminated) for TEXT and HTML_TAG only.
 * Return FALSE and set error to abort conversion without reading more input. */
typedef gboolean (*GMimeRtfEventFunc) (GMimeRtfEvent event, const GMimeRtfFormat *format,
				       const char *text, gsize length, gpointer user_data, GError **error);

typedef struct _GMimeRtfInterpreter GMimeRtfInterpreter;

/* input is the converter's UTF-8 stream (Latin-1 codepoints represent RTF
 * bytes). diagnostics and user_data are borrowed through free(). The first
 * event, BEGIN, has the final from_html value even for an empty document. */
G_GNUC_INTERNAL GMimeRtfInterpreter *
_g_mime_rtf_interpreter_new (GMimeStream *input, gboolean extract_html,
			     guint max_group_depth, guint max_fonts, guint max_colors,
			     GMimeRtfDiagnostics *diagnostics, GMimeRtfEventFunc callback,
			     gpointer user_data);

/* Consume one token; TRUE on success (including EOF), FALSE on I/O or handler error. */
G_GNUC_INTERNAL gboolean _g_mime_rtf_interpreter_step (GMimeRtfInterpreter *interpreter, gboolean *eof, GError **error);
G_GNUC_INTERNAL void _g_mime_rtf_interpreter_free (GMimeRtfInterpreter *interpreter);

G_GNUC_INTERNAL gboolean _g_mime_rtf_interpret (GMimeStream *input, gboolean extract_html,
				guint max_group_depth, guint max_fonts, guint max_colors,
				GMimeRtfDiagnostics *diagnostics, GMimeRtfEventFunc callback,
				gpointer user_data, GError **error);

/* TNEF preflight uses the same decoded UTF-8 input as the interpreter. */
struct _GMimeRtfToHtml;
G_GNUC_INTERNAL gboolean _g_mime_rtf_to_html_count_object_placeholders (struct _GMimeRtfToHtml *converter,
									GMimeStream *input, int *count, GError **error);
G_GNUC_INTERNAL gboolean _g_mime_rtf_to_html_extracts_encapsulated_html (struct _GMimeRtfToHtml *converter,
									 GMimeStream *input, gboolean *extracts, GError **error);

G_END_DECLS

#endif /* __GMIME_RTF_INTERPRETER_PRIVATE_H__ */
