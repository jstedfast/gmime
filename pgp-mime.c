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


/**
 * pgp_mime_part_is_rfc2015_signed:
 * @mime_part: MIME Part
 *
 * Returns TRUE if it is an rfc2015 multipart/signed.
 **/
gboolean
pgp_mime_part_is_rfc2015_signed (GMimePart *mime_part)
{
	GMimePart *multipart, *part;
	const GMimeContentType *type;
	const gchar *param;
	GList *child;
	int nparts;
	
	/* check that we have a multipart/signed */
	type = g_mime_part_get_content_type (mime_part);
	if (!g_mime_content_type_is_type (type, "multipart", "signed"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pgp-signed" */
	param = g_mime_content_type_get_parameter (type, "protocol");
	if (!param || g_strcasecmp (param, "\"application/pgp-signed\""))
		return FALSE;
	
	/* check that we have exactly 2 subparts */
	multipart = mime_part;
	nparts = g_list_length (multipart->children);
	if (nparts != 2)
		return FALSE;
	
	/* The first part may be of any type except for 
	 * application/pgp-signature - check it. */
	child = multipart->children;
	part = child->data;
	type = g_mime_part_get_content_type (part);
	if (g_mime_content_type_is_type (type, "application","pgp-signature"))
		return FALSE;
	
	/* The second part should be application/pgp-signature. */
	child = child->next;
	part = child->data;
	type = g_mime_part_get_content_type (part);
	if (!g_mime_content_type_is_type (type, "application","pgp-siganture"))
		return FALSE;
	
	/* FIXME: Implement multisig stuff */	
	
	return TRUE;
}


/**
 * pgp_mime_part_is_rfc2015_encrypted:
 * @mime_part: MIME Part
 *
 * Returns TRUE if it is an rfc2015 multipart/encrypted.
 **/
gboolean
pgp_mime_part_is_rfc2015_encrypted (GMimePart *mime_part)
{
	GMimePart *multipart, *part;
	const GMimeContentType *type;
	const gchar *param;
	GList *child;
	int nparts;
	
	/* check that we have a multipart/encrypted */
	type = g_mime_part_get_content_type (mime_part);
	if (!g_mime_content_type_is_type (type, "multipart", "encrypted"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pgp-encrypted" */
	param = g_mime_content_type_get_parameter (type, "protocol");
	if (!param || g_strcasecmp (param, "\"application/pgp-encrypted\""))
		return FALSE;
	
	/* check that we have exactly 2 subparts */
	multipart = mime_part;
	nparts = g_list_length (multipart->children);
	if (nparts != 2)
		return FALSE;
	
	/* The first part should be application/pgp-encrypted */
	child = multipart->children;
	part = child->data;
	type = g_mime_part_get_content_type (part);
	if (!g_mime_content_type_is_type (type, "application","pgp-encrypted"))
		return FALSE;
	
	/* The second part should be application/octet-stream - this
           is the one we care most about */
	child = child->next;
	part = child->data;
	type = g_mime_part_get_content_type (part);
	if (!g_mime_content_type_is_type (type, "application","octet-stream"))
		return FALSE;
	
	return TRUE;
}

static void
make_pgp_safe (GString *string, gboolean encode_from)
{
	gchar *ptr;
	
	ptr = string->str;
	while ((ptr = strchr (ptr, '\n'))) {
		g_string_insert_c (string, (ptr - string->str), '\r');
		ptr += 2;
		if (encode_from && !g_strncasecmp (ptr, "From ", 5)) {
			/* encode "From " as "From=20" */
			ptr += 4;
			g_string_erase (string, (ptr - string->str), 1);
			g_string_insert (string, (ptr - string->str),
					 "=20");
		}
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
	make_pgp_safe (string, TRUE);
	cleartext = string->str;
	g_string_free (string, FALSE);
	
	/* get the signature */
	signature = pgp_sign (cleartext, strlen (cleartext), userid, hash, ex);
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
	g_mime_part_set_content_type (multipart, mime_type);
	
	/* add the parts to the multipart */
	g_mime_part_add_subpart (multipart, part);
	g_mime_part_add_subpart (multipart, signed_part);
	
	/* replace the input part with the output part */
	*mime_part = multipart;
}


/**
 * pgp_mime_part_verify_signature:
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns TRUE if the signature is valid otherwise returns
 * FALSE. Note: you may want to check the exception if it fails as
 * there may be useful information to give to the user; example:
 * verification may have failed merely because the user doesn't have
 * the sender's key on her system.
 **/
gboolean
pgp_mime_part_verify_signature (GMimePart *mime_part, GMimeException *ex)
{
	const GMimeContentType *mime_type;
	GMimePart *content_part = NULL;
	GMimePart *signed_part = NULL;
	GMimePart *multipart, *part;
	gchar *content, *signature;
	GString *string;
	gboolean retval;
	GList *child;
	
	g_return_val_if_fail (mime_part != NULL, FALSE);
	
	/* make sure the mime part is a multipart/signed */
	multipart = mime_part;
	mime_type = g_mime_part_get_content_type (multipart);
	if (!g_mime_content_type_is_type (mime_type, "multipart", "signed"))
		return FALSE;
	
	child = multipart->children;
	g_return_val_if_fail (child != NULL, FALSE);
	
	/* get the data part - first part?? */
	content_part = child->data;
	child = child->next;
	
	/* get the signature part - last part?? */
	while (child) {
		part = child->data;
		mime_type = g_mime_part_get_content_type (part);
		if (g_mime_content_type_is_type (mime_type, "application", "pgp-signature")) {
			signed_part = part;
			break;
		}
		
		child = child->next;
	}
	
	if (!signed_part)
		return FALSE;
	
	content = g_mime_part_to_string (content_part, FALSE);
	string = g_string_new (content);
	g_free (content);
	make_pgp_safe (string, TRUE);
	
	retval = pgp_verify (string->str, string->len, signed_part->content->data,
			     signed_part->content->len, ex);
	
	g_string_free (string, TRUE);
	
	return retval;
}


/**
 * pgp_mime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
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
	gchar *cleartext, *ciphertext;
	GString *string;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (recipients != NULL);
	
	part = *mime_part;
	
	cleartext = g_mime_part_to_string (part, FALSE);
	string = g_string_new (cleartext);
	g_free (cleartext);
	
	/* pgp encrypt */
	make_pgp_safe (string, FALSE);
	ciphertext = pgp_encrypt (string->str, string->len, recipients, FALSE, NULL, ex);
	g_string_free (string, TRUE);
	if (g_mime_exception_is_set (ex))
		return;
	
	/* construct the version part */
	version_part = g_mime_part_new_with_type ("application", "pgp-encrypted");
	g_mime_part_set_content (version_part, "Version: 1", strlen ("Version: 1"));
	g_mime_part_set_encoding (version_part, GMIME_PART_ENCODING_7BIT);
	
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
	g_mime_part_set_content_type (multipart, mime_type);
	
	/* add the parts to the multipart */
	g_mime_part_add_subpart (multipart, version_part);
	g_mime_part_add_subpart (multipart, encrypted_part);
	
	/* replace the input part with the output part */
	*mime_part = multipart;
	
	/* destroy the original part */
	g_mime_part_destroy (part);
}


static void
strip (gchar *string, gchar c)
{
	/* strip all occurances of c from the string */
	gchar *src, *dst;
	
	if (!string)
		return;
	
	for (src = dst = string; *src; src++)
		if (*src != c)
			*dst++ = *src;
	*dst = '\0';
}


/**
 * pgp_mime_part_decrypt:
 * @mime_part: a multipart/encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
GMimePart *
pgp_mime_part_decrypt (GMimePart *mime_part, GMimeException *ex)
{
	GMimePart *multipart, *encrypted_part, *part;
	const GMimeContentType *mime_type;
	gchar *cleartext, *ciphertext = NULL;
	GList *child;
	gint outlen;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	
	/* make sure the mime part is a multipart/encrypted */
	multipart = mime_part;
	mime_type = g_mime_part_get_content_type (multipart);
	if (!g_mime_content_type_is_type (mime_type, "multipart", "encrypted"))
		return NULL;
	
	/* find the encrypted part - it should be the second part */
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
		return NULL;
	}
	
	/* get the cleartext */
	cleartext = pgp_decrypt (ciphertext, &outlen, ex);
	g_free (ciphertext);
	if (g_mime_exception_is_set (ex))
		return NULL;
	
	/* construct the new decrypted mime part */
	strip (cleartext, '\r');
	part = g_mime_parser_construct_part (cleartext, outlen);
	g_free (cleartext);
	
	/* return the decrypted mime part */
	return part;
}
