/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) 2000-2026 Jeffrey Stedfast
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_CRYPTO

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "gmime-gpgme-private.h"
#include "gmime-internal.h"

#define _(x) x

/**
 * SECTION: gmime-gpgme
 * @title: GPGME integration
 * @short_description: MIME operations using caller-owned GPGME contexts
 *
 * Include <filename>gmime/gmime-gpgme.h</filename> and use the gmime-gpgme-4.0 pkg-config
 * package. These APIs are available only in crypto-enabled builds and are
 * not exposed through introspection.
 *
 * Contexts are borrowed for synchronous, exclusive use. Applications configure
 * protocols, signers, engines, callbacks, network policies, and session-key
 * flags using GPGME. GMime never releases the context or clears its signers.
 * Stream operations respect its armor and text-mode settings; MIME operations
 * temporarily select the required representation and restore those settings.
 *
 * Optional result outputs start as NULL. Returned results have independent
 * references, including diagnostic results returned on failure, and must be
 * released with gpgme_result_unref(). Nested records are borrowed. Operations
 * replace the context's current results as normal GPGME operations do.
 *
 * Successful verification means the operation completed, not that signatures
 * are valid or trusted. Inspect each signature's status and validity. Discard
 * partial stream output when an operation fails.
 */

static gpgme_ssize_t
stream_read (void *stream, void *buffer, size_t size)
{
	ssize_t nread;

	errno = 0;
	nread = g_mime_stream_read (stream, buffer, size);
	if ((nread < 0 && !errno) || (nread == 0 && size && !g_mime_stream_eos (stream))) {
		errno = EIO;
		return -1;
	}

	return nread;
}

static gpgme_ssize_t
stream_write (void *stream, const void *buffer, size_t size)
{
	ssize_t nwritten;

	errno = 0;
	nwritten = g_mime_stream_write (stream, buffer, size);
	if ((nwritten < 0 && !errno) || (nwritten == 0 && size)) {
		errno = EIO;
		return -1;
	}

	return nwritten;
}

static gpgme_off_t
stream_seek (void *stream, gpgme_off_t offset, int whence)
{
	GMimeSeekWhence mode;
	gint64 position;

	switch (whence) {
	case SEEK_SET: mode = GMIME_STREAM_SEEK_SET; break;
	case SEEK_CUR: mode = GMIME_STREAM_SEEK_CUR; break;
	case SEEK_END: mode = GMIME_STREAM_SEEK_END; break;
	default:
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	position = g_mime_stream_seek (stream, offset, mode);
	if (position < 0 && !errno)
		errno = EIO;

	return position;
}

static struct gpgme_data_cbs stream_callbacks = {
	stream_read, stream_write, stream_seek, g_object_unref
};

static gboolean
check_error (gpgme_error_t status, GError **error)
{
	if (!status)
		return TRUE;

	g_set_error (error, GMIME_GPGME_ERROR, status, _("GPGME operation failed: %s"), gpgme_strerror (status));

	return FALSE;
}

/**
 * g_mime_gpgme_data_new: (skip)
 * @stream: a stream at the desired starting position
 * @data: (out): the new GPGME data handle
 * @error: return location for an error
 *
 * Adapts a stream without copying its contents or resetting its position.
 * The handle owns a reference to @stream, released by gpgme_data_release().
 * Reading, writing, and seeking use the stream's capabilities and absolute
 * stream offsets. The stream is neither flushed nor explicitly closed.
 *
 * Returns: TRUE on success.
 **/
gboolean
g_mime_gpgme_data_new (GMimeStream *stream, gpgme_data_t *data, GError **error)
{
	gpgme_error_t status;

	g_return_val_if_fail (data != NULL, FALSE);

	*data = NULL;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), FALSE);

	g_object_ref (stream);

	if ((status = gpgme_data_new_from_cbs (data, &stream_callbacks, stream)))
		g_object_unref (stream);

	return check_error (status, error);
}

