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

#include <stdlib.h>
#include <string.h>

#include "gmime-internet-address-private.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-common.h"
#include "gmime-iconv-utils.h"
#include "gmime-internal.h"
#include "gmime-events.h"
#include "gmime-utils.h"
#include "gmime-error.h"

#ifdef ENABLE_WARNINGS
#define w(x) (x)
#else
#define w(x)
#endif /* ENABLE_WARNINGS */


#include "gmime-object-private.h"

/**
 * SECTION: gmime-internet-address-list
 * @title: GMimeInternetAddressList
 * @short_description: A list of internet addresses
 * @see_also: #GMimeInternetAddress
 *
 * An #GMimeInternetAddressList is a collection of #GMimeInternetAddress
 * objects.
 **/


static void g_mime_internet_address_list_class_init (GMimeInternetAddressListClass *klass);
static void g_mime_internet_address_list_init (GMimeInternetAddressList *list);
static void g_mime_internet_address_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


G_DEFINE_FINAL_TYPE (GMimeInternetAddressList, g_mime_internet_address_list, G_TYPE_OBJECT)


static void
g_mime_internet_address_list_class_init (GMimeInternetAddressListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->finalize = g_mime_internet_address_list_finalize;
}

static void
g_mime_internet_address_list_init (GMimeInternetAddressList *list)
{
	list->changed = g_mime_event_new ((GObject *) list);
	list->array = g_ptr_array_new ();
}

static void
address_changed (GMimeInternetAddress *ia, gpointer args, GMimeInternetAddressList *list)
{
	g_mime_event_emit (list->changed, NULL);
}

static void
g_mime_internet_address_list_finalize (GObject *object)
{
	GMimeInternetAddressList *list = (GMimeInternetAddressList *) object;
	GMimeInternetAddress *ia;
	guint i;

	for (i = 0; i < list->array->len; i++) {
		GMimeInternetAddressPrivate *priv;

		ia = (GMimeInternetAddress *) list->array->pdata[i];
		priv = _g_mime_internet_address_get_private (ia);

		g_mime_event_remove (priv->changed, (GMimeEventCallback) address_changed, list);
		g_object_unref (ia);
	}

	g_mime_event_free (list->changed);

	g_ptr_array_free (list->array, TRUE);

	G_OBJECT_CLASS (list_parent_class)->finalize (object);
}


/**
 * g_mime_internet_address_list_new:
 *
 * Creates a new #GMimeInternetAddressList.
 *
 * Returns: a new #GMimeInternetAddressList.
 **/
GMimeInternetAddressList *
g_mime_internet_address_list_new (void)
{
	return g_object_new (GMIME_TYPE_INTERNET_ADDRESS_LIST, NULL);
}


/**
 * g_mime_internet_address_list_length:
 * @list: a #GMimeInternetAddressList
 *
 * Gets the length of the list.
 *
 * Returns: the number of #GMimeInternetAddress objects in the list.
 **/
int
g_mime_internet_address_list_length (GMimeInternetAddressList *list)
{
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), -1);

	return list->array->len;
}


/**
 * g_mime_internet_address_list_clear:
 * @list: a #GMimeInternetAddressList
 *
 * Clears the list of addresses.
 **/
void
g_mime_internet_address_list_clear (GMimeInternetAddressList *list)
{
	GMimeInternetAddress *ia;
	guint i;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));

	for (i = 0; i < list->array->len; i++) {
		GMimeInternetAddressPrivate *priv;

		ia = (GMimeInternetAddress *) list->array->pdata[i];
		priv = _g_mime_internet_address_get_private (ia);

		g_mime_event_remove (priv->changed, (GMimeEventCallback) address_changed, list);
		g_object_unref (ia);
	}

	g_ptr_array_set_size (list->array, 0);

	g_mime_event_emit (list->changed, NULL);
}


static int
_g_mime_internet_address_list_add (GMimeInternetAddressList *list, GMimeInternetAddress *ia)
{
	GMimeInternetAddressPrivate *priv = _g_mime_internet_address_get_private (ia);
	int index;

	g_mime_event_add (priv->changed, (GMimeEventCallback) address_changed, list);

	index = list->array->len;
	g_ptr_array_add (list->array, ia);

	return index;
}


/**
 * g_mime_internet_address_list_add:
 * @list: a #GMimeInternetAddressList
 * @ia: a #GMimeInternetAddress
 *
 * Adds an #GMimeInternetAddress to the #GMimeInternetAddressList.
 *
 * Returns: the index of the added #GMimeInternetAddress.
 **/
int
g_mime_internet_address_list_add (GMimeInternetAddressList *list, GMimeInternetAddress *ia)
{
	int index;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), -1);

	index = _g_mime_internet_address_list_add (list, ia);
	g_object_ref (ia);

	g_mime_event_emit (list->changed, NULL);

	return index;
}


/**
 * g_mime_internet_address_list_prepend:
 * @list: a #GMimeInternetAddressList
 * @prepend: a #GMimeInternetAddressList
 *
 * Inserts all of the addresses in @prepend to the beginning of @list.
 **/
