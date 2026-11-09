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

#include "gmime-smtp-address-validator.h"
#include "gmime-smtp-unicode-private.h"

/**
 * SECTION: gmime-smtp-address-validator
 * @title: SMTP address validation
 * @short_description: Validate envelope mailbox addresses
 *
 * These validators accept a bare mailbox, not a MIME address header. SMTP
 * validates RFC 5321 syntax and ASCII; SMTPUTF8 additionally applies the
 * Unicode, NFC, contextual and script policies of MimeKit's implementation of
 * draft-ietf-mailmaint-smtputf8-syntax-06. Unicode 18.0.0 tables are pinned,
 * independently of the system GLib Unicode/IDNA version.
 **/

typedef struct {
	const char *domain;
	gboolean local_ace;
	gboolean domain_ace;
} SmtpSyntax;

static gboolean
has_ace_prefix (const char *start, const char *end)
{
	return end - start >= 4 && g_ascii_strncasecmp (start, "xn--", 4) == 0;
}

static gboolean
is_atext (unsigned char c)
{
	return c >= 128 || g_ascii_isalnum (c) || (c != 0 && strchr ("!#$%&'*+-/=?^_`{|}~", c));
}

static gboolean
skip_quoted (const char **in, const char *end, gboolean iso2022jp)
{
	const char *p = *in + 1;
	int mode = 0, pending = 0;
	unsigned char c;

	while (p < end) {
		c = *p++;

		if (c == '"') {
			*in = p;
			return mode == 0;
		}

		if (c == 27 && iso2022jp) {
			if (pending || end - p < 2)
				return FALSE;

			if (p[0] == '(' && p[1] == 'B')
				mode = 0;
			else if (p[0] == '(' && p[1] == 'J')
				mode = 1;
			else if (p[0] == '$' && (p[1] == '@' || p[1] == 'B'))
				mode = 2;
			else
				return FALSE;

			p += 2;
			continue;
		}

		if (c == '\\') {
			if (p == end || (unsigned char) *p < 32 || (unsigned char) *p > 126)
				return FALSE;
			c = *p++;
		} else if (c < 32 || c == 127) {
			return FALSE;
		}

		if (mode == 2) {
			if (c < 33 || c > 126)
				return FALSE;
			pending ^= 1;
		} else if (mode == 1 && c >= 128) {
			return FALSE;
		}
	}

	return FALSE;
}

static gboolean
is_ipv4 (const char *p, const char *end)
{
	const char *start;
	int i, value;

	for (i = 0; i < 4; i++) {
		if (i > 0) {
			if (p == end || *p++ != '.')
				return FALSE;
		}

		start = p;
		value = 0;

		while (p < end && p - start < 3 && g_ascii_isdigit (*p))
			value = value * 10 + (*p++ - '0');

		if (p == start || value > 255)
			return FALSE;
	}

	return p == end;
}

static gboolean
ipv6_groups (const char *p, const char *end, gboolean allow_ipv4, int *groups, gboolean *ipv4)
{
	const char *start;

	do {
		start = p;

		while (p < end && p - start < 4 && g_ascii_isxdigit (*p))
			p++;

		if (allow_ipv4 && p < end && *p == '.') {
			*ipv4 = TRUE;
			return is_ipv4 (start, end);
		}

		if (p == start)
			return FALSE;

		if (++(*groups) > 8)
			return FALSE;

		if (p == end)
			return TRUE;

		if (*p++ != ':')
			return FALSE;
	} while (p < end);

	return FALSE;
}

