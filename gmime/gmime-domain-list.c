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

#include "gmime-internet-address-private.h"
#include "gmime-events.h"
#include "gmime-error.h"
#include "gmime-parse-utils.h"
#include "gmime-common.h"

/**
 * SECTION: gmime-domain-list
 * @title: GMimeDomainList
 * @short_description: Mailbox source-route domains
 *
 * A mutable collection of domains. Changes propagate to the owning mailbox.
 **/

static void g_mime_domain_list_finalize (GObject *object);

G_DEFINE_TYPE (GMimeDomainList, g_mime_domain_list, G_TYPE_OBJECT)

static void
g_mime_domain_list_class_init (GMimeDomainListClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_domain_list_finalize;
}

static void
g_mime_domain_list_init (GMimeDomainList *list)
{
	list->array = g_ptr_array_new_with_free_func (g_free);
	list->changed = g_mime_event_new (list);
}

static void
g_mime_domain_list_finalize (GObject *object)
{
	GMimeDomainList *list = GMIME_DOMAIN_LIST (object);

	g_mime_event_free (list->changed);
	g_ptr_array_unref (list->array);
	G_OBJECT_CLASS (g_mime_domain_list_parent_class)->finalize (object);
}

/**
 * g_mime_domain_list_new:
 *
 * Returns: (transfer full): a new empty domain list.
 **/
GMimeDomainList *
g_mime_domain_list_new (void)
{
	return g_object_new (GMIME_TYPE_DOMAIN_LIST, NULL);
}

/**
 * g_mime_domain_list_clone:
 * @list: a #GMimeDomainList
 *
 * Returns: (transfer full): an independent copy of @list.
 **/
GMimeDomainList *
g_mime_domain_list_clone (GMimeDomainList *list)
{
	GMimeDomainList *clone;
	guint i;

	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), NULL);

	clone = g_mime_domain_list_new ();
	for (i = 0; i < list->array->len; i++)
		g_ptr_array_add (clone->array, g_strdup (list->array->pdata[i]));

	return clone;
}

/**
 * g_mime_domain_list_length:
 * @list: a #GMimeDomainList
 *
 * Returns: the number of domains, including empty entries.
 **/
int
g_mime_domain_list_length (GMimeDomainList *list)
{
	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), 0);

	return list->array->len;
}

/**
 * g_mime_domain_list_clear:
 * @list: a #GMimeDomainList
 *
 * Removes all domains.
 **/
void
g_mime_domain_list_clear (GMimeDomainList *list)
{
	g_return_if_fail (GMIME_IS_DOMAIN_LIST (list));

	g_ptr_array_set_size (list->array, 0);
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_domain_list_add:
 * @list: a #GMimeDomainList
 * @domain: a UTF-8 domain
 *
 * Appends @domain without trimming it.
 *
 * Returns: the index of the added domain.
 **/
int
g_mime_domain_list_add (GMimeDomainList *list, const char *domain)
{
	int index;

	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), -1);
	g_return_val_if_fail (domain != NULL && g_utf8_validate (domain, -1, NULL), -1);

	index = list->array->len;
	g_ptr_array_add (list->array, g_strdup (domain));
	g_mime_event_emit (list->changed, NULL);

	return index;
}

/**
 * g_mime_domain_list_insert:
 * @list: a #GMimeDomainList
 * @index: an index between zero and the list length, inclusive
 * @domain: a UTF-8 domain
 *
 * Inserts @domain at @index without trimming it.
 **/
void
g_mime_domain_list_insert (GMimeDomainList *list, int index, const char *domain)
{
	g_return_if_fail (GMIME_IS_DOMAIN_LIST (list));
	g_return_if_fail (index >= 0 && (guint) index <= list->array->len);
	g_return_if_fail (domain != NULL && g_utf8_validate (domain, -1, NULL));

	g_ptr_array_insert (list->array, index, g_strdup (domain));
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_domain_list_index_of:
 * @list: a #GMimeDomainList
 * @domain: a domain
 *
 * Returns: the first case-sensitive match, or -1.
 **/
int
g_mime_domain_list_index_of (GMimeDomainList *list, const char *domain)
{
	guint i;

	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), -1);
	g_return_val_if_fail (domain != NULL, -1);

	for (i = 0; i < list->array->len; i++) {
		if (!strcmp (list->array->pdata[i], domain))
			return i;
	}

	return -1;
}

/**
 * g_mime_domain_list_contains:
 * @list: a #GMimeDomainList
 * @domain: a domain
 *
 * Returns: whether @list contains a case-sensitive match for @domain.
 **/