void
g_mime_internet_address_list_prepend (GMimeInternetAddressList *list, GMimeInternetAddressList *prepend)
{
	GMimeInternetAddress *ia;
	guint len, count, i;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (prepend));
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));

	if (prepend->array->len == 0)
		return;

	count = prepend->array->len;
	len = list->array->len;
	g_return_if_fail (count <= G_MAXINT - len);
	g_ptr_array_set_size (list->array, len + count);
	memmove (list->array->pdata + count, list->array->pdata, sizeof (gpointer) * len);

	for (i = 0; i < count; i++) {
		GMimeInternetAddressPrivate *priv;

		ia = (GMimeInternetAddress *) prepend->array->pdata[i];
		priv = _g_mime_internet_address_get_private (ia);

		g_mime_event_add (priv->changed, (GMimeEventCallback) address_changed, list);
		list->array->pdata[i] = ia;
		g_object_ref (ia);
	}

	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_internet_address_list_append:
 * @list: a #GMimeInternetAddressList
 * @append: a #GMimeInternetAddressList
 *
 * Adds all of the addresses in @append to @list.
 **/
void
g_mime_internet_address_list_append (GMimeInternetAddressList *list, GMimeInternetAddressList *append)
{
	GMimeInternetAddress *ia;
	guint len, count, i;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (append));
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));

	len = list->array->len;
	count = append->array->len;
	g_return_if_fail (count <= G_MAXINT - len);
	g_ptr_array_set_size (list->array, len + count);

	for (i = 0; i < count; i++) {
		GMimeInternetAddressPrivate *priv;

		ia = (GMimeInternetAddress *) append->array->pdata[i];
		priv = _g_mime_internet_address_get_private (ia);

		g_mime_event_add (priv->changed, (GMimeEventCallback) address_changed, list);
		list->array->pdata[len + i] = ia;
		g_object_ref (ia);
	}

	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_internet_address_list_insert:
 * @list: a #GMimeInternetAddressList
 * @index: index to insert at
 * @ia: a #GMimeInternetAddress
 *
 * Inserts an #GMimeInternetAddress into the #GMimeInternetAddressList at the
 * specified index.
 **/
void
g_mime_internet_address_list_insert (GMimeInternetAddressList *list, int index, GMimeInternetAddress *ia)
{
	GMimeInternetAddressPrivate *priv;
	char *dest, *src;
	size_t n;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS (ia));
	g_return_if_fail (index >= 0);
	g_return_if_fail ((guint) index <= list->array->len);

	priv = _g_mime_internet_address_get_private (ia);

	g_mime_event_add (priv->changed, (GMimeEventCallback) address_changed, list);
	g_object_ref (ia);

	if ((guint) index < list->array->len) {
		g_ptr_array_set_size (list->array, list->array->len + 1);

		dest = ((char *) list->array->pdata) + (sizeof (void *) * (index + 1));
		src = ((char *) list->array->pdata) + (sizeof (void *) * index);
		n = list->array->len - index - 1;

		memmove (dest, src, (sizeof (void *) * n));
		list->array->pdata[index] = ia;
	} else {
		/* the easy case */
		g_ptr_array_add (list->array, ia);
	}

	g_mime_event_emit (list->changed, NULL);
}


/**
 * g_mime_internet_address_list_remove:
 * @list: a #GMimeInternetAddressList
 * @ia: a #GMimeInternetAddress
 *
 * Removes an #GMimeInternetAddress from the #GMimeInternetAddressList.
 *
 * Returns: %TRUE if the specified #GMimeInternetAddress was removed or
 * %FALSE otherwise.
 **/
gboolean
g_mime_internet_address_list_remove (GMimeInternetAddressList *list, GMimeInternetAddress *ia)
{
	int index;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), FALSE);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), FALSE);

	if ((index = g_mime_internet_address_list_index_of (list, ia)) == -1)
		return FALSE;

	g_mime_internet_address_list_remove_at (list, index);

	return TRUE;
}


/**
 * g_mime_internet_address_list_remove_at:
 * @list: a #GMimeInternetAddressList
 * @index: index to remove
 *
 * Removes an #GMimeInternetAddress from the #GMimeInternetAddressList at the
 * specified index.
 *
 * Returns: %TRUE if an #GMimeInternetAddress was removed or %FALSE
 * otherwise.
 **/
gboolean
g_mime_internet_address_list_remove_at (GMimeInternetAddressList *list, int index)
{
	GMimeInternetAddressPrivate *priv;
	GMimeInternetAddress *ia;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0, FALSE);

	if ((guint) index >= list->array->len)
		return FALSE;

	ia = list->array->pdata[index];
	priv = _g_mime_internet_address_get_private (ia);

	g_mime_event_remove (priv->changed, (GMimeEventCallback) address_changed, list);
	g_object_unref (ia);

	g_ptr_array_remove_index (list->array, index);

	g_mime_event_emit (list->changed, NULL);

	return TRUE;
}


/**
 * g_mime_internet_address_list_contains:
 * @list: a #GMimeInternetAddressList
 * @ia: a #GMimeInternetAddress
 *
 * Checks whether or not the specified #GMimeInternetAddress is contained
 * within the #GMimeInternetAddressList.
 *
 * Returns: %TRUE if the specified #GMimeInternetAddress is contained
 * within the specified #GMimeInternetAddressList or %FALSE otherwise.
 **/
gboolean
g_mime_internet_address_list_contains (GMimeInternetAddressList *list, GMimeInternetAddress *ia)
{
	return g_mime_internet_address_list_index_of (list, ia) != -1;
}


