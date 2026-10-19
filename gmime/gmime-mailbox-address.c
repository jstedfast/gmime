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

#include "gmime-internet-address-private.h"
#include "gmime-events.h"
#include "gmime-error.h"
#include "gmime-parse-utils.h"


/**
 * SECTION: gmime-mailbox-address
 * @title: GMimeMailboxAddress
 * @short_description: rfc822 'mailbox' address
 * @see_also: #GMimeInternetAddress
 *
 * An #GMimeMailboxAddress represents what is a typical "email
 * address".
 **/


static void g_mime_mailbox_address_class_init (GMimeMailboxAddressClass *klass);
static void g_mime_mailbox_address_init (GMimeMailboxAddress *mailbox, GMimeMailboxAddressClass *klass);
static void g_mime_mailbox_address_finalize (GObject *object);

static void mailbox_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			       size_t *linelen, GString *out);
static GMimeInternetAddress *mailbox_clone (GMimeInternetAddress *ia);
static gboolean mailbox_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other);


static GObjectClass *mailbox_parent_class = NULL;


GType
g_mime_mailbox_address_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMailboxAddressClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_mailbox_address_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMailboxAddress),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_mailbox_address_init,
		};

		type = g_type_register_static (GMIME_TYPE_INTERNET_ADDRESS, "GMimeMailboxAddress", &info, 0);
	}

	return type;
}


static void
g_mime_mailbox_address_class_init (GMimeMailboxAddressClass *klass)
{
	GMimeInternetAddressClass *address_class = GMIME_INTERNET_ADDRESS_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	mailbox_parent_class = g_type_class_ref (GMIME_TYPE_INTERNET_ADDRESS);

	object_class->finalize = g_mime_mailbox_address_finalize;

	address_class->to_string = mailbox_to_string;
	address_class->clone = mailbox_clone;
	address_class->equal = mailbox_equal;
}

static void
route_changed (GMimeDomainList *route, gpointer args, GMimeMailboxAddress *mailbox)
{
	g_mime_event_emit (mailbox->parent_object.changed, NULL);
}

static void
g_mime_mailbox_address_init (GMimeMailboxAddress *mailbox, GMimeMailboxAddressClass *klass)
{
	mailbox->addrspec = g_strdup ("");
	mailbox->at = -1;
	mailbox->route = g_mime_domain_list_new ();
	g_mime_event_add (mailbox->route->changed, (GMimeEventCallback) route_changed, mailbox);
}

static void
g_mime_mailbox_address_finalize (GObject *object)
{
	GMimeMailboxAddress *mailbox = (GMimeMailboxAddress *) object;

	g_free (mailbox->addrspec);
	g_mime_event_remove (mailbox->route->changed, (GMimeEventCallback) route_changed, mailbox);
	g_object_unref (mailbox->route);

	G_OBJECT_CLASS (mailbox_parent_class)->finalize (object);
}

GMimeInternetAddress *
_g_mime_mailbox_address_new (const char *name, const char *addr, int at, GMimeDomainList *route)
{
	GMimeMailboxAddress *mailbox;

	mailbox = g_object_new (GMIME_TYPE_MAILBOX_ADDRESS, NULL);
	g_free (mailbox->addrspec);
	mailbox->addrspec = g_strdup (addr);
	mailbox->at = at;

	if (route) {
		g_mime_event_remove (mailbox->route->changed, (GMimeEventCallback) route_changed, mailbox);
		g_object_unref (mailbox->route);
		mailbox->route = g_object_ref (route);
		g_mime_event_add (route->changed, (GMimeEventCallback) route_changed, mailbox);
	}

	_g_mime_internet_address_set_name ((GMimeInternetAddress *) mailbox, name);

	return (GMimeInternetAddress *) mailbox;
}

static GMimeInternetAddress *
mailbox_clone (GMimeInternetAddress *ia)
{
	GMimeMailboxAddress *mailbox = GMIME_MAILBOX_ADDRESS (ia);
	GMimeDomainList *route = g_mime_domain_list_clone (mailbox->route);
	GMimeInternetAddress *clone;

	clone = _g_mime_mailbox_address_new (ia->name, mailbox->addrspec, mailbox->at, route);
	clone->charset = g_strdup (ia->charset);
	g_object_unref (route);

	return clone;
}

