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

#include "gmime-pgp-mime.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-crlf.h"
#include "gmime-filter-from.h"
#include "gmime-stream-mem.h"
#include "gmime-parser.h"

#define d(x) x

/** rfc2015 stuff (aka PGP/MIME) *******************************/

gboolean
g_mime_pgp_mime_is_rfc2015_signed (GMimePart *mime_part)
{
	const GMimeContentType *type;
	GMimeDataWrapper *wrapper;
	GMimePart *part;
#ifdef ENABLE_PEDANTIC_PGPMIME
	const char *param, *micalg;
#endif
	int nparts;
	
	/* check that we have a multipart/signed */
	type = g_mime_part_get_content_type (mime_part);
	if (!g_mime_content_type_is_type (type, "multipart", "signed"))
		return FALSE;
	
#ifdef ENABLE_PEDANTIC_PGPMIME
	/* check that we have a protocol param with the value: "application/pgp-signature" */
	param = g_mime_content_type_get_parameter (type, "protocol");
	if (!param || strcasecmp (param, "application/pgp-signature"))
		return FALSE;
	
	/* check that we have a micalg parameter */
	micalg = g_mime_content_type_get_parameter (type, "micalg");
	if (!micalg)
		return FALSE;
#endif /* ENABLE_PEDANTIC_PGPMIME */
	
	/* check that we have exactly 2 subparts */
	nparts = g_list_length (mime_part->children);
	if (nparts != 2)
		return FALSE;
	
	/* The first part may be of any type except for 
	 * application/pgp-signature - check it. */
	part = GMIME_PART (mime_part->children->data);
	type = g_mime_part_get_content_type (part);
	if (g_mime_content_type_is_type (type, "application", "pgp-signature"))
		return FALSE;
	
	/* The second part should be application/pgp-signature. */
	part = GMIME_PART (mime_part->children->next->data);
	type = g_mime_part_get_content_type (part);
	if (!g_mime_content_type_is_type (type, "application", "pgp-signature"))
		return FALSE;
	
	return TRUE;
}

gboolean
g_mime_pgp_mime_is_rfc2015_encrypted (GMimePart *mime_part)
{
	const GMimeContentType *type;
	GMimeDataWrapper *wrapper;
	GMimePart *part;
#ifdef ENABLE_PEDANTIC_PGPMIME
	const char *param;
#endif
	int nparts;
	
	/* check that we have a multipart/encrypted */
	type = g_mime_part_get_content_type (mime_part);
	if (!g_mime_content_type_is_type (type, "multipart", "encrypted"))
		return FALSE;
	
#ifdef ENABLE_PEDANTIC_PGPMIME
	/* check that we have a protocol param with the value: "application/pgp-encrypted" */
	param = g_mime_content_type_get_parameter (type, "protocol");
	if (!param || g_strcasecmp (param, "application/pgp-encrypted"))
		return FALSE;
#endif /* ENABLE_PEDANTIC_PGPMIME */
	
	/* check that we have at least 2 subparts */
	nparts = g_list_length (mime_part->children);
	if (nparts < 2)
		return FALSE;
	
	/* The first part should be application/pgp-encrypted */
	part = GMIME_PART (mime_part->children->data);
	type = g_mime_part_get_content_type (part);
	if (!g_mime_content_type_is_type (type, "application", "pgp-encrypted"))
		return FALSE;
	
	/* The second part should be application/octet-stream - this
           is the one we care most about */
	part = GMIME_PART (mime_part->children->next->data);
	type = g_mime_part_get_content_type (part);
	if (!g_mime_content_type_is_type (type, "application", "octet-stream"))
		return FALSE;
	
	return TRUE;
}