static gboolean
is_ipv6 (const char *p, const char *end)
{
	const char *compressed = NULL, *q;
	gboolean ipv4 = FALSE;
	int groups = 0;

	for (q = p; end - q >= 2; q++) {
		if (q[0] == ':' && q[1] == ':') {
			compressed = q;
			break;
		}
	}

	if (!compressed)
		return ipv6_groups (p, end, TRUE, &groups, &ipv4) && groups == (ipv4 ? 6 : 8);

	if (compressed + 2 < end && compressed[2] == ':')
		return FALSE;

	if (compressed > p && !ipv6_groups (p, compressed, FALSE, &groups, &ipv4))
		return FALSE;

	if (compressed + 2 < end && !ipv6_groups (compressed + 2, end, TRUE, &groups, &ipv4))
		return FALSE;

	/* RFC 5321 requires :: to replace at least two groups, unlike generic IPv6. */
	return groups <= (ipv4 ? 4 : 6);
}

static gboolean
is_literal (const char *p, const char *end)
{
	const char *colon, *q;

	if (end - p >= 5 && g_ascii_strncasecmp (p, "IPv6:", 5) == 0)
		return is_ipv6 (p + 5, end);

	if (!(colon = memchr (p, ':', end - p)))
		return is_ipv4 (p, end);

	if (colon == p || colon[-1] == '-' || colon + 1 == end)
		return FALSE;

	for (q = p; q < colon; q++) {
		if (*q != '-' && !g_ascii_isalnum (*q))
			return FALSE;
	}

	for (q = colon + 1; q < end; q++) {
		if ((unsigned char) *q < 33 || (unsigned char) *q > 126 || *q == '[' || *q == '\\')
			return FALSE;
	}

	return TRUE;
}

static gboolean
validate_syntax (const char *p, const char *end, gboolean iso2022jp, SmtpSyntax *syntax)
{
	const char *start;

	memset (syntax, 0, sizeof (*syntax));
	if (p < end && *p == '"') {
		if (!skip_quoted (&p, end, iso2022jp))
			return FALSE;
	} else {
		do {
			start = p;

			while (p < end && is_atext (*p))
				p++;

			if (p == start)
				return FALSE;

			syntax->local_ace |= has_ace_prefix (start, p);

			if (p == end || *p != '.')
				break;

			p++;
		} while (TRUE);
	}

	if (p == end || *p++ != '@' || p == end)
		return FALSE;

	syntax->domain = p;

	if (*p == '[') {
		start = ++p;

		while (p < end && *p != ']')
			p++;

		return p < end && p + 1 == end && is_literal (start, p);
	}

	do {
		start = p;

		while (p < end && ((unsigned char) *p >= 128 || *p == '-' || g_ascii_isalnum (*p)))
			p++;

		if (p == start || *start == '-' || p[-1] == '-')
			return FALSE;

		syntax->domain_ace |= has_ace_prefix (start, p);

		if (p == end || *p != '.')
			break;

		p++;
	} while (TRUE);

	return p == end;
}

static guint32
punycode_adapt (guint64 delta, guint count, gboolean first)
{
	guint32 k = 0;

	delta = first ? delta / 700 : delta / 2;
	delta += delta / count;

	while (delta > 455) {
		delta /= 35;
		k += 36;
	}

	return k + 36 * delta / (delta + 38);
}

static guint
punycode_threshold (guint k, guint bias)
{
	return k <= bias ? 1 : (k >= bias + 26 ? 26 : k - bias);
}

static char
punycode_digit (guint digit)
{
	return digit < 26 ? 'a' + digit : '0' + digit - 26;
}

