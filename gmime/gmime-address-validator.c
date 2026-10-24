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

#include <config.h>
#include <string.h>

#include "gmime-address-validator-private.h"

/*
 * A strict RFC 5322/RFC 6532 validator, deliberately independent of the tolerant
 * address parser. Diagnostics never affect the addresses recovered by a reader.
 * Ported from MimeKit's AddressValidator.cs, including its recovery indexes.
 * All byte positions use signed pointer-width indexes for the -1/-2 sentinels;
 * public source coordinates retain their full 64-bit width.
 */
typedef struct {
	const guint8 *text;
	GMimeComplianceIssueFunc report;
	gpointer user_data;
	GMimeComplianceContext context;
	gint64 stream_offset;
	guint64 line_number, column_number;
	gssize start_index, end_index, index;

	// Note: The position of the first ISO-2022 shift or escape sequence that has not yet turned out
	// to be inside a local-part, or -1 once none are left. Sequences are held back rather than
	// reported where they are found because how one should be described depends on where it turns
	// out to be, which is not known until the value has been parsed. Only the first survivor is
	// ever reported, and local-parts are visited in increasing order, so a cursor that moves
	// forward as each is claimed is all the bookkeeping that is needed.
	gssize iso2022_cursor;

	// Note: The position of the first null byte that has not yet turned out to be inside a
	// display-name, or -1 once none are left. Held back and advanced for the same reasons as the
	// ISO-2022 cursor above: a null in a display-name is described differently from one in an
	// addr-spec, and which it is cannot be known until the value has been parsed.
	gssize null_cursor;

	gboolean reported_control_character;
	gboolean reported_stray_carriage_return;
	gboolean reported_iso2022_local_part;
	gboolean reported_null_byte_in_display_name;
	gboolean reported_null_byte_in_address;

	// Note: Whether the address currently being scanned was cut short by a damaged local-part.
	// Set by LogDamagedLocalPart and cleared at the start of each address, it tells the list loops
	// that the character the scan stopped on has already been accounted for.
	gboolean damaged_local_part;

	// Note: Whether scanning is inside a group-list. A ';' ends a group, but rfc5322 address-list
	// is comma-separated only, so outside a group there is nothing for a ';' to delimit and it is
	// an unquoted special in whatever token it interrupted.
	gboolean in_group_list;

	// Note: The number of nested groups that are open within the current group. rfc5322 does not permit
	// a group inside a group, but tracking them (rather than recursing, which would let the input drive
	// the stack depth) means that the ';' that terminates a nested group is not mistaken for the end of
	// its parent, which keeps the validator in agreement with MimeKit's parser about where groups end.
	gssize nested_group_depth;

	// The furthest position indexed by Log. Backward diagnostics use a lazily populated index
	// of line starts rather than repeatedly scanning the header prefix.
	gssize scan_index;
	guint64 scan_line;
	gssize scan_line_begin;

	// The last reusable phrase extent, and indexes allocated only when malformed input would
	// otherwise make lookahead repeatedly scan overlapping regions.
	gboolean skipped_comment;
	gssize phrase_from;
	gssize phrase_end;
	gboolean phrase_simple;
	// -2 means not searched; -1 means the simple phrase contains no '@'.
	gssize phrase_at;

	GArray *line_starts;
	gssize *phrase_ends, *unquoted_ats;
	gssize name_addr_from;
	gboolean name_addr_follows;
} AddressValidator;