gboolean
_g_mime_gpgme_check_protocol (gpgme_ctx_t ctx, gpgme_protocol_t protocol, GError **error)
{
	if (ctx && gpgme_get_protocol (ctx) == protocol)
		return TRUE;

	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("The GPGME context does not use the required protocol."));

	return FALSE;
}

const char *
_g_mime_gpgme_signature_protocol (gpgme_ctx_t ctx, GError **error)
{
	if (ctx) {
		switch (gpgme_get_protocol (ctx)) {
		case GPGME_PROTOCOL_OpenPGP: return "application/pgp-signature";
		case GPGME_PROTOCOL_CMS: return "application/pkcs7-signature";
		default: break;
		}
	}

	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("A GPGME OpenPGP or CMS context is required."));

	return NULL;
}

/**
 * g_mime_gpgme_sign: (skip)
 * @ctx: a caller-owned OpenPGP or CMS context
 * @mode: the signature mode
 * @input: input stream
 * @output: signature or signed-content stream
 * @result: (out) (optional): a retained signing result
 * @error: return location for an error
 *
 * Signs using the context's configured signers and representation settings.
 * No signer list or context configuration is changed.
 *
 * Returns: TRUE if signing completed.
 **/
gboolean
g_mime_gpgme_sign (gpgme_ctx_t ctx, gpgme_sig_mode_t mode, GMimeStream *input, GMimeStream *output,
		   gpgme_sign_result_t *result, GError **error)
{
	gpgme_data_t in, out;
	gpgme_error_t status;

	if (result)
		*result = NULL;

	if (!_g_mime_gpgme_signature_protocol (ctx, error))
		return FALSE;

	if (!g_mime_gpgme_data_new (input, &in, error))
		return FALSE;

	if (!g_mime_gpgme_data_new (output, &out, error)) {
		gpgme_data_release (in);
		return FALSE;
	}

	status = gpgme_op_sign (ctx, in, out, mode);

	if (result && (*result = gpgme_op_sign_result (ctx)))
		gpgme_result_ref (*result);

	gpgme_data_release (out);
	gpgme_data_release (in);

	return check_error (status, error);
}

/**
 * g_mime_gpgme_verify: (skip)
 * @ctx: a caller-owned OpenPGP or CMS context
 * @input: signed content
 * @signature: (nullable): detached signature, or NULL for embedded signatures
 * @output: (nullable): extracted content for embedded verification
 * @result: (out) (optional): a retained verification result
 * @error: return location for an error
 *
 * Verifies without changing context policy. An output stream is required for
 * embedded verification. Inspect individual signatures even on success.
 *
 * Returns: TRUE if verification completed.
 **/
gboolean
g_mime_gpgme_verify (gpgme_ctx_t ctx, GMimeStream *input, GMimeStream *signature, GMimeStream *output,
		     gpgme_verify_result_t *result, GError **error)
{
	gpgme_data_t in, other;
	gpgme_error_t status;

	if (result)
		*result = NULL;

	if (!_g_mime_gpgme_signature_protocol (ctx, error))
		return FALSE;

	if (!signature && !output) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("Embedded verification requires an output stream."));
		return FALSE;
	}

	if (!g_mime_gpgme_data_new (input, &in, error))
		return FALSE;

	if (!g_mime_gpgme_data_new (signature ? signature : output, &other, error)) {
		gpgme_data_release (in);
		return FALSE;
	}

	status = signature ? gpgme_op_verify (ctx, other, in, NULL) : gpgme_op_verify (ctx, in, NULL, other);
	if (result && (*result = gpgme_op_verify_result (ctx)))
		gpgme_result_ref (*result);

	gpgme_data_release (other);
	gpgme_data_release (in);

	return check_error (status, error);
}

/**
 * g_mime_gpgme_encrypt: (skip)
 * @ctx: a caller-owned OpenPGP or CMS context
 * @sign: whether to sign using the configured signers
 * @flags: native GPGME encryption flags
 * @recipients: (nullable): borrowed NULL-terminated recipient keys
 * @input: plaintext stream
 * @output: ciphertext stream
 * @encrypt_result: (out) (optional): a retained encryption result
 * @sign_result: (out) (optional): a retained signing result
 * @error: return location for an error
 *
 * Encrypts using native keys and context settings. NULL recipients request
 * GPGME's symmetric-encryption behavior. Combined signing and encryption
 * requires OpenPGP; use nested signed/encrypted MIME entities for CMS.
 *
 * Returns: TRUE if encryption completed.
 **/