/* Labels are bounded before either quadratic Bootstring operation. */
static int
punycode_encode (const gunichar *input, guint count, char output[63])
{
	guint bias = 72, basic = 0, h, j, k, t;
	gunichar n = 128, m;
	guint64 delta = 0, q;
	int length = 0;

	for (j = 0; j < count; j++) {
		if (input[j] < 128) {
			if (length == 63)
				return -1;

			output[length++] = input[j];
			basic++;
		}
	}

	if (basic) {
		if (length == 63)
			return -1;

		output[length++] = '-';
	}

	h = basic;

	while (h < count) {
		m = G_MAXUINT32;

		for (j = 0; j < count; j++) {
			if (input[j] >= n && input[j] < m)
				m = input[j];
		}

		delta += (guint64) (m - n) * (h + 1);
		n = m;

		for (j = 0; j < count; j++) {
			if (input[j] < n) {
				delta++;
			} else if (input[j] == n) {
				q = delta;

				for (k = 36; ; k += 36) {
					t = punycode_threshold (k, bias);

					if (q < t)
						break;

					if (length == 63)
						return -1;

					output[length++] = punycode_digit (t + (q - t) % (36 - t));
					q = (q - t) / (36 - t);
				}

				if (length == 63)
					return -1;

				output[length++] = punycode_digit (q);
				bias = punycode_adapt (delta, h + 1, h == basic);
				delta = 0;
				h++;
			}
		}

		delta++;
		n++;
	}

	return length;
}

static gboolean
punycode_decode (const char *p, const char *end, gunichar output[63], guint *count)
{
	const char *delimiter = NULL, *q;
	guint32 n = 128, i = 0, bias = 72, old, w, k, t, digit;
	unsigned char c;

	*count = 0;

	for (q = p; q < end; q++) {
		if (*q == '-')
			delimiter = q;
	}

	if (delimiter && delimiter > p) {
		while (p < delimiter) {
			if ((unsigned char) *p >= 128 || *count == 63)
				return FALSE;

			output[(*count)++] = *p++;
		}

		p++;
	}

	while (p < end) {
		old = i;
		w = 1;

		for (k = 36; ; k += 36) {
			if (p == end)
				return FALSE;

			c = *p++;
			if (g_ascii_isalpha (c))
				digit = g_ascii_tolower (c) - 'a';
			else if (g_ascii_isdigit (c))
				digit = c - '0' + 26;
			else
				return FALSE;

			if (digit > (G_MAXINT32 - i) / w)
				return FALSE;

			i += digit * w;
			t = punycode_threshold (k, bias);

			if (digit < t)
				break;

			if (w > G_MAXINT32 / (36 - t))
				return FALSE;

			w *= 36 - t;
		}

		bias = punycode_adapt (i - old, *count + 1, old == 0);

		if (i / (*count + 1) > 0x10ffff - n)
			return FALSE;

		n += i / (*count + 1);
		i %= *count + 1;

		if ((n >= 0xd800 && n <= 0xdfff) || *count == 63)
			return FALSE;

		memmove (output + i + 1, output + i, (*count - i) * sizeof (*output));
		output[i++] = n;
		(*count)++;
	}

	return TRUE;
}

static guint
alabel_length (const char *p, const char *end)
{
	gunichar label[63];
	guint count = 0, basic = 0;
	char encoded[63];
	int length;

	while (p < end) {
		if (count == G_N_ELEMENTS (label))
			return 64;

		label[count] = g_utf8_get_char (p);
		basic += label[count++] < 128;
		p = g_utf8_next_char (p);
	}

	if (basic == count)
		return count;

	if (4 + count + (basic > 0) > 63)
		return 64;

	length = punycode_encode (label, count, encoded);

	return length < 0 ? 64 : 4 + length;
}

static GMimeSmtpUtf8AddressError
validate_lengths (const char *address, const char *domain, const char *end)
{
	const char *p = domain, *start;
	gsize total = 0;
	guint length;

	if (domain - address - 1 > 64)
		return GMIME_SMTP_UTF8_ADDRESS_ERROR_LOCAL_PART_TOO_LONG;

	if (*domain != '[') {
		do {
			start = p;

			while (p < end && *p != '.')
				p++;

			length = alabel_length (start, p);
			if (length > 63)
				return GMIME_SMTP_UTF8_ADDRESS_ERROR_LABEL_TOO_LONG;

			/* Saturate without skipping later label errors, which take precedence. */
			total = MIN (total + length, 254);

			if (p < end) {
				total = MIN (total + 1, 254);
				p++;
			}
		} while (p < end);

		if (total > 253)
			return GMIME_SMTP_UTF8_ADDRESS_ERROR_DOMAIN_TOO_LONG;
	}

	if (end - address > 254)
		return GMIME_SMTP_UTF8_ADDRESS_ERROR_ADDRESS_TOO_LONG;

	return GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE;
}

