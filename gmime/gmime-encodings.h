/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifndef __GMIME_ENCODINGS_H__
#define __GMIME_ENCODINGS_H__

#include <glib.h>
#include <sys/types.h>

G_BEGIN_DECLS


/**
 * GMimeContentEncoding:
 * @GMIME_CONTENT_ENCODING_UNSPECIFIED: Unspecified transfer encoding.
 * @GMIME_CONTENT_ENCODING_7BIT: 7bit text transfer encoding.
 * @GMIME_CONTENT_ENCODING_8BIT: 8bit text transfer encoding.
 * @GMIME_CONTENT_ENCODING_BINARY: Binary transfer encoding.
 * @GMIME_CONTENT_ENCODING_BASE64: Base64 transfer encoding.
 * @GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE: Quoted-printable transfer encoding.
 * @GMIME_CONTENT_ENCODING_UUENCODE: Uuencode transfer encoding.
 *
 * A Content-Transfer-Encoding enumeration.
 **/
typedef enum {
	GMIME_CONTENT_ENCODING_UNSPECIFIED,
	GMIME_CONTENT_ENCODING_7BIT,
	GMIME_CONTENT_ENCODING_8BIT,
	GMIME_CONTENT_ENCODING_BINARY,
	GMIME_CONTENT_ENCODING_BASE64,
	GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE,
	GMIME_CONTENT_ENCODING_UUENCODE
} GMimeContentEncoding;


/**
 * GMimeEncodingConstraint:
 * @GMIME_ENCODING_CONSTRAINT_7BIT: The stream data must fit within the 7bit ASCII range.
 * @GMIME_ENCODING_CONSTRAINT_8BIT: The stream data may have bytes with the high bit set, but no null bytes.
 * @GMIME_ENCODING_CONSTRAINT_BINARY: The stream may contain any binary data.
 *
 * Used with functions like g_mime_filter_best_encoding() and
 * g_mime_entity_prepare() as the 'constraint' argument. These values
 * provide a means of letting the filter know what the encoding
 * constraints are for the stream.
 **/
typedef enum {
	GMIME_ENCODING_CONSTRAINT_7BIT,
	GMIME_ENCODING_CONSTRAINT_8BIT,
	GMIME_ENCODING_CONSTRAINT_BINARY
} GMimeEncodingConstraint;


GMimeContentEncoding g_mime_content_encoding_from_string (const char *str);
const char *g_mime_content_encoding_to_string (GMimeContentEncoding encoding);


G_END_DECLS

#endif /* __GMIME_ENCODINGS_H__ */
