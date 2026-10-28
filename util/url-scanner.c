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

/* URL scanning adapted from MimeKit.
 *
 * Copyright (c) 2013-2026 .NET Foundation and Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "url-scanner.h"
#include "gmime/gmime-internet-address-private.h"

/* Match MimeKit's UTF-16 limits while retaining byte offsets in UTF-8 input. */
#define MAX_LOCAL_PART_LENGTH 64
#define MAX_DOMAIN_LENGTH 255
#define MAX_LABEL_LENGTH 63

typedef struct {
	urlpattern_t *url;
	size_t anchor;
	size_t length;
} AnchoredPattern;

struct _UrlScanner {
	GPtrArray *patterns;
};

UrlScanner *
url_scanner_new (void)
{
	UrlScanner *scanner = g_new (UrlScanner, 1);

	scanner->patterns = g_ptr_array_new_with_free_func (g_free);

	return scanner;
}

void
url_scanner_free (UrlScanner *scanner)
{
	g_return_if_fail (scanner != NULL);

	g_ptr_array_free (scanner->patterns, TRUE);
	g_free (scanner);
}

void
url_scanner_add (UrlScanner *scanner, urlpattern_t *pattern)
{
	AnchoredPattern *anchored, *other;
	const char *anchor;
	guint index;

	g_return_if_fail (scanner != NULL);
	g_return_if_fail (pattern != NULL);

	anchor = strpbrk (pattern->pattern, "@:.");
	g_return_if_fail (anchor != NULL && strpbrk (anchor + 1, "@:.") == NULL);

	anchored = g_new (AnchoredPattern, 1);
	anchored->url = pattern;
	anchored->anchor = anchor - pattern->pattern;
	anchored->length = strlen (pattern->pattern);

	for (index = 0; index < scanner->patterns->len; index++) {
		other = g_ptr_array_index (scanner->patterns, index);
		if (other->anchor < anchored->anchor ||
		    (other->anchor == anchored->anchor && other->length < anchored->length))
			break;
	}

	g_ptr_array_insert (scanner->patterns, index, anchored);
}

gboolean
url_scanner_scan (UrlScanner *scanner, const char *in, size_t inlen, urlmatch_t *match)
{
	const char *pos, *start, *inend;
	AnchoredPattern *anchored;
	urlpattern_t *pattern;
	guint i;

	g_return_val_if_fail (scanner != NULL, FALSE);
	g_return_val_if_fail (in != NULL, FALSE);
	g_return_val_if_fail (match != NULL, FALSE);

	inend = in + inlen;
	/* Each pattern has one anchor. Earlier anchors also imply earlier starts;
	 * sorting patterns by anchor offset makes equal-anchor matches leftmost-longest. */
	for (pos = in; pos < inend; pos++) {
		if (*pos != '@' && *pos != ':' && *pos != '.')
			continue;

		for (i = 0; i < scanner->patterns->len; i++) {
			anchored = g_ptr_array_index (scanner->patterns, i);
			pattern = anchored->url;
			if (pattern->pattern[anchored->anchor] != *pos ||
			    (size_t) (pos - in) < anchored->anchor ||
			    (size_t) (inend - pos) < anchored->length - anchored->anchor)
				continue;

			start = pos - anchored->anchor;
			if (g_ascii_strncasecmp (start, pattern->pattern, anchored->length))
				continue;

			match->pattern = pattern->pattern;
			match->prefix = pattern->prefix;
			if (pattern->start (in, start, inend, match) &&
			    pattern->end (in, start, inend, match))
				return TRUE;
		}
	}

	return FALSE;
}

static size_t
url_character_length (const char *pos, const char *end, const char *ascii)
{
	gunichar c;

	if (pos == end)
		return 0;

	if ((guchar) *pos < 128)
		return g_ascii_isalnum (*pos) || (*pos && strchr (ascii, *pos)) ? 1 : 0;

	c = g_utf8_get_char_validated (pos, end - pos);
	if (c == (gunichar) -1 || c == (gunichar) -2 ||
	    g_unichar_isspace (c) || g_unichar_iscntrl (c) || g_unichar_type (c) == G_UNICODE_FORMAT)
		return 0;

	return g_utf8_next_char (pos) - pos;
}

