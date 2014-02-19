/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include <stdio.h>
#include <string.h>

#include "gmime-multipart-encrypted.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-filter-from.h"
#include "gmime-filter-crlf.h"
#include "gmime-stream-mem.h"
#include "gmime-parser.h"
#include "gmime-part.h"
#include "gmime-error.h"

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


/**
 * SECTION: gmime-multipart-encrypted
 * @title: GMimeMultipartEncrypted
 * @short_description: Encrypted MIME multiparts
 * @see_also: #GMimeMultipart
 *
 * A #GMimeMultipartEncrypted part is a special subclass of
 * #GMimeMultipart to make it easier to manipulate the
 * multipart/encrypted MIME type.
 **/


/* GObject class methods */
static void g_mime_multipart_encrypted_class_init (GMimeMultipartEncryptedClass *klass);
static void g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mps, GMimeMultipartEncryptedClass *klass);
static void g_mime_multipart_encrypted_finalize (GObject *object);


static GMimeMultipartClass *parent_class = NULL;


GType
g_mime_multipart_encrypted_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMultipartEncryptedClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_multipart_encrypted_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMultipartEncrypted),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_multipart_encrypted_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_MULTIPART, "GMimeMultipartEncrypted", &info, 0);
	}
	
	return type;
}


static void
g_mime_multipart_encrypted_class_init (GMimeMultipartEncryptedClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_MULTIPART);
	
	gobject_class->finalize = g_mime_multipart_encrypted_finalize;
}

static void
g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mpe, GMimeMultipartEncryptedClass *klass)
{
	
}

static void
g_mime_multipart_encrypted_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_multipart_encrypted_new:
 *
 * Creates a new MIME multipart/encrypted object.
 *
 * Returns: an empty MIME multipart/encrypted object.
 **/
GMimeMultipartEncrypted *
g_mime_multipart_encrypted_new (void)
{
	GMimeMultipartEncrypted *multipart;
	GMimeContentType *content_type;
	
	multipart = g_object_newv (GMIME_TYPE_MULTIPART_ENCRYPTED, 0, NULL);
	
	content_type = g_mime_content_type_new ("multipart", "encrypted");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), content_type);
	g_object_unref (content_type);
	
	return multipart;
}


/**
 * g_mime_multipart_encrypted_encrypt:
 * @mpe: multipart/encrypted object
 * @content: MIME part to encrypt
 * @ctx: encryption context
 * @sign: %TRUE if the content should also be signed or %FALSE otherwise
 * @userid: user id to use for signing (only used if @sign is %TRUE)
 * @digest: digest algorithm to use when signing
 * @recipients: (element-type utf8): an array of recipients to encrypt to
 * @err: a #GError
 *
 * Attempts to encrypt (and conditionally sign) the @content MIME part
 * to the public keys of @recipients using the @ctx encryption
 * context. If successful, the encrypted #GMimeObject is set as the
 * encrypted part of the multipart/encrypted object @mpe.
 *
 * Returns: %0 on success or %-1 on fail. If the encryption fails, an
 * exception will be set on @err to provide information as to why the
 * failure occured.
 **/
int
g_mime_multipart_encrypted_encrypt (GMimeMultipartEncrypted *mpe, GMimeObject *content,
				    GMimeCryptoContext *ctx, gboolean sign,
				    const char *userid, GMimeDigestAlgo digest,
				    GPtrArray *recipients, GError **err)
{
	GMimeStream *filtered_stream, *ciphertext, *stream;
	GMimePart *version_part, *encrypted_part;
	GMimeContentType *content_type;
	GMimeDataWrapper *wrapper;
	GMimeFilter *crlf_filter;
	const char *protocol;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), -1);
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_OBJECT (content), -1);
	
	if (!(protocol = g_mime_crypto_context_get_encryption_protocol (ctx))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("Encryption not supported."));
		return -1;
	}
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new (stream);
	
	crlf_filter = g_mime_filter_crlf_new (TRUE, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_object_unref (crlf_filter);
	
	g_mime_object_write_to_stream (content, filtered_stream);
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* encrypt the content stream */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_crypto_context_encrypt (ctx, sign, userid, digest, recipients, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		return -1;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	
	/* construct the version part */
	content_type = g_mime_content_type_new_from_string (protocol);
	version_part = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	g_object_unref (content_type);
	
	content_type = g_mime_content_type_new_from_string (protocol);
	g_mime_object_set_content_type (GMIME_OBJECT (version_part), content_type);
	g_mime_part_set_content_encoding (version_part, GMIME_CONTENT_ENCODING_7BIT);
	stream = g_mime_stream_mem_new_with_buffer ("Version: 1\n", strlen ("Version: 1\n"));
	wrapper = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content_object (version_part, wrapper);
	g_object_unref (wrapper);
	g_object_unref (stream);
	
	/* construct the encrypted mime part */
	encrypted_part = g_mime_part_new_with_type ("application", "octet-stream");
	g_mime_part_set_content_encoding (encrypted_part, GMIME_CONTENT_ENCODING_7BIT);
	wrapper = g_mime_data_wrapper_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content_object (encrypted_part, wrapper);
	g_object_unref (ciphertext);
	g_object_unref (wrapper);
	
	/* save the version and encrypted parts */
	/* FIXME: make sure there aren't any other parts?? */
	g_mime_multipart_add (GMIME_MULTIPART (mpe), GMIME_OBJECT (version_part));
	g_mime_multipart_add (GMIME_MULTIPART (mpe), GMIME_OBJECT (encrypted_part));
	g_object_unref (encrypted_part);
	g_object_unref (version_part);
	
	/* set the content-type params for this multipart/encrypted part */
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (mpe), "protocol", protocol);
	g_mime_multipart_set_boundary (GMIME_MULTIPART (mpe), NULL);
	
	return 0;
}


