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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-url-scanner.h"
#include "gmime-reader-simd-private.h"

/**
 * SECTION: gmime-url-scanner
 * @title: URL scanning
 * @short_description: Find URLs and email addresses in UTF-8 text
 *
 * The stateless URL scanner recognizes explicit URI schemes, bare www. and
 * ftp. hostnames, and email addresses. Matches use UTF-8 byte offsets and
 * include a prefix for constructing link targets. This is the same scanner
 * used by #GMimeTextToHtml and #GMimeFlowedToHtml.
 *
 * Scanning detects links in prose; it does not validate arbitrary URIs or
 * determine whether a link is safe to open. Applications must apply their own
 * scheme policy and escape link targets and visible text when generating HTML.
 **/

typedef enum {
	ADDRESS,
	MAILTO,
	FILE_URL,
	WEB
} PatternType;

typedef struct {
	const char *text, *prefix;
	PatternType type;
	guint anchor;
} Pattern;

/* Anchor order gives leftmost-longest matching without rescanning the input. */
static const Pattern patterns[] = { { "mailto:", "", MAILTO, 6 }, { "telnet://", "", WEB, 6 },
				    { "webcal://", "", WEB, 6 },  { "https://", "", WEB, 5 },
				    { "callto:", "", WEB, 6 },    { "file://", "", FILE_URL, 4 },
				    { "sftp://", "", WEB, 4 },    { "http://", "", WEB, 4 },
				    { "news://", "", WEB, 4 },    { "nntp://", "", WEB, 4 },
				    { "h323:", "", WEB, 4 },      { "ftp://", "", WEB, 3 },
				    { "sip:", "", WEB, 3 },       { "www.", "http://", WEB, 3 },
				    { "ftp.", "ftp://", WEB, 3 }, { "@", "mailto:", ADDRESS, 0 } };

static gsize
next (const char *s, gsize i)
{
	return g_utf8_next_char (s + i) - s;
}

static gsize
prev (const char *s, gsize i)
{
	return g_utf8_find_prev_char (s, s + i) - s;
}

static gsize
forward_limit (const char *s, gsize start, gsize end, guint count)
{
	while (start < end && count--)
		start = next (s, start);

	return start;
}

static gsize
backward_limit (const char *s, gsize start, gsize end, guint count)
{
	while (end > start && count--)
		end = prev (s, end);

	return end;
}

/* Whitespace, controls and invisible formatting characters never belong to
 * URLs: they can otherwise disguise the actual link target. */
static gboolean
nonascii (gunichar c)
{
	return !g_unichar_isspace (c) && !g_unichar_iscntrl (c) && g_unichar_type (c) != G_UNICODE_FORMAT;
}

static gboolean
atom (gunichar c)
{
	return c >= 128 ? nonascii (c) : g_ascii_isalnum (c) || (c && strchr ("!#$%&'*+-/=?^_`{|}~", c));
}

static gboolean
domain (gunichar c)
{
	return c >= 128 ? nonascii (c) : g_ascii_isalnum (c) || c == '-';
}

static gboolean
urlsafe (gunichar c)
{
	return c >= 128 ? nonascii (c) : g_ascii_isalnum (c) || (c && strchr ("$-_.+!*'(),{}|\\^~[]`#%\";/?:@&=", c));
}

static gboolean
subdomain_start (const char *s, gsize i)
{
	return s[i] != '-' && domain (g_utf8_get_char (s + i));
}

static gboolean
skip_subdomain (const char *s, gsize end, gsize *index)
{
	guint count = 1;

	*index = next (s, *index);
	/* Scan one character beyond the limit to detect overlong labels. Native
	 * character counts use Unicode scalars rather than UTF-16 code units. */
	while (*index < end && count <= 63 && domain (g_utf8_get_char (s + *index))) {
		*index = next (s, *index);
		count++;
	}

	return count <= 63;
}

static gboolean
skip_domain (const char *s, gsize end, gsize start, gsize *index)
{
	gsize limit = forward_limit (s, start, end, 256);

	if (*index >= limit || !subdomain_start (s, *index) || !skip_subdomain (s, limit, index))
		return FALSE;

	while (*index < limit && s[*index] == '.') {
		gsize dot = (*index)++;

		if (*index == limit) {
			if (*index < end && subdomain_start (s, *index))
				return FALSE;

			*index = dot;
			break;
		}

		if (!subdomain_start (s, *index)) {
			*index = dot;
			break;
		}

		if (!skip_subdomain (s, limit, index))
			return FALSE;
	}

	return g_utf8_strlen (s + start, *index - start) <= 255;
}