static gboolean
mailbox_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other)
{
	return GMIME_IS_MAILBOX_ADDRESS (other) && !g_strcmp0 (ia->name, other->name) &&
		!strcmp (GMIME_MAILBOX_ADDRESS (ia)->addrspec, GMIME_MAILBOX_ADDRESS (other)->addrspec);
}

static gboolean
parse_mailbox_addr (const char *addr, char **parsed, int *at, GError **error)
{
	const char *inptr = addr;

	*parsed = NULL;
	*at = -1;

	if (!*addr) {
		*parsed = g_strdup ("");
		return TRUE;
	}

	if (strlen (addr) <= G_MAXINT && g_utf8_validate (addr, -1, NULL) &&
	    _g_mime_internet_address_parse_addrspec (&inptr, "", g_mime_parser_options_get_address_compliance_mode (NULL), parsed, at) &&
	    skip_cfws (&inptr) && !*inptr)
		return TRUE;

	g_clear_pointer (parsed, g_free);
	g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
		     "Invalid mailbox addr-spec at byte offset %" G_GSIZE_FORMAT ".", (gsize) (inptr - addr));

	return FALSE;
}


/**
 * g_mime_mailbox_address_new:
 * @name: (nullable): person's name
 * @addr: person's address
 * @error: (nullable): return location for a #GError
 *
 * Creates a new #GMimeInternetAddress object with the specified @name and
 * @addr. Leading and trailing Unicode whitespace is removed from @name.
 *
 * The complete @addr must be a valid addr-spec or an empty string. The
 * domain's IDN representation is preserved rather than decoded.
 *
 * Returns: (nullable) (transfer full): a new mailbox, or %NULL on error.
 *
 * Note: The @name string should be in UTF-8.
 **/
GMimeInternetAddress *
g_mime_mailbox_address_new (const char *name, const char *addr, GError **error)
{
	GMimeInternetAddress *mailbox;
	char *parsed;
	int at;

	g_return_val_if_fail (addr != NULL, NULL);
	g_return_val_if_fail (name == NULL || g_utf8_validate (name, -1, NULL), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (!parse_mailbox_addr (addr, &parsed, &at, error))
		return NULL;

	mailbox = _g_mime_mailbox_address_new (name, parsed, at, NULL);
	g_free (parsed);

	return mailbox;
}


/**
 * g_mime_mailbox_address_set_addrspec:
 * @mailbox: a #GMimeMailboxAddress
 * @addr: contact's email address
 * @error: (nullable): return location for a #GError
 *
 * Sets the mailbox address. Invalid input leaves the address unchanged.
 * Empty addresses are allowed. The domain's IDN
 * representation is preserved rather than decoded.
 *
 * Returns: whether the address was valid.
 **/
gboolean
g_mime_mailbox_address_set_addrspec (GMimeMailboxAddress *mailbox, const char *addr, GError **error)
{
	char *parsed;
	int at;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), FALSE);
	g_return_val_if_fail (addr != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!strcmp (mailbox->addrspec, addr))
		return TRUE;

	if (!parse_mailbox_addr (addr, &parsed, &at, error))
		return FALSE;

	g_free (mailbox->addrspec);

	mailbox->addrspec = parsed;
	mailbox->at = at;

	g_mime_event_emit (((GMimeInternetAddress *) mailbox)->changed, NULL);

	return TRUE;
}


/**
 * g_mime_mailbox_address_get_addrspec:
 * @mailbox: a #GMimeMailboxAddress
 *
 * Gets the addr-spec without changing its domain's IDN representation.
 * Use g_mime_mailbox_address_get_address() to encode or decode the domain.
 *
 * Returns: (transfer none): the stored addr-spec string.
 **/
const char *
g_mime_mailbox_address_get_addrspec (GMimeMailboxAddress *mailbox)
{
	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	return mailbox->addrspec;
}


/**
 * g_mime_mailbox_address_get_local_part:
 * @mailbox: a #GMimeMailboxAddress
 *
 * Returns: (transfer full): the local part, including any quotes.
 **/
char *
g_mime_mailbox_address_get_local_part (GMimeMailboxAddress *mailbox)
{
	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	return mailbox->at < 0 ? g_strdup (mailbox->addrspec) : g_strndup (mailbox->addrspec, mailbox->at);
}

