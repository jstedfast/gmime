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

#ifndef __GMIME_ENCODING_VALIDATOR_PRIVATE_H__
#define __GMIME_ENCODING_VALIDATOR_PRIVATE_H__

#include <glib-object.h>
#include "gmime-encodings.h"

G_BEGIN_DECLS

typedef enum {
	GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER,
	GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT,
	GMIME_ENCODING_VIOLATION_INVALID_BASE64_PADDING,
	GMIME_ENCODING_VIOLATION_BASE64_CHARACTERS_AFTER_PADDING,
	GMIME_ENCODING_VIOLATION_INCOMPLETE_BASE64_QUANTUM,
	GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING,
	GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK,
	GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_PRETEXT,
	GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE,
	GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE,
	GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_LENGTH,
	GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_CONTENT,
	GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_EXTRA_DATA,
	GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER,
	GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT
} GMimeEncodingViolation;

typedef struct {
	GMimeEncodingViolation violation;
	gint64 stream_offset;
	guint64 line_number;
	guint64 column_number;
} GMimeEncodingIssue;

/* The issue is borrowed for the callback; user_data must outlive the validator. */
typedef void (*GMimeEncodingIssueFunc) (const GMimeEncodingIssue *issue, gpointer user_data);

#define GMIME_TYPE_ENCODING_VALIDATOR (_g_mime_encoding_validator_get_type ())
G_GNUC_INTERNAL GType _g_mime_encoding_validator_get_type (void);
G_DECLARE_INTERFACE (GMimeEncodingValidator, _g_mime_encoding_validator, GMIME, ENCODING_VALIDATOR, GObject)

struct _GMimeEncodingValidatorInterface {
	GTypeInterface parent_iface;
	GMimeContentEncoding (*get_encoding) (GMimeEncodingValidator *validator);
	void (*write) (GMimeEncodingValidator *validator, const guint8 *input, gsize length);
	void (*flush) (GMimeEncodingValidator *validator);
};

G_GNUC_INTERNAL GMimeContentEncoding _g_mime_encoding_validator_get_encoding (GMimeEncodingValidator *validator);
G_GNUC_INTERNAL void _g_mime_encoding_validator_write (GMimeEncodingValidator *validator,
						      const guint8 *input, gsize length);
/* Flush reports incomplete input without resetting or closing the validator. */
G_GNUC_INTERNAL void _g_mime_encoding_validator_flush (GMimeEncodingValidator *validator);

/* Nonnegative byte offsets; one-based line/column numbers start at this position. */
G_GNUC_INTERNAL GMimeEncodingValidator *_g_mime_base64_validator_new (GMimeEncodingIssueFunc report,
				gpointer user_data, gint64 stream_offset, guint line_number);
G_GNUC_INTERNAL GMimeEncodingValidator *_g_mime_quoted_printable_validator_new (GMimeEncodingIssueFunc report,
				gpointer user_data, gint64 stream_offset, guint line_number);
G_GNUC_INTERNAL GMimeEncodingValidator *_g_mime_uu_validator_new (GMimeEncodingIssueFunc report,
				gpointer user_data, gint64 stream_offset, guint line_number);

G_END_DECLS
#endif
