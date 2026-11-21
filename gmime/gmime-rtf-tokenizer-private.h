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

#ifndef __GMIME_RTF_TOKENIZER_PRIVATE_H__
#define __GMIME_RTF_TOKENIZER_PRIVATE_H__

#include "gmime-rtf-compliance-private.h"
#include "gmime-stream.h"

typedef struct _GMimeRtfTokenizer GMimeRtfTokenizer;

typedef enum {
	GMIME_RTF_GROUP_START,
	GMIME_RTF_GROUP_END,
	GMIME_RTF_CONTROL_WORD,
	GMIME_RTF_CONTROL_SYMBOL,
	GMIME_RTF_HEX_CHAR,
	GMIME_RTF_TEXT,
	GMIME_RTF_EOF
} GMimeRtfTokenKind;

typedef struct {
	GMimeRtfTokenKind kind;
	guint64 offset, line, column, depth;
	const guint8 *text;
	gsize length;
	char keyword[33];
	gint32 parameter;
	gunichar literal;
	guint8 value;
	gboolean keyword_truncated, has_parameter, parameter_invalid, truncated;
} GMimeRtfToken;

G_GNUC_INTERNAL GMimeRtfTokenizer *_g_mime_rtf_tokenizer_new (GMimeStream *stream);
G_GNUC_INTERNAL void _g_mime_rtf_tokenizer_free (GMimeRtfTokenizer *tokenizer);
G_GNUC_INTERNAL void _g_mime_rtf_tokenizer_set_diagnostics (GMimeRtfTokenizer *tokenizer, GMimeRtfDiagnostics *diag);
G_GNUC_INTERNAL gboolean _g_mime_rtf_tokenizer_next (GMimeRtfTokenizer *tokenizer, GMimeRtfToken *token, GError **error);

#endif