/**
 * g_mime_mailbox_address_get_raw_domain:
 * @mailbox: a #GMimeMailboxAddress
 *
 * Gets the domain without changing its IDN representation.
 * Use g_mime_mailbox_address_get_domain() to encode or decode it.
 *
 * Returns: (transfer none): the stored domain, or an empty string if absent.
 **/
const char *
g_mime_mailbox_address_get_raw_domain (GMimeMailboxAddress *mailbox)
{
	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	return mailbox->at < 0 ? "" : mailbox->addrspec + mailbox->at + 1;
}

/**
 * g_mime_mailbox_address_get_domain:
 * @mailbox: a #GMimeMailboxAddress
 * @idn_encode: %TRUE to IDN-encode the domain, or %FALSE to decode it
 *
 * Converts the domain without modifying the mailbox. Domain literals are
 * returned unchanged. Mapping failures preserve the stored domain.
 *
 * Returns: (transfer full): the converted domain, or an empty string if absent.
 **/
char *
g_mime_mailbox_address_get_domain (GMimeMailboxAddress *mailbox, gboolean idn_encode)
{
	const char *domain;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	domain = g_mime_mailbox_address_get_raw_domain (mailbox);

	return idn_encode ? _g_mime_internet_address_encode_domain (domain) :
		_g_mime_internet_address_decode_domain (domain);
}

/**
 * g_mime_mailbox_address_get_address:
 * @mailbox: a #GMimeMailboxAddress
 * @idn_encode: %TRUE to IDN-encode the domain, or %FALSE to decode it
 *
 * Converts the domain without modifying the mailbox. The local part and
 * domain literals are preserved. Mapping failures preserve the stored domain.
 *
 * Returns: (transfer full): the converted addr-spec.
 **/
char *
g_mime_mailbox_address_get_address (GMimeMailboxAddress *mailbox, gboolean idn_encode)
{
	char *domain, *local, *address;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	if (mailbox->at < 0)
		return g_strdup (mailbox->addrspec);

	domain = g_mime_mailbox_address_get_domain (mailbox, idn_encode);
	local = g_strndup (mailbox->addrspec, mailbox->at + 1);
	address = g_strconcat (local, domain, NULL);

	g_free (local);
	g_free (domain);

	return address;
}

char *
_g_mime_mailbox_address_get_hashable_addrspec (GMimeMailboxAddress *mailbox)
{
	char *encoded, *domain, *local, *address;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	if (mailbox->at < 0)
		return g_strdup (mailbox->addrspec);

	encoded = g_mime_mailbox_address_get_domain (mailbox, TRUE);
	domain = g_ascii_strdown (encoded, -1);
	g_free (encoded);

	local = g_strndup (mailbox->addrspec, mailbox->at + 1);
	address = g_strconcat (local, domain, NULL);

	g_free (local);
	g_free (domain);

	return address;
}

/**
 * g_mime_mailbox_address_get_route:
 * @mailbox: a #GMimeMailboxAddress
 *
 * Returns: (transfer none): the mutable source-route domains.
 **/
GMimeDomainList *
g_mime_mailbox_address_get_route (GMimeMailboxAddress *mailbox)
{
	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	return mailbox->route;
}

/**
 * g_mime_mailbox_address_is_international:
 * @mailbox: a #GMimeMailboxAddress
 *
 * Returns: whether a nonempty mailbox address or its route contains non-ASCII.
 **/
gboolean
g_mime_mailbox_address_is_international (GMimeMailboxAddress *mailbox)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), FALSE);

	if (!*mailbox->addrspec)
		return FALSE;
	if (g_hostname_is_non_ascii (mailbox->addrspec))
		return TRUE;
	for (i = 0; i < mailbox->route->array->len; i++) {
		if (g_hostname_is_non_ascii (mailbox->route->array->pdata[i]))
			return TRUE;
	}

	return FALSE;
}

static char *
convert_addrspec (const char *addr, gboolean encode)
{
	const char *inptr = addr;
	char *parsed, *ascii, *local, *converted;
	int at;

	g_return_val_if_fail (addr != NULL, NULL);

	if (!*addr || !_g_mime_internet_address_parse_addrspec (&inptr, "", GMIME_RFC_COMPLIANCE_LOOSER, &parsed, &at))
		return g_strdup (addr);
	if (at < 0)
		return parsed;

	ascii = encode ? _g_mime_internet_address_encode_domain (parsed + at + 1) :
		_g_mime_internet_address_decode_domain (parsed + at + 1);
	local = g_strndup (parsed, at + 1);
	converted = g_strconcat (local, ascii, NULL);
	g_free (parsed);
	g_free (ascii);
	g_free (local);

	return converted;
}

