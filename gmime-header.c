/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-header.h"
#include "gmime-utils.h"
#include <string.h>
#include <ctype.h>

struct raw_header {
	struct raw_header *next;
	char *name;
	char *value;
};

struct _GMimeHeader {
	GHashTable *hash;
	struct raw_header *headers;
};


static gint
header_equal (gconstpointer v, gconstpointer v2)
{
	return g_strcasecmp ((const gchar *) v, (const gchar *) v2) == 0;
}

static guint
header_hash (gconstpointer key)
{
	const char *p = key;
	guint h = tolower (*p);
	
	if (h)
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + tolower (*p);
	
	return h;
}


/**
 * g_mime_header_new:
 *
 * Returns a new header object.
 **/
GMimeHeader *
g_mime_header_new ()
{
	GMimeHeader *new;
	
	new = g_new (GMimeHeader, 1);
	new->hash = g_hash_table_new (header_hash, header_equal);
	new->headers = NULL;
	
	return new;
}


/**
 * g_mime_header_destroy:
 * @header: header object
 *
 * Destroy the header object
 **/
void
g_mime_header_destroy (GMimeHeader *header)
{
	if (header) {
		struct raw_header *h, *n;
		
		h = header->headers;
		while (h) {
			g_free (h->name);
			g_free (h->value);
			n = h->next;
			g_free (h);
			h = n;
		}
		
		g_hash_table_destroy (header->hash);
		g_free (header);
	}
}


/**
 * g_mime_header_foreach:
 * @header: header object
 * @func: function to be called for each header.
 *
 * Calls @func for each header name/value pair.
 */
void
g_mime_header_foreach (const GMimeHeader *header, GMimeHeaderFunc func, gpointer data)
{
	const struct raw_header *h;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (header->hash != NULL);
	
	for (h = header->headers; h != NULL; h = h->next)
		(*func) (h->name, h->value, data);
}


/**
 * g_mime_header_set:
 * @header: header object
 * @name: header name
 * @value: header value (or %NULL to remove header)
 *
 * Set the value of the specified header. If @value is %NULL and the
 * header, @name, had not been previously set, a space will be set
 * aside for it (useful for setting the order of headers before values
 * can be obtained for them) otherwise the header will be removed.
 **/
void
g_mime_header_set (GMimeHeader *header, const gchar *name, const gchar *value)
{
	struct raw_header *h, *n;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (name != NULL);
	
	if ((h = g_hash_table_lookup (header->hash, name))) {
		if (value) {
			g_free (h->value);
			h->value = g_mime_utils_8bit_header_encode (value);
		} else {
			/* remove the header */
			g_hash_table_remove (header->hash, name);
			n = header->headers;
			
			if (h == n) {
				header->headers = h->next;
			} else {
				while (n->next != h)
					n = n->next;
				
				n->next = h->next;
			}
			
			g_free (h->name);
			g_free (h->value);
			g_free (h);
		}
	} else {
		n = g_new (struct raw_header, 1);
		n->next = NULL;
		n->name = g_strdup (name);
		n->value = value ? g_mime_utils_8bit_header_encode (value) : NULL;
		
		for (h = header->headers; h && h->next; h = h->next);
		if (h)
			h->next = n;
		else
			header->headers = n;
		
		g_hash_table_insert (header->hash, n->name, n);
	}
}


/**
 * g_mime_header_get:
 * @header: header object
 * @name: header name
 *
 * Returns the value of the header requested
 **/
const gchar *
g_mime_header_get (const GMimeHeader *header, const gchar *name)
{
	const struct raw_header *h;
	
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	h = g_hash_table_lookup (header->hash, name);
	
	return h ? h->value : NULL;
}


/**
 * g_mime_header_write_to_string:
 * @header: header object
 * @string: string
 *
 * Write the headers to a string
 **/
void
g_mime_header_write_to_string (const GMimeHeader *header, GString *string)
{
	struct raw_header *h;
	
	g_return_if_fail (header != NULL);
	g_return_if_fail (string != NULL);
	
	h = header->headers;
	while (h) {
		char *val;
		
		if (h->value) {
			val = g_mime_utils_header_printf ("%s: %s\n", h->name, h->value);
			g_string_append (string, val);
			g_free (val);
		}
		
		h = h->next;
	}
}


/**
 * g_mime_header_to_string:
 * @header: header object
 *
 * Returns a string containing the header block
 **/
gchar *
g_mime_header_to_string (const GMimeHeader *header)
{
	GString *string;
	gchar *str;
	
	g_return_val_if_fail (header != NULL, NULL);
	
	string = g_string_new ("");
	g_mime_header_write_to_string (header, string);
	str = string->str;
	g_string_free (string, FALSE);
	
	return str;
}
