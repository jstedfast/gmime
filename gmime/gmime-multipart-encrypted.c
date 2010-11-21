/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2009 Jeffrey Stedfast
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
#include "gmime-filter-crlf.h"
#include "gmime-filter-from.h"
#include "gmime-filter-crlf.h"
#include "gmime-stream-mem.h"
#include "gmime-parser.h"
#include "gmime-part.h"
#include "gmime-error.h"


#define d(x)


/**
 * SECTION: gmime-multipart-encrypted
 * @title: GMimeMultpartEncrypted
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

/* GMimeObject class methods */
static void multipart_encrypted_prepend_header (GMimeObject *object, const char *header, const char *value);
static void multipart_encrypted_append_header (GMimeObject *object, const char *header, const char *value);
static void multipart_encrypted_set_header (GMimeObject *object, const char *header, const char *value);
static const char *multipart_encrypted_get_header (GMimeObject *object, const char *header);
static gboolean multipart_encrypted_remove_header (GMimeObject *object, const char *header);
static void multipart_encrypted_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *multipart_encrypted_get_headers (GMimeObject *object);
static ssize_t multipart_encrypted_write_to_stream (GMimeObject *object, GMimeStream *stream);


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
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_MULTIPART);
	
	gobject_class->finalize = g_mime_multipart_encrypted_finalize;
	
	object_class->prepend_header = multipart_encrypted_prepend_header;
	object_class->append_header = multipart_encrypted_append_header;
	object_class->remove_header = multipart_encrypted_remove_header;
	object_class->set_header = multipart_encrypted_set_header;
	object_class->get_header = multipart_encrypted_get_header;
	object_class->set_content_type = multipart_encrypted_set_content_type;
	object_class->get_headers = multipart_encrypted_get_headers;
	object_class->write_to_stream = multipart_encrypted_write_to_stream;
}

static void
g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mpe, GMimeMultipartEncryptedClass *klass)
{
	mpe->decrypted = NULL;
	mpe->validity = NULL;
}

static void
g_mime_multipart_encrypted_finalize (GObject *object)
{
	GMimeMultipartEncrypted *mpe = (GMimeMultipartEncrypted *) object;
	
	if (mpe->decrypted)
		g_object_unref (mpe->decrypted);
	
	if (mpe->validity)
		g_mime_signature_validity_free (mpe->validity);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
multipart_encrypted_prepend_header (GMimeObject *object, const char *header, const char *value)
{
	GMIME_OBJECT_CLASS (parent_class)->prepend_header (object, header, value);
}

static void
multipart_encrypted_append_header (GMimeObject *object, const char *header, const char *value)
{
	GMIME_OBJECT_CLASS (parent_class)->append_header (object, header, value);
}

static void
multipart_encrypted_set_header (GMimeObject *object, const char *header, const char *value)
{
	GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
}

static const char *
multipart_encrypted_get_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
}

static gboolean
multipart_encrypted_remove_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
multipart_encrypted_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}

static char *
multipart_encrypted_get_headers (GMimeObject *object)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_headers (object);
}

static ssize_t
multipart_encrypted_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	return GMIME_OBJECT_CLASS (parent_class)->write_to_stream (object, stream);
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
 * @ctx: encryption cipher context
 * @sign: %TRUE if the content should also be signed or %FALSE otherwise
 * @userid: user id to use for signing (only used if @sign is %TRUE)
 * @recipients: an array of recipients to encrypt to
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
				    GMimeCipherContext *ctx, gboolean sign,
				    const char *userid, GPtrArray *recipients,
				    GError **err)
{
	GMimeStream *filtered_stream, *ciphertext, *stream;
	GMimePart *version_part, *encrypted_part;
	GMimeContentType *content_type;
	GMimeDataWrapper *wrapper;
	GMimeFilter *crlf_filter;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), -1);
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (ctx->encrypt_protocol != NULL, -1);
	g_return_val_if_fail (GMIME_IS_OBJECT (content), -1);
	
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
	if (g_mime_cipher_context_encrypt (ctx, sign, userid, recipients, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		return -1;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	
	/* construct the version part */
	content_type = g_mime_content_type_new_from_string (ctx->encrypt_protocol);
	version_part = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	g_object_unref (content_type);
	
	content_type = g_mime_content_type_new_from_string (ctx->encrypt_protocol);
	g_mime_object_set_content_type (GMIME_OBJECT (version_part), content_type);
	g_mime_part_set_content_encoding (version_part, GMIME_CONTENT_ENCODING_7BIT);
	stream = g_mime_stream_mem_new_with_buffer ("Version: 1\n", strlen ("Version: 1\n"));
	wrapper = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content_object (version_part, wrapper);
	g_object_unref (wrapper);
	g_object_unref (stream);
	
	mpe->decrypted = content;
	g_object_ref (content);
	
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
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (mpe), "protocol",
						  ctx->encrypt_protocol);
	g_mime_multipart_set_boundary (GMIME_MULTIPART (mpe), NULL);
	
	return 0;
}