static gboolean
append_decoded_label (GString *decoded, const char *start, const char *end)
{
	gunichar label[63];
	gboolean non_ascii = FALSE;
	char encoded[63];
	guint count, j;
	int length;

	if (end - start > 63 || !punycode_decode (start + 4, end, label, &count))
		return FALSE;

	for (j = 0; j < count; j++)
		non_ascii |= label[j] >= 128;

	if (!non_ascii || label[0] == '-' || label[count - 1] == '-' ||
	    (count >= 4 && label[2] == '-' && label[3] == '-'))
		return FALSE;

	length = punycode_encode (label, count, encoded);

	if (length < 0 || length != end - start - 4 || g_ascii_strncasecmp (encoded, start + 4, length))
		return FALSE;

	for (j = 0; j < count; j++)
		g_string_append_unichar (decoded, label[j]);

	return TRUE;
}

static guint32
lookup_range (const guint32 *table, guint count, guint shift, gunichar cp)
{
	guint low = 0, high = count - 1, mid;

	while (low < high) {
		mid = low + (high - low + 1) / 2;

		if ((table[mid] >> shift) <= cp)
			low = mid;
		else
			high = mid - 1;
	}

	return table[low];
}

static guint32
codepoint_data (gunichar cp)
{
	return lookup_range (smtp_codepoint_data, G_N_ELEMENTS (smtp_codepoint_data), 11, cp);
}

static SmtpUnicodeScript
codepoint_script (gunichar cp)
{
	return (codepoint_data (cp) >> 2) & 511;
}

static guint32
joining_data (gunichar cp)
{
	return lookup_range (smtp_joining_data, G_N_ELEMENTS (smtp_joining_data), 4, cp);
}

static guint32
normalization_data (gunichar cp)
{
	return lookup_range (smtp_normalization_data, G_N_ELEMENTS (smtp_normalization_data), 10, cp);
}

#define CODEPOINT_MASK ((1u << 21) - 1)

static void
decompose (GArray *buffer, gunichar cp)
{
	int low = 0, high = G_N_ELEMENTS (smtp_composition_data) / 3 - 1, mid;
	const guint32 *entry;
	guint32 value;
	guint i, ccc;

	if (cp >= 0xac00 && cp < 0xac00 + 11172) {
		decompose (buffer, 0x1100 + (cp - 0xac00) / 588);
		decompose (buffer, 0x1161 + ((cp - 0xac00) % 588) / 28);

		if ((cp - 0xac00) % 28)
			decompose (buffer, 0x11a7 + (cp - 0xac00) % 28);

		return;
	}

	if (cp >= 0xc0) {
		while (low <= high) {
			mid = low + (high - low) / 2;
			entry = smtp_composition_data + mid * 3;

			if (entry[0] < cp)
				low = mid + 1;
			else if (entry[0] > cp)
				high = mid - 1;
			else {
				decompose (buffer, entry[1]);
				decompose (buffer, entry[2]);
				return;
			}
		}
	}

	ccc = cp < 0x300 ? 0 : normalization_data (cp) & 255;
	value = (ccc << 21) | cp;
	g_array_append_val (buffer, value);

	if (!ccc)
		return;

	/* Quick-check has already rejected unordered input. Only decomposed
	 * starters introduce a bounded number of out-of-order marks here. */
	i = buffer->len - 1;
	while (i > 0 && (g_array_index (buffer, guint32, i - 1) >> 21) > ccc) {
		g_array_index (buffer, guint32, i) = g_array_index (buffer, guint32, i - 1);
		i--;
	}

	g_array_index (buffer, guint32, i) = value;
}

