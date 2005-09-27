/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
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
#include "gmime-part.h"

#define d(x) x

/* GObject class methods */
static void g_mime_multipart_signed_class_init (GMimeMultipartSignedClass *klass);
static void g_mime_multipart_signed_init (GMimeMultipartSigned *mps, GMimeMultipartSignedClass *klass);
static void g_mime_multipart_signed_finalize (GObject *object);

/* GMimeObject class methods */
static void multipart_signed_init (GMimeObject *object);
static void multipart_signed_add_header (GMimeObject *object, const char *header, const char *value);
static void multipart_signed_set_header (GMimeObject *object, const char *header, const char *value);
static const char *multipart_signed_get_header (GMimeObject *object, const char *header);
static void multipart_signed_remove_header (GMimeObject *object, const char *header);
static void multipart_signed_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *multipart_signed_get_headers (GMimeObject *object);
static ssize_t multipart_signed_write_to_stream (GMimeObject *object, GMimeStream *stream);


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
			16,   /* n_preallocs */
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
	
	object_class->init = multipart_signed_init;
	object_class->add_header = multipart_signed_add_header;
	object_class->set_header = multipart_signed_set_header;
	object_class->get_header = multipart_signed_get_header;
	object_class->remove_header = multipart_signed_remove_header;
	object_class->set_content_type = multipart_signed_set_content_type;
	object_class->get_headers = multipart_signed_get_headers;
	object_class->write_to_stream = multipart_signed_write_to_stream;
}

static void
g_mime_multipart_signed_init (GMimeMultipartSigned *mps, GMimeMultipartSignedClass *klass)
{
	mps->protocol = NULL;
	mps->micalg = NULL;
}

static void
g_mime_multipart_signed_finalize (GObject *object)
{
	GMimeMultipartSigned *mps = (GMimeMultipartSigned *) object;
	
	g_free (mps->protocol);
	g_free (mps->micalg);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
multipart_signed_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}

static void
multipart_signed_add_header (GMimeObject *object, const char *header, const char *value)
{
	GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
}

static void
multipart_signed_set_header (GMimeObject *object, const char *header, const char *value)
{
	GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
}

static const char *
multipart_signed_get_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
}

static void
multipart_signed_remove_header (GMimeObject *object, const char *header)
{
	GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
multipart_signed_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMimeMultipartSigned *mps = (GMimeMultipartSigned *) object;
	const char *protocol, *micalg;
	
	protocol = g_mime_content_type_get_parameter (content_type, "protocol");
	g_free (mps->protocol);
	mps->protocol = g_strdup (protocol);
	
	micalg = g_mime_content_type_get_parameter (content_type, "micalg");
	g_free (mps->micalg);
	mps->micalg = g_strdup (micalg);
	
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}

static char *
multipart_signed_get_headers (GMimeObject *object)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_headers (object);
}

static ssize_t
multipart_signed_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	return GMIME_OBJECT_CLASS (parent_class)->write_to_stream (object, stream);
}


/**
 * g_mime_multipart_signed_new:
 *
 * Creates a new MIME multipart/signed object.
 *
 * Returns an empty MIME multipart/signed object.
 **/
GMimeMultipartSigned *
g_mime_multipart_signed_new ()
{
	GMimeMultipartSigned *multipart;
	GMimeContentType *type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART_SIGNED, NULL, NULL);
	
	type = g_mime_content_type_new ("multipart", "signed");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), type);
	
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
	GMimePartEncodingType encoding;
	GMimeObject *subpart;
	
	if (GMIME_IS_MULTIPART (mime_part)) {
		GList *lpart;
		
		if (GMIME_IS_MULTIPART_SIGNED (mime_part) || GMIME_IS_MULTIPART_ENCRYPTED (mime_part)) {
			/* must not modify these parts as they must be treated as opaque */
			return;
		}
		
		lpart = GMIME_MULTIPART (mime_part)->subparts;
		while (lpart) {
			subpart = GMIME_OBJECT (lpart->data);
			sign_prepare (subpart);
			lpart = lpart->next;
		}
	} else if (GMIME_IS_MESSAGE_PART (mime_part)) {
		subpart = GMIME_MESSAGE_PART (mime_part)->message->mime_part;
		sign_prepare (subpart);
	} else {
		encoding = g_mime_part_get_encoding (GMIME_PART (mime_part));
		
		if (encoding != GMIME_PART_ENCODING_BASE64)
			g_mime_part_set_encoding (GMIME_PART (mime_part),
						  GMIME_PART_ENCODING_QUOTEDPRINTABLE);
	}
}


