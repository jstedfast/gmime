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

#include "gmime-parameter-validator-private.h"
#include "gmime-table-private.h"

/* Ported from MimeKit's ParameterValidator: inspect raw declarations rather
 * than the decoded model, whose precedence rules hide competing values. */
typedef struct {
	GMimeComplianceIssue missing_segment_issue;
	GTree *segments;
	int highest_segment;
	gboolean extended, plain;
} ParameterState;

typedef struct {
	const guint8 *text;
	gsize length, index, scan_index;
	GMimeComplianceContext context;
	gint64 stream_offset;
	guint64 line_number, column_number;
} ParameterValidator;

static void
parameter_state_free (gpointer data)
{
	ParameterState *state = data;

	g_clear_pointer (&state->segments, g_tree_destroy);
	g_free (state);
}

static gint
compare_segments (gconstpointer a, gconstpointer b)
{
	int x = GPOINTER_TO_INT (a), y = GPOINTER_TO_INT (b);

	return (x > y) - (x < y);
}

static gboolean
parameter_state_add (ParameterState *state, int segment, gboolean encoded, gboolean allow_fallback)
{
	gboolean repeated;

	if (segment >= 0) {
		gint count;

		repeated = FALSE;
		if (!state->segments) {
			/* Balanced insertion avoids adversarial integer hash collisions. */
			state->segments = g_tree_new (compare_segments);
			repeated = state->extended || (state->plain && !allow_fallback);
		}

		state->highest_segment = MAX (state->highest_segment, segment);
		count = g_tree_nnodes (state->segments);
		g_tree_insert (state->segments, GINT_TO_POINTER (segment), NULL);

		return count == g_tree_nnodes (state->segments) || repeated;
	}

	if (encoded) {
		repeated = state->extended || state->segments || (state->plain && !allow_fallback);
		state->extended = TRUE;
	} else {
		repeated = state->plain || (!allow_fallback && (state->extended || state->segments));
		state->plain = TRUE;
	}

	return repeated;
}

static GMimeComplianceIssue
create_issue (ParameterValidator *v, GMimeComplianceViolation violation, gsize at,
	      GMimeCompliancePositionKind kind)
{
	GMimeComplianceIssue issue;

	/* Capture positions monotonically, even when reporting is deferred. */
	while (v->scan_index < at) {
		if (v->text[v->scan_index++] == '\n') {
			v->line_number++;
			v->column_number = 1;
		} else {
			v->column_number++;
		}
	}

	issue.context = v->context;
	issue.violation = violation;
	issue.stream_offset = v->stream_offset + at;
	issue.line_number = v->line_number;
	issue.column_number = v->column_number;
	issue.position_kind = kind;

	return issue;
}