static gunichar
try_compose (gunichar first, gunichar second)
{
	int low = 0, high = G_N_ELEMENTS (smtp_composition_pairs) - 1, mid;
	const guint32 *entry;

	if (first >= 0x1100 && first < 0x1113 && second >= 0x1161 && second < 0x1176)
		return 0xac00 + ((first - 0x1100) * 21 + second - 0x1161) * 28;

	if (first >= 0xac00 && first < 0xac00 + 11172 && (first - 0xac00) % 28 == 0 &&
	    second > 0x11a7 && second < 0x11c3)
		return first + second - 0x11a7;

	while (low <= high) {
		mid = low + (high - low) / 2;
		entry = smtp_composition_data + smtp_composition_pairs[mid] * 3;

		if (entry[1] < first || (entry[1] == first && entry[2] < second))
			low = mid + 1;
		else if (entry[1] > first || (entry[1] == first && entry[2] > second))
			high = mid - 1;
		else
			return entry[0];
	}

	return 0;
}

static gboolean
is_nfc (const char *text, const char *end)
{
	guint last_ccc = 0, ccc, qc, i, written, starter;
	gboolean maybe = FALSE, normalized = TRUE;
	gunichar cp, composite;
	guint32 data, value;
	GArray *buffer;
	const char *p;

	for (p = text; p < end; p = g_utf8_next_char (p)) {
		cp = g_utf8_get_char (p);
		data = cp < 0x300 ? 0 : normalization_data (cp);
		ccc = data & 255;
		qc = (data >> 8) & 3;

		if ((ccc && last_ccc > ccc) || qc == 2)
			return FALSE;

		maybe |= qc == 1;
		last_ccc = ccc;
	}

	if (!maybe)
		return TRUE;

	buffer = g_array_new (FALSE, FALSE, sizeof (guint32));

	for (p = text; p < end; p = g_utf8_next_char (p))
		decompose (buffer, g_utf8_get_char (p));

	last_ccc = written = 0;
	starter = G_MAXUINT;

	for (i = 0; i < buffer->len; i++) {
		value = g_array_index (buffer, guint32, i);
		cp = value & CODEPOINT_MASK;
		ccc = value >> 21;

		if (starter != G_MAXUINT && (starter == written - 1 || last_ccc < ccc) &&
		    (composite = try_compose (g_array_index (buffer, guint32, starter), cp))) {
			g_array_index (buffer, guint32, starter) = composite;
			continue;
		}

		if (!ccc)
			starter = written;

		last_ccc = ccc;
		g_array_index (buffer, guint32, written++) = value;
	}

	p = text;

	for (i = 0; i < written; i++) {
		if (p == end || g_utf8_get_char (p) != (g_array_index (buffer, guint32, i) & CODEPOINT_MASK)) {
			normalized = FALSE;
			break;
		}

		p = g_utf8_next_char (p);
	}

	g_array_free (buffer, TRUE);

	return normalized && p == end;
}

static guint
cjk_script (SmtpUnicodeScript script)
{
	switch (script) {
	case SMTP_SCRIPT_HAN: return 1;
	case SMTP_SCRIPT_HIRAGANA: return 2;
	case SMTP_SCRIPT_KATAKANA: return 4;
	case SMTP_SCRIPT_HANGUL: return 8;
	case SMTP_SCRIPT_BOPOMOFO: return 16;
	default: return 0;
	}
}

static gboolean
joining_context (const char *text, const char *p, const char *end)
{
	const char *q = p;
	guint type = 0;

	while (q > text) {
		q = g_utf8_prev_char (q);
		type = joining_data (g_utf8_get_char (q)) & 7;
		if (type != 4)
			break;
	}

	if (type != 1 && type != 2)
		return FALSE;

	for (q = g_utf8_next_char (p); q < end; q = g_utf8_next_char (q)) {
		type = joining_data (g_utf8_get_char (q)) & 7;
		if (type != 4)
			return type == 2 || type == 3;
	}

	return FALSE;
}

