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
#include "gmime-pgp-context.h"

static char *path = "/usr/bin/gpg";
static GMimePgpType type = GMIME_PGP_TYPE_GPG;
static char *userid = "pgp-mime@xtorshun.org";
static char *passphrase = "PGP/MIME is rfc2015, now go and read it.";

static char *
get_passwd (const char *prompt, gpointer data)
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
test_clearsign (GMimePgpContext *ctx, const char *cleartext)
{
	GMimeStream *stream, *ciphertext;
	GByteArray *buffer;
	GMimeException *ex;
	
	stream = g_mime_stream_mem_new_with_buffer (cleartext, strlen (cleartext));
	ciphertext = g_mime_stream_mem_new ();
	
	ex = g_mime_exception_new ();
	g_mime_pgp_clearsign (ctx, userid, GMIME_CIPHER_HASH_DEFAULT, stream, ciphertext, ex);
	g_mime_stream_unref (stream);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_clearsign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	buffer = GMIME_STREAM_MEM (ciphertext)->buffer;
	fprintf (stderr, "clearsign:\n%.*s\n", buffer->len, buffer->data);
	g_mime_stream_unref (ciphertext);
	
	return 1;
}

static int
test_sign (GMimePgpContext *ctx, const char *cleartext, GMimeCipherHash hash)
{
	GMimeStream *stream, *ciphertext;
	GByteArray *buffer;
	GMimeException *ex;
	
	stream = g_mime_stream_mem_new_with_buffer (cleartext, strlen (cleartext));
	ciphertext = g_mime_stream_mem_new ();
	
	ex = g_mime_exception_new ();
	g_mime_pgp_sign (ctx, userid, hash, stream, ciphertext, ex);
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
test_encrypt (GMimePgpContext *ctx, const char *in, int inlen)
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
	
	g_mime_pgp_encrypt (ctx, FALSE, userid, recipients, stream, ciphertext, ex);
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
test_decrypt (GMimePgpContext *ctx, const char *ciphertext)
{
	GMimeStream *stream, *cleartext;
	GByteArray *buffer;
	GMimeException *ex;
	int len;
	
	stream = g_mime_stream_mem_new_with_buffer (ciphertext, strlen (ciphertext));
	cleartext = g_mime_stream_mem_new ();
	
	ex = g_mime_exception_new ();
	
	g_mime_pgp_decrypt (ctx, stream, cleartext, ex);
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
	GMimePgpContext *ctx;
	int i;
	
	ctx = g_mime_pgp_context_new (type, path, get_passwd, NULL);
	
	if (!test_clearsign (ctx, "This is a test of clearsign\n"))
		return 1;
	
	if (!test_sign (ctx, "This is a test of pgp sign using md5\n",
			GMIME_CIPHER_HASH_MD5))
		return 1;
	
	if (!test_sign (ctx, "This is a test of pgp sign using sha1\n",
			GMIME_CIPHER_HASH_SHA1))
		return 1;
	
	if (!test_encrypt (ctx, "Hello, this is a test\n", strlen ("Hello, this is a test\n")))
		return 1;
	
	g_mime_object_unref (GMIME_OBJECT (ctx));
	
	return 0;
}