static gboolean
word (const char *s, gsize end, gsize *index)
{
	gsize start = *index;

	if (s[*index] == '"') {
		gboolean escaped = FALSE;

		(*index)++;
		while (*index < end) {
			if (s[*index] == '\\')
				escaped = !escaped;
			else if (!escaped && s[*index] == '"') {
				(*index)++;
				return TRUE;
			} else
				escaped = FALSE;
			*index = next (s, *index);
		}

		return FALSE;
	}

	while (*index < end && atom (g_utf8_get_char (s + *index)))
		*index = next (s, *index);

	return *index > start;
}

static gboolean
word_back (const char *s, gsize start, gsize *index)
{
	if (s[*index] == '"') {
		while (*index > start) {
			*index = prev (s, *index);
			if (*index < start)
				return FALSE;

			if (s[*index] == '"' && (*index == start || s[*index - 1] != '\\'))
				return TRUE;
		}

		return FALSE;
	}

	if (!atom (g_utf8_get_char (s + *index)))
		return FALSE;

	while (*index > start) {
		gsize p = prev (s, *index);

		if (p < start || !atom (g_utf8_get_char (s + p)))
			break;

		*index = p;
	}

	return TRUE;
}

static gboolean
ipv4 (const char *s, gsize end, gsize *index, gboolean leading_zeros)
{
	for (guint group = 0; group < 4; group++) {
		gsize start = *index;
		guint value = 0;

		while (*index < end && *index - start < 4 && g_ascii_isdigit (s[*index]))
			value = value * 10 + (s[(*index)++] - '0');

		if (*index == start || *index - start > 3 || value > 255 ||
		    (!leading_zeros && *index - start > 1 && s[start] == '0'))
			return FALSE;

		if (group < 3 && (*index >= end || s[(*index)++] != '.'))
			return FALSE;
	}

	return TRUE;
}

static gboolean
ipv6 (const char *s, gsize end, gsize *index, gboolean smtp)
{
	gboolean compact = FALSE;
	guint groups = 0;

	if (*index < end && s[*index] == ':') {
		if (end - *index < 2 || s[*index + 1] != ':')
			return FALSE;

		*index += 2;
		compact = TRUE;
	}

	while (*index < end && s[*index] != ']') {
		gsize start = *index;

		if (groups == 8)
			return FALSE;

		while (*index < end && *index - start < 5 && g_ascii_isxdigit (s[*index]))
			(*index)++;

		if (*index == start || *index - start > 4)
			return FALSE;

		if (*index < end && s[*index] == '.') {
			*index = start;
			if (groups > 6 || !ipv4 (s, end, index, smtp))
				return FALSE;

			groups += 2;
			break;
		}

		groups++;
		if (*index >= end || s[*index] != ':')
			break;

		(*index)++;
		if (*index < end && s[*index] == ':') {
			if (compact)
				return FALSE;

			compact = TRUE;
			(*index)++;
		} else if (*index >= end || !g_ascii_isxdigit (s[*index])) {
			return FALSE;
		}
	}

	/* SMTP requires :: to replace at least two groups; URI hosts allow one. */
	return compact ? groups <= (smtp ? 6 : 7) : groups == 8;
}

static gboolean
address_domain (const char *s, gsize end, gsize *index, gboolean qualified)
{
	gsize start = *index;

	if (*index >= end)
		return FALSE;

	if (s[*index] == '[') {
		(*index)++;

		if (end - *index < 8)
			return FALSE;

		if (!g_ascii_strncasecmp (s + *index, "IPv6:", 5)) {
			*index += 5;

			if (!ipv6 (s, end, index, TRUE))
				return FALSE;
		} else if (!ipv4 (s, end, index, TRUE)) {
			return FALSE;
		}

		return *index < end && s[(*index)++] == ']';
	}

	if (!skip_domain (s, end, start, index))
		return FALSE;

	if (qualified) {
		gboolean letters = TRUE, valid;
		char *label, *unicode, *ascii;
		gsize tld = *index;

		while (tld > start && s[tld - 1] != '.') {
			tld = prev (s, tld);
			if (!g_unichar_isalpha (g_utf8_get_char (s + tld)))
				letters = FALSE;
		}

		if (tld == start || g_utf8_strlen (s + tld, *index - tld) < 2)
			return FALSE;

		if (letters)
			return TRUE;

		if (*index - tld <= 4 || g_ascii_strncasecmp (s + tld, "xn--", 4))
			return FALSE;

		label = g_strndup (s + tld, *index - tld);
		unicode = g_hostname_to_unicode (label);
		ascii = unicode ? g_hostname_to_ascii (unicode) : NULL;
		valid = unicode && ascii && g_ascii_strcasecmp (unicode, label) && !g_ascii_strcasecmp (ascii, label);

		g_free (label);
		g_free (unicode);
		g_free (ascii);

		return valid;
	}

	return TRUE;
}

