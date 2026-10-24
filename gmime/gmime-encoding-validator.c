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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include "gmime-encoding-validator-private.h"
#include "gmime-codec-private.h"

G_DEFINE_INTERFACE (GMimeEncodingValidator, _g_mime_encoding_validator, G_TYPE_OBJECT)

static void
_g_mime_encoding_validator_default_init (GMimeEncodingValidatorInterface *iface)
{
	(void) iface;
}

GMimeContentEncoding
_g_mime_encoding_validator_get_encoding (GMimeEncodingValidator *validator)
{
	g_return_val_if_fail (GMIME_IS_ENCODING_VALIDATOR (validator), GMIME_CONTENT_ENCODING_UNSPECIFIED);

	return GMIME_ENCODING_VALIDATOR_GET_IFACE (validator)->get_encoding (validator);
}

void
_g_mime_encoding_validator_write (GMimeEncodingValidator *validator, const guint8 *input, gsize length)
{
	g_return_if_fail (GMIME_IS_ENCODING_VALIDATOR (validator));
	g_return_if_fail (input != NULL || length == 0);

	GMIME_ENCODING_VALIDATOR_GET_IFACE (validator)->write (validator, input, length);
}

void
_g_mime_encoding_validator_flush (GMimeEncodingValidator *validator)
{
	g_return_if_fail (GMIME_IS_ENCODING_VALIDATOR (validator));

	GMIME_ENCODING_VALIDATOR_GET_IFACE (validator)->flush (validator);
}

typedef struct {
	GMimeEncodingIssueFunc report;
	gpointer user_data;
	gint64 stream_offset;
	gint64 line_begin_offset;
	guint64 line_number;
} ValidatorContext;

static void
validator_context_init (ValidatorContext *context, GMimeEncodingIssueFunc report,
			gpointer user_data, gint64 stream_offset, guint64 line_number)
{
	context->report = report;
	context->user_data = user_data;
	context->stream_offset = context->line_begin_offset = stream_offset;
	context->line_number = line_number;
}

static void
validator_report (ValidatorContext *context, GMimeEncodingViolation violation, gint64 offset)
{
	GMimeEncodingIssue issue;

	issue.violation = violation;
	issue.stream_offset = offset;
	issue.line_number = context->line_number;
	/* Diagnostics may refer to an earlier octet, not the current input cursor. */
	issue.column_number = (guint64) (offset - context->line_begin_offset) + 1;
	context->report (&issue, context->user_data);
}

typedef struct {
	GObject parent_instance;
	ValidatorContext context;
	guint32 total;
	guint padding;
	gboolean invalid;
	/* Latch each violation once per line to prevent hostile input from flooding
	 * the report. These must survive write calls that split a malformed line. */
	gboolean reported_invalid_character;
	gboolean reported_comment;
} GMimeBase64Validator;

typedef GObjectClass GMimeBase64ValidatorClass;

G_GNUC_INTERNAL GType _g_mime_base64_validator_get_type (void);
static void base64_validator_interface_init (GMimeEncodingValidatorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GMimeBase64Validator, _g_mime_base64_validator, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODING_VALIDATOR, base64_validator_interface_init))

static GMimeContentEncoding
base64_get_encoding (GMimeEncodingValidator *validator)
{
	(void) validator;
	return GMIME_CONTENT_ENCODING_BASE64;
}

