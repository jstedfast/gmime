/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include "gmime-multipart-encrypted.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-crlf.h"
#include "gmime-filter-from.h"
#include "gmime-filter-crlf.h"
#include "gmime-stream-mem.h"
#include "gmime-parser.h"
#include "gmime-part.h"
#include "strlib.h"

#define d(x)

/* GObject class methods */
static void g_mime_multipart_encrypted_class_init (GMimeMultipartEncryptedClass *klass);
static void g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mps, GMimeMultipartEncryptedClass *klass);
static void g_mime_multipart_encrypted_finalize (GObject *object);

/* GMimeObject class methods */
static void multipart_encrypted_init (GMimeObject *object);
static void multipart_encrypted_add_header (GMimeObject *object, const char *header, const char *value);
static void multipart_encrypted_set_header (GMimeObject *object, const char *header, const char *value);
static const char *multipart_encrypted_get_header (GMimeObject *object, const char *header);
static void multipart_encrypted_remove_header (GMimeObject *object, const char *header);
static void multipart_encrypted_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *multipart_encrypted_get_headers (GMimeObject *object);
static int multipart_encrypted_write_to_stream (GMimeObject *object, GMimeStream *stream);


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
			16,   /* n_preallocs */
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
	
	object_class->init = multipart_encrypted_init;
	object_class->add_header = multipart_encrypted_add_header;
	object_class->set_header = multipart_encrypted_set_header;
	object_class->get_header = multipart_encrypted_get_header;
	object_class->remove_header = multipart_encrypted_remove_header;
	object_class->set_content_type = multipart_encrypted_set_content_type;
	object_class->get_headers = multipart_encrypted_get_headers;
	object_class->write_to_stream = multipart_encrypted_write_to_stream;
}

static void
g_mime_multipart_encrypted_init (GMimeMultipartEncrypted *mpe, GMimeMultipartEncryptedClass *klass)
{
	mpe->protocol = NULL;
	mpe->decrypted = NULL;
}

static void
g_mime_multipart_encrypted_finalize (GObject *object)
{
	GMimeMultipartEncrypted *mpe = (GMimeMultipartEncrypted *) object;
	
	g_free (mpe->protocol);
	
	if (mpe->decrypted)
		g_mime_object_unref (mpe->decrypted);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
multipart_encrypted_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}

static void
multipart_encrypted_add_header (GMimeObject *object, const char *header, const char *value)
{
	GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
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

static void
multipart_encrypted_remove_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
multipart_encrypted_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMimeMultipartEncrypted *mpe = (GMimeMultipart *) object;
	const char *protocol;
	
	protocol = g_mime_content_type_get_parameter (content_type, "protocol");
	g_free (mpe->protocol);
	mpe->protocol = g_strdup (protocol);
	
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
 * Returns an empty MIME multipart/encrypted object.
 **/
GMimeMultipart *
g_mime_multipart_encrypted_new ()
{
	GMimeMultipart *multipart;
	GMimeContentType *type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART_ENCRYPTED, NULL, NULL);
	
	type = g_mime_content_type_new ("multipart", "encrypted");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), type);
	
	return multipart;
}


int
g_mime_multipart_encrypted_encrypt (GMimeMultipartEncrypted *mpe, GMimeObject *content,
				    GMimeCipherContext *ctx, GPtrArray *recipients,
				    GMimeException *ex)
{
	GMimePart *version_part, *encrypted_part;
	GMimeContentType *content_type;
	GMimeDataWrapper *wrapper;
	GMimeStream *filtered_stream;
	GMimeStream *stream, *ciphertext;
	GMimeFilter *crlf_filter;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), -1);
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), -1);
	g_return_val_if_fail (ctx->encrypt_protocol != NULL, -1);
	g_return_val_if_fail (GMIME_IS_OBJECT (content), -1);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	
	g_mime_object_write_to_stream (content, filtered_stream);
	g_mime_stream_flush (filtered_stream);
	g_mime_stream_unref (filtered_stream);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* encrypt the content stream */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_cipher_encrypt (ctx, FALSE, NULL, recipients, stream, ciphertext, ex) == -1) {
		g_mime_stream_unref (ciphertext);
		g_mime_stream_unref (stream);
		return -1;
	}
	
	g_mime_stream_unref (stream);
	g_mime_stream_reset (ciphertext);
	
	/* construct the version part */
	version_part = g_mime_part_new ();
	g_mime_part_set_encoding (version_part, GMIME_PART_ENCODING_7BIT);
	g_mime_part_set_content (version_part, "Version: 1\n", strlen ("Version: 1\n"));
	content_type = g_mime_content_type_new_from_string (ctx->encrypt_protocol);
	g_mime_object_set_content_type (GMIME_OBJECT (version_part), content_type);
	
	mpe->protocol = g_strdup (ctx->encrypt_protocol);
	g_mime_object_ref (content);
	mpe->decrypted = content;
	
	/* construct the encrypted mime part */
	encrypted_part = g_mime_part_new_with_type ("application", "octet-stream");
	wrapper = g_mime_data_wrapper_new ();
	g_mime_data_wrapper_set_stream (wrapper, ciphertext);
	g_mime_stream_unref (ciphertext);
	g_mime_part_set_content_object (encrypted_part, wrapper);
	g_mime_part_set_filename (encrypted_part, "encrypted.asc");
	g_mime_part_set_encoding (encrypted_part, GMIME_PART_ENCODING_7BIT);
	
	/* save the version and encrypted parts */
	/* FIXME: make sure there aren't any other parts?? */
	g_mime_multipart_add_part (GMIME_MULTIPART (mpe), version_part);
	g_mime_object_unref (GMIME_OBJECT (version_part));
	g_mime_multipart_add_part (GMIME_MULTIPART (mpe), encrypted_part);
	g_mime_object_unref (GMIME_OBJECT (encrypted_part));
	
	/* set the content-type params for this multipart/encrypted part */
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (mpe), "protocol", mpe->protocol);
	g_mime_multipart_set_boundary (GMIME_MULTIPART (mpe), NULL);
	
	return 0;
}