static gboolean
addrspec (const char *s, gsize end, gsize *index)
{
	gsize start = *index, limit = forward_limit (s, start, end, 65);

	if (*index >= limit || !word (s, limit, index) || *index >= limit)
		return FALSE;

	while (s[*index] == '.') {
		if (++(*index) >= limit || !word (s, limit, index) || *index >= limit)
			return FALSE;
	}

	if (g_utf8_strlen (s + start, *index - start) > 64 || *index + 1 >= end || s[(*index)++] != '@')
		return FALSE;

	return address_domain (s, end, index, FALSE);
}

static char
closing (const char *s, gsize start, gsize index)
{
	if (index == start)
		return 0;

	switch (s[index - 1]) {
	case '(':
		return ')';
	case '{':
		return '}';
	case '[':
		return ']';
	case '<':
		return '>';
	case '|':
		return '|';
	default:
		return 0;
	}
}

static gboolean
enclosing (char c, char close, guint *depth)
{
	if (close && c == close) {
		if (!*depth)
			return TRUE;

		(*depth)--;
	} else if ((c == '(' && close == ')') || (c == '[' && close == ']') || (c == '{' && close == '}')) {
		(*depth)++;
	}

	return FALSE;
}

static void
suffix (const char *s, gsize end, char close, gsize *index)
{
	guint depth = 0;

	while (*index < end && urlsafe (g_utf8_get_char (s + *index)) && !enclosing (s[*index], close, &depth))
		*index = next (s, *index);
}

static void
trim (const char *s, gsize start, gsize *index)
{
	gint parens = 0, brackets = 0, braces = 0;

	for (gsize i = start; i < *index; i++) {
		switch (s[i]) {
		case '(':
			parens++;
			break;
		case ')':
			parens--;
			break;
		case '[':
			brackets++;
			break;
		case ']':
			brackets--;
			break;
		case '{':
			braces++;
			break;
		case '}':
			braces--;
			break;
		}
	}

	/* Balanced inner pairs belong to the URL; unbalanced closers and prose
	 * punctuation do not. */
	while (*index > start) {
		char c = s[*index - 1];

		if (strchr (".,:;!?'\"*", c))
			(*index)--;
		else if (c == ')' && parens < 0) {
			parens++;
			(*index)--;
		} else if (c == ']' && brackets < 0) {
			brackets++;
			(*index)--;
		} else if (c == '}' && braces < 0) {
			braces++;
			(*index)--;
		} else
			break;
	}
}

static void
userinfo (const char *s, gsize end, char close, gsize *index)
{
	gsize cursor = *index;
	guint depth = 0;

	while (cursor < end) {
		gunichar c = g_utf8_get_char (s + cursor);

		if (c == '@') {
			*index = cursor + 1;
			return;
		}

		if (enclosing (s[cursor], close, &depth))
			return;

		if (c == '%') {
			if (end - cursor < 3 || !g_ascii_isxdigit (s[cursor + 1]) || !g_ascii_isxdigit (s[cursor + 2]))
				return;

			cursor += 3;
		} else if ((c >= 128 && nonascii (c)) ||
			   (c < 128 && (g_ascii_isalnum (c) || strchr ("-._~!$&'()*+,;=:", c)))) {
			cursor = next (s, cursor);
		} else
			return;
	}
}