static gboolean
context_satisfied (const char *text, const char *p, const char *end, gboolean japanese)
{
	gunichar cp = g_utf8_get_char (p);
	gunichar before = p > text ? g_utf8_get_char (g_utf8_prev_char (p)) : 0;
	const char *next = g_utf8_next_char (p);
	gunichar after = next < end ? g_utf8_get_char (next) : 0;

	switch (cp) {
	case 0x200c:
		return (before && (joining_data (before) & 8)) || joining_context (text, p, end);
	case 0x200d:
		return before && (joining_data (before) & 8);
	case 0x00b7:
		return (before == 'l' || before == 'L') && (after == 'l' || after == 'L');
	case 0x0375:
		return after && codepoint_script (after) == SMTP_SCRIPT_GREEK;
	case 0x05f3: case 0x05f4:
		return before && codepoint_script (before) == SMTP_SCRIPT_HEBREW;
	case 0x30fb:
		return japanese;
	default:
		return TRUE;
	}
}

static GMimeSmtpUtf8AddressError
validate_codepoints (const char *text, const char *domain, const char *end, GMimeSmtpUtf8AddressValidationOptions options)
{
	gboolean ascii_latin = (options & GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ASCII_IS_LATIN) != 0;
	gboolean latin = FALSE, non_latin = FALSE, mixed = FALSE, context = FALSE, nfc = FALSE;
	gboolean arabic_digits = FALSE, extended_digits = FALSE;
	SmtpUnicodeScript first = SMTP_SCRIPT_UNKNOWN, script;
	guint cjk = 0, family, data;
	const char *p;
	gunichar cp;

	for (p = text; p < end; p = g_utf8_next_char (p)) {
		cp = g_utf8_get_char (p);

		if (ascii_latin && p >= domain && (p == domain || cp == '.'))
			latin = non_latin = FALSE;

		if (cp < 128) {
			if (!ascii_latin || !g_ascii_isalpha (cp))
				continue;
			script = SMTP_SCRIPT_LATIN;
		} else {
			data = codepoint_data (cp);

			if ((data & 3) == 0)
				return GMIME_SMTP_UTF8_ADDRESS_ERROR_DISALLOWED_CODE_POINT;

			script = (data >> 2) & 511;
			context |= (data & 3) != 1;
			nfc |= cp >= 0x300;
			arabic_digits |= cp >= 0x660 && cp <= 0x669;
			extended_digits |= cp >= 0x6f0 && cp <= 0x6f9;
		}

		if (script == SMTP_SCRIPT_COMMON || script == SMTP_SCRIPT_INHERITED)
			continue;

		if (cp >= 128) {
			family = cjk_script (script);

			if (family)
				cjk |= family;
			else if (first == SMTP_SCRIPT_UNKNOWN)
				first = script;
			else
				mixed |= first != script;
		}

		if (ascii_latin) {
			latin |= script == SMTP_SCRIPT_LATIN;
			non_latin |= script != SMTP_SCRIPT_LATIN;
			mixed |= latin && non_latin;
		}
	}

	if (cjk)
		mixed |= first != SMTP_SCRIPT_UNKNOWN || ((cjk & ~7u) && (cjk & ~9u) && (cjk & ~17u));

	if (nfc && !is_nfc (text, end))
		return GMIME_SMTP_UTF8_ADDRESS_ERROR_NOT_NORMALIZED;

	if (context) {
		if (arabic_digits && extended_digits)
			return GMIME_SMTP_UTF8_ADDRESS_ERROR_CONTEXT_RULE;

		for (p = text; p < end; p = g_utf8_next_char (p)) {
			if (!context_satisfied (text, p, end, (cjk & 7) != 0))
				return GMIME_SMTP_UTF8_ADDRESS_ERROR_CONTEXT_RULE;
		}
	}

	return mixed ? GMIME_SMTP_UTF8_ADDRESS_ERROR_MIXED_SCRIPTS : GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE;
}

