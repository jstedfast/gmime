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

#ifndef __GMIME_CODEC_PRIVATE_H__
#define __GMIME_CODEC_PRIVATE_H__

#include <glib.h>
#include "gmime-decoder.h"

G_GNUC_INTERNAL gsize _g_mime_rfc2231_encoder_estimate_output_length (gsize length);
G_GNUC_INTERNAL gsize _g_mime_rfc2231_encode (const guint8 *input, gsize length,
					guint8 *output, gsize capacity);
G_GNUC_INTERNAL GType _g_mime_rfc2231_decoder_get_type (void);
G_GNUC_INTERNAL GMimeDecoder *_g_mime_rfc2231_decoder_new (void);

G_GNUC_INTERNAL extern const guint32 _g_mime_crc32_table[256];

static inline guint32
codec_crc32_update (guint32 crc, guint8 value)
{
	return _g_mime_crc32_table[(crc ^ value) & 0xff] ^ (crc >> 8);
}

static inline gboolean
codec_buffers_overlap (const guint8 *input, gsize length, guint8 *output, gsize capacity)
{
	guintptr a = (guintptr) input, b = (guintptr) output;

	if (!length || !capacity)
		return FALSE;

	return a <= b ? b - a < length : a - b < capacity;
}

static inline void
codec_encode_triplet (const guint8 *input, guint8 *output, gboolean uu)
{
	static const guint8 alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	guint8 ranks[4];
	guint i;

	ranks[0] = input[0] >> 2;
	ranks[1] = ((input[0] << 4) | (input[1] >> 4)) & 63;
	ranks[2] = ((input[1] << 2) | (input[2] >> 6)) & 63;
	ranks[3] = input[2] & 63;

	for (i = 0; i < 4; i++)
		output[i] = uu ? (ranks[i] ? ranks[i] + 32 : '`') : alphabet[ranks[i]];
}

G_GNUC_INTERNAL gsize _g_mime_codec_encode_blocks (const guint8 *input, gsize length,
						  guint8 *output, guint quartets, gboolean uu);
G_GNUC_INTERNAL gsize _g_mime_codec_decode_block (const guint8 *input, guint8 *output, gboolean uu);

typedef struct {
	gsize alphabet_count;
	gsize line_feed_count;
	gsize line_begin_offset;
} GMimeCodecScan;

/* Counts and the next-line offset are relative to the consumed prefix. */
G_GNUC_INTERNAL gsize _g_mime_codec_scan_base64 (const guint8 *input, gsize length, GMimeCodecScan *scan);
G_GNUC_INTERNAL gsize _g_mime_codec_index_of_qp_special (const guint8 *input, gsize length);

#endif