static void
base64_write (GMimeEncodingValidator *validator, const guint8 *input, gsize length)
{
	GMimeBase64Validator *base64 = (GMimeBase64Validator *) validator;
	ValidatorContext *context = &base64->context;
	gsize i = 0;
	guint8 c;

	g_return_if_fail (length <= (guint64) (G_MAXINT64 - context->stream_offset));

	if (base64->invalid)
		return;

	if (base64->padding == 0) {
		while (i < length) {
			/* Most input is alphabet or whitespace. The bulk scan stops at bytes
			 * such as padding or comment markers that need scalar handling. */
			if (length - i >= 16) {
				GMimeCodecScan scan;
				gsize consumed = _g_mime_codec_scan_base64 (input + i, length - i, &scan);

				base64->total += (guint32) scan.alphabet_count;
				if (scan.line_feed_count) {
					context->line_number += scan.line_feed_count;
					context->line_begin_offset = context->stream_offset + i + scan.line_begin_offset;
					base64->reported_invalid_character = FALSE;
					base64->reported_comment = FALSE;
				}
				i += consumed;
				if (i == length)
					break;
			}
			c = input[i++];
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			    (c >= '0' && c <= '9') || c == '+' || c == '/') {
				base64->total++;
			} else if (c == '\n') {
				context->line_begin_offset = context->stream_offset + i;
				context->line_number++;
				base64->reported_invalid_character = FALSE;
				base64->reported_comment = FALSE;
			} else if (c == '\r' || c == ' ' || c == '\t') {
				continue;
			} else if (c == '=') {
				/* Padding is only valid in the final two positions of a quantum. */
				if (base64->total % 4 < 2) {
					validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_BASE64_PADDING,
							  context->stream_offset + i - 1);
					base64->invalid = TRUE;
				} else {
					base64->padding = 1;
					base64->total++;
				}
				break;
			} else if (c == '*') {
				/* RFC 1113 allowed comments in the predecessor of Base64.
				 * RFC 1421 obsoleted them and MIME disallows them, but mailers
				 * may still emit them, so diagnose this separately from garbage. */
				if (!base64->reported_comment) {
					base64->reported_comment = TRUE;
					validator_report (context, GMIME_ENCODING_VIOLATION_OBSOLETE_BASE64_COMMENT,
							  context->stream_offset + i - 1);
				}
			} else if (!base64->reported_invalid_character) {
				base64->reported_invalid_character = TRUE;
				validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_BASE64_CHARACTER,
						  context->stream_offset + i - 1);
			}
		}
	}

	while (!base64->invalid && i < length) {
		c = input[i++];
		if (c == '\n') {
			context->line_begin_offset = context->stream_offset + i;
			context->line_number++;
		} else if (c == '=') {
			base64->total++;
			if (++base64->padding > 2) {
				validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_BASE64_PADDING,
						  context->stream_offset + i - 1);
				base64->invalid = TRUE;
			}
		} else if (c != '\r' && c != ' ' && c != '\t') {
			validator_report (context, GMIME_ENCODING_VIOLATION_BASE64_CHARACTERS_AFTER_PADDING,
					  context->stream_offset + i - 1);
			base64->invalid = TRUE;
		}
	}
	context->stream_offset += length;
}

static void
base64_flush (GMimeEncodingValidator *validator)
{
	GMimeBase64Validator *base64 = (GMimeBase64Validator *) validator;

	if (!base64->invalid && base64->total % 4 != 0)
		validator_report (&base64->context, GMIME_ENCODING_VIOLATION_INCOMPLETE_BASE64_QUANTUM,
				  base64->context.stream_offset);
}

static void
base64_validator_interface_init (GMimeEncodingValidatorInterface *iface)
{
	iface->get_encoding = base64_get_encoding;
	iface->write = base64_write;
	iface->flush = base64_flush;
}

static void
_g_mime_base64_validator_class_init (GMimeBase64ValidatorClass *klass)
{
	(void) klass;
}

static void
_g_mime_base64_validator_init (GMimeBase64Validator *base64)
{
	(void) base64;
}

GMimeEncodingValidator *
_g_mime_base64_validator_new (GMimeEncodingIssueFunc report, gpointer user_data,
			    gint64 stream_offset, guint64 line_number)
{
	GMimeBase64Validator *base64;

	g_return_val_if_fail (report != NULL, NULL);
	g_return_val_if_fail (stream_offset >= 0, NULL);
	g_return_val_if_fail (line_number > 0, NULL);

	base64 = g_object_new (_g_mime_base64_validator_get_type (), NULL);
	validator_context_init (&base64->context, report, user_data, stream_offset, line_number);

	return GMIME_ENCODING_VALIDATOR (base64);
}