static gboolean
url_skip_domain (const char **position, const char *end, const char *start)
{
	const char *pos = *position;
	size_t length, label_length, domain_length = pos - start;

	do {
		label_length = 0;
		if (pos == end || *pos == '-' || !(length = url_character_length (pos, end, "-")))
			return FALSE;

		do {
			pos += length;
			label_length += length == 4 ? 2 : 1;
			domain_length += length == 4 ? 2 : 1;
			if (label_length > MAX_LABEL_LENGTH || domain_length > MAX_DOMAIN_LENGTH)
				return FALSE;
		} while ((length = url_character_length (pos, end, "-")));

		if (pos == end || *pos != '.')
			break;

		if (pos + 1 == end || pos[1] == '-' || !url_character_length (pos + 1, end, "-"))
			break;

		pos++;
		domain_length++;
	} while (TRUE);

	*position = pos;
	return TRUE;
}

static gboolean
url_skip_local_part (const char **position, const char *end)
{
	const char *pos = *position;
	gboolean escaped, closed;
	size_t length, local_length = 0;

	do {
		if (pos < end && *pos == '"') {
			escaped = closed = FALSE;
			pos++;
			local_length++;
			while (pos < end) {
				length = (guchar) *pos < 128 ? 1 : url_character_length (pos, end, "");
				if (!length || !*pos)
					return FALSE;
				local_length += length == 4 ? 2 : 1;
				if (local_length > MAX_LOCAL_PART_LENGTH)
					return FALSE;
				if (*pos == '\\') {
					escaped = !escaped;
				} else {
					if (*pos == '"' && !escaped) {
						pos += length;
						closed = TRUE;
						break;
					}
					escaped = FALSE;
				}
				pos++;
			}
			if (!closed)
				return FALSE;
		} else {
			if (!(length = url_character_length (pos, end, "!#$%&'*+-/=?^_`{|}~")))
				return FALSE;
			do {
				pos += length;
				local_length += length == 4 ? 2 : 1;
				if (local_length > MAX_LOCAL_PART_LENGTH)
					return FALSE;
			} while ((length = url_character_length (pos, end, "!#$%&'*+-/=?^_`{|}~")));
		}

		if (pos == end || *pos != '.')
			break;
		pos++;
		local_length++;
	} while (TRUE);

	*position = pos;
	return TRUE;
}

static gboolean
url_skip_ipv4 (const char **position, const char *end, gboolean smtp)
{
	const char *pos = *position, *start;
	int group, value;

	for (group = 0; group < 4; group++) {
		start = pos;
		value = 0;
		while (pos < end && g_ascii_isdigit (*pos) && pos - start < 3)
			value = value * 10 + (*pos++ - '0');

		if (pos == start || value > 255 || (!smtp && pos - start > 1 && *start == '0'))
			return FALSE;
		if (group < 3 && (pos == end || *pos++ != '.'))
			return FALSE;
	}

	*position = pos;
	return TRUE;
}

static gboolean
url_skip_ipv6 (const char **position, const char *end, gboolean smtp)
{
	const char *pos = *position, *start;
	gboolean compressed = FALSE;
	int groups = 0;

	if (pos < end && *pos == ':') {
		if (end - pos < 2 || pos[1] != ':')
			return FALSE;
		compressed = TRUE;
		pos += 2;
	}

	while (pos < end && *pos != ']') {
		start = pos;
		while (pos < end && g_ascii_isxdigit (*pos) && pos - start < 4)
			pos++;
		if (pos == start || ++groups > 8)
			return FALSE;

		if (pos < end && *pos == '.') {
			pos = start;
			if (++groups > 8 || !url_skip_ipv4 (&pos, end, smtp))
				return FALSE;
			break;
		}

		if (pos == end || *pos != ':')
			break;
		pos++;
		if (pos == end || *pos == ']')
			return FALSE;
		if (*pos == ':') {
			if (compressed)
				return FALSE;
			compressed = TRUE;
			pos++;
		}
	}

	if (compressed ? groups > (smtp ? 6 : 7) : groups != 8)
		return FALSE;

	*position = pos;
	return TRUE;
}