static void
pgp_mime_part_sign_restore_part (GMimePart *mime_part, GSList **encodings)
{
	const GMimeContentType *type;
	
	type = g_mime_part_get_content_type (mime_part);
	
	if (g_mime_content_type_is_type (type, "multipart", "*")) {
		GList *lpart;
		
		lpart = mime_part->children;
		while (lpart) {
			GMimePart *part = GMIME_PART (lpart->data);
			
			pgp_mime_part_sign_restore_part (part, encodings);
			lpart = lpart->next;
		}
	} else {
		GMimePartEncodingType encoding;
		
		if (g_mime_content_type_is_type (type, "message", "rfc822")) {
			/* no-op - don't descend into sub-messages */
		} else {
			encoding = GPOINTER_TO_INT ((*encodings)->data);
			
			g_mime_part_set_encoding (mime_part, encoding);
			
			*encodings = (*encodings)->next;
		}
	}
}

static void
pgp_mime_part_sign_prepare_part (GMimePart *mime_part, GSList **encodings)
{
	const GMimeContentType *type;
	
	type = g_mime_part_get_content_type (mime_part);
	
	if (g_mime_content_type_is_type (type, "multipart", "*")) {
		GList *lpart;
		
		lpart = mime_part->children;
		while (lpart) {
			GMimePart *part = GMIME_PART (lpart->data);
			
			pgp_mime_part_sign_prepare_part (part, encodings);
			lpart = lpart->next;
		}
	} else {
		GMimePartEncodingType encoding;
		
		if (g_mime_content_type_is_type (type, "message", "rfc822")) {
			/* no-op - don't descend into sub-messages */
		} else {
			encoding = g_mime_part_get_encoding (mime_part);
			
			/* FIXME: find the best encoding for this part and use that instead?? */
			/* the encoding should really be QP or Base64 */
			if (encoding != GMIME_PART_ENCODING_BASE64)
				g_mime_part_set_encoding (mime_part, GMIME_PART_ENCODING_QUOTEDPRINTABLE);
			
			*encodings = g_slist_append (*encodings, GINT_TO_POINTER (encoding));
		}
	}
}


/**
 * g_mime_pgp_mime_part_sign:
 * @context: PGP Context
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of GMIME_PGP_HASH_TYPE_MD5 or GMIME_PGP_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces @part with the generated multipart/signed. On failure,
 * @ex will be set and #part will remain untouched.
 **/
void
g_mime_pgp_mime_part_sign (GMimePgpContext *context, GMimePart **mime_part, const char *userid,
			   GMimeCipherHash hash, GMimeException *ex)
{
	GMimePart *part, *multipart, *signed_part;
	GMimeContentType *mime_type;
	GMimeDataWrapper *wrapper;
	GMimeStream *filtered_stream;
	GMimeFilter *crlf_filter, *from_filter;
	GMimeStream *stream, *sigstream;
	GSList *encodings = NULL;
	char *hash_type = NULL;
	
	g_return_if_fail (mime_part != NULL);
	g_return_if_fail (GMIME_IS_PART (*mime_part));
	g_return_if_fail (userid != NULL);
	
	part = *mime_part;
	
	/* Prepare all the parts for signing... */
	pgp_mime_part_sign_prepare_part (part, &encodings);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	from_filter = g_mime_filter_from_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), from_filter);
	g_mime_part_write_to_stream (part, filtered_stream);
	g_mime_stream_unref (filtered_stream);
	
	/* reset the stream */
	g_mime_stream_reset (stream);
	
	/* construct the signature stream */
	sigstream = g_mime_stream_mem_new ();
	
	switch (hash) {
	case GMIME_CIPHER_HASH_MD5:
		hash_type = "pgp-md5";
		break;
	case GMIME_CIPHER_HASH_SHA1:
		hash_type = "pgp-sha1";
		break;
	default:
		/* set a reasonable default */
		hash = GMIME_CIPHER_HASH_SHA1;
		hash_type = "pgp-sha1";
		break;
	}
	
	/* get the signature */
	if (g_mime_pgp_sign (context, userid, hash, stream, sigstream, ex) == -1) {
		GSList *list;
		
		g_mime_stream_unref (stream);
		g_mime_stream_unref (sigstream);
		
		/* restore the original encoding */
		list = encodings;
		pgp_mime_part_sign_restore_part (part, &list);
		g_slist_free (encodings);
		return;
	}
	
	g_mime_stream_unref (stream);
	g_mime_stream_reset (sigstream);
	
	/* we don't need these anymore... */
	g_slist_free (encodings);
	
	/* construct the pgp-signature mime part */
	signed_part = g_mime_part_new_with_type ("application", "pgp-signature");
	wrapper = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (wrapper, sigstream);
	g_mime_stream_unref (sigstream);
	g_mime_part_set_content_object (signed_part, wrapper);
	g_mime_part_set_filename (signed_part, "signature.asc");
	
	/* construct the container multipart/signed */
	multipart = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("multipart", "signed");
	g_mime_content_type_add_parameter (mime_type, "micalg", hash_type);
	g_mime_content_type_add_parameter (mime_type, "protocol", "application/pgp-signature");
	g_mime_part_set_content_type (multipart, mime_type);
	g_mime_part_set_boundary (multipart, NULL);
	
	/* add the parts to the multipart */
	g_mime_part_add_subpart (multipart, part);
	g_mime_object_unref (GMIME_OBJECT (part));
	g_mime_part_add_subpart (multipart, signed_part);
	g_mime_object_unref (GMIME_OBJECT (signed_part));
	
	/* replace the input part with the output part */
	g_mime_object_unref (GMIME_OBJECT (*mime_part));
	*mime_part = multipart;
}