/**
 * g_mime_multipart_encrypted_decrypt:
 * @mpe: multipart/encrypted object
 * @ctx: decryption cipher context
 * @err: a #GError
 *
 * Attempts to decrypt the encrypted MIME part contained within the
 * multipart/encrypted object @mpe using the @ctx decryption context.
 *
 * If @validity is non-NULL, then on a successful decrypt operation,
 * it will be updated to point to a newly-allocated
 * #GMimeSignatureValidity with signature status information.
 *
 * Returns: the decrypted MIME part on success or %NULL on fail. If the
 * decryption fails, an exception will be set on @err to provide
 * information as to why the failure occured.
 **/
GMimeObject *
g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *mpe, GMimeCipherContext *ctx,
				    GError **err)
{
	GMimeObject *decrypted, *version, *encrypted;
	GMimeStream *stream, *ciphertext;
	GMimeStream *filtered_stream;
	GMimeContentType *mime_type;
	GMimeSignatureValidity *sv;
	GMimeDataWrapper *wrapper;
	GMimeFilter *crlf_filter;
	GMimeParser *parser;
	const char *protocol;
	char *content_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), NULL);
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	g_return_val_if_fail (ctx->encrypt_protocol != NULL, NULL);
	
	if (mpe->decrypted) {
		/* we seem to have already decrypted the part */
		return mpe->decrypted;
	}
	
	protocol = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mpe), "protocol");
	
	if (protocol) {
		/* make sure the protocol matches the cipher encrypt protocol */
		if (g_ascii_strcasecmp (ctx->encrypt_protocol, protocol) != 0) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     "Cannot decrypt multipart/encrypted part: unsupported encryption protocol '%s'.",
				     protocol);
			
			return NULL;
		}
	} else {
		/* *shrug* - I guess just go on as if they match? */
		protocol = ctx->encrypt_protocol;
	}
	
	version = g_mime_multipart_get_part (GMIME_MULTIPART (mpe), GMIME_MULTIPART_ENCRYPTED_VERSION);
	
	/* make sure the protocol matches the version part's content-type */
	content_type = g_mime_content_type_to_string (version->content_type);
	if (g_ascii_strcasecmp (content_type, protocol) != 0) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR, "%s",
			     "Cannot decrypt multipart/encrypted part: content-type does not match protocol.");
		
		g_free (content_type);
		
		return NULL;
	}
	g_free (content_type);
	
	/* get the encrypted part and check that it is of type application/octet-stream */
	encrypted = g_mime_multipart_get_part (GMIME_MULTIPART (mpe), GMIME_MULTIPART_ENCRYPTED_CONTENT);
	mime_type = g_mime_object_get_content_type (encrypted);
	if (!g_mime_content_type_is_type (mime_type, "application", "octet-stream")) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR, "%s",
			     "Cannot decrypt multipart/encrypted part: unexpected content type");
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	wrapper = g_mime_part_get_content_object (GMIME_PART (encrypted));
	ciphertext = g_mime_data_wrapper_get_stream (wrapper);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new (stream);
	crlf_filter = g_mime_filter_crlf_new (FALSE, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_object_unref (crlf_filter);
	
	/* get the cleartext */
	if (!(sv = g_mime_cipher_context_decrypt (ctx, ciphertext, filtered_stream, err))) {
		g_object_unref (filtered_stream);
		g_object_unref (stream);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	
	g_mime_stream_reset (stream);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	decrypted = g_mime_parser_construct_part (parser);
	g_object_unref (parser);
	
	if (!decrypted) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "%s",
			     "Cannot decrypt multipart/encrypted part: failed to parse decrypted content");
		
		g_mime_signature_validity_free (sv);
		
		return NULL;
	}
	
	/* cache the decrypted part */
	mpe->decrypted = decrypted;
	mpe->validity = sv;
	
	return decrypted;
}


/**
 * g_mime_multipart_encrypted_get_signature_validity:
 * @mpe: a #GMimeMultipartEncrypted
 *
 * Gets the signature validity of the encrypted MIME part.
 *
 * Note: This is only useful after calling
 * g_mime_multipart_encrypted_decrypt().
 *
 * Returns: a #GMimeSignatureValidity.
 **/
const GMimeSignatureValidity *
g_mime_multipart_encrypted_get_signature_validity (GMimeMultipartEncrypted *mpe)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), NULL);
	
	return mpe->validity;
}
