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
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART_SIGNED, NULL);
	
	content_type = g_mime_content_type_new ("multipart", "signed");
	g_mime_object_set_content_type ((GMimeObject *) multipart, content_type);
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
		encoding = g_mime_part_get_content_encoding ((GMimePart *) mime_part);
		
		if (encoding != GMIME_CONTENT_ENCODING_BASE64)
			g_mime_part_set_content_encoding ((GMimePart *) mime_part, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
	}
}


/**
 * g_mime_multipart_signed_sign:
 * @ctx: a #GMimeCryptoContext
 * @entity: MIME part to sign
 * @userid: user id to sign with
 * @err: a #GError
 *
 * Attempts to sign the @content MIME part with @userid's private key
 * using the @ctx signing context. If successful, a new multipart/signed
 * object is returned.
 *
 * Returns: (nullable) (transfer full): a new #GMimeMultipartSigned object on success
 * or %NULL on fail. If signing fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeMultipartSigned *
g_mime_multipart_signed_sign (GMimeCryptoContext *ctx, GMimeObject *entity,
			      const char *userid, GError **err)
{
	GMimeStream *stream, *filtered, *sigstream;
	GMimeContentType *content_type;
	GMimeDataWrapper *content;
	GMimeMultipartSigned *mps;
	GMimePart *signature;
	GMimeFilter *filter;
	GMimeParser *parser;
	const char *protocol;
	const char *micalg;
	int algo;
	
	g_return_val_if_fail (GMIME_IS_CRYPTO_CONTEXT (ctx), NULL);
	g_return_val_if_fail (GMIME_IS_OBJECT (entity), NULL);
	
	if (!(protocol = g_mime_crypto_context_get_signature_protocol (ctx))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("Signing not supported."));
		return NULL;
	}
	
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
	g_mime_object_write_to_stream (entity, NULL, filtered);
	g_mime_stream_flush (filtered);
	g_mime_stream_reset (stream);
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
	
	/* sign the content stream */
	if ((algo = g_mime_crypto_context_sign (ctx, TRUE, userid, filtered, sigstream, err)) == -1) {
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
	entity = g_mime_parser_construct_part (parser, NULL);
	g_object_unref (stream);
	g_object_unref (parser);
	
	/* construct the signature part */
	content_type = g_mime_content_type_parse (NULL, protocol);
	signature = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	g_object_unref (content_type);
	
	content = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (content, sigstream);
	g_mime_part_set_content (signature, content);
	g_object_unref (sigstream);
	g_object_unref (content);
	
	/* FIXME: temporary hack, this info should probably be set in
	 * the CryptoContext class - maybe ::sign can take/output a
	 * GMimePart instead. */
	if (!g_ascii_strcasecmp (protocol, "application/pkcs7-signature")) {
		g_mime_part_set_content_encoding (signature, GMIME_CONTENT_ENCODING_BASE64);
		g_mime_part_set_filename (signature, "smime.p7m");
	}
	
	/* save the content and signature parts */
	mps = g_mime_multipart_signed_new ();
	g_mime_multipart_add ((GMimeMultipart *) mps, entity);
	g_mime_multipart_add ((GMimeMultipart *) mps, (GMimeObject *) signature);
	g_object_unref (signature);
	g_object_unref (entity);
	
	/* set the multipart/signed protocol and micalg */
	micalg = g_mime_crypto_context_digest_name (ctx, (GMimeDigestAlgo) algo);
	g_mime_object_set_content_type_parameter ((GMimeObject *) mps, "protocol", protocol);
	g_mime_object_set_content_type_parameter ((GMimeObject *) mps, "micalg", micalg);
	g_mime_multipart_set_boundary ((GMimeMultipart *) mps, NULL);
	
	return mps;
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
 * @mps: a #GMimeMultipartSigned
 * @flags: a #GMimeVerifyFlags
 * @err: a #GError
 *
 * Attempts to verify the signed MIME part contained within the
 * multipart/signed object @mps.
 *
 * Returns: (nullable) (transfer full): a new #GMimeSignatureList object on
 * success or %NULL on fail. If the verification fails, an exception
 * will be set on @err to provide information as to why the failure
 * occurred.
 **/
GMimeSignatureList *
g_mime_multipart_signed_verify (GMimeMultipartSigned *mps, GMimeVerifyFlags flags, GError **err)
{
	const char *supported, *protocol;
	GMimeObject *content, *signature;
	GMimeStream *stream, *sigstream;
	GMimeSignatureList *signatures;
	GMimeFormatOptions *options;
	GMimeDataWrapper *wrapper;
	GMimeCryptoContext *ctx;
	char *mime_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_SIGNED (mps), NULL);
	
	if (g_mime_multipart_get_count ((GMimeMultipart *) mps) < 2) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part due to missing subparts."));
		return NULL;
	}
	
	if (!(protocol = g_mime_object_get_content_type_parameter ((GMimeObject *) mps, "protocol"))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
				     _("Cannot verify multipart/signed part: unspecified signature protocol."));
		
		return NULL;
	}
	
	if (!(ctx = g_mime_crypto_context_new (protocol))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot verify multipart/signed part: unregistered signature protocol '%s'."),
			     protocol);
		
		return NULL;
	}
	
	supported = g_mime_crypto_context_get_signature_protocol (ctx);
	
	/* make sure the protocol matches the crypto sign protocol */
	if (!check_protocol_supported (protocol, supported)) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot verify multipart/signed part: unsupported signature protocol '%s'."),
			     protocol);
		g_object_unref (ctx);
		
		return NULL;
	}
	
	signature = g_mime_multipart_get_part ((GMimeMultipart *) mps, GMIME_MULTIPART_SIGNED_SIGNATURE);
	
	/* make sure the protocol matches the signature content-type */
	mime_type = g_mime_content_type_get_mime_type (signature->content_type);
	if (g_ascii_strcasecmp (mime_type, protocol) != 0) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Cannot verify multipart/signed part: signature content-type does not match protocol."));
		g_object_unref (ctx);
		g_free (mime_type);
		
		return NULL;
	}
	g_free (mime_type);
	
	content = g_mime_multipart_get_part ((GMimeMultipart *) mps, GMIME_MULTIPART_SIGNED_CONTENT);
	
	/* get the content stream */
	stream = g_mime_stream_mem_new ();
	
	/* Note: see rfc2015 or rfc3156, section 5.1 */
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	g_mime_object_write_to_stream (content, options, stream);
	g_mime_format_options_free (options);
	g_mime_stream_reset (stream);
	
	/* get the signature stream */
	wrapper = g_mime_part_get_content ((GMimePart *) signature);
	
	sigstream = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (wrapper, sigstream);
	g_mime_stream_reset (sigstream);
	
	/* verify the signature */
	signatures = g_mime_crypto_context_verify (ctx, flags, stream, sigstream, NULL, err);
	
	d(printf ("attempted to verify:\n----- BEGIN SIGNED PART -----\n%.*s----- END SIGNED PART -----\n",
		  (int) GMIME_STREAM_MEM (stream)->buffer->len, GMIME_STREAM_MEM (stream)->buffer->data));
	
	g_object_unref (sigstream);
	g_object_unref (stream);
	g_object_unref (ctx);
	
	return signatures;
}