static gboolean
is_whitespace (guint8 c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static gboolean
is_atom (guint8 c)
{
	return c >= 128 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || (c != 0 && strchr ("!#$%&'*+-/=?^_`{|}~", c) != NULL);
}

/* g_utf8_validate() rejects embedded nulls even with an explicit length. Nulls
 * are valid UTF-8 and have their own address diagnostics, so validate each
 * null-delimited span without losing the original length or treating a null
 * as the end of the input. */
static gboolean
valid_utf8 (const guint8 *buffer, gsize length)
{
	while (length) {
		const guint8 *nul = memchr (buffer, 0, length);
		gsize span = nul ? (gsize) (nul - buffer) : length;

		if (!g_utf8_validate ((const char *) buffer, (gssize) span, NULL))
			return FALSE;

		if (!nul)
			return TRUE;

		buffer += span + 1;
		length -= span + 1;
	}

	return TRUE;
}

static void av_log (AddressValidator *v, GMimeComplianceViolation violation, gssize at);
static gboolean av_skip_white_space (AddressValidator *v);
static gboolean av_skip_comment (AddressValidator *v);
static gboolean av_skip_cfws (AddressValidator *v);
static gboolean av_skip_quoted (AddressValidator *v);
static void av_scan_for_control_characters (AddressValidator *v);
static gboolean av_is_iso2022_sequence (AddressValidator *v, gssize i, gssize *length);
static gssize av_next_iso2022_sequence (AddressValidator *v, gssize from, gssize limit);
static gssize av_next_null_byte (AddressValidator *v, gssize from, gssize limit);
static void av_check_iso2022_local_part (AddressValidator *v, gssize start, gssize end);
static void av_report_unattributed_iso2022_sequences (AddressValidator *v);
static void av_check_null_bytes_in_display_name (AddressValidator *v, gssize start, gssize end);
static void av_report_unattributed_null_bytes (AddressValidator *v);
static gboolean av_is_control_character (AddressValidator *v, gssize i);
static gboolean av_check_line_break (AddressValidator *v, gssize start, gssize end, gboolean *reported);
static gboolean av_is_atom_or_control (AddressValidator *v G_GNUC_UNUSED, guint8 c);
static gboolean av_can_begin_address (AddressValidator *v, guint8 c);
static gboolean av_is_address_list_separator (AddressValidator *v G_GNUC_UNUSED, guint8 c);
static void av_skip_to_next_possible_address (AddressValidator *v);
static void av_log_missing_address_separator (AddressValidator *v, gssize addressStart);
static void av_log_damaged_local_part (AddressValidator *v, gssize at);
static gboolean av_contains_angle_addr (AddressValidator *v, gssize start, gssize end);
static gboolean av_skip_atom (AddressValidator *v);
static void av_skip_domain_literal (AddressValidator *v);
static void av_validate_dot_atom (AddressValidator *v, GMimeComplianceViolation violation);
static void av_validate_domain_literal (AddressValidator *v);
static void av_validate_domain (AddressValidator *v);
static void av_validate_addrspec (AddressValidator *v, gboolean inAngleAddr, gboolean confirmedAddrSpec);
static void av_validate_angle_addr (AddressValidator *v);
static void av_record_phrase_extent (AddressValidator *v, gssize start, gboolean simple, gboolean reusableTerminator);
static void av_index_lookahead (AddressValidator *v);
static gboolean av_try_get_phrase_end (AddressValidator *v, gssize start, gssize *end);
static gboolean av_try_scan_phrase (AddressValidator *v, gboolean *unquotedSpecial, gssize *specialIndex, gboolean *hasContent);
static gboolean av_scan_phrase (AddressValidator *v, gboolean *unquotedSpecial, gssize *specialIndex, gboolean *hasContent);
static void av_validate_group (AddressValidator *v, gboolean unquotedSpecial, gssize specialIndex);
static void av_validate_group_list (AddressValidator *v);
static gboolean av_validate_address (AddressValidator *v, gboolean inGroup);
static gboolean av_contains_at (AddressValidator *v, gssize start, gssize end);
static gboolean av_contains_addrspec (AddressValidator *v, gssize start, gssize end, gboolean *quotedAddrspec);
static gboolean av_is_dotted_domain (AddressValidator *v, gssize i, gssize end);
static gboolean av_followed_by_name_addr (AddressValidator *v, gssize start);
static void av_validate_address_list (AddressValidator *v);

static void
av_log (AddressValidator *v, GMimeComplianceViolation violation, gssize at)
{
	gssize end = MIN (at, v->end_index);
	guint64 line;
	gssize line_begin;

	for (gssize i = v->scan_index; i < end; i++) {
		if (v->text[i] == '\n') {
			v->scan_line++;
			v->scan_line_begin = i + 1;

			if (v->line_starts) {
				gssize next = i + 1;
				g_array_append_val (v->line_starts, next);
			}
		}
	}

	v->scan_index = MAX (v->scan_index, end);
	line = v->scan_line;
	line_begin = v->scan_line_begin;

	if (at < line_begin) {
		if (!v->line_starts) {
			v->line_starts = g_array_new (FALSE, FALSE, sizeof (gssize));

			for (gssize i = v->start_index; i < v->scan_index; i++) {
				if (v->text[i] == '\n') {
					gssize next = i + 1;
					g_array_append_val (v->line_starts, next);
				}
			}
		}

		guint low = 0, high = v->line_starts->len;

		while (low < high) {
			guint mid = low + (high - low) / 2;

			if (g_array_index (v->line_starts, gssize, mid) <= at)
				low = mid + 1;
			else
				high = mid;
		}

		line = v->line_number + low;
		line_begin = low > 0 ? g_array_index (v->line_starts, gssize, low - 1) : -1;
	}

	// Note: Until the value has been folded, the column is still relative to the field name
	// that preceded it on the same line.
	GMimeComplianceIssue issue = {
		v->context, violation, v->stream_offset + (at - v->start_index), line,
		line_begin < 0 ? v->column_number + (guint64) (at - v->start_index) : (guint64) (at - line_begin) + 1,
		GMIME_COMPLIANCE_POSITION_EXACT
	};

	v->report (&issue, v->user_data);
}

static gboolean
av_skip_white_space (AddressValidator *v)
{
	gssize start = v->index;

	while (v->index < v->end_index && is_whitespace (v->text[v->index]))
		v->index++;

	return v->index > start;
}

/*
 * Skip over a comment, returning false if it was never closed.
 */
static gboolean
av_skip_comment (AddressValidator *v)
{
	gssize start = v->index;
	gssize depth = 1;

	// skip over the opening '('
	v->index++;

	while (v->index < v->end_index) {
		if (v->text[v->index] == '\\') {
			// quoted-pair
			v->index++;

			if (v->index >= v->end_index)
				break;
		} else if (v->text[v->index] == '(') {
			depth++;
		} else if (v->text[v->index] == ')' && --depth == 0) {
			v->index++;
			return TRUE;
		}

		v->index++;
	}

	av_log (v, GMIME_COMPLIANCE_UNBALANCED_PARENTHESES_IN_ADDRESS, start);

	return FALSE;
}

/*
 * Skip over folding whitespace and comments, returning false if a comment
 * was never closed (in which case the remainder of the value has been consumed).
 */
static gboolean
av_skip_cfws (AddressValidator *v)
{
	do {
		av_skip_white_space (v);

		if (v->index >= v->end_index || v->text[v->index] != '(')
			return TRUE;

		v->skipped_comment = TRUE;

		if (!av_skip_comment (v))
			return FALSE;
	} while (TRUE);
}

/*
 * Skip over a quoted-string, returning false if it was never closed.
 */
static gboolean
av_skip_quoted (AddressValidator *v)
{
	gssize start = v->index;

	// skip over the opening '"'
	v->index++;

	while (v->index < v->end_index) {
		if (v->text[v->index] == '\\') {
			v->index++;

			if (v->index >= v->end_index)
				break;
		} else if (v->text[v->index] == '"') {
			v->index++;
			return TRUE;
		}

		v->index++;
	}

	av_log (v, GMIME_COMPLIANCE_UNBALANCED_QUOTES_IN_ADDRESS, start);

	return FALSE;
}

/*
 * Report null bytes and other control characters anywhere within the value.
 * These are scanned for up front rather than token by token because they are not legal
 * anywhere in a header value, so there is no position in which one needs to be tolerated, and
 * because a control character derails the grammar: scanning first means the finding survives
 * even when the structural parse of the surrounding address goes on to fail.
 * Each class is reported once, at its first occurrence. A linefeed is deliberately
 * excluded: folding whitespace is legal between address tokens, so a line break can only be
 * judged in context and is handled during parsing instead. A carriage return with no linefeed
 * after it is not excluded, because it cannot be part of a fold in any context; it is
 * reported as MimeComplianceViolation.LineBreakInAddress rather than as a
 * control character, since what makes it dangerous is that it is half of a line break.
 * ISO-2022 shift and escape sequences are recorded rather than reported. Inside a
 * local-part they are a legacy Japanese mailer convention rather than an arbitrary control
 * character, and telling the two apart needs the parse, so the decision is deferred to
 * CheckIso2022LocalPart and ReportUnattributedIso2022Sequences.
 * Null bytes are recorded rather than reported for the same reason: one in a display-name
 * is described differently from one in an addr-spec, so the decision is deferred to
 * CheckNullBytesInDisplayName and ReportUnattributedNullBytes.
 */
static void
av_scan_for_control_characters (AddressValidator *v)
{
	gssize length;

	// Note: Only the position of the first NULL byte and of the first ISO-2022 sequence is kept.
	// Each class is reported at most once, and the cursors are advanced by searching forward
	// again if the token they land in turns &to account for them, so there is nothing to be
	// gained by remembering the rest.
	for (gssize i = v->start_index; i < v->end_index; i++) {
		guint8 c = v->text[i];

		if (c == 0) {
			if (v->null_cursor < 0)
				v->null_cursor = i;
		} else if (av_is_iso2022_sequence (v, i, &length)) {
			if (v->iso2022_cursor < 0)
				v->iso2022_cursor = i;

			// Note: Step over the rest of the sequence so that its intermediate and final
			// bytes are not considered again.
			i += length - 1;
		} else if (av_is_control_character (v, i)) {
			// Note: A carriage return reaching here is one with no linefeed after it, which is
			// the primitive a header injection is built from: a transport that normalizes it
			// to CRLF turns it into a header split. Describing it as a line break rather than
			// as a generic control character keeps it at the same severity as the folded form
			// in CheckLineBreak, which is the far more benign of the two and would otherwise
			// outrank it.
			if (v->text[i] == '\r') {
				if (!v->reported_stray_carriage_return) {
					av_log (v, GMIME_COMPLIANCE_LINE_BREAK_IN_ADDRESS, i);
					v->reported_stray_carriage_return = TRUE;
				}
			} else if (!v->reported_control_character) {
				av_log (v, GMIME_COMPLIANCE_CONTROL_CHARACTER_IN_ADDRESS, i);
				v->reported_control_character = TRUE;
			}
		}
	}
}

/*
 * Determine whether an ISO-2022 shift or escape sequence begins at the given index, and if so,
 * how many bytes long it is.
 * An escape is only recognized as a character set designation when it is followed by at
 * least one intermediate byte in the 0x20..0x2f range and then a final byte in the
 * 0x30..0x7e range, which is the shape of every ISO-2022 designation sequence: the
 * ESC ( B and ESC $ B of rfc1468, the ESC $ ( D and ESC . A added by
 * rfc1554, and the ESC $ ) C of rfc1557 among them.
 * Requiring an intermediate byte is what keeps an escape that merely happens to precede a
 * letter from being mistaken for a designation. Such an escape is an ordinary control character
 * and is reported as one.
 * Shift-out and shift-in are recognized on their own. ISO-2022-JP proper has no use for
 * them, but the Korean and Chinese variants use them to switch between the designated sets, and
 * neither byte has any other meaning in a header.
 */
static gboolean
av_is_iso2022_sequence (AddressValidator *v, gssize i, gssize *length)
{
	guint8 c = v->text[i];

	if (c == 0x0e || c == 0x0f) {
		(*length) = 1;
		return TRUE;
	}

	(*length) = 0;

	if (c != 0x1b)
		return FALSE;

	gssize j = i + 1;

	while (j < v->end_index && v->text[j] >= 0x20 && v->text[j] <= 0x2f)
		j++;

	if (j == i + 1 || j >= v->end_index || v->text[j] < 0x30 || v->text[j] > 0x7e)
		return FALSE;

	(*length) = (j - i) + 1;

	return TRUE;
}

/*
 * Find the next ISO-2022 shift or escape sequence beginning at or after from
 * and before limit, or -1 if there is none.
 * No sequence can begin inside another, because the intermediate and final bytes of one are
 * drawn from ranges that contain none of the bytes that can start one, so it is safe to begin
 * looking from an arbitrary position.
 */
static gssize
av_next_iso2022_sequence (AddressValidator *v, gssize from, gssize limit)
{
	while (from < limit) {
		if (v->text[from] == 0x0e || v->text[from] == 0x0f || v->text[from] == 0x1b) {
			gssize length;

			if (av_is_iso2022_sequence (v, from, &length))
				return from;

			from += MAX (length, 1);
		} else {
			from++;
		}
	}

	return -1;
}

/*
 * Find the next null byte at or after from and before
 * limit, or -1 if there is none.
 */
static gssize
av_next_null_byte (AddressValidator *v, gssize from, gssize limit)
{
	if (from >= limit)
		return -1;

	const guint8 *at = memchr (v->text + from, 0, (gsize) (limit - from));

	return at ? at - v->text : -1;
}

/*
 * Attribute any ISO-2022 sequence that falls within the given range to the local-part that
 * occupies it.
 * A sequence claimed here is struck from the pending set so that the same bytes are not also
 * described as control characters by ReportUnattributedIso2022Sequences.
 * start: The start of the local-part.
 * end: The end of the local-part.
 */
static void
av_check_iso2022_local_part (AddressValidator *v, gssize start, gssize end)
{
	if (v->iso2022_cursor < 0)
		return;

	// Note: Only the first local-part sequence is reported, so once one has been found the
	// range no longer needs to be examined at all.
	if (!v->reported_iso2022_local_part) {
		gssize at = av_next_iso2022_sequence (v, start, end);

		if (at >= 0) {
			av_log (v, GMIME_COMPLIANCE_ISO2022_SEQUENCE_IN_LOCAL_PART, at);
			v->reported_iso2022_local_part = TRUE;
		}
	}

	// Note: Local-parts are visited in increasing order, so a pending sequence that this one
	// accounts for can never be wanted again and the cursor only ever moves forward.
	if (v->iso2022_cursor >= start && v->iso2022_cursor < end)
		v->iso2022_cursor = av_next_iso2022_sequence (v, end, v->end_index);
}

/*
 * Report the first ISO-2022 sequence that did not turn out to be inside a local-part as the
 * control character that it is.
 * This has to run after the parse, because nothing before it can say where a sequence ended up,
 * but it runs on every exit path so that the finding still survives a parse that gave up early.
 */
static void
av_report_unattributed_iso2022_sequences (AddressValidator *v)
{
	if (v->iso2022_cursor < 0 || v->reported_control_character)
		return;

	av_log (v, GMIME_COMPLIANCE_CONTROL_CHARACTER_IN_ADDRESS, v->iso2022_cursor);
	v->reported_control_character = TRUE;
}

/*
 * Attribute any null byte that falls within the given range to the display-name that occupies
 * it.
 * A null byte claimed here is struck from the pending set so that the same byte is not also
 * described as an addr-spec null by ReportUnattributedNullBytes.
 * start: The start of the display-name.
 * end: The end of the display-name.
 */
static void
av_check_null_bytes_in_display_name (AddressValidator *v, gssize start, gssize end)
{
	if (v->null_cursor < 0)
		return;

	// Note: Only the first display-name NULL is reported, so once one has been found the range
	// no longer needs to be examined at all.
	if (!v->reported_null_byte_in_display_name) {
		gssize at = av_next_null_byte (v, start, end);

		if (at >= 0) {
			av_log (v, GMIME_COMPLIANCE_NULL_BYTE_IN_DISPLAY_NAME, at);
			v->reported_null_byte_in_display_name = TRUE;
		}
	}

	// Note: Display-names are visited in increasing order, so a pending NULL that this one
	// accounts for can never be wanted again and the cursor only ever moves forward.
	if (v->null_cursor >= start && v->null_cursor < end)
		v->null_cursor = av_next_null_byte (v, end, v->end_index);
}

/*
 * Report the first null byte that did not turn out to be inside a display-name as an addr-spec
 * null byte.
 * This has to run after the parse, because nothing before it can say where a null byte ended
 * up, but it runs on every exit path so that the finding still survives a parse that gave up
 * early. A null byte that the parse never reached is reported here too: it is not in any
 * display-name, and the addr-spec description is the more serious of the two.
 */
static void
av_report_unattributed_null_bytes (AddressValidator *v)
{
	if (v->null_cursor < 0 || v->reported_null_byte_in_address)
		return;

	av_log (v, GMIME_COMPLIANCE_NULL_BYTE_IN_ADDRESS, v->null_cursor);
	v->reported_null_byte_in_address = TRUE;
}

/*
 * Determine whether the byte at the given index is a control character that cannot appear in
 * an address.
 * A tab is whitespace rather than a control character, and a linefeed may be part of a fold.
 * A carriage return may only ever appear as the first half of the CRLF of a fold: both
 * FWS and obs-FWS require a linefeed and at least one whitespace character after
 * it, and obs-NO-WS-CTL excludes both CR and LF, so a carriage return with nothing
 * following it is illegal under every tier of the grammar, inside a quoted-string or a comment
 * as much as anywhere else.
 */
static gboolean
av_is_control_character (AddressValidator *v, gssize i)
{
	guint8 c = v->text[i];

	if (c == '\r')
		return i + 1 >= v->end_index || v->text[i + 1] != '\n';

	return (c < 0x20 && c != '\t' && c != '\n') || c == 0x7f;
}

/*
 * Report a line break within a range that appeared inside a token.
 * true if the range contained a line break.
 * start: The start of the range.
 * end: The end of the range.
 * reported: Whether a line break has already been reported for this address.
 */
static gboolean
av_check_line_break (AddressValidator *v, gssize start, gssize end, gboolean *reported)
{
	gboolean found = FALSE;

	for (gssize i = start; i < end; i++) {
		if (v->text[i] != '\r' && v->text[i] != '\n')
			continue;

		found = TRUE;

		// Note: A carriage return with no linefeed after it has already been (*reported) by
		// ScanForControlCharacters, which owns it because it is illegal regardless of where
		// it appears. The token still has to resume across it, but the same byte must not be
		// described twice, so keep looking for a genuine line break instead. Both paths now
		// report LineBreakInAddress, so this only avoids logging that violation twice.
		if (v->text[i] == '\r' && (i + 1 >= v->end_index || v->text[i + 1] != '\n'))
			continue;

		if (!(*reported)) {
			av_log (v, GMIME_COMPLIANCE_LINE_BREAK_IN_ADDRESS, i);
			(*reported) = TRUE;
		}

		break;
	}

	return found;
}

/*
 * Determine whether a byte may be consumed as part of an atom.
 * Control characters are included deliberately. ScanForControlCharacters has
 * already reported them, so absorbing them into the token they appear in loses nothing, and it
 * keeps a single bad byte from splitting a token and cascading into a series of structural
 * violations that describe nothing the sender actually did.
 */
static gboolean
av_is_atom_or_control (AddressValidator *v G_GNUC_UNUSED, guint8 c)
{
	return is_atom (c) || c == 0 || c == 0x7f || (c < 0x20 && c != '\t' && c != '\r' && c != '\n');
}

/*
 * Determine whether a byte could begin an address, once the list separators and any
 * preceding CFWS have been dealt with by the caller.
 * This is not a grammar production but a progress guarantee. An address is a phrase, an
 * addr-spec or an angle-addr, so it begins with atom text, a quoted-string or a '<'. The
 * '.', '@' and ':' are not legal starts either, but each of them is consumed by the
 * productions that report them, so they are admitted here to leave that reporting alone. Any
 * other byte is one no production can consume, which makes re-entering the parser at it a
 * guaranteed no-op.
 */
static gboolean
av_can_begin_address (AddressValidator *v, guint8 c)
{
	return av_is_atom_or_control (v, c) || c == '"' || c == '<' || c == '.' || c == '@' || c == ':';
}

/*
 * Determine whether a byte delimits the elements of an address list or a group.
 */
static gboolean
av_is_address_list_separator (AddressValidator *v G_GNUC_UNUSED, guint8 c)
{
	return c == ',' || c == ';';
}

/*
 * Advance to the next byte either address-list loop could make progress on.
 * Both loops report a missing separator and then re-enter the parser at the offending byte,
 * on the assumption that a new address begins there. When the byte is one no production can
 * consume, that assumption is wrong: the previous address stopped precisely because of it,
 * and re-entering re-walks the same token, describing it a second time before the
 * no-progress guard breaks the spin. Skipping the whole run of such bytes keeps each one
 * described once and keeps a run from tripping that guard repeatedly. The run stops at a
 * list separator, which the loops consume themselves and which must not be swallowed.
 */
static void
av_skip_to_next_possible_address (AddressValidator *v)
{
	while (v->index < v->end_index && !av_is_address_list_separator (v, v->text[v->index]) && !av_can_begin_address (v, v->text[v->index]))
		v->index++;
}

/*
 * Report a missing address separator, along with the ambiguity it creates when an angle-addr
 * is involved.
 * An ordinary missing comma, as in a@example.com b@example.com, leaves two bare
 * addr-specs with no grammatical relationship to each other: a display-name only appears in
 * a name-addr, which requires angle brackets, so no conformant reading folds the left token
 * into the right one. Parsers may still disagree about how many mailboxes to recover, but a
 * parser that recovers a single mailbox has to invent a production the grammar does not
 * offer. An angle-addr on either side of the gap is different: phrase angle-addr is a
 * real production, so reading everything in front of the angle-addr as its display-name is a
 * legitimate application of the grammar rather than a departure from it. Both readings are
 * then defensible and they disagree about which mailbox the address names rather than merely
 * about how many there are. That is what makes it worth reporting separately from the syntax
 * error.
 */
static void
av_log_missing_address_separator (AddressValidator *v, gssize addressStart)
{
	av_log (v, GMIME_COMPLIANCE_MISSING_ADDRESS_SEPARATOR, v->index);

	if (v->text[v->index] == '<' || av_contains_angle_addr (v, addressStart, v->index))
		av_log (v, GMIME_COMPLIANCE_AMBIGUOUS_MAILBOX_BOUNDARY, v->index);
}

/*
 * Report a local-part that was cut short by a character no addr-spec production can contain.
 * The local-part ended somewhere other than at an @, a list delimiter or the end of
 * the value, which means an unquoted special landed inside it: a[b@example.com,
 * a\b@example.com, a b@example.com. Quoting it, as section 3.4.1 of rfc5322
 * requires, would have made all of these legal.
 * This also suppresses the missing-separator report for the address, because the two
 * findings are mutually exclusive descriptions of the same character rather than two faults.
 * The scan stops mid-token, so the character it stopped on is whatever damaged the local-part,
 * not the place a comma was left out -- there is no second address for a comma to have
 * separated.
 * Resynchronization steps over that character when it is a list separator, since
 * SkipToNextPossibleAddress halts on one and would otherwise hand the same
 * already-reported byte back to the list loop as the start of another address. Anything else
 * it can skip on its own.
 * at: The offset of the character that ended the local-part.
 */
static void
av_log_damaged_local_part (AddressValidator *v, gssize at)
{
	av_log (v, GMIME_COMPLIANCE_INVALID_LOCAL_PART, at);
	v->damaged_local_part = TRUE;
}

/*
 * Determine whether the given range contains a '<' that opens an angle-addr.
 * Quoted-strings and comments are skipped, because a '<' inside either of them is
 * ordinary text rather than the start of an angle-addr.
 */
static gboolean
av_contains_angle_addr (AddressValidator *v, gssize start, gssize end)
{
	gssize i = start;
	while (i < end) {
		if (v->text[i] == '"' || v->text[i] == '(') {
			guint8 opener = v->text[i++];
			gssize depth = 1;

			while (i < end) {
				guint8 c = v->text[i++];

				if (c == '\\') {
					if (i < end)
						i++;
				} else if (opener == '"' && c == '"') {
					break;
				} else if (opener == '(') {
					if (c == '(')
						depth++;
					else if (c == ')' && --depth == 0)
						break;
				}
			}
		} else if (v->text[i] == '<') {
			return TRUE;
		} else {
			i++;
		}
	}

	return FALSE;
}

static gboolean
av_skip_atom (AddressValidator *v)
{
	gssize start = v->index;

	while (v->index < v->end_index && av_is_atom_or_control (v, v->text[v->index]))
		v->index++;

	return v->index > start;
}

/*
 * Skip over a bracketed domain-literal without reporting anything.
 * A domain-literal is a single lexical token, so the specials inside it - most obviously the
 * colons of an IPv6 literal - must not be mistaken for address-list delimiters.
 */
static void
av_skip_domain_literal (AddressValidator *v)
{
	// skip over the opening '['
	v->index++;

	while (v->index < v->end_index && v->text[v->index] != ']')
		v->index++;

	if (v->index < v->end_index)
		v->index++;
}

/*
 * Validate a dot-atom, reporting the given violation if it contains an empty element (which
 * covers consecutive dots as well as a leading or trailing dot).
 */
static void
av_validate_dot_atom (AddressValidator *v, GMimeComplianceViolation violation)
{
	do {
		gssize start = v->index;

		if (!av_skip_atom (v)) {
			// An empty element: either two dots in a row, or a dot at one end of the token.
			av_log (v, violation, start);

			if (v->index >= v->end_index || v->text[v->index] != '.')
				return;
		}

		if (v->index >= v->end_index || v->text[v->index] != '.')
			return;

		// skip over the '.'
		v->index++;
	} while (TRUE);
}

static void
av_validate_domain_literal (AddressValidator *v)
{
	gssize start = v->index;

	// skip over the opening '['
	v->index++;

	gboolean reportedInvalidCharacter = FALSE;
	gboolean reportedWhiteSpace = FALSE;

	while (v->index < v->end_index && v->text[v->index] != ']') {
		// Note: A lone carriage return is whitespace, but ScanForControlCharacters has already
		// reported it as a control character, which is the more accurate description of it.
		if (is_whitespace (v->text[v->index]) && !reportedWhiteSpace && !av_is_control_character (v, v->index)) {
			av_log (v, GMIME_COMPLIANCE_WHITESPACE_IN_DOMAIN_LITERAL, v->index);
			reportedWhiteSpace = TRUE;
		} else if ((v->text[v->index] == '[' || v->text[v->index] == '\\') && !reportedInvalidCharacter) {
			// Note: These are the only two characters dtext excludes that can reach this loop. A ']'
			// ends it, control characters and invalid 8-bit bytes have already been reported by their
			// character class, and rfc6532 adds the remaining 8-bit bytes to dtext.
			av_log (v, GMIME_COMPLIANCE_INVALID_CHARACTER_IN_DOMAIN_LITERAL, v->index);
			reportedInvalidCharacter = TRUE;
		}

		v->index++;
	}

	if (v->index >= v->end_index) {
		av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, start);
		return;
	}

	// skip over the closing ']'
	v->index++;
}

