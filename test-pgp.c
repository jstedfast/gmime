/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmime.h"

static char *path = "/usr/bin/gpg";
static char *userid = "pgp-mime@xtorshun.org";
static char *passphrase = "PGP/MIME is rfc2015, now go and read it.";

typedef struct _TestSession TestSession;
typedef struct _TestSessionClass TestSessionClass;

struct _TestSession {
	GMimeSession parent_object;
	
};

struct _TestSessionClass {
	GMimeSessionClass parent_class;
	
};

static void test_session_class_init (TestSessionClass *klass);
static void test_session_init (TestSession *session, TestSessionClass *klass);
static void test_session_finalize (GObject *object);

static char *request_passwd (GMimeSession *session, const char *prompt,
			     gboolean secret, const char *item,
			     GMimeException *ex);


static GMimeSessionClass *parent_class = NULL;


static GType
test_session_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (TestSessionClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) test_session_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (TestSession),
			0,    /* n_preallocs */
			NULL, /* object_init */
		};
		
		type = g_type_register_static (GMIME_TYPE_SESSION, "TestSession", &info, 0);
	}
	
	return type;
}


static void
test_session_class_init (TestSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeSessionClass *session_class = GMIME_SESSION_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_SESSION);
	
	session_class->request_passwd = request_passwd;
}


static char *
request_passwd (GMimeSession *session, const char *prompt, gboolean secret, const char *item, GMimeException *ex)
{
#if 0
	char buffer[256];
	
	fprintf (stderr, "%s\nPassphrase: %s\n", prompt, passphrase);
	fgets (buffer, 255, stdin);
	buffer[strlen (buffer)] = '\0'; /* chop off \n */
#endif
	return g_strdup (/*buffer*/passphrase);
}



static int
test_sign (GMimeCipherContext *ctx, const char *cleartext, GMimeCipherHash hash)
{
	GMimeStream *stream, *ciphertext;
	GByteArray *buffer;
	GMimeException *ex;
	
	stream = g_mime_stream_mem_new ();
	g_mime_stream_write_string (stream, cleartext);
	g_mime_stream_reset (stream);
	ciphertext = g_mime_stream_mem_new ();
	
	ex = g_mime_exception_new ();
	g_mime_cipher_sign (ctx, userid, hash, stream, ciphertext, ex);
	g_mime_stream_unref (stream);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_sign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	buffer = GMIME_STREAM_MEM (ciphertext)->buffer;
	fprintf (stderr, "signature:\n%.*s\n", buffer->len, buffer->data);
	g_mime_stream_unref (ciphertext);
	
	return 1;
}

static int
test_encrypt (GMimeCipherContext *ctx, const char *in, int inlen)
{
	GMimeStream *stream, *ciphertext;
	GPtrArray *recipients;
	GByteArray *buffer;
	GMimeException *ex;
	
	stream = g_mime_stream_mem_new_with_buffer (in, inlen);
	ciphertext = g_mime_stream_mem_new ();
	
	ex = g_mime_exception_new ();
	
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, userid);
	
	g_mime_cipher_encrypt (ctx, FALSE, userid, recipients, stream, ciphertext, ex);
	g_ptr_array_free (recipients, TRUE);
	g_mime_stream_unref (stream);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_encrypt failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	buffer = GMIME_STREAM_MEM (ciphertext)->buffer;
	fprintf (stderr, "ciphertext:\n%.*s\n", buffer->len, buffer->data);
	g_mime_stream_unref (ciphertext);
	
	return 1;
}

static int
test_decrypt (GMimeCipherContext *ctx, const char *ciphertext)
{
	GMimeStream *stream, *cleartext;
	GByteArray *buffer;
	GMimeException *ex;
	int len;
	
	stream = g_mime_stream_mem_new_with_buffer (ciphertext, strlen (ciphertext));
	cleartext = g_mime_stream_mem_new ();
	
	ex = g_mime_exception_new ();
	
	g_mime_cipher_decrypt (ctx, stream, cleartext, ex);
	g_mime_stream_unref (stream);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_encrypt failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	buffer = GMIME_STREAM_MEM (cleartext)->buffer;
	fprintf (stderr, "cleartext:\n%*.s\n", buffer->len, buffer->data);
	g_mime_stream_unref (cleartext);
	
	return 1;
}

int main (int argc, char **argv)
{
	GMimeSession *session;
	GMimeCipherContext *ctx;
	int i;
	
	g_mime_init (0);
	
	session = g_object_new (test_session_get_type (), NULL, NULL);
	
	ctx = g_mime_gpg_context_new (session, path);
	g_mime_gpg_context_set_always_trust ((GMimeGpgContext *) ctx, TRUE);
	
	if (!test_sign (ctx, "This is a test of pgp sign using md5\r\n",
			GMIME_CIPHER_HASH_DEFAULT))
		return 1;
	
	if (!test_sign (ctx, "This is a test of pgp sign using sha1\r\n",
			GMIME_CIPHER_HASH_SHA1))
		return 1;
	
	if (!test_encrypt (ctx, "Hello, this is a test\n", strlen ("Hello, this is a test\n")))
		return 1;
	
	g_object_unref (ctx);
	
	g_object_unref (session);
	
	return 0;
}
