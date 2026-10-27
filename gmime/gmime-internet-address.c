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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#ifdef LIBIDN
#include <idn2.h>
#undef G_GNUC_UNUSED
#undef G_GNUC_DEPRECATED
#endif

#include "gmime-internet-address-private.h"
#include "gmime-common.h"
#include "gmime-table-private.h"
#include "gmime-events.h"
#include "gmime-utils.h"
#include "gmime-error.h"


/**
 * SECTION: gmime-internet-address
 * @title: GMimeInternetAddress
 * @short_description: Internet addresses
 * @see_also: #GMimeGroupAddress, #GMimeMailboxAddress
 *
 * An #GMimeInternetAddress is the base class for #GMimeGroupAddress and
 * #GMimeMailboxAddress.
 **/


static void g_mime_internet_address_class_init (GMimeInternetAddressClass *klass);
static void g_mime_internet_address_init (GMimeInternetAddress *ia, GMimeInternetAddressClass *klass);
static void g_mime_internet_address_finalize (GObject *object);


static GObjectClass *parent_class = NULL;


GType
g_mime_internet_address_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeInternetAddressClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_internet_address_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeInternetAddress),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_internet_address_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeInternetAddress",
					       &info, G_TYPE_FLAG_ABSTRACT);
	}
	
	return type;
}


static void
g_mime_internet_address_class_init (GMimeInternetAddressClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_internet_address_finalize;
	
	klass->to_string = NULL;
}

static void
g_mime_internet_address_init (GMimeInternetAddress *ia, GMimeInternetAddressClass *klass)
{
	ia->changed = g_mime_event_new ((GObject *) ia);
	ia->charset = NULL;
	ia->name = NULL;
}

static void
g_mime_internet_address_finalize (GObject *object)
{
	GMimeInternetAddress *ia = (GMimeInternetAddress *) object;
	
	g_mime_event_free (ia->changed);
	g_free (ia->charset);
	g_free (ia->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


gboolean
_g_mime_internet_address_set_name (GMimeInternetAddress *ia, const char *name)
{
	const char *end;
	char *buf = NULL;
	size_t len;

	g_return_val_if_fail (name == NULL || g_utf8_validate (name, -1, NULL), FALSE);

	/* Edge linear whitespace cannot survive folding and RFC 2047 round trips reliably. */
	if (name) {
		while (is_lwsp (*name))
			name++;

		end = name + strlen (name);

		while (end > name && is_lwsp (end[-1]))
			end--;

		len = end - name;

		if (ia->name && strlen (ia->name) == len && !memcmp (ia->name, name, len))
			return FALSE;

		buf = g_strndup (name, len);
	} else if (!ia->name) {
		return FALSE;
	}

	g_free (ia->name);
	ia->name = buf;

	return TRUE;
}


/**
 * g_mime_internet_address_set_name:
 * @ia: a #GMimeInternetAddress
 * @name: (nullable): the display name for the address group or mailbox
 *
 * Set the display name of the #GMimeInternetAddress. Leading and trailing
 * ASCII linear whitespace (SP, HTAB, CR and LF) is removed; interior
 * whitespace, VT, FF and Unicode whitespace are preserved. %NULL remains
 * distinct from an empty name. An unchanged name does not emit a change
 * notification.
 *
 * Note: The @name string should be in UTF-8.
 **/
void
g_mime_internet_address_set_name (GMimeInternetAddress *ia, const char *name)
{
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS (ia));
	
	if (_g_mime_internet_address_set_name (ia, name))
		g_mime_event_emit (ia->changed, NULL);
}


/**
 * g_mime_internet_address_get_name:
 * @ia: a #GMimeInternetAddress
 *
 * Gets the display name of the #GMimeInternetAddress.
 *
 * Returns: (nullable): the name of the mailbox or group in a form suitable
 * for display if available or %NULL otherwise. If the name is available,
 * the returned string will be in UTF-8.
 **/
const char *
g_mime_internet_address_get_name (GMimeInternetAddress *ia)
{
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), NULL);
	
	return ia->name;
}