/**
 * g_mime_mailbox_address_encode_addrspec:
 * @addr: an addr-spec
 *
 * Normalizes an addr-spec with looser compliance and IDN-encodes its domain.
 * Unparseable input is copied unchanged; local parts and literals are not
 * IDN-encoded.
 *
 * Returns: (transfer full): the encoded addr-spec.
 **/
char *
g_mime_mailbox_address_encode_addrspec (const char *addr)
{
	return convert_addrspec (addr, TRUE);
}

/**
 * g_mime_mailbox_address_decode_addrspec:
 * @addr: an addr-spec
 *
 * Normalizes an addr-spec with looser compliance and decodes IDN domains.
 * Unparseable input is copied unchanged; domain literals are preserved.
 *
 * Returns: (transfer full): the decoded addr-spec.
 **/
char *
g_mime_mailbox_address_decode_addrspec (const char *addr)
{
	return convert_addrspec (addr, FALSE);
}

/**
 * g_mime_mailbox_address_parse:
 * @options: (nullable): parser options
 * @str: mailbox text, optionally including a name and source route
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Returns: (nullable) (transfer full): exactly one mailbox, or %NULL on error.
 **/
GMimeInternetAddress *
g_mime_mailbox_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	return _g_mime_internet_address_parse (options, str, length, GMIME_TYPE_MAILBOX_ADDRESS, error);
}


static void
mailbox_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags, size_t *linelen, GString *str)
{
	GMimeMailboxAddress *mailbox = (GMimeMailboxAddress *) ia;
	gboolean encode = flags & GMIME_INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & GMIME_INTERNET_ADDRESS_FOLD;
	guint maxlen = g_mime_format_options_get_max_line_length (options);
	const char *newline, *addr;
	char *name, *route, *encoded_addr = NULL;
	size_t len, columns;

	newline = g_mime_format_options_get_newline (options);

	if (encode && !g_mime_format_options_get_allow_international (options))
		encoded_addr = g_mime_mailbox_address_get_address (mailbox, TRUE);
	addr = encoded_addr ? encoded_addr : mailbox->addrspec;
	route = g_mime_domain_list_to_string (mailbox->route, options, encode);
	if (*route) {
		char *routed = g_strconcat (route, ":", addr, NULL);

		g_free (route);
		route = routed;
		addr = route;
	}

	if (ia->name && *ia->name) {
		name = _g_mime_internet_address_encode_name (options, ia->name, encode, ia->charset);
		_g_mime_internet_address_append_name (str, linelen, name, options, flags);
		g_free (name);

		len = strlen (addr);
		columns = _g_mime_internet_address_text_length (addr, len, options);

		if (fold && (*linelen + columns + 3) > maxlen) {
			g_string_append (str, newline);
			g_string_append_len (str, "\t<", 2);
			*linelen = 2;
		} else {
			g_string_append_len (str, " <", 2);
			*linelen += 2;
		}

		g_string_append_len (str, addr, len);
		g_string_append_c (str, '>');
		*linelen += columns + 1;
	} else {
		/* Bare domainless addr-specs require a parser option, but angle-addrs
		 * without a domain are always parseable. */
		gboolean angle = *route || (encode && mailbox->at < 0 && *addr);

		len = strlen (addr);
		columns = _g_mime_internet_address_text_length (addr, len, options);

		if (fold && !(flags & GMIME_INTERNET_ADDRESS_FIRST) && (*linelen + columns + (angle ? 2 : 0)) > maxlen) {
			if (*route) {
				g_string_append (str, newline);
				g_string_append_c (str, '\t');
			} else {
				_g_mime_internet_address_linewrap (str, newline);
			}
			*linelen = 1;
		}

		if (angle)
			g_string_append_c (str, '<');
		g_string_append_len (str, addr, len);
		if (angle)
			g_string_append_c (str, '>');
		*linelen += columns + (angle ? 2 : 0);
	}
	g_free (route);
	g_free (encoded_addr);
}