/**
 * g_mime_pgp_mime_part_verify:
 * @context: PGP Context
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns a GMimeCipherValidity on success or NULL on fail.
 **/
GMimeCipherValidity *
g_mime_pgp_mime_part_verify (GMimePgpContext *context, GMimePart *mime_part, GMimeException *ex)
{
	GMimeDataWrapper *wrapper;
	GMimePart *part, *multipart, *sigpart;
	GMimeStream *filtered_stream;
	GMimeFilter *crlf_filter, *from_filter;
	GMimeStream *stream, *sigstream;
	GMimeCipherValidity *valid;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if (!g_mime_pgp_mime_is_rfc2015_signed (mime_part))
		return NULL;
	
	multipart = mime_part;
	
	/* get the plain part */
	part = GMIME_PART (multipart->children->data);
	stream = g_mime_stream_mem_new ();
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	from_filter = g_mime_filter_from_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), from_filter);
	
	g_mime_part_write_to_stream (part, filtered_stream);
	g_mime_stream_unref (filtered_stream);
	g_mime_stream_reset (stream);
	
	/* get the signed part */
	sigpart = GMIME_PART (multipart->children->next->data);
	sigstream = g_mime_stream_mem_new ();
	wrapper = (GMimeDataWrapper *) g_mime_part_get_content_object (sigpart);
	g_mime_data_wrapper_write_to_stream (wrapper, sigstream);
	g_mime_stream_reset (sigstream);
	
	/* verify */
	valid = g_mime_pgp_verify (context, stream, sigstream, ex);
	
	g_mime_stream_unref (sigstream);
	g_mime_stream_unref (stream);
	
	return valid;
}