gboolean
g_mime_gpgme_encrypt (gpgme_ctx_t ctx, gboolean sign, gpgme_encrypt_flags_t flags, gpgme_key_t *recipients,
		      GMimeStream *input, GMimeStream *output, gpgme_encrypt_result_t *encrypt_result,
		      gpgme_sign_result_t *sign_result, GError **error)
{
	gpgme_data_t in, out;
	gpgme_error_t status;

	if (encrypt_result)
		*encrypt_result = NULL;

	if (sign_result)
		*sign_result = NULL;

	if (!_g_mime_gpgme_signature_protocol (ctx, error))
		return FALSE;

	if (sign && gpgme_get_protocol (ctx) != GPGME_PROTOCOL_OpenPGP) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("Combined signing and encryption requires OpenPGP."));
		return FALSE;
	}

	if (!g_mime_gpgme_data_new (input, &in, error))
		return FALSE;

	if (!g_mime_gpgme_data_new (output, &out, error)) {
		gpgme_data_release (in);
		return FALSE;
	}

	status = sign ? gpgme_op_encrypt_sign (ctx, recipients, flags, in, out) :
		gpgme_op_encrypt (ctx, recipients, flags, in, out);

	if (encrypt_result && (*encrypt_result = gpgme_op_encrypt_result (ctx)))
		gpgme_result_ref (*encrypt_result);

	if (sign && sign_result && (*sign_result = gpgme_op_sign_result (ctx)))
		gpgme_result_ref (*sign_result);

	gpgme_data_release (out);
	gpgme_data_release (in);

	return check_error (status, error);
}

/**
 * g_mime_gpgme_decrypt: (skip)
 * @ctx: a caller-owned OpenPGP or CMS context
 * @verify: whether to verify embedded signatures (OpenPGP only)
 * @input: ciphertext stream
 * @output: plaintext stream
 * @decrypt_result: (out) (optional): a retained decryption result
 * @verify_result: (out) (optional): a retained verification result
 * @error: return location for an error
 *
 * Decrypts without changing network or session-key policies. Configure
 * export-session-key and override-session-key on @ctx when needed.
 *
 * Returns: TRUE if decryption completed.
 **/
gboolean
g_mime_gpgme_decrypt (gpgme_ctx_t ctx, gboolean verify, GMimeStream *input, GMimeStream *output,
		      gpgme_decrypt_result_t *decrypt_result, gpgme_verify_result_t *verify_result, GError **error)
{
	gpgme_data_t in, out;
	gpgme_error_t status;

	if (decrypt_result)
		*decrypt_result = NULL;

	if (verify_result)
		*verify_result = NULL;

	if (!_g_mime_gpgme_signature_protocol (ctx, error))
		return FALSE;

	if (verify && !_g_mime_gpgme_check_protocol (ctx, GPGME_PROTOCOL_OpenPGP, error))
		return FALSE;

	if (!g_mime_gpgme_data_new (input, &in, error))
		return FALSE;

	if (!g_mime_gpgme_data_new (output, &out, error)) {
		gpgme_data_release (in);
		return FALSE;
	}

	status = verify ? gpgme_op_decrypt_verify (ctx, in, out) : gpgme_op_decrypt (ctx, in, out);

	if (decrypt_result && (*decrypt_result = gpgme_op_decrypt_result (ctx)))
		gpgme_result_ref (*decrypt_result);

	if (verify && verify_result && (*verify_result = gpgme_op_verify_result (ctx)))
		gpgme_result_ref (*verify_result);

	gpgme_data_release (out);
	gpgme_data_release (in);

	return check_error (status, error);
}