static gboolean
url_skip_literal (const char **position, const char *end, gboolean smtp)
{
	const char *pos = *position + 1;

	if (smtp && end - pos >= 5 && !g_ascii_strncasecmp (pos, "IPv6:", 5)) {
		pos += 5;
		if (!url_skip_ipv6 (&pos, end, TRUE))
			return FALSE;
	} else if (smtp) {
		if (!url_skip_ipv4 (&pos, end, TRUE))
			return FALSE;
	} else if (!url_skip_ipv6 (&pos, end, FALSE)) {
		return FALSE;
	}

	if (pos == end || *pos != ']')
		return FALSE;

	*position = pos + 1;
	return TRUE;
}

static gboolean
url_domain_is_qualified (const char *start, const char *end)
{
	const char *label = end, *pos;
	char *ascii, *unicode;
	gboolean valid;

	while (label > start && label[-1] != '.')
		label--;
	if (label == start || end - label < 2)
		return FALSE;

	if (end - label >= 5 && !g_ascii_strncasecmp (label, "xn--", 4)) {
		ascii = g_strndup (label, end - label);
		unicode = _g_mime_internet_address_decode_domain (ascii);
		valid = g_ascii_strcasecmp (ascii, unicode) != 0;
		g_free (unicode);
		g_free (ascii);
		return valid;
	}

	for (pos = label; pos < end; pos = g_utf8_next_char (pos)) {
		if (!g_unichar_isalpha (g_utf8_get_char (pos)))
			return FALSE;
	}

	return g_utf8_strlen (label, end - label) >= 2;
}

gboolean
url_addrspec_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *start = pos, *previous, *word;
	gboolean closed;
	size_t length, local_length = 0;

	do {
		if (start == in)
			return FALSE;

		word = start;
		if (start[-1] == '"') {
			start--;
			local_length++;
			closed = FALSE;
			while (start > in) {
				previous = g_utf8_find_prev_char (in, start);
				if (!previous)
					return FALSE;
				if ((guchar) *previous >= 128 && !url_character_length (previous, start, ""))
					return FALSE;
				length = start - previous;
				start = previous;
				local_length += length == 4 ? 2 : 1;
				if (local_length > MAX_LOCAL_PART_LENGTH || !*start)
					return FALSE;
				if (*start == '"' && (start == in || start[-1] != '\\')) {
					closed = TRUE;
					break;
				}
			}
			if (!closed)
				return FALSE;
		} else {
			while (start > in) {
				previous = g_utf8_find_prev_char (in, start);
				if (!previous || !(length = url_character_length (previous, start, "!#$%&'*+-/=?^_`{|}~")))
					break;
				start = previous;
				local_length += length == 4 ? 2 : 1;
				if (local_length > MAX_LOCAL_PART_LENGTH)
					return FALSE;
			}
			if (start == word)
				return FALSE;
		}

		if (start == in || start[-1] != '.')
			break;
		start--;
		if (++local_length > MAX_LOCAL_PART_LENGTH)
			return FALSE;
	} while (TRUE);

	match->um_so = start - in;
	return TRUE;
}

gboolean
url_addrspec_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *start = pos + 1, *end = start;

	if (start == inend)
		return FALSE;

	if (*start == '[') {
		if (!url_skip_literal (&end, inend, TRUE))
			return FALSE;
	} else if (!url_skip_domain (&end, inend, start) || !url_domain_is_qualified (start, end)) {
		return FALSE;
	}

	match->um_eo = end - in;
	return TRUE;
}