static void
av_validate_domain (AddressValidator *v)
{
	if (v->index < v->end_index && v->text[v->index] == '[') {
		av_validate_domain_literal (v);
		return;
	}

	gssize start = v->index;
	gboolean reportedLineBreak = FALSE;

	do {
		gssize atomStart = v->index;

		if (!av_skip_atom (v)) {
			// An empty element: either two dots in a row or a dot at the start of the domain.
			// Note: Unlike ValidateDotAtom(), a dot at the *end* of the domain is reported as
			// TrailingDotInDomain below, so the only way to get here is with a dot that is
			// followed by another dot or that begins the domain.
			if (atomStart >= v->end_index || v->text[atomStart] != '.') {
				av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, start);
				return;
			}

			av_log (v, GMIME_COMPLIANCE_INVALID_DOMAIN, atomStart);
		}

		// Note: A conforming domain is a single dot-atom, which admits no comments or folding
		// whitespace between its atoms and dots. Anything else is obs-domain.
		gssize beforeCFWS = v->index;

		if (!av_skip_cfws (v))
			return;

		gboolean obsolete = v->index > beforeCFWS;

		if (v->index >= v->end_index || v->text[v->index] != '.') {
			if (obsolete) {
				// Note: The token continues without an intervening '.', so this whitespace is
				// inside the dot-atom-text rather than around it.
				if (v->index < v->end_index && av_is_atom_or_control (v, v->text[v->index]) && av_check_line_break (v, beforeCFWS, v->index, &reportedLineBreak))
					continue;

				v->index = beforeCFWS;
			}

			return;
		}

		// skip over the '.'
		gssize dot = v->index;
		v->index++;

		beforeCFWS = v->index;

		if (!av_skip_cfws (v))
			return;

		if (v->index > beforeCFWS)
			obsolete = TRUE;

		if (obsolete)
			av_log (v, GMIME_COMPLIANCE_OBSOLETE_DOMAIN_SYNTAX, beforeCFWS);

		if (v->index >= v->end_index || (!av_is_atom_or_control (v, v->text[v->index]) && v->text[v->index] != '.')) {
			// Note: The dot itself is the violation, so it is reported at the dot rather than
			// at whatever follows it (which, for the common case, is the end of the domain).
			av_log (v, GMIME_COMPLIANCE_TRAILING_DOT_IN_DOMAIN, dot);
			return;
		}
	} while (TRUE);
}