static GMimeStream *
g_mime_data_wrapper_get_decoded_stream (GMimeDataWrapper *wrapper)
{
	GMimeStream *decoded_stream;
	GMimeFilter *decoder;
	
	g_mime_stream_reset (wrapper->stream);
	
	switch (wrapper->encoding) {
	case GMIME_CONTENT_ENCODING_BASE64:
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
	case GMIME_CONTENT_ENCODING_UUENCODE:
		decoder = g_mime_filter_basic_new (wrapper->encoding, FALSE);
		decoded_stream = g_mime_stream_filter_new (wrapper->stream);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (decoded_stream), decoder);
		g_object_unref (decoder);
		break;
	default:
		decoded_stream = wrapper->stream;
		g_object_ref (wrapper->stream);
		break;
	}
	
	return decoded_stream;
}


/**
 * g_mime_multipart_encrypted_decrypt:
 * @mpe: multipart/encrypted object
 * @ctx: decryption context
 * @result: a #GMimeDecryptionResult
 * @err: a #GError
 *
 * Attempts to decrypt the encrypted MIME part contained within the
 * multipart/encrypted object @mpe using the @ctx decryption context.
 *
 * If @result is non-%NULL, then on a successful decrypt operation, it will be
 * updated to point to a newly-allocated #GMimeDecryptResult with signature
 * status information as well as a list of recipients that the part was
 * encrypted to.
 *
 * Returns: (transfer full): the decrypted MIME part on success or
 * %NULL on fail. If the decryption fails, an exception will be set on
 * @err to provide information as to why the failure occured.
 **/
GMimeObject *
g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *mpe, GMimeCryptoContext *ctx,
				    GMimeDecryptResult **result, GError **err)
{
	GMimeObject *decrypted, *version, *encrypted;
	GMimeStream *stream, *ciphertext;
	const char *protocol, *supported;
	GMimeStream *filtered_stream;
	GMimeContentType *mime_type;
	GMimeDataWrapper *wrapper;
	GMimeFilter *crlf_filter;
	GMimeDecryptResult *res;
	GMimeParser *parser;
	char *content_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), NULL);
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	if (result)
		*result = NULL;
	
	protocol = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mpe), "protocol");
	supported = g_mime_crypto_context_get_encryption_protocol (ctx);
	
	if (protocol) {
		/* make sure the protocol matches the crypto encrypt protocol */
		if (!supported || g_ascii_strcasecmp (supported, protocol) != 0) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unsupported encryption protocol '%s'."),
				     protocol);
			
			return NULL;
		}
	} else if (supported != NULL) {
		/* *shrug* - I guess just go on as if they match? */
		protocol = supported;
	} else {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unspecified encryption protocol."));
		
		return NULL;
	}
	
	version = g_mime_multipart_get_part (GMIME_MULTIPART (mpe), GMIME_MULTIPART_ENCRYPTED_VERSION);
	
	/* make sure the protocol matches the version part's content-type */
	content_type = g_mime_content_type_to_string (version->content_type);
	if (g_ascii_strcasecmp (content_type, protocol) != 0) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: content-type does not match protocol."));
		
		g_free (content_type);
		
		return NULL;
	}
	g_free (content_type);
	
	/* get the encrypted part and check that it is of type application/octet-stream */
	encrypted = g_mime_multipart_get_part (GMIME_MULTIPART (mpe), GMIME_MULTIPART_ENCRYPTED_CONTENT);
	mime_type = g_mime_object_get_content_type (encrypted);
	if (!g_mime_content_type_is_type (mime_type, "application", "octet-stream")) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unexpected content type."));
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	wrapper = g_mime_part_get_content_object (GMIME_PART (encrypted));
	ciphertext = g_mime_data_wrapper_get_decoded_stream (wrapper);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new (stream);
	crlf_filter = g_mime_filter_crlf_new (FALSE, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_object_unref (crlf_filter);
	
	/* get the cleartext */
	if (!(res = g_mime_crypto_context_decrypt (ctx, ciphertext, filtered_stream, err))) {
		g_object_unref (filtered_stream);
		g_object_unref (ciphertext);
		g_object_unref (stream);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	g_object_unref (ciphertext);
	
	g_mime_stream_reset (stream);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	decrypted = g_mime_parser_construct_part (parser);
	g_object_unref (parser);
	
	if (!decrypted) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: failed to parse decrypted content."));
		
		g_object_unref (res);
		
		return NULL;
	}
	
	if (!result)
		g_object_unref (res);
	else
		*result = res;
	
	return decrypted;
}