static const char *
url_trim_punctuation (const char *start, const char *end)
{
	gssize parentheses = 0, brackets = 0, braces = 0;
	const char *pos;

	for (pos = start; pos < end; pos++) {
		switch (*pos) {
		case '(': parentheses++; break;
		case ')': parentheses--; break;
		case '[': brackets++; break;
		case ']': brackets--; break;
		case '{': braces++; break;
		case '}': braces--; break;
		}
	}

	while (end > start) {
		if (end[-1] == ')' && parentheses < 0)
			parentheses++;
		else if (end[-1] == ']' && brackets < 0)
			brackets++;
		else if (end[-1] == '}' && braces < 0)
			braces++;
		else if (!strchr (".,:;!?'\"*", end[-1]))
			break;
		end--;
	}

	return end;
}

static const char *
url_skip_suffix (const char *in, const char *url, const char *pos, const char *end)
{
	const char *start = pos;
	char open = url > in ? url[-1] : '\0', close;
	size_t depth = 0, length;

	switch (open) {
	case '(': close = ')'; break;
	case '[': close = ']'; break;
	case '{': close = '}'; break;
	case '<': close = '>'; break;
	case '|': close = '|'; break;
	default: close = '\0'; break;
	}

	while ((length = url_character_length (pos, end, "$-_.+!*'(),{}|\\^~[]`#%\";/?:@&="))) {
		if (close) {
			if (*pos == close) {
				if (!depth)
					break;
				depth--;
			} else if (*pos == open) {
				depth++;
			}
		}
		pos += length;
	}

	return url_trim_punctuation (start, pos);
}

gboolean
url_file_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	match->um_so = pos - in;
	return TRUE;
}

gboolean
url_file_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *start = pos + strlen (match->pattern);
	const char *end = url_skip_suffix (in, pos, start, inend);

	if (end == start)
		return FALSE;

	match->um_eo = end - in;
	return TRUE;
}

gboolean
url_web_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	match->um_so = pos - in;
	return TRUE;
}

static gboolean
url_mailto_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *start = pos + strlen (match->pattern), *end = start;

	if (url_skip_local_part (&end, inend) && end < inend && *end == '@') {
		end++;
		if (end == inend)
			return FALSE;
		if (*end == '[') {
			if (!url_skip_literal (&end, inend, TRUE))
				return FALSE;
		} else if (!url_skip_domain (&end, inend, end)) {
			return FALSE;
		}
	} else {
		end = start;
	}

	if (end < inend && *end == '?')
		end = url_skip_suffix (in, pos, end, inend);

	if (end == start)
		return FALSE;

	match->um_eo = end - in;
	return TRUE;
}

static void
url_skip_userinfo (const char **position, const char *end)
{
	const char *pos = *position;
	size_t length;

	while (pos < end) {
		if (*pos == '@') {
			*position = pos + 1;
			return;
		}
		if (*pos == '%') {
			if (end - pos < 3 || !g_ascii_isxdigit (pos[1]) || !g_ascii_isxdigit (pos[2]))
				return;
			pos += 3;
		} else if ((length = url_character_length (pos, end, "-._~!$&'()*+,;=:"))) {
			pos += length;
		} else {
			return;
		}
	}
}

gboolean
url_web_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *end = pos + strlen (match->pattern), *host;

	if (!g_ascii_strcasecmp (match->pattern, "mailto:"))
		return url_mailto_end (in, pos, inend, match);

	if (g_str_has_suffix (match->pattern, "://"))
		url_skip_userinfo (&end, inend);

	if (end == inend)
		return FALSE;

	host = !g_ascii_strcasecmp (match->pattern, "www.") ||
		!g_ascii_strcasecmp (match->pattern, "ftp.") ? pos : end;

	if (*end == '[') {
		if (!url_skip_literal (&end, inend, FALSE))
			return FALSE;
	} else if (!url_skip_domain (&end, inend, host)) {
		return FALSE;
	}

	if (end < inend && *end == ':') {
		end++;
		while (end < inend && g_ascii_isdigit (*end))
			end++;
	}

	if (end < inend && (*end == '/' || *end == '?' || *end == '#'))
		end = url_skip_suffix (in, pos, end, inend);

	match->um_eo = end - in;
	return TRUE;
}
