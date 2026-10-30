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


#include "gmime-object-private.h"

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
static void g_mime_mailbox_address_init (GMimeMailboxAddress *mailbox);
static void g_mime_mailbox_address_finalize (GObject *object);

static void mailbox_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			       size_t *linelen, GString *out);
static GMimeInternetAddress *mailbox_clone (GMimeInternetAddress *ia);
static gboolean mailbox_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other);


static GObjectClass *mailbox_parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeMailboxAddress, g_mime_mailbox_address, GMIME_TYPE_INTERNET_ADDRESS)

GMimeMailboxAddressPrivate *
_g_mime_mailbox_address_get_private (GMimeMailboxAddress *object)
{
	return g_mime_mailbox_address_get_instance_private (object);
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
route_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeMailboxAddress *mailbox = user_data;
	GMimeInternetAddressPrivate *priv = _g_mime_internet_address_get_private (&mailbox->parent_instance);

	g_mime_event_emit (priv->changed, NULL);
}

static void
g_mime_mailbox_address_init (GMimeMailboxAddress *mailbox)
{
	GMimeMailboxAddressPrivate *priv = _g_mime_mailbox_address_get_private (mailbox);

	priv->addrspec = g_strdup ("");
	priv->at = -1;
	priv->route = g_mime_domain_list_new ();

	g_mime_event_add (priv->route->changed, (GMimeEventCallback) route_changed, mailbox);
}

static void
g_mime_mailbox_address_finalize (GObject *object)
{
	GMimeMailboxAddress *mailbox = (GMimeMailboxAddress *) object;
	GMimeMailboxAddressPrivate *priv = _g_mime_mailbox_address_get_private (mailbox);

	g_free (priv->addrspec);
	g_mime_event_remove (priv->route->changed, (GMimeEventCallback) route_changed, mailbox);
	g_object_unref (priv->route);

	G_OBJECT_CLASS (mailbox_parent_class)->finalize (object);
}

GMimeInternetAddress *
_g_mime_mailbox_address_new (const char *name, const char *addr, int at, GMimeDomainList *route)
{
	GMimeMailboxAddressPrivate *priv;
	GMimeMailboxAddress *mailbox;

	mailbox = g_object_new (GMIME_TYPE_MAILBOX_ADDRESS, NULL);
	priv = _g_mime_mailbox_address_get_private (mailbox);

	g_free (priv->addrspec);
	priv->addrspec = g_strdup (addr);
	priv->at = at;

	if (route) {
		g_mime_event_remove (priv->route->changed, (GMimeEventCallback) route_changed, mailbox);
		g_object_unref (priv->route);
		priv->route = g_object_ref (route);
		g_mime_event_add (route->changed, (GMimeEventCallback) route_changed, mailbox);
	}

	_g_mime_internet_address_set_name ((GMimeInternetAddress *) mailbox, name);

	return (GMimeInternetAddress *) mailbox;
}

static GMimeInternetAddress *
mailbox_clone (GMimeInternetAddress *ia)
{
	GMimeInternetAddressPrivate *ia_priv = _g_mime_internet_address_get_private (ia);
	GMimeMailboxAddress *mailbox = GMIME_MAILBOX_ADDRESS (ia);
	GMimeMailboxAddressPrivate *mailbox_priv = _g_mime_mailbox_address_get_private (mailbox);
	GMimeDomainList *route = g_mime_domain_list_clone (mailbox_priv->route);
	GMimeInternetAddressPrivate *clone_priv;
	GMimeInternetAddress *clone;

	clone = _g_mime_mailbox_address_new (ia_priv->name, mailbox_priv->addrspec, mailbox_priv->at, route);
	clone_priv = _g_mime_internet_address_get_private (clone);

	clone_priv->charset = g_strdup (ia_priv->charset);
	g_object_unref (route);

	return clone;
}