/*
 * Validate an addr-spec, optionally allowing it to be terminated by '>'.
 * inAngleAddr: Whether the addr-spec is enclosed in angle brackets.
 * confirmedAddrSpec:
 * Whether this is known to be an addr-spec rather than a phrase that is being re-read as one.
 * Folding is legal between the words of a phrase but not inside the tokens of an addr-spec, so
 * line breaks may only be reported when the distinction has already been made.
 */
static void
av_validate_addrspec (AddressValidator *v, gboolean inAngleAddr, gboolean confirmedAddrSpec)
{
	gssize start = v->index;
	gboolean reportedLineBreak = FALSE;

	do {
		gssize localPartStart = v->index;
		gboolean closed;

		// local-part = dot-atom / quoted-string
		if (v->index < v->end_index && v->text[v->index] == '"') {
			gssize quoted = v->index;

			closed = av_skip_quoted (v);

			// Note: Section 3.2.4 of rfc5322 does permit FWS inside a quoted-string, so this
			// one is reported despite being legal. A quoted local-part is rare enough that
			// implementations get it wrong, and both MimeKit and Exchange have mishandled a
			// line break here, so the divergence this violation exists to report is real
			// whether or not the grammar allows the construct.
			if (closed)
				av_check_line_break (v, quoted, v->index, &reportedLineBreak);
		} else {
			closed = TRUE;

			av_validate_dot_atom (v, GMIME_COMPLIANCE_INVALID_LOCAL_PART);
		}

		// Note: The local-part is attributed as soon as its token has been consumed, rather
		// than once the whole addr-spec is known, so that the attribution survives a
		// quoted-string that was never closed and so that trailing comments are excluded.
		av_check_iso2022_local_part (v, localPartStart, v->index);

		if (!closed)
			return;

		gssize beforeCFWS = v->index;

		if (!av_skip_cfws (v))
			return;

		// Note: rfc5322 permits CFWS around a dot-atom but none inside its dot-atom-text, so
		// whitespace followed by more of the same token is not legal folding.
		if (!confirmedAddrSpec || v->index == beforeCFWS || v->index >= v->end_index || !av_is_atom_or_control (v, v->text[v->index]))
			break;

		// Once the line break has been reported, carry on as though the whitespace were not
		// there, so that the rest of the addr-spec is described accurately rather than
		// cascading into structural violations the sender is not responsible for. Ordinary
		// embedded whitespace is left to the existing handling below.
		if (!av_check_line_break (v, beforeCFWS, v->index, &reportedLineBreak))
			break;
	} while (TRUE);

	if (v->index >= v->end_index || v->text[v->index] != '@') {
		// Note: An addr-spec with no domain. Inside an angle-addr this is still a naked
		// local-part; outside of one it is section 7.1.7 of rfc7103.
		if (v->index >= v->end_index || v->text[v->index] == ',' || (v->in_group_list && v->text[v->index] == ';') || (inAngleAddr && v->text[v->index] == '>'))
			av_log (v, GMIME_COMPLIANCE_ADDRESS_WITHOUT_DOMAIN, start);
		else
			av_log_damaged_local_part (v, v->index);

		return;
	}

	// skip over the '@'
	v->index++;

	if (!av_skip_cfws (v))
		return;

	if (v->index >= v->end_index) {
		av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, start);
		return;
	}

	av_validate_domain (v);
}

