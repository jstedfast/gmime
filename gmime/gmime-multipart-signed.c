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

#include <string.h>
#include <errno.h>

#include "gmime-common.h"
#include "gmime-multipart-signed.h"
#include "gmime-multipart-encrypted.h"
#include "gmime-message-part.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-unix2dos.h"
#include "gmime-filter-strip.h"
#include "gmime-filter-from.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-parser.h"
#include "gmime-error.h"
#include "gmime-gpgme-private.h"
#include "gmime-part.h"

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


#include "gmime-object-private.h"

/**
 * SECTION: gmime-multipart-signed
 * @title: GMimeMultipartSigned
 * @short_description: Signed MIME multiparts
 * @see_also: #GMimeMultipart
 *
 * A #GMimeMultipartSigned part is a special subclass of
 * #GMimeMultipart to make it easier to manipulate the
 * multipart/signed MIME type.
 **/

/* GObject class methods */
static void g_mime_multipart_signed_class_init (GMimeMultipartSignedClass *klass);
static void g_mime_multipart_signed_init (GMimeMultipartSigned *mps);
static void g_mime_multipart_signed_finalize (GObject *object);

/* GMimeEntity class methods */
static gboolean multipart_signed_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint,
					  guint max_line_length, GError **error);


static GMimeMultipartClass *parent_class = NULL;


G_DEFINE_TYPE (GMimeMultipartSigned, g_mime_multipart_signed, GMIME_TYPE_MULTIPART)


static void
g_mime_multipart_signed_class_init (GMimeMultipartSignedClass *klass)
{
	GMimeEntityClass *object_class = GMIME_ENTITY_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_MULTIPART);
	
	gobject_class->finalize = g_mime_multipart_signed_finalize;
	
	object_class->prepare_with_max_line_length = multipart_signed_prepare;
}

static void
g_mime_multipart_signed_init (GMimeMultipartSigned *mps)
{
	
}

static void
g_mime_multipart_signed_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
multipart_signed_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint, guint max_line_length, GError **error)
{
	/* Do not visit children that have already been signed: changing their
	 * encoding or headers would invalidate the existing signature. */
	return TRUE;
}


/**
 * g_mime_multipart_signed_new:
 *
 * Creates a new MIME multipart/signed object.
 *
 * Returns: an empty MIME multipart/signed object.
 **/
GMimeMultipartSigned *
g_mime_multipart_signed_new (void)
{
	GMimeMultipartSigned *multipart;
	GMimeContentType *content_type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART_SIGNED, NULL);
	
	content_type = g_mime_content_type_new ("multipart", "signed");
	g_mime_entity_set_content_type ((GMimeEntity *) multipart, content_type);
	g_object_unref (content_type);
	
	return multipart;
}


#ifdef ENABLE_CRYPTO

/**
 * sign_prepare:
 * @mime_part: MIME part
 *
 * Prepare a part (and all subparts) to be signed. To do this we need
 * to set the encoding of all parts (that are not already encoded to
 * either QP or Base64) to QP.
 **/
