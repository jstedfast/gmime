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

G_BEGIN_DECLS

typedef struct {
	gsize length;
	gsize previous_line;
	guint64 lines;
	guint formats;
} GMimeReaderScan;

/* Skip complete non-boundary lines, always including the first complete line.
 * Leave both the next possible delimiter and an incomplete final line intact.
 * The input has no readable sentinel or alignment requirement. */
G_GNUC_INTERNAL GMimeReaderScan _g_mime_reader_scan_lines (const guint8 *input, gsize length,
							 guint8 first, guint8 second);
G_GNUC_INTERNAL GMimeReaderScan _g_mime_reader_scan_lines_scalar (const guint8 *input, gsize length,
								guint8 first, guint8 second);

G_END_DECLS
#endif