/**
 * g_mime_multipart_signed_sign:
 * @mps: multipart/signed object
 * @content: MIME part to sign
 * @ctx: encryption cipher context
 * @userid: user id to sign with
 * @hash: preferred digest algorithm
 * @err: exception
 *
 * Attempts to sign the @content MIME part with @userid's private key
 * using the @ctx signing context with the @hash algorithm. If
 * successful, the signed #GMimeObject is set as the signed part of
 * the multipart/signed object @mps.
 *
 * Returns 0 on success or -1 on fail. If the signing fails, an
 * exception will be set on @err to provide information as to why the
 * failure occured.
 **/
int
g_mime_multipart_signed_sign (GMimeMultipartSigned *mps, GMimeObject *content,
			      GMimeCipherContext *ctx, const char *userid,
			      GMimeCipherHash hash, GError **err)
{
	GMimeObject *signature;
	GMimeDataWrapper *wrapper;
	GMimeStream *filtered_stream;
	GMimeStream *stream, *sigstream;
	GMimeFilter *crlf_filter, *from_filter, *strip_filter;
	GMimeContentType *content_type;
	GMimeParser *parser;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_SIGNED (mps), -1);
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (ctx->sign_protocol != NULL, -1);
	g_return_val_if_fail (GMIME_IS_OBJECT (content), -1);
	
	/* Prepare all the parts for signing... */
	sign_prepare (content);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	
	/* Note: see rfc3156, section 3 - second note */
	from_filter = g_mime_filter_from_new (GMIME_FILTER_FROM_MODE_ARMOR);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), from_filter);
	g_object_unref (from_filter);
	
	/* Note: see rfc3156, section 5.4 (this is the main difference between rfc2015 and rfc3156) */
	strip_filter = g_mime_filter_strip_new ();
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), strip_filter);
	g_object_unref (strip_filter);
	
	g_mime_object_write_to_stream (content, filtered_stream);
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	g_mime_stream_reset (stream);
	
	/* Note: see rfc2015 or rfc3156, section 5.1 */
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_object_unref (crlf_filter);
	
	/* construct the signature stream */
	sigstream = g_mime_stream_mem_new ();
	
	/* sign the content stream */
	if (g_mime_cipher_sign (ctx, userid, hash, filtered_stream, sigstream, err) == -1) {
		g_object_unref (filtered_stream);
		g_object_unref (sigstream);
		g_object_unref (stream);
		return -1;
	}
	
	g_object_unref (filtered_stream);
	g_mime_stream_reset (sigstream);
	g_mime_stream_reset (stream);
	
	/* construct the content part */
	parser = g_mime_parser_new_with_stream (stream);
	content = g_mime_parser_construct_part (parser);
	g_object_unref (stream);
	g_object_unref (parser);
	
	/* construct the signature part */
	signature = (GMimeObject *) g_mime_part_new ();
	wrapper = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (wrapper, sigstream);
	g_mime_part_set_content_object (GMIME_PART (signature), wrapper);
	g_object_unref (sigstream);
	g_object_unref (wrapper);
	
	mps->protocol = g_strdup (ctx->sign_protocol);
	mps->micalg = g_strdup (g_mime_cipher_hash_name (ctx, hash));
	
	/* set the content-type of the signature part */
	content_type = g_mime_content_type_new_from_string (mps->protocol);
	g_mime_object_set_content_type (signature, content_type);
	
	/* FIXME: temporary hack, this info should probably be set in
	 * the CipherContext class - maybe ::sign can take/output a
	 * GMimePart instead. */
	if (!g_ascii_strcasecmp (mps->protocol, "application/pkcs7-signature")) {
		g_mime_part_set_filename (GMIME_PART (signature), "smime.p7m");
		g_mime_part_set_encoding (GMIME_PART (signature), GMIME_PART_ENCODING_BASE64);
	}
	
	/* save the content and signature parts */
	/* FIXME: make sure there aren't any other parts?? */
	g_mime_multipart_add_part (GMIME_MULTIPART (mps), content);
	g_mime_multipart_add_part (GMIME_MULTIPART (mps), signature);
	g_object_unref (signature);
	g_object_unref (content);
	
	/* set the content-type params for this multipart/signed part */
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (mps), "micalg", mps->micalg);
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (mps), "protocol", mps->protocol);
	g_mime_multipart_set_boundary (GMIME_MULTIPART (mps), NULL);
	
	return 0;
}