static gboolean
mailbox_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other)
{
	GMimeInternetAddressPrivate *other_priv = _g_mime_internet_address_get_private (other);
	GMimeInternetAddressPrivate *ia_priv = _g_mime_internet_address_get_private (ia);
	GMimeMailboxAddressPrivate *mailbox_priv, *other_mailbox_priv;

	if (!GMIME_IS_MAILBOX_ADDRESS (other) || g_strcmp0 (ia_priv->name, other_priv->name))
		return FALSE;

	mailbox_priv = _g_mime_mailbox_address_get_private (GMIME_MAILBOX_ADDRESS (ia));
	other_mailbox_priv = _g_mime_mailbox_address_get_private (GMIME_MAILBOX_ADDRESS (other));

	return !strcmp (mailbox_priv->addrspec, other_mailbox_priv->addrspec);
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
	GMimeMailboxAddressPrivate *mailbox_priv;
	GMimeInternetAddressPrivate *ia_priv;
	char *parsed;
	int at;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), FALSE);
	g_return_val_if_fail (addr != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	mailbox_priv = _g_mime_mailbox_address_get_private (mailbox);

	if (!strcmp (mailbox_priv->addrspec, addr))
		return TRUE;

	if (!parse_mailbox_addr (addr, &parsed, &at, error))
		return FALSE;

	g_free (mailbox_priv->addrspec);

	mailbox_priv->addrspec = parsed;
	mailbox_priv->at = at;

	ia_priv = _g_mime_internet_address_get_private ((GMimeInternetAddress *) mailbox);

	g_mime_event_emit (ia_priv->changed, NULL);

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
	GMimeMailboxAddressPrivate *priv;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	return priv->addrspec;
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
	GMimeMailboxAddressPrivate *priv;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	return priv->at < 0 ? g_strdup (priv->addrspec) : g_strndup (priv->addrspec, priv->at);
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
	GMimeMailboxAddressPrivate *priv;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	return priv->at < 0 ? "" : priv->addrspec + priv->at + 1;
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
	GMimeMailboxAddressPrivate *priv;
	char *domain, *local, *address;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	if (priv->at < 0)
		return g_strdup (priv->addrspec);

	domain = g_mime_mailbox_address_get_domain (mailbox, idn_encode);
	local = g_strndup (priv->addrspec, priv->at + 1);
	address = g_strconcat (local, domain, NULL);

	g_free (local);
	g_free (domain);

	return address;
}

char *
_g_mime_mailbox_address_get_hashable_addrspec (GMimeMailboxAddress *mailbox)
{
	GMimeMailboxAddressPrivate *priv;
	char *encoded, *domain, *local, *address;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	if (priv->at < 0)
		return g_strdup (priv->addrspec);

	encoded = g_mime_mailbox_address_get_domain (mailbox, TRUE);
	domain = g_ascii_strdown (encoded, -1);
	g_free (encoded);

	local = g_strndup (priv->addrspec, priv->at + 1);
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
	GMimeMailboxAddressPrivate *priv;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), NULL);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	return priv->route;
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
	GMimeMailboxAddressPrivate *priv;
	guint i;

	g_return_val_if_fail (GMIME_IS_MAILBOX_ADDRESS (mailbox), FALSE);

	priv = _g_mime_mailbox_address_get_private (mailbox);

	if (!*priv->addrspec)
		return FALSE;

	if (g_hostname_is_non_ascii (priv->addrspec))
		return TRUE;

	for (i = 0; i < priv->route->array->len; i++) {
		if (g_hostname_is_non_ascii (priv->route->array->pdata[i]))
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
	GMimeMailboxAddressPrivate *priv = _g_mime_mailbox_address_get_private ((GMimeMailboxAddress *) ia);
	GMimeInternetAddressPrivate *ia_priv = _g_mime_internet_address_get_private (ia);
	guint maxlen = g_mime_format_options_get_max_line_length (options);
	GMimeMailboxAddress *mailbox = (GMimeMailboxAddress *) ia;
	gboolean encode = flags & GMIME_INTERNET_ADDRESS_ENCODE;
	gboolean fold = flags & GMIME_INTERNET_ADDRESS_FOLD;
	char *name, *route, *encoded_addr = NULL;
	const char *newline, *addr;
	size_t len, columns;

	newline = g_mime_format_options_get_newline (options);

	if (encode && !g_mime_format_options_get_allow_international (options))
		encoded_addr = g_mime_mailbox_address_get_address (mailbox, TRUE);

	addr = encoded_addr ? encoded_addr : priv->addrspec;
	route = g_mime_domain_list_to_string (priv->route, options, encode);

	if (*route) {
		char *routed = g_strconcat (route, ":", addr, NULL);

		g_free (route);
		route = routed;
		addr = route;
	}

	if (ia_priv->name && *ia_priv->name) {
		name = _g_mime_internet_address_encode_name (options, ia_priv->name, encode, ia_priv->charset);
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
		gboolean angle = *route || (encode && priv->at < 0 && *addr);

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