static void
sign_prepare (GMimeEntity *mime_part)
{
	GMimeContentEncoding encoding;
	GMimeMultipart *multipart;
	GMimeEntity *subpart;
	int i, n;
	
	if (GMIME_IS_MULTIPART (mime_part)) {
		multipart = (GMimeMultipart *) mime_part;
		
		if (GMIME_IS_MULTIPART_SIGNED (multipart) ||
		    GMIME_IS_MULTIPART_ENCRYPTED (multipart)) {
			/* must not modify these parts as they must be treated as opaque */
			return;
		}
		
		n = g_mime_multipart_get_count (multipart);
		for (i = 0; i < n; i++) {
			subpart = g_mime_multipart_get_part (multipart, i);
			sign_prepare (subpart);
		}
	} else if (GMIME_IS_MESSAGE_PART (mime_part)) {
		GMimeMessagePartPrivate *priv = _g_mime_message_part_get_private (GMIME_MESSAGE_PART (mime_part));
		GMimeMessagePrivate *message_priv = _g_mime_message_get_private (priv->message);

		subpart = message_priv->mime_part;
		sign_prepare (subpart);
	} else {
		encoding = g_mime_part_get_content_encoding ((GMimePart *) mime_part);
		
		if (encoding != GMIME_CONTENT_ENCODING_BASE64)
			g_mime_part_set_content_encoding ((GMimePart *) mime_part, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	}
}


/**
 * g_mime_multipart_signed_sign: (skip)
 * @ctx: a borrowed GPGME OpenPGP or CMS context
 * @entity: MIME part to sign
 * @result: (out) (optional): a retained native signing result
 * @err: a #GError
 *
 * Signs @entity using the context's configured signers. Prepares the input
 * entity's transfer encodings for signing and returns a new multipart/signed
 * object. Temporarily selects MIME armor/text settings, restoring them on
 * success and failure. See the GPGME integration ownership contract.
 *
 * Returns: (nullable) (transfer full): a new #GMimeMultipartSigned object on success
 * or %NULL on fail. If signing fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeMultipartSigned *
g_mime_multipart_signed_sign (gpgme_ctx_t ctx, GMimeEntity *entity,
			      gpgme_sign_result_t *result, GError **err)
{
	GMimeStream *stream, *filtered, *sigstream;
	gpgme_sign_result_t signed_result = NULL;
	GMimeContentType *content_type;
	GMimeMultipartSigned *mps;
	GMimeContent *content;
	GMimePart *signature;
	GMimeFilter *filter;
	GMimeParser *parser;
	const char *protocol;
	int armor, textmode;
	gboolean success;
	char *micalg;

	if (result)
		*result = NULL;

	g_return_val_if_fail (GMIME_IS_ENTITY (entity), NULL);
	
	if (!(protocol = _g_mime_gpgme_signature_protocol (ctx, err)))
		return NULL;
	
	/* Prepare all the parts for signing... */
	sign_prepare (entity);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	
	/* Note: see rfc3156, section 3 - second note */
	filter = g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ARMOR);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* Note: see rfc3156, section 5.4 (this is the main difference between rfc2015 and rfc3156) */
	filter = g_mime_filter_strip_new ();
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* write the entity out to the stream */
	errno = 0;
	if (g_mime_entity_write_to_stream (entity, NULL, filtered) < 0 ||
	    g_mime_stream_flush (filtered) < 0 || g_mime_stream_reset (stream) < 0) {
		_g_mime_gpgme_io_error (err);
		g_object_unref (filtered);
		g_object_unref (stream);
		return NULL;
	}

	g_object_unref (filtered);
	
	/* Note: see rfc2015 or rfc3156, section 5.1 - we do this *after* writing out
	 * the entity because we'll end up parsing the mime part back out again and
	 * we don't want it to be in DOS format. */
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_unix2dos_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* construct the signature stream */
	sigstream = g_mime_stream_mem_new ();

	/* capture the current context settings */
	armor = gpgme_get_armor (ctx);
	textmode = gpgme_get_textmode (ctx);

	/* override the context settings */
	gpgme_set_armor (ctx, gpgme_get_protocol (ctx) == GPGME_PROTOCOL_OpenPGP);
	gpgme_set_textmode (ctx, FALSE);
	
	/* sign the content stream */
	success = g_mime_gpgme_sign (ctx, GPGME_SIG_MODE_DETACH, filtered, sigstream, &signed_result, err);

	/* restore the original context settings */
	gpgme_set_textmode (ctx, textmode);
	gpgme_set_armor (ctx, armor);

	if (result && signed_result) {
		*result = signed_result;
		gpgme_result_ref (signed_result);
	}

	micalg = success ? _g_mime_gpgme_micalg (ctx, signed_result, err) : NULL;

	if (signed_result)
		gpgme_result_unref (signed_result);

	if (!micalg) {
		g_object_unref (sigstream);
		g_object_unref (filtered);
		g_object_unref (stream);
		return NULL;
	}
	
	g_object_unref (filtered);
	g_mime_stream_reset (sigstream);
	g_mime_stream_reset (stream);
	
	/* construct the content part */
	parser = g_mime_parser_new_with_stream (stream);
	entity = g_mime_parser_construct_part (parser, NULL, err);
	g_object_unref (stream);
	g_object_unref (parser);

	if (entity == NULL) {
		if (err == NULL || *err == NULL)
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     _("Cannot sign MIME part: failed to parse canonicalized content."));
		g_object_unref (sigstream);
		g_free (micalg);
		return NULL;
	}
	
	/* construct the signature part */
	content_type = g_mime_content_type_parse (NULL, protocol);
	signature = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	g_object_unref (content_type);
	
	content = g_mime_content_new ();
	g_mime_content_set_stream (content, sigstream);
	g_mime_part_set_content (signature, content);
	g_object_unref (sigstream);
	g_object_unref (content);
	
	if (!g_ascii_strcasecmp (protocol, "application/pkcs7-signature")) {
		g_mime_part_set_content_encoding (signature, GMIME_CONTENT_ENCODING_BASE64);
		g_mime_part_set_filename (signature, "smime.p7m");
	}
	
	/* save the content and signature parts */
	mps = g_mime_multipart_signed_new ();
	g_mime_multipart_add ((GMimeMultipart *) mps, entity);
	g_mime_multipart_add ((GMimeMultipart *) mps, (GMimeEntity *) signature);
	g_object_unref (signature);
	g_object_unref (entity);
	
	/* set the multipart/signed protocol and micalg */
	g_mime_entity_set_content_type_parameter ((GMimeEntity *) mps, "protocol", protocol);
	g_mime_entity_set_content_type_parameter ((GMimeEntity *) mps, "micalg", micalg);
	g_free (micalg);
	g_mime_multipart_set_boundary ((GMimeMultipart *) mps, NULL);
	
	return mps;
}

