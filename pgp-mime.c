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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pgp-utils.h"
#include "pgp-mime.h"

/**
 * pgp_mime_init:
 * @path: path to pgp program
 * @type: type of pgp program
 * @callback: the get-password callback
 * @data: user data
 *
 * Initializes pgp-utils (same as calling pgp_init with the same args)
 **/
void
pgp_mime_init (const gchar *path, PgpType type, PgpPasswdFunc callback, gpointer data)
{
	pgp_init (path, type, callback, data);
}


static void
crlf_filter (GString *string)
{
	gchar *ptr;
	
	ptr = string->str;
	while ((ptr = strchr (ptr, '\n'))) {
		g_string_insert_c (string, (ptr - string->str), '\r');
		ptr += 2;
	}
}
	

/**
 * pgp_mime_part_sign:
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of PGP_HASH_TYPE_MD5 or PGP_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
pgp_mime_part_sign (GMimePart **mime_part, const gchar *userid, PgpHashType hash, GMimeException *ex)
{
	GMimePart *multipart, *part, *signed_part;
	GMimePartEncodingType encoding;
	GMimeContentType *mime_type;
	gchar *cleartext, *signature;
	GString *string;
	gchar *hash_type;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (userid != NULL);
	g_return_if_fail (hash != PGP_HASH_TYPE_NONE);
	
	part = *mime_part;
	encoding = g_mime_part_get_encoding (part);
	if (encoding != GMIME_PART_ENCODING_BASE64)
		g_mime_part_set_encoding (part, GMIME_PART_ENCODING_QUOTEDPRINTABLE);
	
	/* get the cleartext */
	cleartext = g_mime_part_to_string (part, FALSE);
	string = g_string_new (cleartext);
	g_free (cleartext);
	crlf_filter (string);
	cleartext = string->str;
	g_string_free (string, FALSE);
	
	/* get the signature */
	signature = pgp_detached_clearsign (cleartext, userid, hash, ex);
	g_free (cleartext);
	if (g_mime_exception_is_set (ex)) {
		/* restore the original encoding */
		g_mime_part_set_encoding (part, encoding);
		return;
	}
	
	/* construct the pgp-signature mime part */
	signed_part = g_mime_part_new_with_type ("application", "pgp-signature");
	g_mime_part_set_encoding (signed_part, GMIME_PART_ENCODING_7BIT);
	g_mime_part_set_content_description (signed_part, "pgp signature");
	g_mime_part_set_content (signed_part, signature, strlen (signature));
	g_free (signature);
	
	/* construct the container multipart/signed */
	multipart = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("multipart", "signed");
	g_mime_part_set_boundary (multipart, NULL);
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_type = "pgp-md5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_type = "pgp-sha1";
		break;
	}
	g_mime_content_type_add_parameter (mime_type, "micalg", hash_type);
	g_mime_content_type_add_parameter (mime_type, "protocol", "application/pgp-signature");
	
	/* add the parts to the multipart */
	g_mime_part_add_child (multipart, part);
	g_mime_part_add_child (multipart, signed_part);
	
	/* replace the input part with the output part */
	*mime_part = multipart;
}


gboolean
pgp_mime_part_verify_signature (GMimePart *mime_part, const gchar *sign_key, GMimeException *ex)
{
	return FALSE;
}


/**
 * pgp_mime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @userid: userid to sign with
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
pgp_mime_part_encrypt (GMimePart **mime_part, const GPtrArray *recipients, GMimeException *ex)
{
	GMimePart *part, *multipart, *version_part, *encrypted_part;
	GMimeContentType *mime_type;
	gchar *ciphertext;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (recipients != NULL);
	
	part = *mime_part;
	
	/* pgp encrypt */
	ciphertext = pgp_encrypt (part->content->data, part->content->len, recipients, FALSE, ex);
	if (g_mime_exception_is_set (ex))
		return;
	
	/* construct the version part */
	version_part = g_mime_part_new_with_type ("application", "pgp-encrypted");
	g_mime_part_set_content (version_part, "Version: 1", strlen ("Version: 1"));
	
	/* construct the pgp-encrypted mime part */
	encrypted_part = g_mime_part_new_with_type ("application", "octet-stream");
	g_mime_part_set_encoding (encrypted_part, GMIME_PART_ENCODING_7BIT);
	g_mime_part_set_content_description (encrypted_part, "pgp encrypted part");
	g_mime_part_set_content (encrypted_part, ciphertext, strlen (ciphertext));
	g_free (ciphertext);
	
	/* construct the container multipart/signed */
	multipart = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("multipart", "encrypted");
	g_mime_part_set_boundary (multipart, NULL);
	g_mime_content_type_add_parameter (mime_type, "protocol", "application/pgp-encrypted");
	
	/* add the parts to the multipart */
	g_mime_part_add_child (multipart, version_part);
	g_mime_part_add_child (multipart, encrypted_part);
	
	/* replace the input part with the output part */
	*mime_part = multipart;
	
	/* destroy the original part */
	g_mime_part_destroy (part);
}

void
pgp_mime_part_decrypt (GMimePart **mime_part, GMimeException *ex)
{
	GMimePart *multipart, *encrypted_part, *part;
	const GMimeContentType *mime_type;
	gchar *cleartext, *ciphertext = NULL;
	GList *child;
	gint outlen;
	
	g_return_if_fail (*mime_part != NULL);
	
	/* make sure the mime part is a multipart/encrypted */
	multipart = *mime_part;
	mime_type = g_mime_part_get_content_type (multipart);
	if (!g_mime_content_type_is_type (mime_type, "multipart", "encrypted"))
		return;
	
	/* find the encrypted part */
	child = multipart->children;
	while (child) {
		encrypted_part = child->data;
		mime_type = g_mime_part_get_content_type (encrypted_part);
		if (g_mime_content_type_is_type (mime_type, "application", "octet-stream")) {
			guint len;
			
			ciphertext = (gchar *) g_mime_part_get_content (encrypted_part, &len);
			ciphertext = g_strndup (ciphertext, len);
			if (pgp_detect (ciphertext))
				break;
			
			g_free (ciphertext);
		}
		child = child->next;
	}
	
	/* check if we found it */
	if (!child || !encrypted_part || !ciphertext) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_INVALID_PARAM,
				       "No encrypted part found.");
		return;
	}
	
	/* get the cleartext */
	cleartext = pgp_decrypt (ciphertext, &outlen, ex);
	g_free (ciphertext);
	if (g_mime_exception_is_set (ex))
		return;
	
	/* construct the new decrypted mime part */
	/* FIXME: should this be application/octet-stream? or what?
	   It's not safe to assume text/plain so I guess it pretty
	   much has to default to application/octet-stream or at least
	   until I find a way to get a mime type from a stream. Maybe
	   I should look into gnome-vfs? And what encoding should it
	   default to? Probably nothing less than QP? Maybe Base64 is
	   better for now? */
	part = g_mime_part_new_with_type ("application", "octet-stream");
	g_mime_part_set_encoding (part, GMIME_PART_ENCODING_BASE64);
	g_mime_part_set_content (part, cleartext, outlen);
	g_free (cleartext);
	
	/* replace the mime part with the decrypted mime part */
	*mime_part = part;
	
	/* destroy the original multipart/encrypted part */
	g_mime_part_destroy (multipart);
}
