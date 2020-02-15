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

#include <string.h>

#include "gmime-application-pkcs7-mime.h"
#include "gmime-filter-dos2unix.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-parser.h"
#include "gmime-error.h"

#define _(x) x

/**
 * SECTION: gmime-application-pkcs7-mime
 * @title: GMimeApplicationPkcs7Mime
 * @short_description: Pkcs7 MIME parts
 * @see_also:
 *
 * A #GMimeApplicationPkcs7Mime represents the application/pkcs7-mime MIME part.
 **/

/* GObject class methods */
static void g_mime_application_pkcs7_mime_class_init (GMimeApplicationPkcs7MimeClass *klass);
static void g_mime_application_pkcs7_mime_init (GMimeApplicationPkcs7Mime *catpart, GMimeApplicationPkcs7MimeClass *klass);
static void g_mime_application_pkcs7_mime_finalize (GObject *object);

/* GMimeObject class methods */
static void application_pkcs7_mime_set_content_type (GMimeObject *object, GMimeContentType *content_type);


static GMimePartClass *parent_class = NULL;


GType
g_mime_application_pkcs7_mime_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeApplicationPkcs7MimeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_application_pkcs7_mime_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeApplicationPkcs7Mime),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_application_pkcs7_mime_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_PART, "GMimeApplicationPkcs7Mime", &info, 0);
	}
	
	return type;
}


static void
g_mime_application_pkcs7_mime_class_init (GMimeApplicationPkcs7MimeClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_PART);
	
	gobject_class->finalize = g_mime_application_pkcs7_mime_finalize;
	
	object_class->set_content_type = application_pkcs7_mime_set_content_type;
}

static void
g_mime_application_pkcs7_mime_init (GMimeApplicationPkcs7Mime *pkcs7_mime, GMimeApplicationPkcs7MimeClass *klass)
{
	pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
}