typedef enum {
	QP_PASS_THROUGH,
	QP_EQUAL_SIGN,
	QP_SOFT_BREAK,
	QP_DECODE_BYTE
} QpValidatorState;

typedef struct {
	GObject parent_instance;
	ValidatorContext context;
	QpValidatorState state;
	/* Bound each kind of diagnostic to once per line, including across writes. */
	gboolean reported_invalid_encoding;
	gboolean reported_invalid_soft_break;
} GMimeQuotedPrintableValidator;

typedef GObjectClass GMimeQuotedPrintableValidatorClass;

G_GNUC_INTERNAL GType _g_mime_quoted_printable_validator_get_type (void);
static void qp_validator_interface_init (GMimeEncodingValidatorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GMimeQuotedPrintableValidator, _g_mime_quoted_printable_validator, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODING_VALIDATOR, qp_validator_interface_init))

static GMimeContentEncoding
qp_get_encoding (GMimeEncodingValidator *validator)
{
	(void) validator;
	return GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE;
}

static void
qp_newline (GMimeQuotedPrintableValidator *qp, gsize offset)
{
	qp->context.line_begin_offset = qp->context.stream_offset + offset;
	qp->context.line_number++;
	qp->reported_invalid_encoding = FALSE;
	qp->reported_invalid_soft_break = FALSE;
}

static void
qp_write (GMimeEncodingValidator *validator, const guint8 *input, gsize length)
{
	GMimeQuotedPrintableValidator *qp = (GMimeQuotedPrintableValidator *) validator;
	ValidatorContext *context = &qp->context;
	gsize i = 0;
	guint8 c;

	g_return_if_fail (length <= (guint64) (G_MAXINT64 - context->stream_offset));

	while (i < length) {
		c = input[i];
		switch (qp->state) {
		case QP_PASS_THROUGH: {
			/* Heavily encoded text has short literal runs; scan a small prefix
			 * before paying for the bulk search. */
			gsize scan_end = i + MIN (length - i, (gsize) 8);

			while (i < scan_end) {
				c = input[i++];
				if (c == '=') {
					qp->state = QP_EQUAL_SIGN;
					break;
				}
				if (c == '\n')
					qp_newline (qp, i);
			}
			if (qp->state != QP_PASS_THROUGH)
				break;

			/* Literal content needs no inspection until the next '=' or LF. */
			while (i < length) {
				i += _g_mime_codec_index_of_qp_special (input + i, length - i);
				if (i == length)
					break;
				if (input[i++] == '=') {
					qp->state = QP_EQUAL_SIGN;
					break;
				}
				qp_newline (qp, i);
			}
			break;
		}
		case QP_EQUAL_SIGN:
			if (g_ascii_isxdigit (c)) {
				qp->state = QP_DECODE_BYTE;
			} else if (c == '\r') {
				qp->state = QP_SOFT_BREAK;
			} else {
				if (c == '\n') {
					qp_newline (qp, i + 1);
				} else if (!qp->reported_invalid_encoding) {
					qp->reported_invalid_encoding = TRUE;
					validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING,
							  context->stream_offset + i);
				}
				qp->state = QP_PASS_THROUGH;
			}
			i++;
			break;
		case QP_SOFT_BREAK:
			if (c == '\n') {
				qp_newline (qp, ++i);
			} else if (!qp->reported_invalid_soft_break) {
				qp->reported_invalid_soft_break = TRUE;
				validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK,
						  context->stream_offset + i);
			}
			/* A malformed soft break leaves this octet to be processed again. */
			qp->state = QP_PASS_THROUGH;
			break;
		case QP_DECODE_BYTE:
			if (!g_ascii_isxdigit (c)) {
				if (!qp->reported_invalid_encoding) {
					qp->reported_invalid_encoding = TRUE;
					validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING,
							  context->stream_offset + i);
				}
				if (c == '\n')
					qp_newline (qp, i + 1);
			}
			qp->state = QP_PASS_THROUGH;
			i++;
			break;
		}
	}
	context->stream_offset += length;
}

