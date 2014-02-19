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

#include <string.h>

#include "gmime-multipart-signed.h"
#include "gmime-multipart-encrypted.h"
#include "gmime-message-part.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-strip.h"
#include "gmime-filter-from.h"
#include "gmime-filter-crlf.h"
#include "gmime-stream-mem.h"
#include "gmime-parser.h"
#include "gmime-error.h"
#include "gmime-part.h"

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


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
static void g_mime_multipart_signed_init (GMimeMultipartSigned *mps, GMimeMultipartSignedClass *klass);
static void g_mime_multipart_signed_finalize (GObject *object);

/* GMimeObject class methods */
static void multipart_signed_encode (GMimeObject *object, GMimeEncodingConstraint constraint);


static GMimeMultipartClass *parent_class = NULL;


GType
g_mime_multipart_signed_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMultipartSignedClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_multipart_signed_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMultipartSigned),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_multipart_signed_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_MULTIPART, "GMimeMultipartSigned", &info, 0);
	}
	
	return type;
}


static void
g_mime_multipart_signed_class_init (GMimeMultipartSignedClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_MULTIPART);
	
	gobject_class->finalize = g_mime_multipart_signed_finalize;
	
	object_class->encode = multipart_signed_encode;
}

static void
g_mime_multipart_signed_init (GMimeMultipartSigned *mps, GMimeMultipartSignedClass *klass)
{
	
}

static void
g_mime_multipart_signed_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
multipart_signed_encode (GMimeObject *object, GMimeEncodingConstraint constraint)
{
	/* Do NOT encode subparts of a multipart/signed */
	return;
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
	
	multipart = g_object_newv (GMIME_TYPE_MULTIPART_SIGNED, 0, NULL);
	
	content_type = g_mime_content_type_new ("multipart", "signed");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), content_type);
	g_object_unref (content_type);
	
	return multipart;
}


/**
 * sign_prepare:
 * @mime_part: MIME part
 *
 * Prepare a part (and all subparts) to be signed. To do this we need
 * to set the encoding of all parts (that are not already encoded to
 * either QP or Base64) to QP.
 **/
static void
sign_prepare (GMimeObject *mime_part)
{
	GMimeContentEncoding encoding;
	GMimeMultipart *multipart;
	GMimeObject *subpart;
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
		subpart = GMIME_MESSAGE_PART (mime_part)->message->mime_part;
		sign_prepare (subpart);
	} else {
		encoding = g_mime_part_get_content_encoding (GMIME_PART (mime_part));
		
		if (encoding != GMIME_CONTENT_ENCODING_BASE64)
			g_mime_part_set_content_encoding (GMIME_PART (mime_part),
							  GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	}
}


/**
 * g_mime_multipart_signed_sign:
 * @mps: multipart/signed object
 * @content: MIME part to sign
 * @ctx: encryption crypto context
 * @userid: user id to sign with
 * @digest: preferred digest algorithm
 * @err: exception
 *
 * Attempts to sign the @content MIME part with @userid's private key
 * using the @ctx signing context with the @digest algorithm. If
 * successful, the signed #GMimeObject is set as the signed part of
 * the multipart/signed object @mps.
 *
 * Returns: %0 on success or %-1 on fail. If the signing fails, an
 * exception will be set on @err to provide information as to why the
 * failure occured.
 **/