/**
 * g_mime_internet_address_list_index_of:
 * @list: a #GMimeInternetAddressList
 * @ia: a #GMimeInternetAddress
 *
 * Gets the index of the specified #GMimeInternetAddress inside the
 * #GMimeInternetAddressList.
 *
 * Returns: the index of the requested #GMimeInternetAddress within the
 * #GMimeInternetAddressList or %-1 if it is not contained within the
 * #GMimeInternetAddressList.
 **/
int
g_mime_internet_address_list_index_of (GMimeInternetAddressList *list, GMimeInternetAddress *ia)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (ia), -1);

	for (i = 0; i < list->array->len; i++) {
		if (g_mime_internet_address_equal (list->array->pdata[i], ia))
			return i;
	}

	return -1;
}


/**
 * g_mime_internet_address_list_get_address:
 * @list: a #GMimeInternetAddressList
 * @index: index of #GMimeInternetAddress to get
 *
 * Gets the #GMimeInternetAddress at the specified index.
 *
 * Returns: (transfer none): the #GMimeInternetAddress at the specified
 * index or %NULL if the index is out of range.
 **/
GMimeInternetAddress *
g_mime_internet_address_list_get_address (GMimeInternetAddressList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), NULL);
	g_return_val_if_fail (index >= 0, NULL);

	if ((guint) index >= list->array->len)
		return NULL;

	return list->array->pdata[index];
}


/**
 * g_mime_internet_address_list_set_address:
 * @list: a #GMimeInternetAddressList
 * @index: index of #GMimeInternetAddress to set
 * @ia: a #GMimeInternetAddress
 *
 * Sets the #GMimeInternetAddress at the specified index to @ia.
 **/
void
g_mime_internet_address_list_set_address (GMimeInternetAddressList *list, int index, GMimeInternetAddress *ia)
{
	GMimeInternetAddressPrivate *old_priv;
	GMimeInternetAddressPrivate *ia_priv;
	GMimeInternetAddress *old;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS (ia));
	g_return_if_fail (index >= 0);

	g_return_if_fail ((guint) index < list->array->len);

	if ((old = list->array->pdata[index]) == ia)
		return;

	g_object_ref (ia);

	old_priv = _g_mime_internet_address_get_private (old);
	g_mime_event_remove (old_priv->changed, (GMimeEventCallback) address_changed, list);
	g_object_unref (old);

	ia_priv = _g_mime_internet_address_get_private (ia);
	g_mime_event_add (ia_priv->changed, (GMimeEventCallback) address_changed, list);
	list->array->pdata[index] = ia;

	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_internet_address_list_clone:
 * @list: a #GMimeInternetAddressList
 *
 * Returns: (transfer full): a deep copy with independent addresses and routes.
 **/
GMimeInternetAddressList *
g_mime_internet_address_list_clone (GMimeInternetAddressList *list)
{
	GMimeInternetAddressList *clone;
	guint i;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), NULL);

	clone = g_mime_internet_address_list_new ();
	for (i = 0; i < list->array->len; i++) {
		GMimeInternetAddress *ia = g_mime_internet_address_clone (list->array->pdata[i]);

		g_mime_internet_address_list_add (clone, ia);
		g_object_unref (ia);
	}

	return clone;
}

/**
 * g_mime_internet_address_list_equal:
 * @list: a #GMimeInternetAddressList
 * @other: a #GMimeInternetAddressList
 *
 * Returns: whether both lists contain equal addresses in the same order.
 **/
gboolean
g_mime_internet_address_list_equal (GMimeInternetAddressList *list, GMimeInternetAddressList *other)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), FALSE);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (other), FALSE);

	if (list == other)
		return TRUE;
	if (list->array->len != other->array->len)
		return FALSE;
	for (i = 0; i < list->array->len; i++) {
		if (!g_mime_internet_address_equal (list->array->pdata[i], other->array->pdata[i]))
			return FALSE;
	}

	return TRUE;
}

/**
 * g_mime_internet_address_list_compare:
 * @list: a #GMimeInternetAddressList
 * @other: a #GMimeInternetAddressList
 *
 * Compares addresses lexicographically, using length to break a tied prefix.
 *
 * Returns: a negative number, zero, or a positive number.
 **/
int
g_mime_internet_address_list_compare (GMimeInternetAddressList *list, GMimeInternetAddressList *other)
{
	guint i;
	int result;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), 0);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (other), 0);

	for (i = 0; i < MIN (list->array->len, other->array->len); i++) {
		result = g_mime_internet_address_compare (list->array->pdata[i], other->array->pdata[i]);
		if (result)
			return result;
	}

	return (list->array->len > other->array->len) - (list->array->len < other->array->len);
}

static void
append_mailboxes (GMimeInternetAddressList *result, GMimeInternetAddressList *list)
{
	guint i;

	for (i = 0; i < list->array->len; i++) {
		GMimeInternetAddress *ia = list->array->pdata[i];

		if (GMIME_IS_MAILBOX_ADDRESS (ia)) {
			g_mime_internet_address_list_add (result, ia);
		} else if (GMIME_IS_GROUP_ADDRESS (ia)) {
			GMimeGroupAddressPrivate *priv = _g_mime_group_address_get_private (GMIME_GROUP_ADDRESS (ia));

			append_mailboxes (result, priv->members);
		}
	}
}

/**
 * g_mime_internet_address_list_get_mailboxes:
 * @list: a #GMimeInternetAddressList
 *
 * Flattens nested groups in depth-first order. The result owns references to
 * the original mailboxes, not copies, and is a snapshot of list membership.
 *
 * Returns: (transfer full): a list containing only mailboxes.
 **/
