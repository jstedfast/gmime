/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <config.h>

#include "internet-address.h"
#include "gmime-utils.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>


/**
 * internet_address_new: Create a new Internet Address object
 * @name: person's name
 * @address: person's address
 * 
 * Creates a new Internet Address object.
 **/
InternetAddress *
internet_address_new (const gchar *name, const gchar *address)
{
	InternetAddress *ia;
	
	g_return_val_if_fail (address != NULL, NULL);
	
	ia = g_new (InternetAddress, 1);
	
	if (name)
		ia->name = g_mime_utils_8bit_header_decode (name);
	else
		ia->name = NULL;
	
	ia->address = g_strdup (address);
	
	return ia;
}

static gchar *
get_next_token (const gchar *in, guint inlen, guint *len)
{
	gchar *token;
	gchar *inptr, *inend, *end;
	gint depth = 0;
	gchar dp = '\0', dm = '\0';
	
	inptr = (gchar *) in;
	inend = inptr + inlen;
	
	while (isspace (*inptr) && inptr < inend)
		inptr++;
	
	if (*inptr == '"') {
		dm = '"';
		depth = 1;
	} else if (*inptr == '(') {
		dp = '(';
		dm = ')';
		depth = 1;
	}
	
	end = inptr;
	while (end < inend) {
		end++;
		
		if (*end == dp)
			depth++;
		else if (*end == dm)
			depth--;
		else if (!depth && isspace (*end))
			break;
	}
	
	token = g_strndup (inptr, end - inptr);
	*len = end - in;
	
	return token;
}

static GPtrArray *
rfc822_tokenize (const gchar *in, guint inlen)
{
	GPtrArray *tokens;
	gchar *token, *inptr, *inend;
	guint len;
	
	inptr = (gchar *) in;
	inend = inptr + inlen;
	
	tokens = g_ptr_array_new ();
	
	while (inptr < inend) {
		token = get_next_token (inptr, inend - inptr, &len);
		inptr += len;
		g_ptr_array_add (tokens, token);
	}
	
	return tokens;
}

/**
 * internet_address_new_from_string: Create a new Internet Address object
 * @string: rfc822 internet address string
 * 
 * Create a new Internet Address object based upon the rfc822 address
 * string.
 **/
InternetAddress *
internet_address_new_from_string (const gchar *string)
{
	InternetAddress *ia;
	GPtrArray *tokens;
	gchar *name = NULL, *address = NULL;
	int i;
	
	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (*string != '\0', NULL);
	
	tokens = rfc822_tokenize (string, strlen (string));
	if (!tokens->len) {
		g_ptr_array_free (tokens, TRUE);
		return NULL;
	}
	
	/* find the address */
	for (i = 0; i < tokens->len; i++) {
		gchar *token = tokens->pdata[i];
		
		if (*token == '<' && *(token + strlen (token) - 1) == '>') {
			address = token;
			
			/* strip the <>'s */
			memmove (address, address + 1, strlen (address));
			*(address + strlen (address) - 1) = '\0';
			
			/* remove the address from the list of tokens */
			g_ptr_array_remove_index (tokens, i);
			break;
		}
	}
	
	if (!address) {
		/* the first token should be the address if it wasn't surrounded in <>'s */
		address = tokens->pdata[0];
		g_ptr_array_remove_index (tokens, 0);
	}
	
	/* recreate the name from the tokens */
	if (tokens->len)
		name = g_strjoinv (" ", (gchar **) tokens->pdata);
	
	for (i = 0; i < tokens->len; i++)
		g_free (tokens->pdata[i]);
	g_ptr_array_free (tokens, TRUE);
	
	ia = internet_address_new (name, address);
	g_free (name);
	g_free (address);
	
	return ia;
}


/**
 * internet_address_destroy: Destroy an Internet Address object
 * @ia: Internet Address object to destroy
 * 
 * Destroy the InternetAddress object pointed to by #ia.
 **/
void
internet_address_destroy (InternetAddress *ia)
{
	g_return_if_fail (ia != NULL);
	
	g_free (ia->name);
	g_free (ia->address);
	g_free (ia);
}

static gchar *
encoded_name (const gchar *raw, gboolean rfc2047_encode)
{
	gchar *name;
	
	g_return_val_if_fail (raw != NULL, NULL);
	
	if (rfc2047_encode && g_mime_utils_text_is_8bit (raw)) {
		name = g_mime_utils_8bit_header_encode_phrase (raw);
	} else {
		if (strchr (raw, ',') || strchr (raw, '.'))
			name = g_strdup_printf ("\"%s\"", raw);
		else
			name = g_strdup (raw);
	}
	
	return name;
}

/**
 * internet_address_to_string: Write the InternetAddress object to a string
 * @ia: Internet Address object
 * 
 * Writes the InternetAddress object to a string in rfc822 format.
 **/
gchar *
internet_address_to_string (InternetAddress *ia, gboolean rfc2047_encode)
{
	gchar *name, *string;
	
	g_return_val_if_fail (ia != NULL, NULL);
	
	name = encoded_name (ia->name, rfc2047_encode);
	if (ia->name)
		string = g_strdup_printf ("%s <%s>", name, ia->address);
	else
		string = g_strdup (ia->address);
	
	g_free (name);
	
	return string;
}