static void
av_validate_angle_addr (AddressValidator *v)
{
	gssize start = v->index;

	// skip over the '<'
	v->index++;

	if (!av_skip_cfws (v))
		return;

	// Note: Section 7.1.2 of rfc7103.
	if (v->index < v->end_index && v->text[v->index] == '<') {
		av_log (v, GMIME_COMPLIANCE_EXCESSIVE_ANGLE_BRACKETS_IN_ADDRESS, v->index);

		while (v->index < v->end_index && v->text[v->index] == '<')
			v->index++;

		if (!av_skip_cfws (v))
			return;
	}

	// Note: The obs-route production from section 4.4 of rfc5322.
	if (v->index < v->end_index && v->text[v->index] == '@') {
		av_log (v, GMIME_COMPLIANCE_OBSOLETE_ROUTE_ADDRESS, v->index);

		while (v->index < v->end_index && v->text[v->index] != ':' && v->text[v->index] != '>')
			v->index++;

		if (v->index < v->end_index && v->text[v->index] == ':')
			v->index++;

		if (!av_skip_cfws (v))
			return;
	}

	av_validate_addrspec (v, TRUE, TRUE);

	if (!av_skip_cfws (v))
		return;

	if (v->index >= v->end_index || v->text[v->index] != '>') {
		av_log (v, GMIME_COMPLIANCE_UNBALANCED_ANGLE_BRACKETS_IN_ADDRESS, start);
		return;
	}

	// skip over the '>'
	v->index++;

	// Note: Section 7.1.2 of rfc7103.
	if (v->index < v->end_index && v->text[v->index] == '>') {
		av_log (v, GMIME_COMPLIANCE_EXCESSIVE_ANGLE_BRACKETS_IN_ADDRESS, v->index);

		while (v->index < v->end_index && v->text[v->index] == '>')
			v->index++;
	}
}

/*
 * Remember how far a phrase scan reached, if a later scan can be trusted to agree with it.
 * A phrase scan runs forward until it finds something that could terminate a
 * display-name, so a list of bare addr-specs with nothing separating them re-scans the whole
 * remainder of the value once per address, which costs O(length squared).
 * A simple region can be reused directly. If recovery re-enters a region containing
 * quoted-strings, comments or domain-literals, a suffix index is built instead: recovery can
 * start inside one of those tokens, where the same delimiters may no longer be hidden.
 * The extent is only recorded when the terminator is neither '<' nor ':', because
 * those are the only two that make ValidateAddress look at what the phrase
 * contained rather than merely how far it ran, and the recorded extent deliberately says
 * nothing about the contents.
 * start: The index the phrase scan began at.
 * simple: Whether the scanned region contained no quoted-string, domain-literal or comment.
 * reusableTerminator: Whether the scan stopped on something other than '<' or ':'.
 */
static void
av_record_phrase_extent (AddressValidator *v, gssize start, gboolean simple, gboolean reusableTerminator)
{
	if (reusableTerminator && v->index > start) {
		v->phrase_from = start;
		v->phrase_end = v->index;
		v->phrase_simple = simple;
		v->phrase_at = -2;
	}
}

/*
 * Only malformed lists that re-scan overlapping lookahead regions need these indexes. Build
 * them backwards so every quoted-string, nested comment and domain-literal can be skipped in
 * constant time even when recovery resumes inside a previously scanned token.
 */
