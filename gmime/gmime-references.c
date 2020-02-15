/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include "gmime-parse-utils.h"
#include "gmime-references.h"


#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */


/**
 * SECTION: gmime-references
 * @title: GMimeReferences
 * @short_description: a list of Message-Ids
 * @see_also:
 *
 * A list of Message-Ids as found in a References or In-Reply-To header.
 **/

G_DEFINE_BOXED_TYPE (GMimeReferences, g_mime_references, g_mime_references_copy, g_mime_references_free);


/**
 * g_mime_references_new:
 *
 * Creates a new #GMimeReferences.
 *
 * Returns: a new #GMimeReferences.
 **/
GMimeReferences *
g_mime_references_new (void)
{
	GMimeReferences *refs;
	
	refs = g_malloc (sizeof (GMimeReferences));
	refs->array = g_ptr_array_new ();
	
	return refs;
}


/**
 * g_mime_references_free:
 * @refs: a #GMimeReferences list
 *
 * Frees the #GMimeReferences list.
 **/
void
g_mime_references_free (GMimeReferences *refs)
{
	guint i;
	
	g_return_if_fail (refs != NULL);
	
	for (i = 0; i < refs->array->len; i++)
		g_free (refs->array->pdata[i]);
	
	g_ptr_array_free (refs->array, TRUE);
	g_free (refs);
}


/**
 * g_mime_references_parse:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @text: string containing a list of msg-ids
 *
 * Decodes a list of msg-ids as in the References and/or In-Reply-To
 * headers defined in rfc822.
 *
 * Returns: (transfer full): a new #GMimeReferences containing the parsed message ids.
 **/
GMimeReferences *
g_mime_references_parse (GMimeParserOptions *options, const char *text)
{
	const char *word, *inptr = text;
	GMimeReferences *refs;
	char *msgid;
	
	g_return_val_if_fail (text != NULL, NULL);
	
	refs = g_mime_references_new ();
	
	while (*inptr) {
		skip_cfws (&inptr);
		if (*inptr == '<') {
			/* looks like a msg-id */
			if ((msgid = decode_msgid (&inptr))) {
				g_mime_references_append (refs, msgid);
				g_free (msgid);
			} else {
				w(g_warning ("Invalid References header: %s", inptr));
				break;
			}
		} else if (*inptr) {
			/* looks like part of a phrase */
			if (!(word = decode_word (&inptr))) {
				w(g_warning ("Invalid References header: %s", inptr));
				break;
			}
		}
	}
	
	return refs;
}


/**
 * g_mime_references_copy:
 * @refs: the list of references to copy
 *
 * Copies a #GMimeReferences list.
 *
 * Returns: (transfer full): a new #GMimeReferences list that contains
 * an identical list of items as @refs.
 **/
GMimeReferences *
g_mime_references_copy (GMimeReferences *refs)
{
	GMimeReferences *copy;
	guint i;
	
	g_return_val_if_fail (refs != NULL, NULL);
	
	copy = g_mime_references_new ();
	for (i = 0; i < refs->array->len; i++)
		g_mime_references_append (copy, refs->array->pdata[i]);
	
	return copy;
}


/**
 * g_mime_references_length:
 * @refs: a #GMimeReferences
 *
 * Gets the length of the #GMimeReferences list.
 *
 * Returns: the number of message ids in the list.
 **/
int
g_mime_references_length (GMimeReferences *refs)
{
	g_return_val_if_fail (refs != NULL, 0);
	
	return refs->array->len;
}


/**
 * g_mime_references_append:
 * @refs: a #GMimeReferences
 * @msgid: a message-id string
 *
 * Appends a reference to msgid to the list of references.
 **/
void
g_mime_references_append (GMimeReferences *refs, const char *msgid)
{
	g_return_if_fail (refs != NULL);
	g_return_if_fail (msgid != NULL);
	
	g_ptr_array_add (refs->array, g_strdup (msgid));
}


/**
 * g_mime_references_clear:
 * @refs: a #GMimeReferences
 *
 * Clears the #GMimeReferences list.
 **/
void
g_mime_references_clear (GMimeReferences *refs)
{
	guint i;
	
	g_return_if_fail (refs != NULL);
	
	for (i = 0; i < refs->array->len; i++)
		g_free (refs->array->pdata[i]);
	
	g_ptr_array_set_size (refs->array, 0);
}


/**
 * g_mime_references_get_message_id:
 * @refs: a #GMimeReferences
 * @index: the index of the message id
 *
 * Gets the specified Message-Id reference from the #GMimeReferences.
 *
 * Returns: the Message-Id reference from the #GMimeReferences.
 **/
const char *
g_mime_references_get_message_id (GMimeReferences *refs, int index)
{
	g_return_val_if_fail (refs != NULL, NULL);
	g_return_val_if_fail (index >= 0, NULL);
	g_return_val_if_fail (index < refs->array->len, NULL);
	
	return refs->array->pdata[index];
}


/**
 * g_mime_references_set_message_id:
 * @refs: a #GMimeReferences
 * @index: the index of the message id
 * @msgid: the message id
 *
 * Sets the specified Message-Id reference from the #GMimeReferences.
 **/
void
g_mime_references_set_message_id (GMimeReferences *refs, int index, const char *msgid)
{
	char *buf;
	
	g_return_if_fail (refs != NULL);
	g_return_if_fail (index >= 0);
	g_return_if_fail (index < refs->array->len);
	
	buf = g_strdup (msgid);
	g_free (refs->array->pdata[index]);
	refs->array->pdata[index] = buf;
}
