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

#include <stdio.h>
#include <string.h>

#include "gmime-multipart-encrypted.h"
#include "gmime-filter-dos2unix.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-filter-from.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
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
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART_ENCRYPTED, NULL);
	
	content_type = g_mime_content_type_new ("multipart", "encrypted");
	g_mime_object_set_content_type ((GMimeObject *) multipart, content_type);
	g_object_unref (content_type);
	
	return multipart;
}


/**
 * g_mime_multipart_encrypted_encrypt:
 * @ctx: a #GMimeCryptoContext
 * @entity: MIME part to encrypt
 * @sign: %TRUE if the content should also be signed or %FALSE otherwise
 * @userid: (nullable): user id to use for signing (only used if @sign is %TRUE)
 * @flags: a #GMimeEncryptFlags
 * @recipients: (element-type utf8): an array of recipients to encrypt to
 * @err: a #GError
 *
 * Attempts to encrypt (and conditionally sign) the @entity MIME part
 * to the public keys of @recipients using the @ctx encryption
 * context. If successful, a new multipart/encrypted object is returned.
 *
 * Returns: (nullable) (transfer full): a new #GMimeMultipartEncrypted object on success
 * or %NULL on fail. If encrypting fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeMultipartEncrypted *
g_mime_multipart_encrypted_encrypt (GMimeCryptoContext *ctx, GMimeObject *entity, gboolean sign, const char *userid,
				    GMimeEncryptFlags flags, GPtrArray *recipients, GError **err)
{
	GMimePart *version_part, *encrypted_part;
	GMimeMultipartEncrypted *encrypted;
	GMimeStream *stream, *ciphertext;
	GMimeContentType *content_type;
	GMimeFormatOptions *options;
	GMimeDataWrapper *content;
	const char *protocol;
	
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_OBJECT (entity), NULL);
	
	if (!(protocol = g_mime_crypto_context_get_encryption_protocol (ctx))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("Encryption not supported."));
		return NULL;
	}
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	g_mime_object_write_to_stream (entity, options, stream);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* encrypt the content stream */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_crypto_context_encrypt (ctx, sign, userid, flags, recipients, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		return NULL;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	
	/* construct the version part */
	content_type = g_mime_content_type_parse (NULL, protocol);
	version_part = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	g_object_unref (content_type);
	
	g_mime_part_set_content_encoding (version_part, GMIME_CONTENT_ENCODING_7BIT);
	stream = g_mime_stream_mem_new_with_buffer ("Version: 1\n", strlen ("Version: 1\n"));
	content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content (version_part, content);
	g_object_unref (content);
	g_object_unref (stream);
	
	/* construct the encrypted mime part */
	encrypted_part = g_mime_part_new_with_type ("application", "octet-stream");
	g_mime_part_set_content_encoding (encrypted_part, GMIME_CONTENT_ENCODING_7BIT);
	content = g_mime_data_wrapper_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content (encrypted_part, content);
	g_object_unref (ciphertext);
	g_object_unref (content);
	
	/* save the version and encrypted parts */
	encrypted = g_mime_multipart_encrypted_new ();
	g_mime_multipart_add ((GMimeMultipart *) encrypted, (GMimeObject *) version_part);
	g_mime_multipart_add ((GMimeMultipart *) encrypted, (GMimeObject *) encrypted_part);
	g_object_unref (encrypted_part);
	g_object_unref (version_part);
	
	/* set the content-type params for this multipart/encrypted part */
	g_mime_object_set_content_type_parameter ((GMimeObject *) encrypted, "protocol", protocol);
	g_mime_multipart_set_boundary ((GMimeMultipart *) encrypted, NULL);
	
	return encrypted;
}


/**
 * g_mime_multipart_encrypted_decrypt:
 * @encrypted: a #GMimeMultipartEncrypted
 * @flags: a #GMimeDecryptFlags
 * @session_key: session key to use or %NULL
 * @result: (out) (transfer full): a #GMimeDecryptResult
 * @err: a #GError
 *
 * Attempts to decrypt the encrypted MIME part contained within the
 * multipart/encrypted object @encrypted.
 *
 * When non-%NULL, @session_key should be a %NULL-terminated string,
 * such as the one returned by g_mime_decrypt_result_get_session_key()
 * from a previous decryption. If the @session_key is not valid, decryption
 * will fail.
 *
 * If @result is non-%NULL, then on a successful decrypt operation, it will be
 * updated to point to a newly-allocated #GMimeDecryptResult with signature
 * status information as well as a list of recipients that the part was
 * encrypted to.
 *
 * Returns: (nullable) (transfer full): the decrypted MIME part on success or
 * %NULL on fail. If the decryption fails, an exception will be set on
 * @err to provide information as to why the failure occurred.
 **/
GMimeObject *
g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *encrypted, GMimeDecryptFlags flags,
				    const char *session_key, GMimeDecryptResult **result,
				    GError **err)
{
	GMimeObject *decrypted, *version_part, *encrypted_part;
	GMimeStream *filtered, *stream, *ciphertext;
	const char *protocol, *supported;
	GMimeContentType *content_type;
	GMimeDataWrapper *content;
	GMimeDecryptResult *res;
	GMimeCryptoContext *ctx;
	GMimeFilter *filter;
	GMimeParser *parser;
	char *mime_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (encrypted), NULL);
	
	if (result)
		*result = NULL;
	
	if (!(protocol = g_mime_object_get_content_type_parameter ((GMimeObject *) encrypted, "protocol"))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unspecified encryption protocol."));
		
		return NULL;
	}
	
	if (!(ctx = g_mime_crypto_context_new (protocol))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot decrypt multipart/encrypted part: unregistered encryption protocol '%s'."),
			     protocol);
		
		return NULL;
	}
	
	supported = g_mime_crypto_context_get_encryption_protocol (ctx);
	
	/* make sure the protocol matches the crypto encrypt protocol */
	if (!supported || g_ascii_strcasecmp (supported, protocol) != 0) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot decrypt multipart/encrypted part: unsupported encryption protocol '%s'."),
			     protocol);
		g_object_unref (ctx);
		
		return NULL;
	}
	
	version_part = g_mime_multipart_get_part ((GMimeMultipart *) encrypted, GMIME_MULTIPART_ENCRYPTED_VERSION);
	
	/* make sure the protocol matches the version part's content-type */
	mime_type = g_mime_content_type_get_mime_type (version_part->content_type);
	if (g_ascii_strcasecmp (mime_type, protocol) != 0) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: content-type does not match protocol."));
		
		g_object_unref (ctx);
		g_free (mime_type);
		
		return NULL;
	}
	g_free (mime_type);
	
	/* get the encrypted part and check that it is of type application/octet-stream */
	encrypted_part = g_mime_multipart_get_part ((GMimeMultipart *) encrypted, GMIME_MULTIPART_ENCRYPTED_CONTENT);
	content_type = g_mime_object_get_content_type (encrypted_part);
	if (!g_mime_content_type_is_type (content_type, "application", "octet-stream")) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unexpected content type."));
		g_object_unref (ctx);
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	content = g_mime_part_get_content ((GMimePart *) encrypted_part);
	ciphertext = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (content, ciphertext);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* get the cleartext */
	if (!(res = g_mime_crypto_context_decrypt (ctx, flags, session_key, ciphertext, filtered, err))) {
		g_object_unref (ciphertext);
		g_object_unref (filtered);
		g_object_unref (stream);
		g_object_unref (ctx);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (ciphertext);
	g_object_unref (filtered);
	g_object_unref (ctx);
	
	g_mime_stream_reset (stream);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	decrypted = g_mime_parser_construct_part (parser, NULL);
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