static void
av_index_lookahead (AddressValidator *v)
{
	if (v->phrase_ends != NULL)
		return;

	gssize length = v->end_index - v->start_index;
	gssize *quotes = g_new (gssize, length);
	gssize *comments = g_new (gssize, length);

	gssize *phraseEnds = g_new (gssize, length);
	gssize *unquotedAts = g_new (gssize, length);

	v->phrase_ends = phraseEnds;
	v->unquoted_ats = unquotedAts;

	gssize quote = -1, comment = -1, bracket = v->end_index;

	for (gssize i = v->end_index - 1; i >= v->start_index; i--) {
		gssize slot = i - v->start_index;
		guint8 c = v->text[i];
		gssize nextQuote = quote;
		gssize nextComment = comment;

		if (c == '\\') {
			quote = i + 2 < v->end_index ? quotes[slot + 2] : -1;
			comment = i + 2 < v->end_index ? comments[slot + 2] : -1;
		} else {
			if (c == '"')
				quote = i + 1;

			if (c == ')')
				comment = i + 1;
			else if (c == '(')
				comment = nextComment >= 0 && nextComment < v->end_index ? comments[nextComment - v->start_index] : -1;
		}

		quotes[slot] = quote;
		comments[slot] = comment;

		gssize next = i + 1;

		if (c == '"')
			next = nextQuote;
		else if (c == '(')
			next = nextComment;
		else if (c == '[')
			next = bracket;

		if (c == '<' || c == ':' || c == ',' || c == ';' || c == '>')
			phraseEnds[slot] = i;
		else
			phraseEnds[slot] = next < 0 ? -1 : next == v->end_index ? v->end_index : phraseEnds[next - v->start_index];

		next = c == '"' ? nextQuote : i + 1;
		unquotedAts[slot] = c == '@' ? i : next < 0 || next == v->end_index ? -1 : unquotedAts[next - v->start_index];

		if (c == ']')
			bracket = i + 1;
	}

	g_free (quotes);
	g_free (comments);
}

static gboolean
av_try_get_phrase_end (AddressValidator *v, gssize start, gssize *end)
{
	gssize *phraseEnds = v->phrase_ends;

	if (phraseEnds == NULL && start >= v->phrase_from && start < v->phrase_end) {
		if (v->phrase_simple) {
			(*end) = v->phrase_end;
			return TRUE;
		}

		av_index_lookahead (v);
		phraseEnds = v->phrase_ends;
	}

	(*end) = phraseEnds != NULL ? phraseEnds[start - v->start_index] : -1;

	// Display-names need the full scan's findings; an unterminated token needs its diagnostic.
	return (*end) >= 0 && ((*end) == v->end_index || (v->text[(*end)] != '<' && v->text[(*end)] != ':'));
}

/*
 * Try to scan a phrase without walking it token by token, returning false if
 * it holds something that only the full scan can make sense of.
 * A leading token has to be scanned before it is known whether it is a display-name or an
 * addr-spec, and ScanPhrase pays for that by walking every token in it. Almost
 * none of that walk is load-bearing: of everything it looks at, only a quoted-string, a comment
 * or a domain-literal can hide the terminator, and only '<' and ':' select a path that reads
 * what it recorded along the way.
 * So the first byte from either of those two groups decides everything. If it opens a
 * quoted-string, a comment or a domain-literal, this gives up and the full scan runs. If it is
 * a ',', ';' or '>' -- or there is none at all -- the phrase is really an addr-spec, which
 * ValidateAddrspec is about to re-read from the beginning anyway, so nothing more
 * needs to be worked out here. Otherwise the phrase is a display-name made only of atoms,
 * whitespace and unquoted specials, and both findings about it can be had from a further scan
 * apiece rather than from a walk.
 * unquotedSpecial: Whether the phrase contained a character that would have had to be quoted.
 * specialIndex: The index of the first such character.
 * hasContent: Whether the phrase contained anything at all besides comments and whitespace.
 */
static gboolean
av_try_scan_phrase (AddressValidator *v, gboolean *unquotedSpecial, gssize *specialIndex, gboolean *hasContent)
{
	gssize start = v->index;
	gssize at = start;
	*unquotedSpecial = FALSE;
	*specialIndex = -1;
	*hasContent = FALSE;

	// Only quotes, comments and domain-literals can hide a phrase terminator. Scanning
	// for structure first avoids walking ordinary addr-specs twice, just as the source's
	// SearchValues fast path does.
	// strchr also matches the string terminator; a null byte is atom/control text.
	while (at < v->end_index && (v->text[at] == 0 || !strchr ("\"([<:,;>", v->text[at])))
		at++;

	if (at == v->end_index) {
		v->index = at;
		v->skipped_comment = FALSE;
		av_record_phrase_extent (v, start, TRUE, TRUE);
		return TRUE;
	}

	guint8 c = v->text[at];
	if (c == '"' || c == '(' || c == '[')
		return FALSE;

	v->index = at;
	v->skipped_comment = FALSE;

	if (c != '<' && c != ':') {
		// A ',', ';' or '>' means an addr-spec; no display-name findings are needed.
		av_record_phrase_extent (v, start, TRUE, TRUE);
		return TRUE;
	}

	for (gssize i = start; i < at; i++) {
		if (!av_is_atom_or_control (v, v->text[i]) && !is_whitespace (v->text[i])) {
			*unquotedSpecial = TRUE;
			*specialIndex = i;
			*hasContent = TRUE;
			return TRUE;
		}

		if (!is_whitespace (v->text[i]))
			*hasContent = TRUE;
	}

	// '<' and ':' make the extent unusable as a memo; see RecordPhraseExtent.
	return TRUE;
}

/*
 * Scan a phrase, recording whether it contained a character that would have had to be quoted.
 * The same leading token can turn out to be a display-name or an addr-spec depending on what
 * follows it, so the decision about whether an unquoted special is a violation cannot be made
 * until the delimiter is known.
 * unquotedSpecial: Whether the phrase contained a character that would have had to be quoted.
 * specialIndex: The index of the first such character.
 * hasContent: Whether the phrase contained anything at all besides comments and whitespace.
 */
static gboolean
av_scan_phrase (AddressValidator *v, gboolean *unquotedSpecial, gssize *specialIndex, gboolean *hasContent)
{
	gssize start = v->index;
	gboolean simple = TRUE;

	(*unquotedSpecial) = FALSE;
	(*specialIndex) = -1;
	(*hasContent) = FALSE;
	v->skipped_comment = FALSE;

	do {
		if (!av_skip_cfws (v))
			return FALSE;

		if (v->skipped_comment)
			simple = FALSE;

		if (v->index >= v->end_index) {
			av_record_phrase_extent (v, start, simple, TRUE);
			return TRUE;
		}

		guint8 c = v->text[v->index];

		if (c == '"') {
			(*hasContent) = TRUE;
			simple = FALSE;

			if (!av_skip_quoted (v))
				return FALSE;
		} else if (c == '<' || c == ':' || c == ',' || c == ';' || c == '>') {
			av_record_phrase_extent (v, start, simple, c != '<' && c != ':');
			return TRUE;
		} else if (c == '@' || c == '.') {
			(*hasContent) = TRUE;

			if (!(*unquotedSpecial)) {
				(*unquotedSpecial) = TRUE;
				(*specialIndex) = v->index;
			}

			v->index++;
		} else if (c == '[') {
			(*hasContent) = TRUE;
			simple = FALSE;

			if (!(*unquotedSpecial)) {
				(*unquotedSpecial) = TRUE;
				(*specialIndex) = v->index;
			}

			av_skip_domain_literal (v);
		} else if (av_is_atom_or_control (v, c)) {
			(*hasContent) = TRUE;

			av_skip_atom (v);
		} else {
			// Some other character that cannot appear in a phrase at all.
			(*hasContent) = TRUE;

			if (!(*unquotedSpecial)) {
				(*unquotedSpecial) = TRUE;
				(*specialIndex) = v->index;
			}

			v->index++;
		}
	} while (TRUE);
}

