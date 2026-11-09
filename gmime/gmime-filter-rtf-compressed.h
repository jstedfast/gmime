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

#ifndef __GMIME_FILTER_RTF_COMPRESSED_H__
#define __GMIME_FILTER_RTF_COMPRESSED_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

/**
 * GMimeRtfCompressionMode:
 * @GMIME_RTF_COMPRESSION_MODE_UNKNOWN: No compression header has been read.
 * @GMIME_RTF_COMPRESSION_MODE_COMPRESSED: LZFu compression.
 * @GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED: Uncompressed RTF with a MELA header.
 *
 * Compression modes defined by [MS-OXRTFCP].
 **/
typedef enum {
	GMIME_RTF_COMPRESSION_MODE_UNKNOWN = 0,
	GMIME_RTF_COMPRESSION_MODE_COMPRESSED = 0x75465a4c,
	GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED = 0x414c454d
} GMimeRtfCompressionMode;

/**
 * GMimeFilterRtfCompressed:
 *
 * Incrementally decompresses an RtfCompressed MAPI property. Unknown
 * compression modes are discarded. Bytes beyond the declared content
 * length or LZFu end marker are ignored. Check the CRC after completion;
 * a truncated or corrupt stream may have produced partial output.
 **/
#define GMIME_TYPE_FILTER_RTF_COMPRESSED (g_mime_filter_rtf_compressed_get_type ())
G_DECLARE_FINAL_TYPE (GMimeFilterRtfCompressed, g_mime_filter_rtf_compressed, GMIME, FILTER_RTF_COMPRESSED, GMimeFilter)

GMimeFilter *g_mime_filter_rtf_compressed_new (void);

GMimeRtfCompressionMode g_mime_filter_rtf_compressed_get_compression_mode (GMimeFilterRtfCompressed *filter);

gboolean g_mime_filter_rtf_compressed_is_valid_crc32 (GMimeFilterRtfCompressed *filter);

G_END_DECLS

#endif