gboolean
g_mime_domain_list_contains (GMimeDomainList *list, const char *domain)
{
	return g_mime_domain_list_index_of (list, domain) != -1;
}

/**
 * g_mime_domain_list_remove_at:
 * @list: a #GMimeDomainList
 * @index: the index to remove
 *
 * Returns: whether a domain was removed.
 **/
gboolean
g_mime_domain_list_remove_at (GMimeDomainList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0, FALSE);

	if ((guint) index >= list->array->len)
		return FALSE;

	g_ptr_array_remove_index (list->array, index);
	g_mime_event_emit (list->changed, NULL);

	return TRUE;
}

/**
 * g_mime_domain_list_remove:
 * @list: a #GMimeDomainList
 * @domain: a domain
 *
 * Removes the first case-sensitive match.
 *
 * Returns: whether a domain was removed.
 **/
gboolean
g_mime_domain_list_remove (GMimeDomainList *list, const char *domain)
{
	int index = g_mime_domain_list_index_of (list, domain);

	return index != -1 && g_mime_domain_list_remove_at (list, index);
}

/**
 * g_mime_domain_list_get_domain:
 * @list: a #GMimeDomainList
 * @index: an existing index
 *
 * Returns: (nullable) (transfer none): the domain, or %NULL for an invalid index.
 **/
const char *
g_mime_domain_list_get_domain (GMimeDomainList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), NULL);
	g_return_val_if_fail (index >= 0, NULL);

	return (guint) index < list->array->len ? list->array->pdata[index] : NULL;
}

/**
 * g_mime_domain_list_set_domain:
 * @list: a #GMimeDomainList
 * @index: an existing index
 * @domain: a UTF-8 domain
 *
 * Replaces an entry. Assigning the same value does not emit a change.
 **/
void
g_mime_domain_list_set_domain (GMimeDomainList *list, int index, const char *domain)
{
	char *copy;

	g_return_if_fail (GMIME_IS_DOMAIN_LIST (list));
	g_return_if_fail (index >= 0 && (guint) index < list->array->len);
	g_return_if_fail (domain != NULL && g_utf8_validate (domain, -1, NULL));

	if (!strcmp (list->array->pdata[index], domain))
		return;

	copy = g_strdup (domain);
	g_free (list->array->pdata[index]);
	list->array->pdata[index] = copy;
	g_mime_event_emit (list->changed, NULL);
}

/**
 * g_mime_domain_list_to_string:
 * @list: a #GMimeDomainList
 * @options: (nullable): formatting options
 * @encode: whether to encode international domain names
 *
 * Empty and Unicode-whitespace-only entries are omitted. Other entries are
 * preserved, prefixed with '@', and separated by commas.
 *
 * Returns: (transfer full): the formatted source route, without a colon.
 **/
char *
g_mime_domain_list_to_string (GMimeDomainList *list, GMimeFormatOptions *options, gboolean encode)
{
	gboolean idn = encode && !g_mime_format_options_get_allow_international (options);
	const char *domain, *p;
	GString *str;
	guint i;

	g_return_val_if_fail (GMIME_IS_DOMAIN_LIST (list), NULL);

	str = g_string_new (NULL);
	for (i = 0; i < list->array->len; i++) {
		char *ascii;

		domain = list->array->pdata[i];
		for (p = domain; *p && _g_mime_is_unicode_whitespace (g_utf8_get_char (p)); p = g_utf8_next_char (p))
			;
		if (!*p)
			continue;

		if (str->len)
			g_string_append_c (str, ',');
		g_string_append_c (str, '@');
		ascii = idn ? _g_mime_internet_address_encode_domain (domain) : g_strdup (domain);
		g_string_append (str, ascii);
		g_free (ascii);
	}

	return g_string_free (str, FALSE);
}

/**
 * g_mime_domain_list_parse:
 * @str: source-route text beginning with '@'
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Parses a complete domain list, allowing comments and empty comma entries.
 * Unlike mailbox source-route syntax, @str must not include a trailing colon.
 *
 * Returns: (nullable) (transfer full): the domains, or %NULL on error.
 **/
GMimeDomainList *
g_mime_domain_list_parse (const char *str, gssize length, GError **error)
{
	GMimeDomainList *list;
	const char *inptr;
	char *copy;

	copy = _g_mime_internet_address_copy_text (str, length, error);
	if (!copy)
		return NULL;
	inptr = copy;
	list = _g_mime_domain_list_parse (&inptr);
	if (!list || !skip_cfws (&inptr) || *inptr) {
		g_clear_object (&list);
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     "Invalid domain list at byte offset %" G_GSIZE_FORMAT ".", (gsize) (inptr - copy));
	}
	g_free (copy);

	return list;
}