static void
av_validate_group (AddressValidator *v, gboolean unquotedSpecial, gssize specialIndex)
{
	if (unquotedSpecial)
		av_log (v, GMIME_COMPLIANCE_UNQUOTED_DISPLAY_NAME, specialIndex);

	// skip over the ':'
	v->index++;

	gboolean wasInGroupList = v->in_group_list;

	v->in_group_list = TRUE;
	v->nested_group_depth = 0;

	av_validate_group_list (v);
	v->in_group_list = wasInGroupList;
	v->nested_group_depth = 0;
}

static void
av_validate_group_list (AddressValidator *v)
{
	gssize before;

	do {
		if (!av_skip_cfws (v))
			return;

		if (v->index >= v->end_index)
			break;

		if (v->text[v->index] == ';') {
			// skip over the ';'
			v->index++;

			if (v->nested_group_depth == 0)
				return;

			// The ';' terminated a nested group, which is then followed by a separator like any other address.
			v->nested_group_depth--;
			v->damaged_local_part = FALSE;
			before = v->index - 1;
			goto AfterAddress;
		}

		if (v->text[v->index] == ',') {
			av_log (v, GMIME_COMPLIANCE_EXTRANEOUS_COMMA_IN_ADDRESS_LIST, v->index);
			v->index++;
			continue;
		}

		gssize depth = v->nested_group_depth;

		before = v->index;

		v->damaged_local_part = FALSE;

		if (!av_validate_address (v, TRUE))
			return;

		// A nested group was opened; its members follow the ':' without a separator.
		if (v->nested_group_depth > depth)
			continue;

		if (v->index == before) {
			// Defensive: never spin on a character no production consumed.
			av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, v->index);
			v->index++;
			continue;
		}

	AfterAddress:
		if (!av_skip_cfws (v))
			return;

		if (v->index >= v->end_index)
			break;

		if (v->text[v->index] == ';') {
			v->index++;

			if (v->nested_group_depth == 0)
				return;

			v->nested_group_depth--;
			v->damaged_local_part = FALSE;
			before = v->index - 1;
			goto AfterAddress;
		}

		if (v->text[v->index] != ',') {
			if (!v->damaged_local_part)
				av_log_missing_address_separator (v, before);
			else if (av_is_address_list_separator (v, v->text[v->index]))
				v->index++;

			av_skip_to_next_possible_address (v);
			continue;
		}

		// skip over the ','
		v->index++;
	} while (TRUE);

	// Note: This is only reached by breaking &of the loop above, which happens once the end
	// of the value has been reached, so endIndex is where the missing ';' belongs. Pointing at
	// the insertion point rather than at the ':' that opened the group is what tells a reader
	// how much of the value the group swallowed.
	av_log (v, GMIME_COMPLIANCE_MISSING_GROUP_TERMINATOR, v->end_index);
}

/*
 * Validate a single address, returning false if the remainder of the value
 * has been consumed and scanning cannot usefully continue.
 */
static gboolean
av_validate_address (AddressValidator *v, gboolean inGroup)
{
	gssize end;
	gboolean quotedAddrspec;
	gssize start = v->index;
	gboolean unquotedSpecial = FALSE;
	gboolean hasContent = TRUE;
	gssize specialIndex = -1;

	if (start < v->phrase_end && av_try_get_phrase_end (v, start, &end)) {
		v->index = end;
	} else if (!av_try_scan_phrase (v, &unquotedSpecial, &specialIndex, &hasContent) && !av_scan_phrase (v, &unquotedSpecial, &specialIndex, &hasContent)) {
		return FALSE;
	}

	if (v->index >= v->end_index) {
		// The whole token was an addr-spec rather than a display-name.
		gboolean confirmed = av_contains_at (v, start, v->index);

		v->index = start;
		av_validate_addrspec (v, FALSE, confirmed);
		return TRUE;
	}

	guint8 c = v->text[v->index];

	if (c == ':') {
		// Note: rfc5322 does not permit a group inside a group (a group-list is a mailbox-list).
		if (inGroup)
			av_log (v, GMIME_COMPLIANCE_NESTED_ADDRESS_GROUP, start);

		if (!hasContent) {
			// A display-name is a phrase, which requires at least one word.
			av_log (v, GMIME_COMPLIANCE_EMPTY_GROUP_NAME, start);
		} else if (av_contains_addrspec (v, start, v->index, &quotedAddrspec)) {
			// Note: The quoted form is legal rfc5322 and merely misleading, but the unquoted
			// form also leaves a parser free to end the phrase at the '@', so the two are
			// reported as distinct violations.
			av_log (v, quotedAddrspec ? GMIME_COMPLIANCE_ADDRESS_IN_GROUP_DISPLAY_NAME : GMIME_COMPLIANCE_UNQUOTED_ADDRESS_IN_GROUP_DISPLAY_NAME, start);
		}

		av_check_null_bytes_in_display_name (v, start, v->index);

		if (inGroup) {
			if (unquotedSpecial)
				av_log (v, GMIME_COMPLIANCE_UNQUOTED_DISPLAY_NAME, specialIndex);

			// skip over the ':' and let ValidateGroupList() scan the members of the nested group
			v->nested_group_depth++;
			v->index++;
			return TRUE;
		}

		av_validate_group (v, unquotedSpecial, specialIndex);
		return TRUE;
	}

	if (c == '<') {
		if (unquotedSpecial)
			av_log (v, GMIME_COMPLIANCE_UNQUOTED_DISPLAY_NAME, specialIndex);

		if (hasContent && av_contains_addrspec (v, start, v->index, &quotedAddrspec))
			av_log (v, quotedAddrspec ? GMIME_COMPLIANCE_ADDRESS_IN_DISPLAY_NAME : GMIME_COMPLIANCE_UNQUOTED_ADDRESS_IN_DISPLAY_NAME, start);

		av_check_null_bytes_in_display_name (v, start, v->index);

		av_validate_angle_addr (v);
		return TRUE;
	}

	if (c == '>') {
		// A closing bracket with nothing to close: section 7.1.3 of rfc7103.
		av_log (v, GMIME_COMPLIANCE_UNBALANCED_ANGLE_BRACKETS_IN_ADDRESS, v->index);
		v->index++;
		return TRUE;
	}

	// ',' or ';': the phrase we just scanned was really an addr-spec.
	gboolean hasAt = av_contains_at (v, start, v->index);

	if (c == ',' && !hasAt && av_followed_by_name_addr (v, v->index + 1)) {
		// Section 7.1.5 of rfc7103: an unquoted comma inside a display-name splits one
		// address into what looks like a naked local-part followed by a name-addr. Report
		// it as the display-name defect it is rather than as a domainless address, and let
		// the remainder of the display-name be scanned as the next address. The comma is
		// deliberately left for the caller to consume as the list separator.
		av_log (v, GMIME_COMPLIANCE_UNQUOTED_DISPLAY_NAME, start);

		return TRUE;
	}

	v->index = start;
	av_validate_addrspec (v, FALSE, hasAt);

	return TRUE;
}

/*
 * Determine whether the given range contains an unquoted '@'.
 */
static gboolean
av_contains_at (AddressValidator *v, gssize start, gssize end)
{
	if (v->phrase_simple && start >= v->phrase_from && end == v->phrase_end) {
		if (v->phrase_at == -2 || (v->phrase_at >= 0 && v->phrase_at < start)) {
			const guint8 *at = memchr (v->text + start, '@', (gsize) (end - start));
			v->phrase_at = at ? at - v->text : -1;
		}

		return v->phrase_at >= 0;
	}

	gssize *unquotedAts = v->unquoted_ats;

	if (unquotedAts != NULL) {
		gssize at = unquotedAts[start - v->start_index];

		return at >= 0 && at < end;
	}

	for (gssize i = start; i < end; i++) {
		if (v->text[i] == '"') {
			// Skip over the quoted-string; an '@' inside it is part of the local-part.
			for (i++; i < end; i++) {
				if (v->text[i] == '\\')
					i++;
				else if (v->text[i] == '"')
					break;
			}
		} else if (v->text[i] == '@') {
			return TRUE;
		}
	}

	return FALSE;
}