/**
 * g_mime_internet_address_set_charset:
 * @ia: a #GMimeInternetAddress
 * @charset: (nullable): the charset to use when encoding the name or %NULL to use the defaults
 *
 * Set the charset to use for encoding the name of the mailbox or group.
 * A %NULL charset selects UTF-8.
 **/
void
g_mime_internet_address_set_charset (GMimeInternetAddress *ia, const char *charset)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS (ia));

	if (!g_strcmp0 (ia->charset, charset))
		return;

	buf = charset ? g_strdup (charset) : NULL;
	g_free (ia->charset);
	ia->charset = buf;
	
	g_mime_event_emit (ia->changed, NULL);
}


/**
 * g_mime_internet_address_get_charset:
 * @ia: a #GMimeInternetAddress
 *
 * Gets the charset to be used when encoding the name of the mailbox or group.
 *
 * Returns: (nullable): the charset to be used when encoding the name of the
 * mailbox or group if available or %NULL otherwise.
 **/
const char *
g_mime_internet_address_get_charset (GMimeInternetAddress *ia)
{
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), NULL);
	
	return ia->charset;
}


/**
 * g_mime_internet_address_to_string:
 * @ia: Internet Address object
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string containing the contents of the #GMimeInternetAddress
 * object.
 * 
 * Returns: the #GMimeInternetAddress object as an allocated string in
 * rfc822 format.
 **/
char *
g_mime_internet_address_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, gboolean encode)
{
	guint32 flags = encode ? GMIME_INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *string;
	char *str;
	
	string = g_string_new ("");
	GMIME_INTERNET_ADDRESS_GET_CLASS (ia)->to_string (ia, options, flags, &linelen, string);
	
	str = g_string_free (string, FALSE);
	
	return str;
}

/**
 * g_mime_internet_address_clone:
 * @ia: a #GMimeInternetAddress
 *
 * Copies the name, charset, mailbox route, and all nested group members.
 *
 * Returns: (transfer full): an independent deep copy.
 **/
GMimeInternetAddress *
g_mime_internet_address_clone (GMimeInternetAddress *ia)
{
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), NULL);
	g_return_val_if_fail (GMIME_INTERNET_ADDRESS_GET_CLASS (ia)->clone != NULL, NULL);

	return GMIME_INTERNET_ADDRESS_GET_CLASS (ia)->clone (ia);
}

/**
 * g_mime_internet_address_equal:
 * @ia: a #GMimeInternetAddress
 * @other: a #GMimeInternetAddress
 *
 * Compares nullable names and mailbox addresses case-sensitively. Groups
 * additionally compare their members recursively and in order. Charsets and
 * mailbox source routes do not participate in equality.
 *
 * Returns: whether the addresses are equal by value.
 **/
gboolean
g_mime_internet_address_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other)
{
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), FALSE);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (other), FALSE);
	g_return_val_if_fail (GMIME_INTERNET_ADDRESS_GET_CLASS (ia)->equal != NULL, FALSE);

	return ia == other || GMIME_INTERNET_ADDRESS_GET_CLASS (ia)->equal (ia, other);
}

/**
 * g_mime_internet_address_compare:
 * @ia: a #GMimeInternetAddress
 * @other: a #GMimeInternetAddress
 *
 * Compares nullable names case-sensitively using UTF-8 byte order. Mailboxes
 * with equal names compare complete addr-specs with case-sensitive local
 * parts and IDN-encoded, ASCII-lowercase domains. Domainless addresses are
 * compared as stored. Mailboxes sort before equal-name groups, and equal-name
 * groups compare equal regardless of members. Ordering is not a test for
 * value equality.
 *
 * Returns: a negative number, zero, or a positive number.
 **/
