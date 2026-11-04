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

#ifndef __GMIME_READER_SIMD_PRIVATE_H__
#define __GMIME_READER_SIMD_PRIVATE_H__

#include <glib.h>
#include "gmime-encodings.h"

G_BEGIN_DECLS

typedef struct {
	gsize length;
	gsize previous_line;
	guint64 lines;
	guint formats;
} GMimeReaderScan;

typedef enum {
	GMIME_READER_SCAN_SCALAR,
	GMIME_READER_SCAN_SSE2,
	GMIME_READER_SCAN_AVX2,
	GMIME_READER_SCAN_NEON
} GMimeReaderScanBackend;

typedef GMimeReaderScan (*GMimeReaderScanFunc) (const guint8 *input, gsize length,
					      guint8 first, guint8 second);

/* Skip complete non-boundary lines, always including the first complete line.
 * Leave both the next possible delimiter and an incomplete final line intact.
 * The input has no readable sentinel or alignment requirement. */
G_GNUC_INTERNAL GMimeReaderScan _g_mime_reader_scan_lines (const guint8 *input, gsize length,
							 guint8 first, guint8 second);
G_GNUC_INTERNAL GMimeReaderScan _g_mime_reader_scan_lines_scalar (const guint8 *input, gsize length,
								guint8 first, guint8 second);

/* NULL means that this backend is not compiled in or cannot run on this CPU/OS. */
G_GNUC_INTERNAL GMimeReaderScanFunc _g_mime_reader_scan_lines_get_backend (GMimeReaderScanBackend backend);

typedef struct {
	guint8 first, second;
	gboolean detect_null, detect_eight_bit, midline_cr;
	guint64 line_length;
	GMimeContentEncoding encoding;
	gint remaining;
} GMimeReaderContentScanOptions;

typedef struct {
	GMimeReaderScan scan;
	gsize examined;
	guint32 alphabet_count;
} GMimeReaderContentScan;

typedef GMimeReaderContentScan (*GMimeReaderContentScanFunc) (const guint8 *input, gsize length,
							    const GMimeReaderContentScanOptions *options);

/* Skip clean CRLF lines, stopping before possible delimiters, long lines,
 * special bytes or encoding errors. examined also includes a checked,
 * newline-free tail; only scan.length is eligible for validator consumption.
 * UNSPECIFIED checks structural bytes only. No readable sentinel is needed. */
G_GNUC_INTERNAL GMimeReaderContentScan _g_mime_reader_scan_content (const guint8 *input, gsize length,
	const GMimeReaderContentScanOptions *options);
G_GNUC_INTERNAL GMimeReaderContentScanFunc _g_mime_reader_scan_content_get_backend (GMimeReaderScanBackend backend);

typedef struct {
	gsize length;
	gsize null_index;
	gsize eight_bit_index;
} GMimeReaderLineScan;

typedef GMimeReaderLineScan (*GMimeReaderLineScanFunc) (const guint8 *input, gsize length,
						      gboolean detect_null, gboolean detect_eight_bit);

/* length stops before the first LF, or at the end of input. Special-byte
 * indices precede that LF and are G_MAXSIZE when absent or not requested. */
G_GNUC_INTERNAL GMimeReaderLineScan _g_mime_reader_scan_line (const guint8 *input, gsize length,
							    gboolean detect_null, gboolean detect_eight_bit);
G_GNUC_INTERNAL GMimeReaderLineScanFunc _g_mime_reader_scan_line_get_backend (GMimeReaderScanBackend backend);

typedef enum {
	GMIME_READER_UTF8_SCALAR,
	GMIME_READER_UTF8_SSE41,
	GMIME_READER_UTF8_AVX2,
	GMIME_READER_UTF8_NEON
} GMimeReaderUtf8Backend;

typedef gsize (*GMimeReaderUtf8Func) (const guint8 *input, gsize length);

/* Return the first malformed sequence's leading-byte index, or length for
 * valid UTF-8. Embedded NUL is valid; no sentinel or alignment is required. */
G_GNUC_INTERNAL gsize _g_mime_reader_utf8_invalid_index (const guint8 *input, gsize length);
G_GNUC_INTERNAL GMimeReaderUtf8Func _g_mime_reader_utf8_get_backend (GMimeReaderUtf8Backend backend);

G_END_DECLS
#endif