int
g_mime_multipart_signed_sign (GMimeMultipartSigned *mps, GMimeObject *content,
			      GMimeCryptoContext *ctx, const char *userid,
			      GMimeDigestAlgo digest, GError **err)
{
	GMimeStream *stream, *filtered, *sigstream;
	GMimeContentType *content_type;
	GMimeDataWrapper *wrapper;
	GMimePart *signature;
	GMimeFilter *filter;
	GMimeParser *parser;
	const char *protocol;
	const char *micalg;
	int rv;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_SIGNED (mps), -1);
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), -1);
	g_return_val_if_fail (GMIME_IS_OBJECT (content), -1);
	
	if (!(protocol = g_mime_crypto_context_get_signature_protocol (ctx))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("Signing not supported."));
		return -1;
	}
	
	/* Prepare all the parts for signing... */
	sign_prepare (content);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	
	/* Note: see rfc3156, section 3 - second note */
	filter = g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ARMOR);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	
	/* Note: see rfc3156, section 5.4 (this is the main difference between rfc2015 and rfc3156) */
	filter = g_mime_filter_strip_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	
	g_mime_object_write_to_stream (content, filtered);
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_mime_stream_reset (stream);
	
	/* Note: see rfc2015 or rfc3156, section 5.1 */
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_crlf_new (TRUE, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
	g_object_unref (filter);
	
	/* construct the signature stream */
	sigstream = g_mime_stream_mem_new ();
	
	/* sign the content stream */
	if ((rv = g_mime_crypto_context_sign (ctx, userid, digest, filtered, sigstream, err)) == -1) {
		g_object_unref (sigstream);
		g_object_unref (filtered);
		g_object_unref (stream);
		return -1;
	}
	
	g_object_unref (filtered);
	g_mime_stream_reset (sigstream);
	g_mime_stream_reset (stream);
	
	/* set the multipart/signed protocol and micalg */
	content_type = g_mime_object_get_content_type (GMIME_OBJECT (mps));
	g_mime_content_type_set_parameter (content_type, "protocol", protocol);
	micalg = g_strdup (g_mime_crypto_context_digest_name (ctx, (GMimeDigestAlgo) rv));
	g_mime_content_type_set_parameter (content_type, "micalg", micalg);
	g_mime_multipart_set_boundary (GMIME_MULTIPART (mps), NULL);
	
	/* construct the content part */
	parser = g_mime_parser_new_with_stream (stream);
	content = g_mime_parser_construct_part (parser);
	g_object_unref (stream);
	g_object_unref (parser);
	
	/* construct the signature part */
	content_type = g_mime_content_type_new_from_string (protocol);
	signature = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	g_object_unref (content_type);
	
	wrapper = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (wrapper, sigstream);
	g_mime_part_set_content_object (signature, wrapper);
	g_object_unref (sigstream);
	g_object_unref (wrapper);
	
	/* FIXME: temporary hack, this info should probably be set in
	 * the CryptoContext class - maybe ::sign can take/output a
	 * GMimePart instead. */
	if (!g_ascii_strcasecmp (protocol, "application/pkcs7-signature")) {
		g_mime_part_set_content_encoding (signature, GMIME_CONTENT_ENCODING_BASE64);
		g_mime_part_set_filename (signature, "smime.p7m");
	}
	
	/* save the content and signature parts */
	/* FIXME: make sure there aren't any other parts?? */
	g_mime_multipart_add (GMIME_MULTIPART (mps), content);
	g_mime_multipart_add (GMIME_MULTIPART (mps), (GMimeObject *) signature);
	g_object_unref (signature);
	g_object_unref (content);
	
	return 0;
}

static gboolean
check_protocol_supported (const char *protocol, const char *supported)
{
	const char *subtype;
	char *xsupported;
	gboolean rv;
	
	if (!supported)
		return FALSE;
	
	if (!g_ascii_strcasecmp (protocol, supported))
		return TRUE;
	
	if (!(subtype = strrchr (supported, '/')))
		return FALSE;
	
	subtype++;
	
	/* If the subtype already begins with "x-", then there's
	 * nothing else to check. */
	if (!g_ascii_strncasecmp (subtype, "x-", 2))
		return FALSE;
	
	/* Check if the "x-" version of the subtype matches the
	 * protocol. For example, if the supported protocol is
	 * "application/pkcs7-signature", then we also want to
	 * match "application/x-pkcs7-signature". */
	xsupported = g_strdup_printf ("%.*sx-%s", (int) (subtype - supported), supported, subtype);
	rv = !g_ascii_strcasecmp (protocol, xsupported);
	g_free (xsupported);
	
	return rv;
}