/**
 * g_mime_pgp_mime_part_encrypt:
 * @context: PGP Context
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
g_mime_pgp_mime_part_encrypt (GMimePgpContext *context, GMimePart **mime_part,
			      GPtrArray *recipients, GMimeException *ex)
{
	GMimePart *part, *multipart, *version_part, *encrypted_part;
	GMimeDataWrapper *wrapper;
	GMimeContentType *mime_type;
	GMimeStream *filtered_stream;
	GMimeFilter *crlf_filter;
	GMimeStream *stream, *ciphertext;
	
	g_return_if_fail (mime_part != NULL);
	g_return_if_fail (GMIME_IS_PART (*mime_part));
	g_return_if_fail (recipients != NULL);
	
	part = *mime_part;
	
	/* get the contents */
	stream = g_mime_stream_mem_new ();
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_mime_part_write_to_stream (part, filtered_stream);
	g_mime_stream_unref (filtered_stream);
	g_mime_stream_reset (stream);
	
	/* pgp encrypt */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_pgp_encrypt (context, FALSE, NULL, recipients, stream, ciphertext, ex) == -1) {
		g_mime_stream_unref (stream);
		g_mime_stream_unref (ciphertext);
		return;
	}
	
	g_mime_stream_unref (stream);
	g_mime_stream_reset (ciphertext);
	
	/* construct the version part */
	version_part = g_mime_part_new_with_type ("application", "pgp-encrypted");
	g_mime_part_set_encoding (version_part, GMIME_PART_ENCODING_7BIT);
	g_mime_part_set_content (version_part, "Version: 1\n", strlen ("Version: 1\n"));
	
	/* construct the pgp-encrypted mime part */
	encrypted_part = g_mime_part_new_with_type ("application", "octet-stream");
	wrapper = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (wrapper, ciphertext);
	g_mime_stream_unref (ciphertext);
	g_mime_part_set_content_object (encrypted_part, wrapper);
	g_mime_part_set_filename (encrypted_part, "encrypted.asc");
	g_mime_part_set_encoding (encrypted_part, GMIME_PART_ENCODING_7BIT);
	
	/* construct the container multipart/encrypted */
	multipart = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("multipart", "encrypted");
	g_mime_content_type_add_parameter (mime_type, "protocol", "application/pgp-encrypted");
	g_mime_part_set_content_type (multipart, mime_type);
	g_mime_part_set_boundary (multipart, NULL);
	
	/* add the parts to the multipart */
	g_mime_part_add_subpart (multipart, version_part);
	g_mime_object_unref (GMIME_OBJECT (version_part));
	g_mime_part_add_subpart (multipart, encrypted_part);
	g_mime_object_unref (GMIME_OBJECT (encrypted_part));
	
	/* replace the input part with the output part */
	g_mime_object_unref (GMIME_OBJECT (*mime_part));
	*mime_part = multipart;
}


/**
 * g_mime_pgp_mime_part_decrypt:
 * @context: PGP Context
 * @mime_part: a multipart/encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
GMimePart *
g_mime_pgp_mime_part_decrypt (GMimePgpContext *context, GMimePart *mime_part, GMimeException *ex)
{
	GMimePart *encrypted_part, *multipart, *part;
	const GMimeContentType *mime_type;
	GMimeStream *stream, *ciphertext;
	GMimeStream *filtered_stream;
	GMimeFilter *crlf_filter;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	/* make sure the mime part is a multipart/encrypted */
	if (!g_mime_pgp_mime_is_rfc2015_encrypted (mime_part))
		return NULL;
	
	multipart = mime_part;
	
	/* get the encrypted part (second part) */
	encrypted_part = GMIME_PART (multipart->children->next->data);
	mime_type = g_mime_part_get_content_type (encrypted_part);
	if (!g_mime_content_type_is_type (mime_type, "application", "octet-stream"))
		return NULL;
	
	/* get the ciphertext */
	ciphertext = g_mime_stream_mem_new ();
	g_mime_part_write_to_stream (encrypted_part, ciphertext);
	g_mime_stream_reset (ciphertext);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	if (g_mime_pgp_decrypt (context, ciphertext, stream, ex) == -1) {
		g_mime_stream_unref (ciphertext);
		g_mime_stream_unref (stream);
		return NULL;
	}
	
	g_mime_stream_unref (ciphertext);
	g_mime_stream_reset (stream);
	
	/* construct the new decrypted mime part from the stream */
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_DECODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	g_mime_stream_unref (stream);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	
	part = g_mime_parser_construct_part (filtered_stream);
	g_mime_stream_unref (filtered_stream);
	
	return part;
}