static gboolean
mime_types_equal (const char *mime_type, const char *official_type)
{
	const char *subtype;
	char *xmime_type;
	gboolean rv;
	
	if (!official_type)
		return FALSE;
	
	if (!g_ascii_strcasecmp (mime_type, official_type))
		return TRUE;
	
	if (!(subtype = strrchr (official_type, '/')))
		return FALSE;
	
	subtype++;
	
	/* If the official mime-type's subtype already begins with "x-", then there's
	 * nothing else to check. */
	if (!g_ascii_strncasecmp (subtype, "x-", 2))
		return FALSE;
	
	/* Check if the "x-" version of the official mime-type matches the
	 * supplied mime-type. For example, if the official mime-type is
	 * "application/pkcs7-signature", then we also want to
	 * match "application/x-pkcs7-signature". */
	xmime_type = g_strdup_printf ("%.*sx-%s", (int) (subtype - official_type), official_type, subtype);
	rv = !g_ascii_strcasecmp (mime_type, xmime_type);
	g_free (xmime_type);
	
	return rv;
}

/**
 * g_mime_multipart_signed_verify: (skip)
 * @mps: a #GMimeMultipartSigned
 * @ctx: a borrowed GPGME context matching the signature protocol
 * @result: (out) (optional): a retained native verification result
 * @err: a #GError
 *
 * Attempts to verify the signed MIME part contained within the
 * multipart/signed object @mps. Only surrounding ASCII linear whitespace
 * is removed from the protocol parameter.
 *
 * Returns: TRUE if verification completed, not an assertion of signature
 * validity. Inspect the returned signatures. On failure, @err is set.
 **/