static gboolean
skip_comment (ParameterValidator *v)
{
	gsize depth = 1;
	guint8 c;

	v->index++;

	while (v->index < v->length) {
		c = v->text[v->index++];

		if (c == '\\') {
			if (v->index < v->length)
				v->index++;
		} else if (c == '(') {
			depth++;
		} else if (c == ')' && --depth == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
skip_quoted (ParameterValidator *v)
{
	guint8 c;

	v->index++;

	while (v->index < v->length) {
		c = v->text[v->index++];

		if (c == '\\') {
			if (v->index < v->length)
				v->index++;
		} else if (c == '"') {
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
skip_cfws (ParameterValidator *v)
{
	while (v->index < v->length) {
		if (is_lwsp (v->text[v->index])) {
			v->index++;
		} else if (v->text[v->index] == '(') {
			if (!skip_comment (v))
				return FALSE;
		} else {
			break;
		}
	}

	return TRUE;
}

static gboolean
skip_to_next_parameter (ParameterValidator *v)
{
	while (v->index < v->length) {
		switch (v->text[v->index]) {
		case ';':
			v->index++;
			return TRUE;
		case '"':
			if (!skip_quoted (v))
				return FALSE;
			break;
		case '(':
			if (!skip_comment (v))
				return FALSE;
			break;
		default:
			v->index++;
			break;
		}
	}

	return FALSE;
}

static gboolean
parse_parameter (ParameterValidator *v, char **name, int *segment, gboolean *encoded, gsize *start)
{
	gsize length;
	int number = 0, digit;

	*segment = -1;
	*encoded = FALSE;

	*start = v->index;
	while (v->index < v->length && is_attrchar (v->text[v->index]))
		v->index++;

	length = v->index - *start;
	if (!length || !skip_cfws (v) || v->index == v->length)
		return FALSE;

	if (v->text[v->index] == '*') {
		v->index++;

		if (!skip_cfws (v) || v->index == v->length)
			return FALSE;

		if (g_ascii_isdigit (v->text[v->index])) {
			do {
				digit = v->text[v->index] - '0';
				if (number > (G_MAXINT32 - digit) / 10)
					return FALSE;

				number = number * 10 + digit;
				v->index++;
			} while (v->index < v->length && g_ascii_isdigit (v->text[v->index]));

			*segment = number;

			if (!skip_cfws (v) || v->index == v->length)
				return FALSE;

			if (v->text[v->index] == '*') {
				v->index++;
				*encoded = TRUE;
			}
		} else {
			*encoded = TRUE;
		}

		if (!skip_cfws (v) || v->index == v->length)
			return FALSE;
	}

	if (v->text[v->index] != '=')
		return FALSE;

	v->index++;

	*name = g_ascii_strdown ((const char *) v->text + *start, length);

	return TRUE;
}

static gboolean
skip_value (ParameterValidator *v, gboolean *invalid)
{
	*invalid = TRUE;

	if (!skip_cfws (v) || v->index == v->length)
		return FALSE;

	if (v->text[v->index] == '"') {
		if (!skip_quoted (v))
			return FALSE;

		*invalid = FALSE;
	} else {
		gsize start = v->index;

		while (v->index < v->length && v->text[v->index] != ';' && v->text[v->index] != '(')
			v->index++;

		*invalid = v->index == start;
	}

	if (!skip_cfws (v)) {
		*invalid = TRUE;
		return FALSE;
	}

	if (v->index == v->length)
		return FALSE;

	/* Loose parsing permits missing separators after quotes and comments. */
	if (v->text[v->index] == ';')
		v->index++;

	return v->index < v->length;
}

void
_g_mime_parameter_validate (const guint8 *buffer, gsize length, gboolean content_type,
			    GMimeComplianceIssueFunc report, gpointer user_data, GMimeComplianceContext context,
			    gint64 stream_offset, guint64 line_number, guint64 column_number)
{
	ParameterValidator v = { buffer, length, 0, 0, context, stream_offset, line_number, column_number };
	GHashTable *parameters;
	GPtrArray *states;
	ParameterState *state;
	gsize start;
	gboolean encoded, filename;
	char *name;
	int segment;
	guint i;

	g_return_if_fail (buffer != NULL || length == 0);
	g_return_if_fail (length <= G_MAXSSIZE);

	if (!report || !length || !skip_to_next_parameter (&v))
		return;

	/* The reader's header-length limit bounds the declarations retained here. */
	parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, parameter_state_free);
	states = g_ptr_array_new ();

	while (v.index < v.length) {
		GMimeComplianceIssue issue;
		gboolean invalid, more;

		while (v.index < v.length && is_lwsp (v.text[v.index]))
			v.index++;

		start = v.index;

		if (!skip_cfws (&v)) {
			issue = create_issue (&v, GMIME_COMPLIANCE_INVALID_PARAMETER, start,
					      GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			report (&issue, user_data);
			break;
		}

		if (v.index == v.length)
			break;

		/* Empty parameter slots are also ignored by the tolerant decoder. */
		if (v.text[v.index] == ';') {
			v.index++;
			continue;
		}

		if (!parse_parameter (&v, &name, &segment, &encoded, &start)) {
			issue = create_issue (&v, GMIME_COMPLIANCE_INVALID_PARAMETER, start,
					      GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			report (&issue, user_data);

			if (!skip_to_next_parameter (&v))
				break;

			continue;
		}

		filename = !strcmp (name, content_type ? "name" : "filename");
		state = g_hash_table_lookup (parameters, name);

		if (!state) {
			state = g_new0 (ParameterState, 1);
			state->highest_segment = -1;

			g_hash_table_insert (parameters, g_strdup (name), state);
			g_ptr_array_add (states, state);
		}

		if (segment >= 0 && !state->segments)
			state->missing_segment_issue = create_issue (&v, GMIME_COMPLIANCE_MISSING_PARAMETER_SEGMENT,
								    start, GMIME_COMPLIANCE_POSITION_ELEMENT_START);

		if (parameter_state_add (state, segment, encoded, filename)) {
			GMimeComplianceViolation violation = content_type && !strcmp (name, "boundary") ?
				GMIME_COMPLIANCE_REPEATED_BOUNDARY_PARAMETER : filename ?
				GMIME_COMPLIANCE_REPEATED_FILENAME_PARAMETER : GMIME_COMPLIANCE_REPEATED_PARAMETER;

			issue = create_issue (&v, violation, start, GMIME_COMPLIANCE_POSITION_EXACT);
			report (&issue, user_data);
		}

		g_free (name);

		more = skip_value (&v, &invalid);

		if (invalid) {
			issue = create_issue (&v, GMIME_COMPLIANCE_INVALID_PARAMETER, start,
					      GMIME_COMPLIANCE_POSITION_ELEMENT_START);
			report (&issue, user_data);
		}

		if (!more)
			break;
	}

	/* Preserve parameter declaration order when applying reporting budgets. */
	for (i = 0; i < states->len; i++) {
		state = g_ptr_array_index (states, i);

		/* Distinct nonnegative indices cover 0..n-1 iff their maximum is n-1. */
		if (state->segments && state->highest_segment != g_tree_nnodes (state->segments) - 1)
			report (&state->missing_segment_issue, user_data);
	}

	g_ptr_array_unref (states);
	g_hash_table_destroy (parameters);
}