static GMimeSmtpUtf8AddressError
validate_utf8 (const char *address, gsize length, GMimeSmtpUtf8AddressValidationOptions options)
{
	GMimeSmtpUtf8AddressError result;
	const char *end = address + length, *p, *start;
	GString *decoded;
	SmtpSyntax syntax;
	gsize domain_offset;

	if (!g_utf8_validate_len (address, length, NULL) || !validate_syntax (address, end, FALSE, &syntax))
		return GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX;

	if (options & GMIME_SMTP_UTF8_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS) {
		result = validate_lengths (address, syntax.domain, end);

		if (result != GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE)
			return result;
	}

	if (syntax.local_ace)
		return GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL;

	if (!syntax.domain_ace)
		return validate_codepoints (address, syntax.domain, end, options);

	if (!(options & GMIME_SMTP_UTF8_ADDRESS_VALIDATION_DECODE_IDN))
		return GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL;

	domain_offset = syntax.domain - address;
	decoded = g_string_new_len (address, domain_offset);
	p = syntax.domain;

	do {
		start = p;

		while (p < end && *p != '.')
			p++;

		if (has_ace_prefix (start, p)) {
			if (!append_decoded_label (decoded, start, p)) {
				g_string_free (decoded, TRUE);
				return GMIME_SMTP_UTF8_ADDRESS_ERROR_A_LABEL;
			}
		} else {
			g_string_append_len (decoded, start, p - start);
		}

		if (p < end) {
			g_string_append_c (decoded, '.');
			p++;
		}
	} while (p < end);

	result = validate_codepoints (decoded->str, decoded->str + domain_offset, decoded->str + decoded->len, options);
	g_string_free (decoded, TRUE);

	return result;
}

/**
 * g_mime_smtp_address_try_validate:
 * @address: a bare mailbox address in UTF-8
 * @length: byte length, or -1 for a NUL-terminated string
 * @options: validation options
 * @error: (out) (optional): the validation result
 *
 * Checks the complete address without trimming, parsing display names, or
 * changing it. Invalid UTF-8 and embedded NUL bytes are syntax errors.
 * Syntax errors precede non-ASCII errors, which precede optional length errors.
 * Quotes and escape sequences count toward the length limits. Domain literals
 * are exempt from label/domain limits, but not the overall address limit.
 *
 * Returns: %TRUE if valid; otherwise %FALSE with the reason in @error.
 * Since: 4.0
 **/
gboolean
g_mime_smtp_address_try_validate (const char *address, gssize length,
				 GMimeSmtpAddressValidationOptions options, GMimeSmtpAddressError *error)
{
	GMimeSmtpAddressError result = GMIME_SMTP_ADDRESS_ERROR_SYNTAX;
	GMimeSmtpUtf8AddressError length_error;
	const char *p, *end;
	SmtpSyntax syntax;

	if (error)
		*error = result;

	g_return_val_if_fail (address != NULL, FALSE);
	g_return_val_if_fail (length >= -1, FALSE);

	if (length == -1)
		length = strlen (address);

	end = address + length;

	if (!g_utf8_validate_len (address, length, NULL) ||
	    !validate_syntax (address, end, options & GMIME_SMTP_ADDRESS_VALIDATION_ALLOW_ISO2022JP_LOCAL_PART, &syntax))
		return FALSE;

	result = GMIME_SMTP_ADDRESS_ERROR_NONE;

	for (p = address; p < end; p++) {
		if ((unsigned char) *p >= 128) {
			result = GMIME_SMTP_ADDRESS_ERROR_NON_ASCII;
			break;
		}
	}

	if (result == GMIME_SMTP_ADDRESS_ERROR_NONE && (options & GMIME_SMTP_ADDRESS_VALIDATION_ENFORCE_LENGTH_LIMITS)) {
		length_error = validate_lengths (address, syntax.domain, end);

		if (length_error != GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE)
			result = (GMimeSmtpAddressError) (length_error + 1);
	}

	if (error)
		*error = result;

	return result == GMIME_SMTP_ADDRESS_ERROR_NONE;
}