static void
qp_flush (GMimeEncodingValidator *validator)
{
	GMimeQuotedPrintableValidator *qp = (GMimeQuotedPrintableValidator *) validator;

	/* Only pass-through is a valid final state; pending escapes or soft breaks
	 * are incomplete even if no further input arrives. */
	if (qp->state == QP_EQUAL_SIGN || qp->state == QP_DECODE_BYTE)
		validator_report (&qp->context, GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_ENCODING,
				  qp->context.stream_offset);
	else if (qp->state == QP_SOFT_BREAK)
		validator_report (&qp->context, GMIME_ENCODING_VIOLATION_INVALID_QUOTED_PRINTABLE_SOFT_BREAK,
				  qp->context.stream_offset);
}

static void
qp_validator_interface_init (GMimeEncodingValidatorInterface *iface)
{
	iface->get_encoding = qp_get_encoding;
	iface->write = qp_write;
	iface->flush = qp_flush;
}

static void
_g_mime_quoted_printable_validator_class_init (GMimeQuotedPrintableValidatorClass *klass)
{
	(void) klass;
}

static void
_g_mime_quoted_printable_validator_init (GMimeQuotedPrintableValidator *qp)
{
	(void) qp;
}

GMimeEncodingValidator *
_g_mime_quoted_printable_validator_new (GMimeEncodingIssueFunc report, gpointer user_data,
				      gint64 stream_offset, guint64 line_number)
{
	GMimeQuotedPrintableValidator *qp;

	g_return_val_if_fail (report != NULL, NULL);
	g_return_val_if_fail (stream_offset >= 0, NULL);
	g_return_val_if_fail (line_number > 0, NULL);

	qp = g_object_new (_g_mime_quoted_printable_validator_get_type (), NULL);
	validator_context_init (&qp->context, report, user_data, stream_offset, line_number);

	return GMIME_ENCODING_VALIDATOR (qp);
}

typedef enum {
	UU_EXPECT_BEGIN,
	UU_B,
	UU_BE,
	UU_BEG,
	UU_BEGI,
	UU_BEGIN,
	UU_FILE_MODE,
	UU_FILE_NAME,
	UU_PAYLOAD,
	UU_ENDED,
	UU_ENDED_NEWLINE,
	UU_E,
	UU_EN,
	UU_END,
	UU_INVALID
} UUValidatorState;

typedef struct {
	GObject parent_instance;
	ValidatorContext context;
	UUValidatorState state;
	gboolean invalid_pretext;
	gboolean invalid_file_mode;
	/* Malformed payload and extra-data reports are latched per encoded line,
	 * rather than emitted for every attacker-controlled byte. */
	gboolean reported_invalid_content;
	gboolean reported_extra_data;
	gboolean eoln;
	guint64 nsaved;
	guint8 uulen;
} GMimeUUValidator;

typedef GObjectClass GMimeUUValidatorClass;

G_GNUC_INTERNAL GType _g_mime_uu_validator_get_type (void);
static void uu_validator_interface_init (GMimeEncodingValidatorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GMimeUUValidator, _g_mime_uu_validator, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (GMIME_TYPE_ENCODING_VALIDATOR, uu_validator_interface_init))

static GMimeContentEncoding
uu_get_encoding (GMimeEncodingValidator *validator)
{
	(void) validator;
	return GMIME_CONTENT_ENCODING_UUENCODE;
}