GMimeInternetAddressList *
g_mime_internet_address_list_get_mailboxes (GMimeInternetAddressList *list)
{
	GMimeInternetAddressList *result;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), NULL);

	result = g_mime_internet_address_list_new ();
	append_mailboxes (result, list);

	return result;
}


void
_g_mime_internet_address_list_to_string (const GMimeInternetAddressList *list, GMimeFormatOptions *options, guint32 flags,
				  size_t *linelen, GString *str)
{
	GMimeInternetAddress *ia;
	guint i;

	for (i = 0; i < list->array->len; i++) {
		ia = (GMimeInternetAddress *) list->array->pdata[i];

		GMIME_INTERNET_ADDRESS_GET_CLASS (ia)->to_string (ia, options, flags, linelen, str);
		flags &= ~GMIME_INTERNET_ADDRESS_FIRST;

		if (i + 1 < list->array->len) {
			g_string_append (str, ", ");
			*linelen += 2;
		}
	}
}

/**
 * g_mime_internet_address_list_to_string:
 * @list: list of internet addresses
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @encode: %TRUE if the address should be rfc2047 encoded
 *
 * Allocates a string buffer containing the rfc822 formatted addresses
 * in @list.
 *
 * Returns: (nullable): a string containing the list of addresses in rfc822
 * format or %NULL if no addresses are contained in the list.
 **/
char *
g_mime_internet_address_list_to_string (GMimeInternetAddressList *list, GMimeFormatOptions *options, gboolean encode)
{
	guint32 flags = encode ? GMIME_INTERNET_ADDRESS_ENCODE : 0;
	size_t linelen = 0;
	GString *str;

	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list), NULL);

	if (list->array->len == 0)
		return NULL;

	str = g_string_new ("");
	_g_mime_internet_address_list_to_string (list, options, flags, &linelen, str);

	return g_string_free (str, FALSE);
}


/**
 * g_mime_internet_address_list_encode:
 * @list: list of internet addresses
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @str: string to write to
 *
 * Writes the rfc2047-encoded rfc822 formatted addresses in @list to
 * @str, folding using the configured maximum line length and newline format.
 * The first address token remains on the initial line. Oversized tokens and
 * quoted strings are not split and may exceed the configured line length.
 **/
void
g_mime_internet_address_list_encode (GMimeInternetAddressList *list, GMimeFormatOptions *options, GString *str)
{
	guint32 flags = GMIME_INTERNET_ADDRESS_ENCODE | GMIME_INTERNET_ADDRESS_FOLD | GMIME_INTERNET_ADDRESS_FIRST;
	const char *newline;
	size_t linelen;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (str != NULL);

	newline = g_mime_format_options_get_newline (options);
	linelen = str->len;

	_g_mime_internet_address_list_to_string (list, options, flags, &linelen, str);

	g_string_append (str, newline);
}


static char *
decode_name (GMimeParserOptions *options, const char *name, size_t len, gboolean unquote,
	     const char **charset, gint64 offset)
{
	char *value, *buf = NULL;

	if (!g_utf8_validate (name, len, NULL)) {
		/* A (broken) mailer has sent us raw 8bit/multibyte text data... */
		buf = g_mime_utils_decode_8bit (options, name, len);
	} else {
		buf = g_strndup (name, len);
	}

	if (unquote) {
		/* Unquote before decoding so quotes and backslashes inside encoded-words survive. */
		g_mime_utils_unquote_string (buf);
		if (strpbrk (buf, "\r\n")) {
			char *unfolded = g_mime_utils_header_unfold (buf);
			g_free (buf);
			buf = unfolded;
		}
	}

	/* Normalize literal tabs before decoding so encoded tabs are preserved. */
	g_strdelimit (buf, "\t", ' ');
	value = _g_mime_utils_header_decode_phrase (options, buf, strlen (buf), charset, offset);
	g_strstrip (value);
	g_free (buf);

	return value;
}


typedef enum {
	ALLOW_MAILBOX = 1 << 0,
	ALLOW_GROUP   = 1 << 1,
	INTERNAL     = 1 << 2,
	ALLOW_ANY     = ALLOW_MAILBOX | ALLOW_GROUP
} AddressParserFlags;

GMimeDomainList *
_g_mime_domain_list_parse (const char **in)
{
	GMimeDomainList *route = g_mime_domain_list_new ();
	const char *inptr = *in;

	if (!skip_cfws (&inptr) || *inptr != '@')
		goto error;

	do {
		GString *domain = g_string_new (NULL);
		gboolean valid;

		inptr++;
		valid = _g_mime_parse_domain (domain, &inptr, ",:");

		if (!valid) {
			g_string_free (domain, TRUE);
			goto error;
		}

		g_ptr_array_add (route->array, g_string_free (domain, FALSE));

		if (!skip_cfws (&inptr))
			goto error;

		/* The obsolete source-route grammar permits null domains between commas. */
		while (*inptr == ',') {
			inptr++;

			if (!skip_cfws (&inptr))
				goto error;
		}
	} while (*inptr == '@');

	*in = inptr;

	return route;

 error:
	g_object_unref (route);
	*in = inptr;

	return NULL;
}