char *
_g_mime_gpgme_micalg (gpgme_ctx_t ctx, gpgme_sign_result_t result, GError **error)
{
	GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	GString *algorithms = g_string_new (NULL);
	gpgme_new_signature_t signature;
	const char *name, *prefix;
	char *token;

	prefix = gpgme_get_protocol (ctx) == GPGME_PROTOCOL_OpenPGP ? "pgp-" : "";

	for (signature = result ? result->signatures : NULL; signature; signature = signature->next) {
		switch (signature->hash_algo) {
		case GPGME_MD_RMD160: name = "ripemd160"; break;
		case GPGME_MD_TIGER: name = "tiger192"; break;
		case GPGME_MD_HAVAL: name = "haval-5-160"; break;
		default: name = gpgme_hash_algo_name (signature->hash_algo); break;
		}

		if (!name)
			goto invalid;

		token = g_ascii_strdown (name, -1);

		if (g_hash_table_add (seen, token)) {
			if (algorithms->len)
				g_string_append_c (algorithms, ',');
			g_string_append_printf (algorithms, "%s%s", prefix, token);
		}
	}

	if (!algorithms->len)
		goto invalid;

	g_hash_table_destroy (seen);

	return g_string_free (algorithms, FALSE);

invalid:
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     _("The signing result has no supported digest algorithm."));
	g_hash_table_destroy (seen);
	g_string_free (algorithms, TRUE);

	return NULL;
}

gboolean
_g_mime_gpgme_io_error (GError **error)
{
	int saved_errno = errno;

	g_set_error (error, GMIME_ERROR, saved_errno ? saved_errno : GMIME_ERROR_GENERAL,
		     _("Cannot read or write MIME crypto content: %s"),
		     saved_errno ? g_strerror (saved_errno) : _("Unknown error"));

	return FALSE;
}

GMimeStream *
_g_mime_gpgme_decode (GMimePart *part, GError **error)
{
	GMimeContent *content = g_mime_part_get_content (part);
	GMimeStream *stream;

	if (!content || !g_mime_content_get_stream (content)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("The MIME part has no content."));
		return NULL;
	}

	stream = g_mime_stream_mem_new ();
	errno = 0;
	if (g_mime_content_decode_to_stream (content, stream) < 0 || g_mime_stream_reset (stream) < 0) {
		_g_mime_gpgme_io_error (error);
		g_object_unref (stream);
		return NULL;
	}

	return stream;
}

GMimeStream *
_g_mime_gpgme_serialize (GMimeEntity *entity, GError **error)
{
	GMimeFormatOptions *options = _g_mime_format_options_clone (NULL, FALSE);
	GMimeStream *stream = g_mime_stream_mem_new ();
	ssize_t written;

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);

	errno = 0;
	written = g_mime_entity_write_to_stream (entity, options, stream);

	g_mime_format_options_unref (options);

	if (written < 0 || g_mime_stream_reset (stream) < 0) {
		_g_mime_gpgme_io_error (error);
		g_object_unref (stream);
		return NULL;
	}

	return stream;
}

GMimeEntity *
_g_mime_gpgme_parse (GMimeStream *stream, GError **error)
{
	GMimeFilter *filter = g_mime_filter_dos2unix_new (FALSE);
	GMimeStream *normalized = g_mime_stream_mem_new ();
	GMimeStream *filtered;
	GMimeEntity *entity = NULL;

	filtered = g_mime_stream_filter_new (normalized);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);

	errno = 0;
	if (g_mime_stream_reset (stream) < 0 ||
	    g_mime_stream_write_to_stream (stream, filtered) < 0 ||
	    g_mime_stream_flush (filtered) < 0 || g_mime_stream_reset (normalized) < 0) {
		_g_mime_gpgme_io_error (error);
	} else {
		entity = g_mime_entity_load (NULL, normalized, error);

		if (!entity && (!error || !*error))
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     _("Cannot parse the extracted MIME content."));
	}

	g_object_unref (filtered);
	g_object_unref (normalized);

	return entity;
}