int
g_mime_internet_address_compare (GMimeInternetAddress *ia, GMimeInternetAddress *other)
{
	char *left, *right;
	int result;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), 0);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (other), 0);

	if (ia == other)
		return 0;

	if (!ia->name || !other->name) {
		result = (ia->name != NULL) - (other->name != NULL);
	} else {
		result = g_strcmp0 (ia->name, other->name);
	}

	if (result)
		return result;

	if (!GMIME_IS_MAILBOX_ADDRESS (ia) || !GMIME_IS_MAILBOX_ADDRESS (other))
		return GMIME_IS_MAILBOX_ADDRESS (other) - GMIME_IS_MAILBOX_ADDRESS (ia);

	left = _g_mime_mailbox_address_get_hashable_addrspec (GMIME_MAILBOX_ADDRESS (ia));
	right = _g_mime_mailbox_address_get_hashable_addrspec (GMIME_MAILBOX_ADDRESS (other));
	result = g_strcmp0 (left, right);

	g_free (left);
	g_free (right);

	return result;
}

char *
_g_mime_internet_address_encode_domain (const char *domain)
{
	char *ascii = NULL;
	const char *p;

	if (!*domain || *domain == '[' || !g_hostname_is_non_ascii (domain))
		return g_strdup (domain);

#ifdef LIBIDN
	char *mapped;

	if (idn2_to_ascii_8z (domain, &mapped, 0) != IDN2_OK)
		return g_strdup (domain);

	ascii = g_strdup (mapped);
	idn2_free (mapped);
#else
	ascii = g_hostname_to_ascii (domain);

	if (!ascii)
		return g_strdup (domain);
#endif

	/* IDNA mappings must not introduce address delimiters or whitespace. */
	if (!*ascii || *ascii == '.' || strstr (ascii, "..")) {
		g_free (ascii);
		return g_strdup (domain);
	}

	for (p = ascii; *p; p++) {
		if ((guchar) *p >= 128 || (*p != '.' && !is_atom (*p))) {
			g_free (ascii);
			return g_strdup (domain);
		}
	}

	return ascii;
}

char *
_g_mime_internet_address_decode_domain (const char *domain)
{
	char *unicode = NULL;
	const char *p;

	/* Domain literals are not DNS names, even when they contain apparent A-labels. */
	if (!*domain || *domain == '[')
		return g_strdup (domain);

	for (p = domain; *p; p++) {
		if ((p == domain || p[-1] == '.') && !g_ascii_strncasecmp (p, "xn--", 4))
			break;
	}

	if (!*p)
		return g_strdup (domain);

#ifdef LIBIDN
	char *mapped;

	if (idn2_to_unicode_8z8z (domain, &mapped, 0) != IDN2_OK)
		return g_strdup (domain);

	unicode = g_strdup (mapped);
	idn2_free (mapped);
#else
	unicode = g_hostname_to_unicode (domain);
#endif

	if (unicode) {
		char *ascii = _g_mime_internet_address_encode_domain (unicode);
		char *normalized = NULL;

		/* The Unicode conversion alone does not validate an A-label. */
		if (!g_ascii_strcasecmp (ascii, domain))
			normalized = g_utf8_strdown (unicode, -1);

		g_free (ascii);
		g_free (unicode);

		if (normalized)
			return normalized;
	}

	return g_strdup (domain);
}

char *
_g_mime_internet_address_copy_text (const char *str, gssize length, GError **error)
{
	g_return_val_if_fail (str != NULL, NULL);
	g_return_val_if_fail (length >= -1, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (length < 0)
		length = strlen (str);

	if (length > G_MAXINT || memchr (str, '\0', length)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     "Address text contains a NUL byte or exceeds the supported length.");
		return NULL;
	}

	return g_strndup (str, length);
}

/**
 * g_mime_internet_address_parse:
 * @options: (nullable): parser options
 * @str: address text
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses exactly one mailbox or group, allowing trailing comments and whitespace.
 *
 * Returns: (nullable) (transfer full): the address, or %NULL on error.
 **/
GMimeInternetAddress *
g_mime_internet_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	return _g_mime_internet_address_parse (options, str, length, GMIME_TYPE_INTERNET_ADDRESS, error);
}


