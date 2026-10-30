/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) 2026 Jeffrey Stedfast
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#ifndef __GMIME_GPGME_H__
#define __GMIME_GPGME_H__

#include <gmime/gmime.h>
#include <gpgme.h>

G_BEGIN_DECLS

gboolean g_mime_gpgme_data_new (GMimeStream *stream, gpgme_data_t *data, GError **error);

gboolean g_mime_gpgme_sign (gpgme_ctx_t ctx, gpgme_sig_mode_t mode,
			   GMimeStream *input, GMimeStream *output,
			   gpgme_sign_result_t *result, GError **error);
gboolean g_mime_gpgme_verify (gpgme_ctx_t ctx, GMimeStream *input, GMimeStream *signature,
			     GMimeStream *output, gpgme_verify_result_t *result, GError **error);
gboolean g_mime_gpgme_encrypt (gpgme_ctx_t ctx, gboolean sign, gpgme_encrypt_flags_t flags,
			      gpgme_key_t *recipients, GMimeStream *input, GMimeStream *output,
			      gpgme_encrypt_result_t *encrypt_result, gpgme_sign_result_t *sign_result,
			      GError **error);
gboolean g_mime_gpgme_decrypt (gpgme_ctx_t ctx, gboolean verify, GMimeStream *input, GMimeStream *output,
			      gpgme_decrypt_result_t *decrypt_result, gpgme_verify_result_t *verify_result,
			      GError **error);

GMimeMultipartSigned *g_mime_multipart_signed_sign (gpgme_ctx_t ctx, GMimeEntity *entity,
						   gpgme_sign_result_t *result, GError **err);
gboolean g_mime_multipart_signed_verify (GMimeMultipartSigned *mps, gpgme_ctx_t ctx,
					gpgme_verify_result_t *result, GError **err);

GMimeMultipartEncrypted *g_mime_multipart_encrypted_encrypt (gpgme_ctx_t ctx, GMimeEntity *entity,
							     gboolean sign, gpgme_encrypt_flags_t flags,
							     gpgme_key_t *recipients,
							     gpgme_encrypt_result_t *encrypt_result,
							     gpgme_sign_result_t *sign_result, GError **err);
GMimeEntity *g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *encrypted, gpgme_ctx_t ctx,
						gpgme_decrypt_result_t *decrypt_result,
						gpgme_verify_result_t *verify_result, GError **err);

GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_encrypt (gpgme_ctx_t ctx, GMimeEntity *entity,
								gpgme_encrypt_flags_t flags, gpgme_key_t *recipients,
								gpgme_encrypt_result_t *result, GError **err);
GMimeEntity *g_mime_application_pkcs7_mime_decrypt (GMimeApplicationPkcs7Mime *pkcs7_mime, gpgme_ctx_t ctx,
						  gpgme_decrypt_result_t *result, GError **err);
GMimeApplicationPkcs7Mime *g_mime_application_pkcs7_mime_sign (gpgme_ctx_t ctx, GMimeEntity *entity,
							     gpgme_sign_result_t *result, GError **err);
gboolean g_mime_application_pkcs7_mime_verify (GMimeApplicationPkcs7Mime *pkcs7_mime, gpgme_ctx_t ctx,
					     GMimeEntity **entity, gpgme_verify_result_t *result, GError **err);

gboolean g_mime_part_openpgp_encrypt (GMimePart *mime_part, gpgme_ctx_t ctx, gboolean sign,
				     gpgme_encrypt_flags_t flags, gpgme_key_t *recipients,
				     gpgme_encrypt_result_t *encrypt_result, gpgme_sign_result_t *sign_result,
				     GError **err);
gboolean g_mime_part_openpgp_decrypt (GMimePart *mime_part, gpgme_ctx_t ctx,
				     gpgme_decrypt_result_t *decrypt_result, gpgme_verify_result_t *verify_result,
				     GError **err);
gboolean g_mime_part_openpgp_sign (GMimePart *mime_part, gpgme_ctx_t ctx,
				  gpgme_sign_result_t *result, GError **err);
gboolean g_mime_part_openpgp_verify (GMimePart *mime_part, gpgme_ctx_t ctx,
				    gpgme_verify_result_t *result, GError **err);

GMimeAutocryptHeaderList *g_mime_message_get_autocrypt_gossip_headers (GMimeMessage *message,
								     GDateTime *now, gpgme_ctx_t ctx,
								     GError **err);

gboolean g_mime_openpgp_block_verify (GMimeOpenPGPBlock *block, gpgme_ctx_t ctx,
				     GMimeStream *output, gpgme_verify_result_t *result, GError **error);
gboolean g_mime_openpgp_block_decrypt (GMimeOpenPGPBlock *block, gpgme_ctx_t ctx,
				      GMimeStream *output, gpgme_decrypt_result_t *decrypt_result,
				      gpgme_verify_result_t *verify_result, GError **error);

G_END_DECLS

#endif /* __GMIME_GPGME_H__ */