static gboolean
check_openpgp_block (GMimeOpenPGPBlock *block, gpgme_ctx_t ctx, gboolean decrypt, GError **error)
{
	GMimeOpenPGPBlockKind kind = g_mime_openpgp_block_get_kind (block);

	if (!_g_mime_gpgme_check_protocol (ctx, GPGME_PROTOCOL_OpenPGP, error))
		return FALSE;

	if (g_mime_openpgp_block_get_status (block) != GMIME_OPENPGP_BLOCK_COMPLETE) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot process an incomplete or malformed OpenPGP block."));
		return FALSE;
	}

	if (kind != GMIME_OPENPGP_BLOCK_MESSAGE && (decrypt || kind != GMIME_OPENPGP_BLOCK_SIGNED_MESSAGE)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("This OpenPGP block kind does not support the requested operation."));
		return FALSE;
	}

	return TRUE;
}

/**
 * g_mime_openpgp_block_verify: (skip)
 * @block: a complete clear-signed or PGP MESSAGE block
 * @ctx: a borrowed GPGME OpenPGP context
 * @output: stream for extracted bytes
 * @result: (out) (optional): a retained native verification result
 * @error: return location for an error
 *
 * Verifies one block without modifying it, its source part, or caller
 * settings. PGP MESSAGE armor may contain opaque signed data; the engine
 * determines whether verification is possible. Standalone signatures and
 * keys are not supported. No key is automatically imported.
 *
 * Inspect every signature's status and validity; successful completion
 * does not authenticate surrounding text or neighboring blocks. Discard
 * partial output on failure. Release every non-NULL result with
 * gpgme_result_unref(), including diagnostic results returned on failure.
 *
 * Returns: %TRUE if the operation completed, not a signature-validity flag.
 **/
gboolean
g_mime_openpgp_block_verify (GMimeOpenPGPBlock *block, gpgme_ctx_t ctx, GMimeStream *output,
			     gpgme_verify_result_t *result, GError **error)
{
	GMimeStream *input;
	gboolean success;

	if (result)
		*result = NULL;

	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (output), FALSE);

	if (!check_openpgp_block (block, ctx, FALSE, error))
		return FALSE;

	input = g_mime_openpgp_block_open_stream (block);
	success = g_mime_gpgme_verify (ctx, input, NULL, output, result, error);
	g_object_unref (input);

	return success;
}

/**
 * g_mime_openpgp_block_decrypt: (skip)
 * @block: a complete PGP MESSAGE block
 * @ctx: a borrowed GPGME OpenPGP context
 * @output: stream for decrypted bytes
 * @decrypt_result: (out) (optional): a retained decryption result
 * @verify_result: (out) (optional): a retained verification result
 * @error: return location for an error
 *
 * Decrypts and verifies embedded signatures for this block only. Neither
 * the snapshot nor the source part is modified. Caller context settings,
 * including session-key flags and passphrase handling, are preserved.
 * Output is not interpreted as MIME, HTML, or another inline-PGP document.
 *
 * Decryption alone does not authenticate a sender. Inspect native signature
 * results and keep each output associated with its source block. Discard
 * partial output on failure; release all returned results using
 * gpgme_result_unref(), even if the operation failed.
 *
 * Returns: %TRUE if the operation completed.
 **/
gboolean
g_mime_openpgp_block_decrypt (GMimeOpenPGPBlock *block, gpgme_ctx_t ctx, GMimeStream *output,
			      gpgme_decrypt_result_t *decrypt_result, gpgme_verify_result_t *verify_result,
			      GError **error)
{
	GMimeStream *input;
	gboolean success;

	if (decrypt_result)
		*decrypt_result = NULL;

	if (verify_result)
		*verify_result = NULL;

	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), FALSE);
	g_return_val_if_fail (GMIME_IS_STREAM (output), FALSE);

	if (!check_openpgp_block (block, ctx, TRUE, error))
		return FALSE;

	input = g_mime_openpgp_block_open_stream (block);
	success = g_mime_gpgme_decrypt (ctx, TRUE, input, output, decrypt_result, verify_result, error);
	g_object_unref (input);

	return success;
}

#endif /* ENABLE_CRYPTO */