static gboolean
uu_is_whitespace (guint8 c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static guint8
uu_read_byte (GMimeUUValidator *uu, const guint8 *input, gsize *position)
{
	guint8 c = input[(*position)++];

	uu->context.stream_offset++;
	if (c == '\n') {
		uu->context.line_begin_offset = uu->context.stream_offset;
		uu->context.line_number++;
	}
	return c;
}

static void
uu_skip_to_line_feed (GMimeUUValidator *uu, const guint8 *input, gsize length, gsize *position)
{
	const guint8 *newline = memchr (input + *position, '\n', length - *position);
	gsize count = newline ? (gsize) (newline - (input + *position)) : length - *position;

	uu->context.stream_offset += count;
	*position += count;
}

static void
uu_report_pretext (GMimeUUValidator *uu)
{
	if (!uu->invalid_pretext) {
		validator_report (&uu->context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_PRETEXT,
				  uu->context.stream_offset - 1);
		uu->invalid_pretext = TRUE;
	}
}

static void
uu_report_file_mode (GMimeUUValidator *uu, gint64 offset)
{
	if (!uu->invalid_file_mode) {
		validator_report (&uu->context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_FILE_MODE, offset);
		uu->invalid_file_mode = TRUE;
	}
}

static gboolean
uu_scan_begin (GMimeUUValidator *uu, const guint8 *input, gsize length, gsize *position)
{
	while (*position < length) {
		if (uu->state == UU_EXPECT_BEGIN) {
			if (uu->nsaved != '\n') {
				uu_skip_to_line_feed (uu, input, length, position);
				if (*position == length) {
					uu->nsaved = input[*position - 1];
					break;
				}
				uu->nsaved = uu_read_byte (uu, input, position);
				if (*position == length)
					break;
			}
			uu->nsaved = uu_read_byte (uu, input, position);
			if (uu->nsaved != 'b') {
				if (!uu_is_whitespace (uu->nsaved))
					uu_report_pretext (uu);
				continue;
			}
			uu->state = UU_B;
		} else if (uu->state >= UU_B && uu->state <= UU_BEGIN) {
			uu->nsaved = uu_read_byte (uu, input, position);
			if (uu->nsaved != (guint8) "egin "[uu->state - UU_B]) {
				uu_report_pretext (uu);
				uu->state = UU_EXPECT_BEGIN;
			} else if (uu->state == UU_BEGIN) {
				uu->state = UU_FILE_MODE;
				uu->nsaved = 0;
			} else {
				uu->state++;
			}
		} else if (uu->state == UU_FILE_MODE) {
			/* Keep the full digit count so an overlong mode cannot wrap to a valid
			 * length. Always report the fifth digit, independent of write boundaries. */
			while (*position < length && input[*position] >= '0' && input[*position] <= '9') {
				uu->context.stream_offset++;
				uu->nsaved++;
				(*position)++;
			}
			if (uu->nsaved > 4)
				uu_report_file_mode (uu, uu->context.stream_offset - (gint64) uu->nsaved + 4);
			if (*position == length)
				break;
			if (uu->nsaved < 3)
				uu_report_file_mode (uu, uu->context.stream_offset);
			while (*position < length && input[*position] != ' ') {
				uu_report_file_mode (uu, uu->context.stream_offset);
				if (input[*position] == '\n')
					break;
				uu->context.stream_offset++;
				(*position)++;
			}
			if (*position == length)
				break;
			if (input[*position] != '\n') {
				uu->context.stream_offset++;
				(*position)++;
			}
			uu->state = UU_FILE_NAME;
			uu->nsaved = 0;
		} else if (uu->state == UU_FILE_NAME) {
			uu_skip_to_line_feed (uu, input, length, position);
			if (*position == length)
				break;
			uu_read_byte (uu, input, position);
			uu->state = UU_PAYLOAD;
			uu->eoln = TRUE;
			uu->nsaved = 0;
			return TRUE;
		}
	}
	return FALSE;
}

static void
uu_write (GMimeEncodingValidator *validator, const guint8 *input, gsize length)
{
	GMimeUUValidator *uu = (GMimeUUValidator *) validator;
	ValidatorContext *context = &uu->context;
	gsize position = 0;
	guint8 c;

	g_return_if_fail (length <= (guint64) (G_MAXINT64 - context->stream_offset));

	if (uu->state == UU_INVALID)
		return;
	if (uu->state < UU_PAYLOAD && !uu_scan_begin (uu, input, length, &position))
		return;

	while (uu->state == UU_PAYLOAD && position < length) {
		c = input[position];
		if (c == '\r') {
			uu_read_byte (uu, input, &position);
		} else if (c == '\n') {
			if (uu->uulen > 0)
				validator_report (context, GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_LINE,
						  context->stream_offset);
			uu_read_byte (uu, input, &position);
			uu->eoln = TRUE;
		} else if (uu->eoln) {
			uu->eoln = FALSE;
			uu->reported_invalid_content = FALSE;
			uu->reported_extra_data = FALSE;
			if (c == '`') {
				uu->state = UU_ENDED;
			} else {
				uu->uulen = (c - 32) & 63;
				if (uu->uulen > 45)
					validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_LENGTH,
							  context->stream_offset);
			}
			uu_read_byte (uu, input, &position);
		} else {
			uu_read_byte (uu, input, &position);
			/* Octets beyond the declared length are extra data, not payload.
			 * Do not report two violations on the same trailing garbage byte. */
			if (uu->uulen > 0) {
				if ((c < 33 || c > 96) && !uu->reported_invalid_content) {
					uu->reported_invalid_content = TRUE;
					validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_CONTENT,
							  context->stream_offset - 1);
				}
				if (++uu->nsaved == 4) {
					uu->uulen -= MIN (uu->uulen, 3);
					uu->nsaved = 0;
				}
			} else if (!uu->reported_extra_data) {
				uu->reported_extra_data = TRUE;
				validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODED_LINE_EXTRA_DATA,
						  context->stream_offset - 1);
			}
		}
	}

	while (uu->state == UU_ENDED && position < length) {
		c = input[position];
		if (!uu_is_whitespace (c))
			validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER,
					  context->stream_offset);
		uu_read_byte (uu, input, &position);
		if (c == '\n')
			uu->state = UU_ENDED_NEWLINE;
	}
	while (uu->state >= UU_ENDED_NEWLINE && uu->state < UU_END && position < length) {
		if (input[position] != (guint8) "end"[uu->state - UU_ENDED_NEWLINE]) {
			validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER,
					  context->stream_offset);
			uu->state = UU_INVALID;
			return;
		}
		uu->state++;
		uu_read_byte (uu, input, &position);
	}
	while (uu->state == UU_END && position < length) {
		if (!uu_is_whitespace (input[position])) {
			validator_report (context, GMIME_ENCODING_VIOLATION_INVALID_UUENCODE_END_MARKER,
					  context->stream_offset);
			uu->state = UU_INVALID;
			return;
		}
		uu_read_byte (uu, input, &position);
	}
}