/**
 * g_mime_multipart_signed_verify:
 * @mps: multipart/signed object
 * @ctx: encryption cipher context
 * @err: exception
 *
 * Attempts to verify the signed MIME part contained within the
 * multipart/signed object @mps using the @ctx cipher context.
 *
 * Returns a new #GMimeCipherValidity object on success or %NULL on
 * fail. If the signing fails, an exception will be set on @err to
 * provide information as to why the failure occured.
 **/
GMimeSignatureValidity *
g_mime_multipart_signed_verify (GMimeMultipartSigned *mps, GMimeCipherContext *ctx,
				GError **err)
{
	GMimeObject *content, *signature;
	GMimeDataWrapper *wrapper;
	GMimeStream *filtered_stream;
	GMimeFilter *crlf_filter;
	GMimeStream *stream, *sigstream;
	const char *protocol, *micalg;
	GMimeSignatureValidity *valid;
	GMimeCipherHash hash;
	char *content_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_SIGNED (mps), NULL);
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	g_return_val_if_fail (ctx->sign_protocol != NULL, NULL);
	
	if (g_mime_multipart_get_number ((GMimeMultipart *) mps) != 2) {
		/* FIXME: set an exception */
		return NULL;
	}
	
	protocol = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mps), "protocol");
	micalg = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mps), "micalg");
	
	if (protocol) {
		/* make sure the protocol matches the cipher sign protocol */
		if (strcasecmp (ctx->sign_protocol, protocol) != 0)
			return NULL;
	} else {
		/* *shrug* - I guess just go on as if they match? */
		protocol = ctx->sign_protocol;
	}
	
	signature = g_mime_multipart_get_part (GMIME_MULTIPART (mps), GMIME_MULTIPART_SIGNED_SIGNATURE);
	
	/* make sure the protocol matches the signature content-type */
	content_type = g_mime_content_type_to_string (signature->content_type);
	if (strcasecmp (content_type, protocol) != 0) {
		g_object_unref (signature);
		g_free (content_type);
		
		return NULL;
	}
	g_free (content_type);
	
	content = g_mime_multipart_get_part (GMIME_MULTIPART (mps), GMIME_MULTIPART_SIGNED_CONTENT);
	
	/* get the content stream */
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	
	/* Note: see rfc2015 or rfc3156, section 5.1 */
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	g_object_unref (crlf_filter);
	
	g_mime_object_write_to_stream (content, filtered_stream);
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	g_mime_stream_reset (stream);
	g_object_unref (content);
	
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
	g_object_unref (signature);
	g_object_unref (wrapper);
	
	/* verify the signature */
	hash = g_mime_cipher_hash_id (ctx, mps->micalg);
	valid = g_mime_cipher_verify (ctx, hash, stream, sigstream, err);
	
	d(printf ("attempted to verify:\n----- BEGIN SIGNED PART -----\n%.*s----- END SIGNED PART -----\n",
		  (int) GMIME_STREAM_MEM (stream)->buffer->len, GMIME_STREAM_MEM (stream)->buffer->data));
	
	g_object_unref (sigstream);
	g_object_unref (stream);
	
	return valid;
}