static gboolean
try_match (const Pattern *pattern, const char *s, gsize start, gsize at, gsize end, GMimeUrlMatch *match)
{
	gsize n = strlen (pattern->text), i = at + n;
	char close = closing (s, start, at);

	match->start = at;
	match->prefix = pattern->prefix;
	if (pattern->type == ADDRESS) {
		gsize minimum = backward_limit (s, start, at, 65), local;

		if (at == start)
			return FALSE;

		local = prev (s, at);
		do {
			if (!word_back (s, minimum, &local))
				return FALSE;

			if (local == minimum || s[local - 1] != '.')
				break;

			if (local - minimum < 2)
				return FALSE;

			local = prev (s, local - 1);
		} while (TRUE);
		if (g_utf8_strlen (s + local, at - local) > 64)
			return FALSE;

		match->start = local;
		if (!address_domain (s, end, &i, TRUE))
			return FALSE;
	} else if (pattern->type == FILE_URL) {
		suffix (s, end, close, &i);
		trim (s, at + n, &i);
		if (i == at + n)
			return FALSE;
	} else if (pattern->type == MAILTO) {
		gsize content = i;

		if (!addrspec (s, end, &i))
			i = content;

		if (i < end && s[i] == '?') {
			gsize query = i;

			suffix (s, end, close, &i);
			trim (s, query, &i);
		}

		if (i == content)
			return FALSE;
	} else {
		gboolean authority = n >= 3 && !strcmp (pattern->text + n - 3, "://");
		gsize host;

		if (authority)
			userinfo (s, end, close, &i);

		host = pattern->text[n - 1] == '.' ? at : i;
		if (i == end)
			return FALSE;

		if (authority && s[i] == '[') {
			i++;
			if (!ipv6 (s, end, &i, FALSE) || i >= end || s[i++] != ']')
				return FALSE;
		} else if (!skip_domain (s, end, host, &i))
			return FALSE;

		if (i + 1 < end && s[i] == ':' && g_ascii_isdigit (s[i + 1])) {
			i += 2;
			while (i < end && g_ascii_isdigit (s[i]))
				i++;
		}

		if (i < end && (s[i] == '/' || s[i] == '?' || s[i] == '#')) {
			gsize path = i;

			suffix (s, end, close, &i);
			trim (s, path, &i);
		}
	}

	match->end = i;
	return TRUE;
}

/**
 * g_mime_url_scan:
 * @text: a UTF-8 text buffer
 * @start: the byte offset at which to begin scanning
 * @length: the number of bytes to scan, starting at @start
 * @match: (out caller-allocates): storage for the match
 *
 * Finds the next URL or email address in the specified range. The range must
 * contain valid UTF-8 and begin and end on character boundaries; UTF-8 is not
 * validated by this function. The buffer must contain at least @start +
 * @length bytes, but need not be NUL-terminated. Embedded NUL bytes act as
 * separators rather than terminating the scan.
 *
 * Match offsets are relative to @text, not @start, and the end offset is
 * exclusive. To find subsequent matches, resume at the previous match's end
 * offset and reduce @length accordingly. No bytes outside the specified range
 * are examined. On failure to find a match, @match is left unchanged.
 *
 * Recognized schemes are mailto:, telnet://, webcal://, https://, callto:,
 * file://, sftp://, http://, news://, nntp://, h323:, ftp://, and sip:.
 * Scheme matching is case-insensitive. Bare www. and ftp. names and email
 * addresses are also recognized. Surrounding punctuation is excluded when
 * possible; matching is intended for links in prose, not URI validation.
 *
 * Returns: %TRUE if a match was found, or %FALSE otherwise.
 **/
gboolean
g_mime_url_scan (const char *text, gsize start, gsize length, GMimeUrlMatch *match)
{
	/* IndexOfAny('@', ':', '.') from the source uses SIMD. Keep the equivalent
	 * UTF-8 byte search, then inspect only bounded candidates at each anchor. */
	static const GMimeReaderByteSet anchors = { { [0] = 1 << 4, [10] = 1 << 3, [14] = 1 << 2 }, FALSE };
	gsize end;
	gsize search = start;

	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (match != NULL, FALSE);
	g_return_val_if_fail (length <= G_MAXSIZE - start, FALSE);

	end = start + length;

	while (search < end) {
		gsize anchor =
			search + _g_mime_reader_find_byte ((const guint8 *) text + search, end - search, &anchors);

		if (anchor == end)
			break;

		search = anchor + 1;
		for (guint k = 0; k < G_N_ELEMENTS (patterns); k++) {
			const Pattern *pattern = patterns + k;
			GMimeUrlMatch candidate;
			gsize at, n = strlen (pattern->text);

			if (pattern->text[pattern->anchor] != text[anchor] || anchor - start < pattern->anchor)
				continue;

			at = anchor - pattern->anchor;
			if (end - at < n || g_ascii_strncasecmp (pattern->text, text + at, n))
				continue;

			if (try_match (pattern, text, start, at, end, &candidate)) {
				*match = candidate;
				return TRUE;
			}
		}
	}

	return FALSE;
}
