/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2026 Jeffrey Stedfast
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

#include "gmime-common.h"
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
#include "gmime-gpgme-private.h"

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x

#include "gmime-object-private.h"

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
static void g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mps);
static void g_mime_multipart_encrypted_finalize (GObject *object);


static GMimeMultipartClass *parent_class = NULL;


G_DEFINE_TYPE (GMimeMultipartEncrypted, g_mime_multipart_encrypted, GMIME_TYPE_MULTIPART)


static void
g_mime_multipart_encrypted_class_init (GMimeMultipartEncryptedClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_MULTIPART);
	
	gobject_class->finalize = g_mime_multipart_encrypted_finalize;
}

static void
g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mpe)
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
	g_mime_entity_set_content_type ((GMimeEntity *) multipart, content_type);
	g_object_unref (content_type);
	
	return multipart;
}


#ifdef ENABLE_CRYPTO

/**
 * g_mime_multipart_encrypted_encrypt: (skip)
 * @ctx: a borrowed GPGME OpenPGP context
 * @entity: MIME part to encrypt
 * @sign: %TRUE if the content should also be signed or %FALSE otherwise
 * @flags: native GPGME encryption flags
 * @recipients: (nullable): borrowed NULL-terminated recipient keys
 * @encrypt_result: (out) (optional): a retained encryption result
 * @sign_result: (out) (optional): a retained signing result
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
g_mime_multipart_encrypted_encrypt (gpgme_ctx_t ctx, GMimeEntity *entity, gboolean sign,
				    gpgme_encrypt_flags_t flags, gpgme_key_t *recipients,
				    gpgme_encrypt_result_t *encrypt_result, gpgme_sign_result_t *sign_result,
				    GError **err)
{
	const char *protocol = "application/pgp-encrypted";
	GMimePart *version_part, *encrypted_part;
	GMimeMultipartEncrypted *encrypted;
	GMimeStream *stream, *ciphertext;
	GMimeContentType *content_type;
	GMimeContent *content;
	int armor, textmode;
	gboolean success;

	if (encrypt_result)
		*encrypt_result = NULL;

	if (sign_result)
		*sign_result = NULL;

	g_return_val_if_fail (GMIME_IS_ENTITY (entity), NULL);
	
	if (!_g_mime_gpgme_check_protocol (ctx, GPGME_PROTOCOL_OpenPGP, err))
		return NULL;
	
	/* get the cleartext */
	if (!(stream = _g_mime_gpgme_serialize (entity, err)))
		return NULL;
	
	/* encrypt the content stream */
	ciphertext = g_mime_stream_mem_new ();

	/* capture the current context settings */
	armor = gpgme_get_armor (ctx);
	textmode = gpgme_get_textmode (ctx);

	/* override the context settings */
	gpgme_set_armor (ctx, TRUE);
	gpgme_set_textmode (ctx, FALSE);

	/* encrypt the content stream */
	success = g_mime_gpgme_encrypt (ctx, sign, flags, recipients, stream, ciphertext,
				       encrypt_result, sign_result, err);

	/* restore the original context settings */
	gpgme_set_textmode (ctx, textmode);
	gpgme_set_armor (ctx, armor);

	if (!success) {
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
	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content (version_part, content);
	g_object_unref (content);
	g_object_unref (stream);
	
	/* construct the encrypted mime part */
	encrypted_part = g_mime_part_new_with_type ("application", "octet-stream");
	g_mime_part_set_content_encoding (encrypted_part, GMIME_CONTENT_ENCODING_7BIT);
	content = g_mime_content_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_7BIT);
	g_mime_part_set_content (encrypted_part, content);
	g_object_unref (ciphertext);
	g_object_unref (content);
	
	/* save the version and encrypted parts */
	encrypted = g_mime_multipart_encrypted_new ();
	g_mime_multipart_add ((GMimeMultipart *) encrypted, (GMimeEntity *) version_part);
	g_mime_multipart_add ((GMimeMultipart *) encrypted, (GMimeEntity *) encrypted_part);
	g_object_unref (encrypted_part);
	g_object_unref (version_part);
	
	/* set the content-type params for this multipart/encrypted part */
	g_mime_entity_set_content_type_parameter ((GMimeEntity *) encrypted, "protocol", protocol);
	g_mime_multipart_set_boundary ((GMimeMultipart *) encrypted, NULL);
	
	return encrypted;
}