static void
uu_flush (GMimeEncodingValidator *validator)
{
	GMimeUUValidator *uu = (GMimeUUValidator *) validator;

	if (uu->state < UU_END)
		validator_report (&uu->context, GMIME_ENCODING_VIOLATION_INCOMPLETE_UUENCODED_CONTENT,
				  uu->context.stream_offset);
}

static void
uu_validator_interface_init (GMimeEncodingValidatorInterface *iface)
{
	iface->get_encoding = uu_get_encoding;
	iface->write = uu_write;
	iface->flush = uu_flush;
}

static void
_g_mime_uu_validator_class_init (GMimeUUValidatorClass *klass)
{
	(void) klass;
}

static void
_g_mime_uu_validator_init (GMimeUUValidator *uu)
{
	/* During begin-marker scanning, nsaved holds the previous character. */
	uu->nsaved = '\n';
}

GMimeEncodingValidator *
_g_mime_uu_validator_new (GMimeEncodingIssueFunc report, gpointer user_data,
			gint64 stream_offset, guint64 line_number)
{
	GMimeUUValidator *uu;

	g_return_val_if_fail (report != NULL, NULL);
	g_return_val_if_fail (stream_offset >= 0, NULL);
	g_return_val_if_fail (line_number > 0, NULL);

	uu = g_object_new (_g_mime_uu_validator_get_type (), NULL);
	validator_context_init (&uu->context, report, user_data, stream_offset, line_number);

	return GMIME_ENCODING_VALIDATOR (uu);
}