gboolean
g_mime_multipart_signed_verify (GMimeMultipartSigned *mps, gpgme_ctx_t ctx,
				gpgme_verify_result_t *result, GError **err)
{
	GMimeStream *stream = NULL, *sigstream = NULL;
	GMimeEntityPrivate *signature_priv;
	GMimeEntity *content, *signature;
	const char *supported, *value;
	GMimeFormatOptions *options;
	char *mime_type, *protocol;
	gboolean success = FALSE;
	GMimeContent *wrapper;
	ssize_t written;
	int errsv;
	
	if (result)
		*result = NULL;

	g_return_val_if_fail (GMIME_IS_MULTIPART_SIGNED (mps), FALSE);

	if (!(supported = _g_mime_gpgme_signature_protocol (ctx, err)))
		return FALSE;
	
	if (g_mime_multipart_get_count ((GMimeMultipart *) mps) < 2) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part due to missing subparts."));
		return FALSE;
	}
	
	value = g_mime_entity_get_content_type_parameter ((GMimeEntity *) mps, "protocol");
	protocol = g_mime_strdup_trim (value);

	if (!protocol || !*protocol) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot verify multipart/signed part: unspecified signature protocol."));
		
		g_free (protocol);
		return FALSE;
	}
	
	/* make sure the protocol matches the crypto sign protocol */
	if (!mime_types_equal (protocol, supported)) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot verify multipart/signed part: unsupported signature protocol '%s'."),
			     protocol);
		g_free (protocol);
		
		return FALSE;
	}

	g_free (protocol);
	
	signature = g_mime_multipart_get_part ((GMimeMultipart *) mps, GMIME_MULTIPART_SIGNED_SIGNATURE);

	if (!GMIME_IS_PART (signature) || !(wrapper = g_mime_part_get_content ((GMimePart *) signature))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part: signature content is missing."));
		return FALSE;
	}
	
	/* make sure the protocol matches the signature content-type */
	signature_priv = _g_mime_entity_get_private (signature);
	mime_type = g_mime_content_type_get_mime_type (signature_priv->content_type);

	if (!mime_types_equal (mime_type, supported)) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part: signature content-type does not match protocol."));
		g_free (mime_type);
		
		return FALSE;
	}

	g_free (mime_type);
	
	content = g_mime_multipart_get_part ((GMimeMultipart *) mps, GMIME_MULTIPART_SIGNED_CONTENT);
	
	/* get the content stream */
	stream = g_mime_stream_mem_new ();
	
	/* Note: see rfc2015 or rfc3156, section 5.1 */
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	errno = 0;
	written = _g_mime_writer_write_for_signature (content, options, stream);
	g_mime_format_options_free (options);

	if (written < 0)
		goto io_error;

	errno = 0;
	if (g_mime_stream_reset (stream) < 0)
		goto io_error;
	
	/* get the signature stream */
	sigstream = g_mime_stream_mem_new ();
	errno = 0;
	if (g_mime_content_decode_to_stream (wrapper, sigstream) < 0)
		goto io_error;

	errno = 0;
	if (g_mime_stream_reset (sigstream) < 0)
		goto io_error;
	
	/* verify the signature */
	success = g_mime_gpgme_verify (ctx, stream, sigstream, NULL, result, err);
	
	d({
		GMimeStreamMemPrivate *priv = _g_mime_stream_mem_get_private (GMIME_STREAM_MEM (stream));

		printf ("attempted to verify:\n----- BEGIN SIGNED PART -----\n%.*s----- END SIGNED PART -----\n",
			(int) priv->buffer->len, priv->buffer->data);
	});
	
	goto done;

io_error:
	errsv = errno;
	g_set_error (err, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
		     _("Cannot read MIME data for signature verification: %s."),
		     errsv ? g_strerror (errsv) : _("Unknown error"));
done:
	g_clear_object (&sigstream);
	g_clear_object (&stream);
	
	return success;
}

#endif /* ENABLE_CRYPTO */