char *
_g_mime_internet_address_encode_name (GMimeFormatOptions *options, const char *raw, gboolean rfc2047_encode, const char *charset)
{
	char *name;

	g_return_val_if_fail (raw != NULL, NULL);

	if (rfc2047_encode && !g_mime_format_options_get_allow_international (options)) {
		name = g_mime_utils_header_encode_phrase (options, raw, charset ? charset : "UTF-8");
	} else if (rfc2047_encode && strpbrk (raw, "()<>@,;:\\\".[]")) {
		GString *quoted = g_string_new ("\"");
		const char *p;

		for (p = raw; *p; p++) {
			if (*p == '\\' || *p == '"')
				g_string_append_c (quoted, '\\');
			g_string_append_c (quoted, *p);
		}

		g_string_append_c (quoted, '"');
		name = g_string_free (quoted, FALSE);
	} else if (rfc2047_encode) {
		name = g_strdup (raw);
	} else {
		name = g_mime_utils_quote_string (raw);
	}

	return name;
}

void
_g_mime_internet_address_linewrap (GString *str, const char *newline)
{
	if (str->len == 0)
		return;

	if (str->str[str->len - 1] == ' ' || str->str[str->len - 1] == '\t') {
		g_string_insert (str, str->len - 1, newline);
	} else {
		g_string_append (str, newline);
		g_string_append_c (str, '\t');
	}
}

static void
append_folded_name (GString *str, size_t *linelen, const char *name, GMimeFormatOptions *options,
		    gboolean first)
{
	const char *newline = g_mime_format_options_get_newline (options);
	guint maxlen = g_mime_format_options_get_max_line_length (options);
	const char *word, *lwsp;
	size_t len, columns;

	word = name;

	while (*word) {
		lwsp = word;

		if (*word == '"') {
			/* quoted string, don't break these up */
			lwsp++;

			while (*lwsp && *lwsp != '"') {
				if (*lwsp == '\\')
					lwsp++;

				if (*lwsp)
					lwsp++;
			}

			if (*lwsp == '"')
				lwsp++;
		} else {
			/* normal word */
			while (*lwsp && !is_lwsp (*lwsp))
				lwsp++;
		}

		len = lwsp - word;
		columns = _g_mime_internet_address_text_length (word, len, options);
		if (!first && *linelen > 1 && (*linelen + columns) > maxlen) {
			_g_mime_internet_address_linewrap (str, newline);
			*linelen = 1;
		}

		g_string_append_len (str, word, len);
		*linelen += columns;
		first = FALSE;

		word = lwsp;
		while (*word && is_lwsp (*word))
			word++;

		if (*word && is_lwsp (*lwsp)) {
			g_string_append_c (str, ' ');
			(*linelen)++;
		}
	}
}

void
_g_mime_internet_address_append_name (GString *str, size_t *linelen, const char *name, GMimeFormatOptions *options, guint32 flags)
{
	guint maxlen = g_mime_format_options_get_max_line_length (options);
	size_t len = strlen (name), columns = _g_mime_internet_address_text_length (name, len, options);

	if ((flags & GMIME_INTERNET_ADDRESS_FOLD) && (*linelen + columns) > maxlen) {
		if (columns > maxlen) {
			append_folded_name (str, linelen, name, options, flags & GMIME_INTERNET_ADDRESS_FIRST);
			return;
		}

		if (!(flags & GMIME_INTERNET_ADDRESS_FIRST) && *linelen > 1) {
			_g_mime_internet_address_linewrap (str, g_mime_format_options_get_newline (options));
			*linelen = 1;
		}
	}

	g_string_append_len (str, name, len);
	*linelen += columns;
}

size_t
_g_mime_internet_address_text_length (const char *text, size_t length, GMimeFormatOptions *options)
{
	const char *end = text + length;
	size_t columns = 0;

	if (!g_mime_format_options_get_allow_international (options))
		return length;

	while (text < end) {
		columns += g_utf8_get_char (text) >= 0x10000 ? 2 : 1;
		text = g_utf8_next_char (text);
	}

	return columns;
}