static gboolean
localpart_parse (GString *localpart, const char **in, GMimeRfcComplianceMode mode, gboolean skip_trailing_cfws)
{
	const char *inptr = *in;
	const char *word, *cfws;

	do {
		gboolean escaped_at = FALSE;
		const char *p;
		char *utf8;

		word = inptr;

		if (!skip_word (&inptr))
			goto error;

		if (*word == '"') {
			for (p = word; p < inptr; p++) {
				if (((guchar) *p < 32 && *p != '\t') || *p == 127)
					goto error;
			}
		} else if (mode == GMIME_RFC_COMPLIANCE_LOOSER) {
			/* Accept escaped '@' in local-parts without mistaking it for the domain separator.
			 * See https://github.com/jstedfast/MimeKit/issues/1043 */
			while (*inptr == '\\' && inptr[1] == '@') {
				escaped_at = TRUE;
				inptr += 2;
				if (!skip_atom (&inptr))
					break;
			}
		}

		if (!g_utf8_validate (word, inptr - word, NULL)) {
			if (mode == GMIME_RFC_COMPLIANCE_STRICT)
				goto error;
			utf8 = g_convert (word, inptr - word, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
		} else {
			utf8 = g_strndup (word, inptr - word);
		}
		if (escaped_at) {
			char **parts = g_strsplit (utf8, "\\@", -1);
			char *replaced = g_strjoinv ("%40", parts);

			g_string_append (localpart, replaced);
			g_strfreev (parts);
			g_free (replaced);
		} else {
			g_string_append (localpart, utf8);
		}
		g_free (utf8);

		cfws = inptr;
		if (!skip_cfws (&inptr))
			goto error;

		if (*inptr != '.') {
			if (!skip_trailing_cfws)
				inptr = cfws;
			break;
		}

		do {
			g_string_append_c (localpart, *inptr++);
			if (!skip_cfws (&inptr) || !*inptr)
				goto error;
		} while (mode == GMIME_RFC_COMPLIANCE_LOOSER && *inptr == '.');
		if (mode == GMIME_RFC_COMPLIANCE_LOOSER && *inptr == '@')
			break;
	} while (TRUE);

	*in = inptr;

	return TRUE;

 error:
	*in = inptr;

	return FALSE;
}

#define COMMA_GREATER_THAN_OR_SEMICOLON ",>;"

gboolean
_g_mime_internet_address_parse_addrspec (const char **in, const char *sentinels, GMimeRfcComplianceMode mode,
				      char **addrspec, int *at)
{
	const char *inptr = *in;
	GString *str;

	str = g_string_new ("");

	if (!localpart_parse (str, &inptr, mode, TRUE))
		goto error;

	if (*inptr == '\0' || strchr (sentinels, *inptr)) {
		*addrspec = g_string_free (str, FALSE);
		*in = inptr;
		*at = -1;
		return TRUE;
	}

	if (*inptr != '@')
		goto error;

	*at = str->len;
	g_string_append_c (str, *inptr++);

	if (*inptr == '\0')
		goto error;

	if (!skip_cfws (&inptr))
		goto error;

	if (*inptr == '\0')
		goto error;

	if (!_g_mime_parse_domain (str, &inptr, sentinels))
		goto error;

	/* Preserve the domain's original IDN representation; callers can convert it explicitly. */
	*addrspec = g_string_free (str, FALSE);
	*in = inptr;

	return TRUE;

 error:
	g_string_free (str, TRUE);
	*addrspec = NULL;
	*in = inptr;
	*at = -1;

	return FALSE;
}

// TODO: rename to angleaddr_parse??
static gboolean
mailbox_parse (GMimeParserOptions *options, const char **in, const char *name, GMimeInternetAddress **address)
{
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_address_compliance_mode (options);
	const char *inptr = *in;
	char *addrspec = NULL;
	GMimeDomainList *route = NULL;
	int at;

	/* skip over the '<' */
	inptr++;

	/* Note: check for excessive angle brackets like the example described in section 7.1.2 of rfc7103... */
	if (*inptr == '<') {
		if (mode == GMIME_RFC_COMPLIANCE_STRICT)
			goto error;

		do {
			inptr++;
		} while (*inptr == '<');
	}

	if (*inptr == '\0')
		goto error;

	if (!skip_cfws (&inptr))
		goto error;

	if (*inptr == '@') {
		if (!(route = _g_mime_domain_list_parse (&inptr)))
			goto error;

		if (*inptr != ':')
			goto error;

		inptr++;

		if (!skip_cfws (&inptr))
			goto error;
	}

	// Note: The only syntactically correct sentinel token here is the '>', but alas... to deal with the first
	// example in section 7.1.5 of rfc7103, we need to at least handle ',' as a sentinel and might as well handle
	// ';' as well in case the mailbox is within a group address.
	//
	// Example: <third@example.net, fourth@example.net>
	if (!_g_mime_internet_address_parse_addrspec (&inptr, COMMA_GREATER_THAN_OR_SEMICOLON, mode, &addrspec, &at))
		goto error;

	if (!skip_cfws (&inptr))
		goto error;

	if (*inptr != '>') {
		if (mode == GMIME_RFC_COMPLIANCE_STRICT)
			goto error;
	} else {
		/* skip over the '>' */
		inptr++;

		/* Note: check for excessive angle brackets like the example described in section 7.1.2 of rfc7103... */
		if (*inptr == '>') {
			if (mode == GMIME_RFC_COMPLIANCE_STRICT)
				goto error;

			do {
				inptr++;
			} while (*inptr == '>');
		}
	}

	*address = _g_mime_mailbox_address_new (name, addrspec, at, route);
	g_clear_object (&route);
	g_free (addrspec);
	*in = inptr;

	return TRUE;

 error:
	g_free (addrspec);
	g_clear_object (&route);
	*address = NULL;
	*in = inptr;

	return FALSE;
}

static gboolean address_list_parse (GMimeInternetAddressList *list, GMimeParserOptions *options, AddressParserFlags flags,
				    const char **in, gboolean is_group, int group_depth, gint64 offset);

static gboolean
group_parse (GMimeGroupAddress *group, GMimeParserOptions *options, AddressParserFlags flags,
	     const char **in, int group_depth, gint64 offset)
{
	GMimeGroupAddressPrivate *priv = _g_mime_group_address_get_private (group);
	const char *inptr = *in;
	gboolean valid;

	/* skip over the ':' */
	inptr++;

	/* ignore superfluous colons (and whitespace) */
	while (*inptr == ':' || is_lwsp (*inptr))
		inptr++;

	valid = address_list_parse (priv->members, options, flags | ALLOW_ANY, &inptr, TRUE, group_depth, offset);

	if (!valid) {
		if (!(flags & INTERNAL)) {
			*in = inptr;
			return FALSE;
		}

		g_mime_internet_address_list_clear (priv->members);
	}

	if (*inptr != ';') {
		if (!(flags & INTERNAL) && g_mime_parser_options_get_address_compliance_mode (options) == GMIME_RFC_COMPLIANCE_STRICT) {
			*in = inptr;
			return FALSE;
		}

		while (*inptr && *inptr != ';')
			inptr++;
	} else {
		inptr++;
	}

	*in = inptr;

	return TRUE;
}

static gboolean
skip_phrase_atom (const char **in)
{
	const char *start = *in;

	while (is_atom (**in) || **in == '[' || **in == ']' || **in == ')')
		(*in)++;

	return *in > start;
}

static gboolean
address_parse (GMimeParserOptions *options, AddressParserFlags flags, const char **in, const char **charset, GMimeInternetAddress **address, int group_depth, gint64 offset)
{
	GMimeRfcComplianceMode mode = g_mime_parser_options_get_address_compliance_mode (options);
	gboolean trim_leading_quote = FALSE;
	const char *inptr = *in;
	const char *start;
	size_t length;

	if (!skip_cfws (&inptr) || *inptr == '\0')
		goto error;

	/* keep track of the start & length of the phrase */
	start = inptr;
	length = 0;

	while (*inptr) {
		gboolean quoted = *inptr == '"';

		if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
			if (!skip_word (&inptr))
				break;
		} else if (*inptr == '"') {
			const char *qstring = inptr;

			if (!skip_quoted (&inptr)) {
				inptr = qstring + 1;

				skip_lwsp (&inptr);

				if (!skip_phrase_atom (&inptr))
					break;

				if (start == qstring)
					trim_leading_quote = TRUE;
			}
		} else {
			if (!skip_phrase_atom (&inptr))
				break;
		}

		length = (size_t) (inptr - start);

		do {
			if (!skip_cfws (&inptr))
				goto error;

			/* Note: some clients don't quote dots in the name */
			if (*inptr != '.')
				break;

			inptr++;

			length = (size_t) (inptr - start);
		} while (TRUE);

		/* Note: some clients don't quote commas in the name */
		if (g_mime_parser_options_get_allow_unquoted_commas_in_addresses (options) && *inptr == ',' && !quoted) {
			inptr++;

			length = (size_t) (inptr - start);

			if (!skip_cfws (&inptr))
				goto error;
		}
	}

	if (!skip_cfws (&inptr))
		goto error;

	// specials    =  "(" / ")" / "<" / ">" / "@"  ; Must be in quoted-
	//             /  "," / ";" / ":" / "\" / <">  ;  string, to use
	//             /  "." / "[" / "]"              ;  within a word.

	if (*inptr == '\0' || *inptr == ',' || *inptr == '>' || *inptr == ';') {
		/* we've completely gobbled up an addr-spec w/o a domain */
		char sentinel = *inptr ? *inptr : ',';
		char *name, *addrspec;
		GString *localpart;
		int at = -1;

		/* rewind back to the beginning of the local-part */
		inptr = start;

		if (!(flags & ALLOW_MAILBOX))
			goto error;

		if (!g_mime_parser_options_get_allow_addresses_without_domain (options))
			goto error;

		localpart = g_string_new (NULL);
		if (!localpart_parse (localpart, &inptr, mode, FALSE)) {
			g_string_free (localpart, TRUE);
			goto error;
		}
		addrspec = g_string_free (localpart, FALSE);

		skip_lwsp (&inptr);

		if (*inptr == '(') {
			const char *comment = inptr;

			if (!skip_comment (&inptr)) {
				g_free (addrspec);
				goto error;
			}

			comment++;

			name = decode_name (options, comment, (size_t) ((inptr - 1) - comment), FALSE, charset, offset);
			if (!skip_cfws (&inptr)) {
				g_free (addrspec);
				g_free (name);
				goto error;
			}
		} else {
			name = g_strdup ("");
		}

		if (*inptr == '>') {
			if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
				g_free (addrspec);
				g_free (name);
				goto error;
			}

			inptr++;
		}

		if (*inptr && *inptr != sentinel) {
			g_free (addrspec);
			g_free (name);
			goto error;
		}

		*address = _g_mime_mailbox_address_new (name, addrspec, at, NULL);
		g_free (addrspec);
		g_free (name);
		*in = inptr;

		return TRUE;
	}

	if (*inptr == ':') {
		/* rfc2822 group address */
		GMimeGroupAddress *group;
		const char *phrase = start;
		gboolean retval;
		char *name;

		if (!(flags & ALLOW_GROUP))
			goto error;

		if (group_depth >= g_mime_parser_options_get_max_address_group_depth (options))
			goto error;

		if (trim_leading_quote) {
			phrase++;
			length--;
		}

		if (length > 0) {
			name = decode_name (options, phrase, length, TRUE, charset, offset);
		} else {
			name = g_strdup ("");
		}

		group = (GMimeGroupAddress *) g_mime_group_address_new (name);
		*address = (GMimeInternetAddress *) group;
		g_free (name);

		retval = group_parse (group, options, flags & INTERNAL, &inptr, group_depth + 1, offset);
		if (!retval)
			g_clear_object (address);
		*in = inptr;

		return retval;
	}

	if (!(flags & ALLOW_MAILBOX))
		goto error;

	if (*inptr == '@') {
		/* we're either in the middle of an addr-spec token or we completely gobbled up
		 * an addr-spec w/o a domain */
		char *name, *addrspec;
		int at;

		/* rewind back to the beginning of the local-part */
		inptr = start;

		if (!_g_mime_internet_address_parse_addrspec (&inptr, COMMA_GREATER_THAN_OR_SEMICOLON, mode, &addrspec, &at))
			goto error;

		skip_lwsp (&inptr);

		if (*inptr == '(') {
			const char *comment = inptr;

			if (!skip_comment (&inptr)) {
				g_free (addrspec);
				goto error;
			}

			comment++;

			name = decode_name (options, comment, (size_t) ((inptr - 1) - comment), FALSE, charset, offset);
		} else {
			name = g_strdup ("");
		}

		if (!skip_cfws (&inptr)) {
			g_free (addrspec);
			g_free (name);
			goto error;
		}

		if (*inptr == '\0') {
			*address = _g_mime_mailbox_address_new (name, addrspec, at, NULL);
			g_free (addrspec);
			g_free (name);
			*in = inptr;

			return TRUE;
		}

		if (*inptr == '<') {
			/* We have an address like "user@example.com <user@example.com>"; i.e. the name
			 * is an unquoted string with an '@'. */
			const char *end;

			if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
				g_free (addrspec);
				g_free (name);
				goto error;
			}

			end = inptr;
			while (end > start && is_lwsp (*(end - 1)))
				end--;

			length = (size_t) (end - start);
			g_free (addrspec);
			g_free (name);

			/* fall through to the rfc822 angle-addr token case... */
		} else {
			/* Note: since there was no '<', there should not be a '>'... but we handle it
			 * anyway in order to deal with the second Unbalanced Angle Brackets example in
			 * section 7.1.3: second@example.org> */
			if (*inptr == '>') {
				if (mode == GMIME_RFC_COMPLIANCE_STRICT) {
					g_free (addrspec);
					g_free (name);
					goto error;
				}

				inptr++;
			}

			*address = _g_mime_mailbox_address_new (name, addrspec, at, NULL);
			g_free (addrspec);
			g_free (name);
			*in = inptr;

			return TRUE;
		}
	}

	if (*inptr == '<') {
		/* rfc2822 angle-addr token */
		const char *phrase = start;
		gboolean retval;
		char *name;

		if (trim_leading_quote) {
			phrase++;
			length--;
		}

		if (length > 0) {
			name = decode_name (options, phrase, length, TRUE, charset, offset);
		} else {
			name = g_strdup ("");
		}

		retval = mailbox_parse (options, &inptr, name, address);
		g_free (name);
		*in = inptr;

		return retval;
	}

 error:
	if (g_mime_parser_options_get_warning_callback (options) != NULL)
		_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_ADDRESS_LIST, *in);

	*address = NULL;
	*in = inptr;

	return FALSE;
}