/*
 * Determine whether a phrase used as a display-name is itself shaped like an addr-spec.
 * This looks inside quoted-strings, because "admin@example.com" <attacker@example.org>
 * is the whole point: the quoting makes the value legal, not innocuous. Comments are skipped,
 * and an '@' only counts when it has atom text immediately before it and a dotted domain
 * immediately after it, so that phrases such as Bob @ Work and @channel are not
 * mistaken for addresses.
 * true if the phrase is shaped like an addr-spec; otherwise, false.
 * start: The index of the first byte of the phrase.
 * end: The index just past the last byte of the phrase.
 * quotedAddrspec:
 * true if the addr-spec was inside a quoted-string, making the phrase legal
 * rfc5322; otherwise, false.
 */
static gboolean
av_contains_addrspec (AddressValidator *v, gssize start, gssize end, gboolean *quotedAddrspec)
{
	gboolean precededByAtom = FALSE;
	gboolean quoted = FALSE;
	gssize i = start;

	(*quotedAddrspec) = FALSE;

	while (i < end) {
		guint8 c = v->text[i];

		if (c == '"') {
			quoted = !quoted;
			precededByAtom = FALSE;
			i++;
			continue;
		}

		if (c == '(' && !quoted) {
			gssize depth = 1;

			for (i++; i < end && depth > 0; i++) {
				if (v->text[i] == '\\')
					i++;
				else if (v->text[i] == '(')
					depth++;
				else if (v->text[i] == ')')
					depth--;
			}

			precededByAtom = FALSE;
			continue;
		}

		if (c == '\\' && i + 1 < end)
			c = v->text[++i];

		if (c == '@' && precededByAtom && av_is_dotted_domain (v, i + 1, end)) {
			(*quotedAddrspec) = quoted;
			return TRUE;
		}

		precededByAtom = is_atom (c);
		i++;
	}

	return FALSE;
}

/*
 * Determine whether a dot-atom containing at least one dot begins at the given offset.
 * Requiring a dot keeps a local-part followed by a bare word, such as user@work, from
 * being read as an address. That is deliberately conservative: a display-name is free-form
 * text, so the cost of a false positive is higher than the cost of missing a domain that no
 * public mail system would route to anyway.
 */
static gboolean
av_is_dotted_domain (AddressValidator *v, gssize i, gssize end)
{
	gboolean atom = FALSE, dot = FALSE, atomAfterDot = FALSE;

	while (i < end) {
		guint8 c = v->text[i];

		if (is_atom (c)) {
			atom = TRUE;

			if (dot)
				atomAfterDot = TRUE;
		} else if (c == '.') {
			if (!atom)
				return FALSE;

			dot = TRUE;
		} else {
			break;
		}

		i++;
	}

	return atom && dot && atomAfterDot;
}

/*
 * Determine whether the next address in the list is a name-addr.
 * This is the one place the validator looks ahead. It exists solely to tell an unquoted
 * comma in a display-name apart from a genuine list of domainless addresses, which are
 * otherwise the same token sequence.
 */
static gboolean
av_followed_by_name_addr (AddressValidator *v, gssize start)
{
	if (start == v->name_addr_from)
		return v->name_addr_follows;

	v->name_addr_from = start;
	v->name_addr_follows = FALSE;

	for (gssize i = start; i < v->end_index; i++) {
		guint8 c = v->text[i];

		if (c == '"') {
			for (i++; i < v->end_index; i++) {
				if (v->text[i] == '\\')
					i++;
				else if (v->text[i] == '"')
					break;
			}
		} else if (c == '<') {
			v->name_addr_follows = TRUE;
			break;
		} else if (c == ',' || c == ';' || c == ':') {
			// The end of the next address without an angle-addr in it.
			break;
		}
	}

	return v->name_addr_follows;
}

/*
 * Validate the address-list that the validator has been positioned over.
 * This is split out from Validate so that the caller can always run the
 * post-parse passes, no matter which of the early returns below ends the parse.
 */
static void
av_validate_address_list (AddressValidator *v)
{
	gboolean any = FALSE;

	do {
		if (!av_skip_cfws (v))
			return;

		if (v->index >= v->end_index)
			break;

		if (v->text[v->index] == ',') {
			// A comma with no address in front of it.
			av_log (v, GMIME_COMPLIANCE_EXTRANEOUS_COMMA_IN_ADDRESS_LIST, v->index);
			v->index++;
			continue;
		}

		if (v->text[v->index] == ';') {
			// A group terminator with no group to terminate.
			av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, v->index);
			v->index++;
			continue;
		}

		gssize before = v->index;

		v->damaged_local_part = FALSE;

		if (!av_validate_address (v, FALSE))
			return;

		any = TRUE;

		if (v->index == before) {
			// Defensive: never spin on a character no production consumed.
			av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, v->index);
			v->index++;
			continue;
		}

		if (!av_skip_cfws (v))
			return;

		if (v->index >= v->end_index)
			break;

		if (v->text[v->index] != ',') {
			if (v->damaged_local_part) {
				// The offending character has been reported already. Step over it if it is
				// a list separator, which SkipToNextPossibleAddress halts on rather than
				// skips past.
				if (av_is_address_list_separator (v, v->text[v->index]))
					v->index++;
			} else if (v->text[v->index] == ';') {
				// A ';' does not separate the elements of an address-list, it terminates a
				// group, and there is no group open here. Reporting a missing comma would
				// describe the same byte a second time and in the wrong terms: nothing is
				// missing, something extra is present. The top of the loop has a report for
				// exactly that and consumes the byte, so leave it to do both.
				continue;
			} else {
				av_log_missing_address_separator (v, before);
			}

			av_skip_to_next_possible_address (v);
			continue;
		}

		// skip over the ','
		v->index++;

		// A trailing comma is an empty final element.
		gssize afterComma = v->index;

		if (!av_skip_cfws (v))
			return;

		if (v->index >= v->end_index)
			av_log (v, GMIME_COMPLIANCE_EXTRANEOUS_COMMA_IN_ADDRESS_LIST, afterComma);
	} while (TRUE);

	if (!any)
		av_log (v, GMIME_COMPLIANCE_NON_CONFORMANT_ADDRESS, v->start_index);
}

void
_g_mime_address_validate (const guint8 *buffer, gsize length, GMimeComplianceIssueFunc report,
			 gpointer user_data, GMimeComplianceContext context, gint64 stream_offset,
			 guint64 line_number, guint64 column_number)
{
	AddressValidator state = { 0 };
	AddressValidator *v = &state;

	g_return_if_fail (buffer != NULL || length == 0);
	g_return_if_fail (length <= G_MAXSSIZE);
	if (!report || !length)
		return;

	v->text = buffer;
	v->report = report;
	v->user_data = user_data;
	v->context = context;
	v->stream_offset = stream_offset;
	v->line_number = line_number;
	v->column_number = column_number;
	v->end_index = (gssize) length;
	v->iso2022_cursor = -1;
	v->null_cursor = -1;
	v->scan_line = line_number;
	v->scan_line_begin = -1;
	v->phrase_from = -1;
	v->phrase_end = -1;
	v->phrase_at = -2;
	v->name_addr_from = -1;

	// Note: The raw value still carries the line terminator that ended the header (and any
	// trailing folding whitespace). That is not part of the address list, and leaving it in
	// makes a trailing token look as though it were followed by folding whitespace.
	while (v->end_index > 0 && is_whitespace (buffer[v->end_index - 1]))
		v->end_index--;
	if (v->end_index == 0)
		return;

	av_scan_for_control_characters (v);

	// Note: rfc6532 extends the address grammar to UTF-8 and to nothing else, so any 8-bit
	// content that is not valid UTF-8 has no defined interpretation.
	if (!valid_utf8 (buffer, length))
		av_log (v, GMIME_COMPLIANCE_INVALID_8BIT_ADDRESS, 0);

	av_validate_address_list (v);

	// Note: These have to run after the parse, because until then there is no telling which of
	// the recorded ISO-2022 sequences landed in a local-part, or which null bytes landed in a
	// display-name.
	av_report_unattributed_iso2022_sequences (v);
	av_report_unattributed_null_bytes (v);

	if (v->line_starts)
		g_array_free (v->line_starts, TRUE);

	g_free (v->phrase_ends);
	g_free (v->unquoted_ats);
}