static void
g_mime_application_pkcs7_mime_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
application_pkcs7_mime_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime = (GMimeApplicationPkcs7Mime *) object;
	const char *value;
	
	if ((value = g_mime_content_type_get_parameter (content_type, "smime-type")) != NULL) {
		if (!g_ascii_strcasecmp (value, "compressed-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA;
		else if (!g_ascii_strcasecmp (value, "enveloped-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA;
		else if (!g_ascii_strcasecmp (value, "signed-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_SIGNED_DATA;
		else if (!g_ascii_strcasecmp (value, "certs-only"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_CERTS_ONLY;
		else
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
	} else {
		pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
	}
	
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}

/**
 * g_mime_application_pkcs7_mime_new:
 * @type: The type of S/MIME data contained within the part.
 *
 * Creates a new application/pkcs7-mime object.
 *
 * Returns: an empty application/pkcs7-mime object.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_new (GMimeSecureMimeType type)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeContentType *content_type;
	const char *name;
	
	g_return_val_if_fail (type != GMIME_SECURE_MIME_TYPE_UNKNOWN, NULL);
	
	pkcs7_mime = g_object_new (GMIME_TYPE_APPLICATION_PKCS7_MIME, NULL);
	content_type = g_mime_content_type_new ("application", "pkcs7-mime");
	
	switch (type) {
	case GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "compressed-data");
		name = "smime.p7z";
		break;
	case GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "enveloped-data");
		name = "smime.p7m";
		break;
	case GMIME_SECURE_MIME_TYPE_SIGNED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "signed-data");
		name = "smime.p7m";
		break;
	case GMIME_SECURE_MIME_TYPE_CERTS_ONLY:
		g_mime_content_type_set_parameter (content_type, "smime-type", "certs-only");
		name = "smime.p7c";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	g_mime_object_set_content_type ((GMimeObject *) pkcs7_mime, content_type);
	g_object_unref (content_type);
	
	g_mime_part_set_filename ((GMimePart *) pkcs7_mime, name);
	g_mime_part_set_content_encoding ((GMimePart *) pkcs7_mime, GMIME_CONTENT_ENCODING_BASE64);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_get_smime_type:
 * @pkcs7_mime: A #GMimeApplicationPkcs7Mime object
 *
 * Gets the smime-type value of the Content-Type header.
 *
 * Returns: the smime-type value.
 **/
GMimeSecureMimeType
g_mime_application_pkcs7_mime_get_smime_type (GMimeApplicationPkcs7Mime *pkcs7_mime)
{
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), GMIME_SECURE_MIME_TYPE_UNKNOWN);
	
	return pkcs7_mime->smime_type;
}


#if 0
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_compress (GMimeObject *entity, GError **err)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeStream *compressed, *stream;
	GMimeFormatOptions *options;
	GMimeDataWrapper *content;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_OBJECT (entity), NULL);
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot compress application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (entity, options, stream);
	g_mime_format_options_free (options);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* compress the content stream */
	compressed = g_mime_stream_mem_new ();
	if (g_mime_crypto_context_compress (ctx, stream, compressed, err) == -1) {
		g_object_unref (compressed);
		g_object_unref (stream);
		g_object_unref (ctx);
		return NULL;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (compressed);
	g_object_unref (ctx);
	
	/* construct the application/pkcs7-mime part */
	pkcs7_mime = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA);
	content = g_mime_data_wrapper_new_with_stream (compressed, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_part_set_content ((GMimePart *) pkcs7_mime, content);
	g_object_unref (compressed);
	g_object_unref (content);
	
	return pkcs7_mime;
}


GMimeObject *
g_mime_application_pkcs7_mime_decompress (GMimeApplicationPkcs7Mime *pkcs7_mime)
{
	GMimeStream *filtered, *compressed, *stream;
	GMimeObject *decompressed;
	GMimeDataWrapper *content;
	GMimeCryptoContext *ctx;
	GMimeDecryptResult *res;
	GMimeFilter *filter;
	GMimeParser *parser;
	
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), NULL);
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot decompress application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	/* get the compressed stream */
	content = g_mime_part_get_content ((GMimePart *) pkcs7_mime);
	compressed = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (content, compressed);
	g_mime_stream_reset (compressed);
	
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* decompress the content stream */
	if (g_mime_crypto_context_decompress (ctx, compressed, stream, err) == -1) {
		g_object_unref (compressed);
		g_object_unref (filtered);
		g_object_unref (stream);
		g_object_unref (ctx);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (compressed);
	g_object_unref (filtered);
	g_object_unref (ctx);
	
	g_mime_stream_reset (stream);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	decompressed = g_mime_parser_construct_part (parser, NULL);
	g_object_unref (parser);
	
	if (!decompressed) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decompress application/pkcs7-mime part: failed to parse decompressed content."));
		
		return NULL;
	}
	
	return decompressed;
}
#endif


/**
 * g_mime_application_pkcs7_mime_encrypt:
 * @entity: a #GMimeObject to encrypt
 * @flags: a #GMimeEncryptFlags
 * @recipients: (element-type utf8): an array of recipients to encrypt to
 * @err: a #GError
 *
 * Attempts to encrypt the @entity MIME part to the public keys of @recipients
 * using S/MIME. If successful, a new application/pkcs7-mime object is returned.
 *
 * Returns: (nullable) (transfer full): a new #GMimeApplicationPkcs7Mime object on success
 * or %NULL on fail. If encrypting fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_encrypt (GMimeObject *entity, GMimeEncryptFlags flags, GPtrArray *recipients, GError **err)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeStream *ciphertext, *stream;
	GMimeFormatOptions *options;
	GMimeDataWrapper *content;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_OBJECT (entity), NULL);
	g_return_val_if_fail (recipients != NULL, NULL);
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot encrypt application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (entity, options, stream);
	g_mime_format_options_free (options);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* encrypt the content stream */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_crypto_context_encrypt (ctx, FALSE, NULL, flags, recipients, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		g_object_unref (ctx);
		return NULL;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	g_object_unref (ctx);
	
	/* construct the application/pkcs7-mime part */
	pkcs7_mime = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA);
	content = g_mime_data_wrapper_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_part_set_content ((GMimePart *) pkcs7_mime, content);
	g_object_unref (ciphertext);
	g_object_unref (content);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_decrypt:
 * @pkcs7_mime: a #GMimeApplicationPkcs7Mime
 * @flags: a #GMimeDecryptFlags
 * @session_key: session key to use or %NULL
 * @result: the decryption result
 * @err: a #GError
 *
 * Attempts to decrypt the encrypted application/pkcs7-mime part.
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
g_mime_application_pkcs7_mime_decrypt (GMimeApplicationPkcs7Mime *pkcs7_mime,
				       GMimeDecryptFlags flags, const char *session_key,
				       GMimeDecryptResult **result, GError **err)
{
	GMimeStream *filtered, *ciphertext, *stream;
	GMimeDataWrapper *content;
	GMimeCryptoContext *ctx;
	GMimeDecryptResult *res;
	GMimeObject *decrypted;
	GMimeFilter *filter;
	GMimeParser *parser;
	
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), NULL);
	
	if (result)
		*result = NULL;
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot decrypt application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	content = g_mime_part_get_content ((GMimePart *) pkcs7_mime);
	ciphertext = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (content, ciphertext);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* decrypt the content stream */
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
				     _("Cannot decrypt application/pkcs7-mime part: failed to parse decrypted content."));
		
		g_object_unref (res);
		
		return NULL;
	}
	
	if (!result)
		g_object_unref (res);
	else
		*result = res;
	
	return decrypted;
}