static gboolean
address_list_parse (GMimeInternetAddressList *list, GMimeParserOptions *options, AddressParserFlags flags,
		    const char **in, gboolean is_group, int group_depth, gint64 offset)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	GMimeInternetAddress *address;
	const char *charset;
	const char *inptr;

	if (!skip_cfws (in))
		return FALSE;

	inptr = *in;

	if (*inptr == '\0')
		return FALSE;

	while (*inptr) {
		gboolean separator_between_addrs = FALSE;
		const char *start = inptr;

		if (is_group && *inptr ==  ';')
			break;

		charset = NULL;

		if (!address_parse (options, flags, &inptr, &charset, &address, group_depth, offset)) {
			if (!(flags & INTERNAL)) {
				*in = inptr;
				return FALSE;
			}

			inptr = start;

			/* Resynchronize without treating separators inside quotes or comments as
			 * address boundaries. Leave the actual separator for the list/group parser. */
			while (*inptr && *inptr != ',' && (!is_group || *inptr != ';')) {
				if (*inptr == '"')
					skip_quoted (&inptr);
				else if (*inptr == '(')
					skip_comment (&inptr);
				else
					inptr++;
			}
		} else {
			_g_mime_internet_address_list_add (list, address);

			if (charset) {
				GMimeInternetAddressPrivate *priv = _g_mime_internet_address_get_private (address);

				priv->charset = g_strdup (charset);
			}
		}

		/* Note: we loop here in case there are any null addresses between commas */
		do {
			if (!skip_cfws (&inptr)) {
				*in = inptr;

				return FALSE;
			}

			if (*inptr != ',')
				break;

			separator_between_addrs = TRUE;
			inptr++;
		} while (TRUE);

		if (can_warn && !(separator_between_addrs || (*inptr == '\0') || (is_group && *inptr == ';')))
			_g_mime_parser_options_warn (options, offset, GMIME_WARN_INVALID_ADDRESS_LIST, *in);

		if (g_mime_parser_options_get_address_compliance_mode (options) == GMIME_RFC_COMPLIANCE_STRICT &&
		    !separator_between_addrs && *inptr && !(is_group && *inptr == ';')) {
			*in = inptr;
			return FALSE;
		}
	}

	*in = inptr;

	return TRUE;
}


