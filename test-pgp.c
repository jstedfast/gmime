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
#include "pgp-utils.h"

static gchar *path = "/usr/bin/gpg";
static PgpType type = PGP_TYPE_GPG;
static gchar *userid = "pgp-mime@xtorshun.org";
static gchar *passphrase = "PGP/MIME is rfc2015, now go and read it.";

static gchar *
pgp_get_passphrase (const gchar *prompt, gpointer data)
{
#if 0
	gchar buffer[256];
	
	fprintf (stderr, "%s\nPassphrase: %s\n", prompt, passphrase);
	fgets (buffer, 255, stdin);
	buffer[strlen (buffer)] = '\0'; /* chop off \n */
#endif
	return g_strdup (/*buffer*/passphrase);
}

static int
test_clearsign (const gchar *cleartext)
{
	gchar *ciphertext;
	GMimeException *ex;
	
	ex = g_mime_exception_new ();
	ciphertext = pgp_clearsign (cleartext, userid, PGP_HASH_TYPE_NONE, ex);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_clearsign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	fprintf (stderr, "clearsign:\n%s\n", ciphertext);
	g_free (ciphertext);
	
	return 1;
}

static int
test_sign (const gchar *cleartext, PgpHashType hash)
{
	gchar *ciphertext;
	GMimeException *ex;
	
	ex = g_mime_exception_new ();
	ciphertext = pgp_sign (cleartext, strlen (cleartext),
			       userid, hash, ex);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_sign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	fprintf (stderr, "signature:\n%s\n", ciphertext);
	g_free (ciphertext);
	
	return 1;
}

static int
test_encrypt (const gchar *in, gint inlen)
{
	GPtrArray *recipients;
	gchar *ciphertext;
	GMimeException *ex;
	
	ex = g_mime_exception_new ();
	
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, userid);
	
	ciphertext = pgp_encrypt (in, inlen, recipients, FALSE, NULL, ex);
	g_ptr_array_free (recipients, TRUE);
	
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_encrypt failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	fprintf (stderr, "ciphertext:\n%s\n", ciphertext);
	g_free (ciphertext);
	
	return 1;
}

static int
test_decrypt (const gchar *ciphertext)
{
	gchar *cleartext;
	GMimeException *ex;
	gint len;
	
	ex = g_mime_exception_new ();
	
	cleartext = pgp_decrypt (ciphertext, strlen (ciphertext), &len, ex);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stderr, "pgp_encrypt failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return 0;
	}
	
	g_mime_exception_free (ex);
	fprintf (stderr, "cleartext:\n%*.s\n", len, cleartext);
	g_free (cleartext);
	
	return 1;
}

int main (int argc, char **argv)
{
	int i;
	
	pgp_init (path, type, pgp_get_passphrase, NULL);
	
	if (!test_clearsign ("This is a test of clearsign\n"))
		return 1;
	
	if (!test_sign ("This is a test of pgp sign using md5\n",
			PGP_HASH_TYPE_MD5))
		return 1;
	
	if (!test_sign ("This is a test of pgp sign using sha1\n",
			PGP_HASH_TYPE_SHA1))
		return 1;
	
	if (!test_encrypt ("Hello, this is a test\n", strlen ("Hello, this is a test\n")))
		return 1;
	
	return 0;
}