/**
 * g_mime_smtp_utf8_address_try_validate:
 * @address: a bare mailbox address in UTF-8
 * @length: byte length, or -1 for a NUL-terminated string
 * @options: validation options
 * @error: (out) (optional): the validation result
 *
 * Validates syntax, optional lengths, A-label policy, PRECIS code points, NFC,
 * context rules, and script mixtures, in that order. Invalid UTF-8 and embedded
 * NUL bytes are syntax errors. Unicode 18.0.0 data is used on every platform.
 *
 * Local-part and whole-address limits use the original UTF-8 bytes; domain
 * lengths use raw Punycode A-label sizes. Lengths are checked before IDN decoding.
 * Decoded A-labels must be canonical, at most 63 octets, and satisfy the same
 * Unicode rules as U-labels. Local-part A-label atoms are never decoded.
 *
 * The input is not normalized, mapped, case-folded, or modified. Passing a NULL
 * @error provides a simple validity check. This policy is stricter than the
 * SMTPUTF8 transport grammar and is not a guarantee against all spoofing.
 *
 * Returns: %TRUE if valid; otherwise %FALSE with the reason in @error.
 * Since: 4.0
 **/
gboolean
g_mime_smtp_utf8_address_try_validate (const char *address, gssize length,
				      GMimeSmtpUtf8AddressValidationOptions options, GMimeSmtpUtf8AddressError *error)
{
	GMimeSmtpUtf8AddressError result;

	if (error)
		*error = GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX;

	g_return_val_if_fail (address != NULL, FALSE);
	g_return_val_if_fail (length >= -1, FALSE);

	if (length == -1)
		length = strlen (address);

	result = validate_utf8 (address, length, options);

	if (error)
		*error = result;

	return result == GMIME_SMTP_UTF8_ADDRESS_ERROR_NONE;
}

/**
 * g_mime_smtp_address_try_validate_mailbox:
 * @mailbox: a mailbox
 * @options: validation options
 * @error: (out) (optional): the validation result
 *
 * Validates the mailbox's address as stored, ignoring the display name and
 * route. Does not convert its domain to an IDN-encoded form.
 *
 * Returns: %TRUE if valid for SMTP.
 * Since: 4.0
 **/
gboolean
g_mime_smtp_address_try_validate_mailbox (GMimeMailboxAddress *mailbox,
					 GMimeSmtpAddressValidationOptions options, GMimeSmtpAddressError *error)
{
	const char *address;

	if (error)
		*error = GMIME_SMTP_ADDRESS_ERROR_SYNTAX;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), FALSE);

	address = g_mime_mailbox_address_get_addrspec (mailbox);

	return address && g_mime_smtp_address_try_validate (address, -1, options, error);
}

/**
 * g_mime_smtp_utf8_address_try_validate_mailbox:
 * @mailbox: a mailbox
 * @options: validation options
 * @error: (out) (optional): the validation result
 *
 * Validates the mailbox's address as stored using
 * g_mime_smtp_utf8_address_try_validate(), ignoring the display name and route.
 *
 * Returns: %TRUE if valid under the SMTPUTF8 validation policy.
 * Since: 4.0
 **/
gboolean
g_mime_smtp_utf8_address_try_validate_mailbox (GMimeMailboxAddress *mailbox,
					      GMimeSmtpUtf8AddressValidationOptions options, GMimeSmtpUtf8AddressError *error)
{
	const char *address;

	if (error)
		*error = GMIME_SMTP_UTF8_ADDRESS_ERROR_SYNTAX;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), FALSE);

	address = g_mime_mailbox_address_get_addrspec (mailbox);

	return address && g_mime_smtp_utf8_address_try_validate (address, -1, options, error);
}
