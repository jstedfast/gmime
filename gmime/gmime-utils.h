/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GMIME_UTILS_H__
#define __GMIME_UTILS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <time.h>

time_t g_mime_utils_header_decode_date (const gchar *in, gint *saveoffset);
gchar *g_mime_utils_header_format_date (time_t time, gint offset);

/* encoding decision making utilities ;-) */
gboolean g_mime_utils_text_is_8bit (const guchar *text);
gint g_mime_utils_best_encoding (const guchar *text);

/* utilities to (de/en)code headers */
gchar *g_mime_utils_8bit_header_decode (const guchar *in);
gchar *g_mime_utils_8bit_header_encode (const guchar *in);
gchar *g_mime_utils_8bit_header_encode_phrase (const guchar *in);

/* do incremental base64 (de/en)coding */
gint g_mime_utils_base64_decode_step (const guchar *in, gint len, guchar *out, gint *state, guint *save);
gint g_mime_utils_base64_encode_step (const guchar *in, gint len, guchar *out, gint *state, gint *save);
gint g_mime_utils_base64_encode_close (const guchar *in, gint inlen, guchar *out, gint *state, gint *save);

/* do incremental uudecoding */
gint g_mime_utils_uudecode_step (const guchar *in, gint len, guchar *out, gint *state, guint32 *save, gchar *uulen);

/* do incremental quoted-printable (de/en)coding */
gint g_mime_utils_quoted_decode_step (const guchar *in, gint len, guchar *out, gint *savestate, gint *saveme);
gint g_mime_utils_quoted_encode_step (const guchar *in, gint len, guchar *out, gint *state, gint *save);
gint g_mime_utils_quoted_encode_close (const guchar *in, gint len, guchar *out, gint *state, gint *save);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_UTILS_H__ */