/**
 * g_mime_internet_address_list_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @str: a string containing internet addresses
 *
 * Construct a list of internet addresses from the given string.
 * This legacy, best-effort API skips malformed entries. Use
 * g_mime_internet_address_list_parse_full() for all-or-nothing parsing.
 *
 * Returns: (nullable) (transfer full): a #GMimeInternetAddressList or %NULL if the
 * input string does not contain any addresses.
 **/
GMimeInternetAddressList *
g_mime_internet_address_list_parse (GMimeParserOptions *options, const char *str)
{
	return _g_mime_internet_address_list_parse (options, str, -1);
}

/**
 * g_mime_internet_address_list_parse_full:
 * @options: (nullable): parser options
 * @str: address-list text
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses the complete input without skipping malformed entries. Strict
 * compliance requires comma separators and terminated groups. Loose modes
 * retain address-recovery rules within individual mailboxes.
 *
 * Returns: (nullable) (transfer full): the parsed list, or %NULL on error.
 **/
GMimeInternetAddressList *
g_mime_internet_address_list_parse_full (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	GMimeInternetAddressList *list;
	const char *inptr;
	char *copy;

	if (!(copy = _g_mime_internet_address_copy_text (str, length, error)))
		return NULL;
	inptr = copy;
	list = g_mime_internet_address_list_new ();
	if (!address_list_parse (list, options, ALLOW_ANY, &inptr, FALSE, 0, -1) ||
	    !skip_cfws (&inptr) || *inptr || !list->array->len) {
		g_clear_object (&list);
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Invalid address list at byte offset %" G_GSIZE_FORMAT ".", (gsize) (inptr - copy));
	}
	g_free (copy);

	return list;
}