/**
 * g_mime_multipart_encrypted_decrypt: (skip)
 * @encrypted: a #GMimeMultipartEncrypted
 * @ctx: a borrowed GPGME OpenPGP context
 * @decrypt_result: (out) (optional): a retained decryption result
 * @verify_result: (out) (optional): a retained verification result
 * @err: a #GError
 *
 * Attempts to decrypt the encrypted MIME part contained within the
 * multipart/encrypted object @encrypted. Only surrounding ASCII linear
 * whitespace is removed from the protocol parameter.
 *
 * Embedded signatures are verified. Configure session-key and network policies
 * on @ctx directly. Returned native results must be released even if parsing
 * the decrypted MIME entity fails. Decryption alone does not authenticate it.
 *
 * Returns: (nullable) (transfer full): the decrypted MIME part on success or
 * %NULL on fail. If the decryption fails, an exception will be set on
 * @err to provide information as to why the failure occurred.
 **/
GMimeEntity *
g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *encrypted, gpgme_ctx_t ctx,
				    gpgme_decrypt_result_t *decrypt_result,
				    gpgme_verify_result_t *verify_result, GError **err)
{
	const char *value, *supported = "application/pgp-encrypted";
	GMimeEntity *decrypted, *version_part, *encrypted_part;
	GMimeStream *stream, *ciphertext;
	GMimeContentType *content_type;
	GMimeEntityPrivate *priv;
	char *mime_type, *protocol;
	
	if (decrypt_result)
		*decrypt_result = NULL;

	if (verify_result)
		*verify_result = NULL;

	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (encrypted), NULL);

	if (!_g_mime_gpgme_check_protocol (ctx, GPGME_PROTOCOL_OpenPGP, err))
		return NULL;

	if (g_mime_multipart_get_count (GMIME_MULTIPART (encrypted)) < 2) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted: missing subparts."));
		return NULL;
	}
	
	value = g_mime_entity_get_content_type_parameter ((GMimeEntity *) encrypted, "protocol");
	protocol = g_mime_strdup_trim (value);

	if (!protocol || !*protocol) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unspecified encryption protocol."));
		
		g_free (protocol);
		return NULL;
	}
	
	/* make sure the protocol matches the crypto encrypt protocol */
	if (g_ascii_strcasecmp (supported, protocol) != 0) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot decrypt multipart/encrypted part: unsupported encryption protocol '%s'."),
			     protocol);
		g_free (protocol);
		
		return NULL;
	}
	
	version_part = g_mime_multipart_get_part ((GMimeMultipart *) encrypted, GMIME_MULTIPART_ENCRYPTED_VERSION);
	
	/* make sure the protocol matches the version part's content-type */
	priv = _g_mime_entity_get_private (version_part);
	mime_type = g_mime_content_type_get_mime_type (priv->content_type);

	if (g_ascii_strcasecmp (mime_type, protocol) != 0) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: content-type does not match protocol."));
		
		g_free (mime_type);
		g_free (protocol);
		
		return NULL;
	}

	g_free (mime_type);
	g_free (protocol);
	
	/* get the encrypted part and check that it is of type application/octet-stream */
	encrypted_part = g_mime_multipart_get_part ((GMimeMultipart *) encrypted, GMIME_MULTIPART_ENCRYPTED_CONTENT);
	content_type = g_mime_entity_get_content_type (encrypted_part);

	if (!GMIME_IS_PART (encrypted_part) ||
	    !g_mime_content_type_is_type (content_type, "application", "octet-stream")) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot decrypt multipart/encrypted part: unexpected content type."));
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	if (!(ciphertext = _g_mime_gpgme_decode (GMIME_PART (encrypted_part), err)))
		return NULL;
	
	stream = g_mime_stream_mem_new ();
	
	/* get the cleartext */
	if (!g_mime_gpgme_decrypt (ctx, TRUE, ciphertext, stream, decrypt_result, verify_result, err)) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		
		return NULL;
	}
	
	g_object_unref (ciphertext);
	decrypted = _g_mime_gpgme_parse (stream, err);
	g_object_unref (stream);
	
	return decrypted;
}

#endif /* ENABLE_CRYPTO */