GMimeObject *
g_mime_multipart_encrypted_decrypt (GMimeMultipartEncrypted *mpe, GMimeCipherContext *ctx,
				    GMimeException *ex)
{
	GMimeObject *decrypted, *version, *encrypted;
	const GMimeDataWrapper *wrapper;
	GMimeStream *stream, *ciphertext;
	GMimeStream *filtered_stream;
	GMimeFilter *crlf_filter;
	const char *protocol;
	char *content_type;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART_ENCRYPTED (mpe), NULL);
	g_return_val_if_fail (GMIME_IS_CIPHER_CONTEXT (ctx), NULL);
	g_return_val_if_fail (ctx->encrypt_protocol != NULL, NULL);
	
	if (mpe->decrypted) {
		/* we seem to have already decrypted the part */
		g_mime_object_ref (mpe->decrypted);
		return mpe->decrypted;
	}
	
	protocol = g_mime_object_get_content_type_parameter (GMIME_OBJECT (mpe), "protocol");
	
	if (protocol) {
		/* make sure the protocol matches the cipher encrypt protocol */
		if (strcasecmp (ctx->encrypt_protocol, protocol) != 0) {
			g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
					      "Failed to decrypt MIME part: protocol error");
			
			return NULL;
		}
	} else {
		/* *shrug* - I guess just go on as if they match? */
		protocol = ctx->encrypt_protocol;
	}
	
	version = g_mime_multipart_get_part (GMIME_MULTIPART (mpe), 0);
	
	/* make sure the protocol matches the version part's content-type */
	content_type = g_mime_content_type_to_string (version->content_type);
	if (strcasecmp (content_type, protocol) != 0) {
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Failed to decrypt MIME part: protocol error");
		
		g_mime_object_unref (version);
		g_free (content_type);
		
		return NULL;
	}
	g_free (content_type);
	
	/* get the encrypted part and check that it is of type application/octet-stream */
	encrypted = g_mime_multipart_get_part (GMIME_MULTIPART (mpe), 1);
	mime_type = g_mime_object_get_content_type (encrypted_part);
	if (!g_mime_content_type_is_type (mime_type, "application", "octet-stream")) {
		g_mime_object_unref (encrypted);
		g_mime_object_unref (version);
		return NULL;
	}
	
	/* get the ciphertext stream */
	wrapper = g_mime_part_get_content_object (GMIME_PART (encrypted));
	ciphertext = g_mime_data_wrapper_get_stream ((GMimeDataWrapper *) wrapper);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_DECODE,
					      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), crlf_filter);
	
	/* get the cleartext */
	if (g_mime_cipher_decrypt (ctx, ciphertext, filtered_stream, ex) == -1) {
		g_mime_stream_unref (filtered_stream);
		g_mime_stream_unref (ciphertext);
		g_mime_stream_unref (stream);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered_stream);
	g_mime_stream_unref (filtered_stream);
	g_mime_stream_unref (ciphertext);
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_mime_stream_unref (stream);
	
	decrypted = g_mime_parser_construct_part (parser);
	g_object_unref (parser);
	
	if (decrypted) {
		/* cache the decrypted part */
		g_mime_object_ref (decrypted);
		mpe->decrypted = decrypted;
	} else {
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      "Failed to decrypt MIME part: parse error");
	}
	
	return decrypted;
}