/**
 * g_mime_multipart_signed_verify:
 * @mps: multipart/signed object
 * @ctx: encryption crypto context
 * @err: exception
 *
 * Attempts to verify the signed MIME part contained within the
 * multipart/signed object @mps using the @ctx crypto context.
 *
 * Returns: (transfer full): a new #GMimeSignatureList object on
 * success or %NULL on fail. If the verification fails, an exception
 * will be set on @err to provide information as to why the failure
 * occured.
 **/
GMimeSignatureList *
g_mime_multipart_signed_verify (GMimeMultipartSigned *mps, GMimeCryptoContext *ctx,
				GError **err)
{
	const char *supported, *protocol, *micalg;
	GMimeObject *content, *signature;
	GMimeStream *stream, *sigstream;
	GMimeSignatureList *signatures;
	GMimeStream *filtered_stream;
	GMimeDataWrapper *wrapper;
	GMimeFilter *crlf_filter;
	GMimeDigestAlgo digest;
	char *content_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_SIGNED (mps), NULL);
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	
	if (g_mime_multipart_get_count ((GMimeMultipart *) mps) < 2) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part due to missing subparts."));
		return NULL;
	}
	
	protocol = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mps), "protocol");
	micalg = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mps), "micalg");
	
	supported = g_mime_crypto_context_get_signature_protocol (ctx);
	
	if (protocol) {
		/* make sure the protocol matches the crypto sign protocol */
		if (!check_protocol_supported (protocol, supported)) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot verify multipart/signed part: unsupported signature protocol '%s'."),
				     protocol);
			return NULL;
		}
	} else if (supported != NULL) {
		/* *shrug* - I guess just go on as if they match? */
		protocol = supported;
	} else {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot verify multipart/signed part: unspecified signature protocol."));
		
		return NULL;
	}
	
	signature = g_mime_multipart_get_part (GMIME_MULTIPART (mps), GMIME_MULTIPART_SIGNED_SIGNATURE);
	
	/* make sure the protocol matches the signature content-type */
	content_type = g_mime_content_type_to_string (signature->content_type);
	if (g_ascii_strcasecmp (content_type, protocol) != 0) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part: signature content-type does not match protocol."));
		g_free (content_type);
		
		return NULL;
	}
	g_free (content_type);
	
	content = g_mime_multipart_get_part (GMIME_MULTIPART (mps), GMIME_MULTIPART_SIGNED_CONTENT);
	
	/* get the content stream */
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new (stream);
	
	/* Note: see rfc2015 or rfc3156, section 5.1 */
	crlf_filter = g_mime_filter_crlf_new (TRUE, FALSE);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_object_unref (crlf_filter);
	
	g_mime_object_write_to_stream (content, filtered_stream);
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	g_mime_stream_reset (stream);
	
	/* get the signature stream */
	wrapper = g_mime_part_get_content_object (GMIME_PART (signature));
	
	/* FIXME: temporary hack for Balsa to support S/MIME,
	 * ::verify() should probably take a mime part so it can
	 * decode this itself if it needs to. */
	if (!g_ascii_strcasecmp (protocol, "application/pkcs7-signature") ||
	    !g_ascii_strcasecmp (protocol, "application/x-pkcs7-signature")) {
		sigstream = g_mime_stream_mem_new ();
		g_mime_data_wrapper_write_to_stream (wrapper, sigstream);
	} else {
		sigstream = g_mime_data_wrapper_get_stream (wrapper);
	}
	
	g_mime_stream_reset (sigstream);
	
	/* verify the signature */
	digest = g_mime_crypto_context_digest_id (ctx, micalg);
	signatures = g_mime_crypto_context_verify (ctx, digest, stream, sigstream, err);
	
	d(printf ("attempted to verify:\n----- BEGIN SIGNED PART -----\n%.*s----- END SIGNED PART -----\n",
		  (int) GMIME_STREAM_MEM (stream)->buffer->len, GMIME_STREAM_MEM (stream)->buffer->data));
	
	g_object_unref (stream);
	
	return signatures;
}