/**
 * g_mime_application_pkcs7_mime_sign:
 * @entity: a #GMimeObject
 * @userid: the user id to sign with
 * @err: a #GError
 *
 * Attempts to sign the @entity MIME part with @userid's private key using
 * S/MIME. If successful, a new application/pkcs7-mime object is returned.
 *
 * Returns: (nullable) (transfer full): a new #GMimeApplicationPkcs7Mime object on success
 * or %NULL on fail. If signing fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_sign (GMimeObject *entity, const char *userid, GError **err)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeStream *ciphertext, *stream;
	GMimeFormatOptions *options;
	GMimeDataWrapper *content;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_OBJECT (entity), NULL);
	g_return_val_if_fail (userid != NULL, NULL);
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot sign application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (entity, options, stream);
	g_mime_format_options_free (options);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* sign the content stream */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_crypto_context_sign (ctx, FALSE, userid, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		g_object_unref (ctx);
		return NULL;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	g_object_unref (ctx);
	
	/* construct the application/pkcs7-mime part */
	pkcs7_mime = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_SIGNED_DATA);
	content = g_mime_data_wrapper_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_part_set_content ((GMimePart *) pkcs7_mime, content);
	g_object_unref (ciphertext);
	g_object_unref (content);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_verify:
 * @pkcs7_mime: a #GMimeApplicationPkcs7Mime
 * @flags: a #GMimeVerifyFlags
 * @entity: (out) (transfer full): the extracted entity
 * @err: a #GError
 *
 * Attempts to verify the signed @pkcs7_mime part and extract the original
 * MIME entity.
 *
 * Returns: (nullable) (transfer full): a new #GMimeSignatureList object on
 * success or %NULL on fail. If the verification fails, an exception
 * will be set on @err to provide information as to why the failure
 * occurred.
 **/
GMimeSignatureList *
g_mime_application_pkcs7_mime_verify (GMimeApplicationPkcs7Mime *pkcs7_mime, GMimeVerifyFlags flags, GMimeObject **entity, GError **err)
{
	GMimeStream *filtered, *ciphertext, *stream;
	GMimeSignatureList *signatures;
	GMimeDataWrapper *content;
	GMimeCryptoContext *ctx;
	GMimeFilter *filter;
	GMimeParser *parser;
	
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), NULL);
	g_return_val_if_fail (entity != NULL, NULL);
	
	*entity = NULL;
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot verify application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	content = g_mime_part_get_content ((GMimePart *) pkcs7_mime);
	ciphertext = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (content, ciphertext);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* decrypt the content stream */
	if (!(signatures = g_mime_crypto_context_verify (ctx, flags, ciphertext, NULL, filtered, err))) {
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
	
	*entity = g_mime_parser_construct_part (parser, NULL);
	g_object_unref (parser);
	
	if (*entity == NULL) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify application/pkcs7-mime part: failed to parse extracted content."));
		
		g_object_unref (signatures);
		
		return NULL;
	}
	
	return signatures;
}