GMimeInternetAddress *
_g_mime_internet_address_parse (GMimeParserOptions *options, const char *str, gssize length, GType type, GError **error)
{
	GMimeInternetAddress *address = NULL;
	AddressParserFlags flags = ALLOW_ANY;
	const char *inptr, *charset = NULL;
	char *copy;

	if (!(copy = _g_mime_internet_address_copy_text (str, length, error)))
		return NULL;

	if (type == GMIME_TYPE_MAILBOX_ADDRESS)
		flags = ALLOW_MAILBOX;
	else if (type == GMIME_TYPE_GROUP_ADDRESS)
		flags = ALLOW_GROUP;

	inptr = copy;

	if (!address_parse (options, flags, &inptr, &charset, &address, 0, -1) || !skip_cfws (&inptr) || *inptr) {
		g_clear_object (&address);

		g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Invalid address at byte offset %" G_GSIZE_FORMAT ".", (gsize) (inptr - copy));
	} else if (charset) {
		GMimeInternetAddressPrivate *priv = _g_mime_internet_address_get_private (address);

		priv->charset = g_strdup (charset);
	}

	g_free (copy);

	return address;
}

GMimeInternetAddressList *
_g_mime_internet_address_list_parse (GMimeParserOptions *options, const char *str, gint64 offset)
{
	GMimeInternetAddressList *list;
	const char *inptr = str;

	g_return_val_if_fail (str != NULL, NULL);

	list = g_mime_internet_address_list_new ();

	if (!address_list_parse (list, options, ALLOW_ANY | INTERNAL, &inptr, FALSE, 0, offset) || list->array->len == 0) {
		g_object_unref (list);
		return NULL;
	}

	return list;
}

void
_g_mime_internet_address_list_append_parse (GMimeInternetAddressList *list, GMimeParserOptions *options, const char *str, gint64 offset)
{
	GMimeInternetAddressList *parsed;

	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (str != NULL);

	if ((parsed = _g_mime_internet_address_list_parse (options, str, offset))) {
		g_mime_internet_address_list_append (list, parsed);
		g_object_unref (parsed);
	}
}


/**
 * g_mime_internet_address_list_append_parse:
 * @list: a #GMimeInternetAddressList
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @str: a string containing internet addresses
 *
 * Adds the recoverable addresses in @str to @list. A failed header-list
 * parse leaves @list unchanged.
 **/
void
g_mime_internet_address_list_append_parse (GMimeInternetAddressList *list, GMimeParserOptions *options, const char *str)
{
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (list));
	g_return_if_fail (str != NULL);

	_g_mime_internet_address_list_append_parse (list, options, str, -1);
}
