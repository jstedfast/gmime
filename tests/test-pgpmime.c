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
#include <glib.h>

#include "gmime.h"
#include "gmime-exception.h"
#include "gmime-pgp-mime.h"


static char *path = "/usr/bin/gpg";
static GMimePgpType type = GMIME_PGP_TYPE_GPG;
static char *userid = "pgp-mime@xtorshun.org";
static char *passphrase = "PGP/MIME is rfc2015, now go and read it.";

static char *
get_passwd (const char *prompt, gpointer data)
{
#if 0
	char buffer[256];
	
	fprintf (stdout, "%s\nPassphrase: %s\n", prompt, passphrase);
	fgets (buffer, 255, stdin);
	buffer[strlen (buffer)] = '\0'; /* chop off \n */
#endif
	return g_strdup (/*buffer*/passphrase);
}

void
test_multipart_signed (GMimePgpContext *ctx)
{
	GMimeMessage *message;
	GMimeContentType *mime_type;
	GMimePart *text_part, *signed_part;
	GMimeCipherValidity *validity;
	GMimeException *ex;
	char *body, *text;
	gboolean is_html;
	
	text_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, mime_type);
	g_mime_part_set_content (text_part, "This is a test of the emergency broadcast system with an md5 clearsign.\n",
				 strlen ("This is a test of the emergency broadcast system with an md5 clearsign.\n"));
	
	/* sign the part */
	ex = g_mime_exception_new ();
	g_mime_pgp_mime_part_sign (ctx, &text_part, userid, GMIME_CIPHER_HASH_MD5, ex);
	if (g_mime_exception_is_set (ex)) {
		g_mime_part_destroy (text_part);
		fprintf (stdout, "pgp_mime_part_sign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return;
	}
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				      "Federico Mena-Quintero",
				      "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_set_header (message, "X-Mailer", "main.c");
	g_mime_message_set_mime_part (message, text_part);
	
	text = g_mime_message_to_string (message);
	
	fprintf (stdout, "%s\n", text ? text : "(null)");
	g_free (text);
	
	/* get the body in text/plain */
	body = g_mime_message_get_body (message, TRUE, &is_html);
	fprintf (stdout, "Trying to get message body in plain text format:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	
	/* get the body in text/html */
	body = g_mime_message_get_body (message, FALSE, &is_html);
	fprintf (stdout, "Trying to get message body in html format:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	if (is_html)
		fprintf (stdout, "yep...got it in html format\n");
	
	validity = g_mime_pgp_mime_part_verify (ctx, text_part, ex);
	fprintf (stdout, "Trying to verify signature...%s\n",
		 g_mime_cipher_validity_get_valid (validity) ? "valid" : "invalid");
	fprintf (stdout, "Validity diagnostics: \n%s\n",
		 g_mime_cipher_validity_get_description (validity));
	g_mime_cipher_validity_free (validity);
	
	if (g_mime_exception_is_set (ex))
		fprintf (stdout, "error: %s\n", g_mime_exception_get_description (ex));
	
	g_mime_exception_free (ex);
	
	g_mime_message_destroy (message);
}

void
test_multipart_encrypted (GMimePgpContext *ctx)
{
	GMimeMessage *message;
	GMimeContentType *mime_type;
	GMimePart *text_part, *decrypted_part;
	GMimeException *ex;
	GPtrArray *recipients;
	char *body, *text;
	gboolean is_html;
	
	text_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, mime_type);
	g_mime_part_set_content_id (text_part, "1");
	g_mime_part_set_content (text_part, "This is a test of multipart/encrypted.\n",
				 strlen ("This is a test of multipart/encrypted.\n"));
	
	/* encrypt the part */
	ex = g_mime_exception_new ();
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, userid);
	g_mime_pgp_mime_part_encrypt (ctx, &text_part, recipients, ex);
	g_ptr_array_free (recipients, TRUE);
	if (g_mime_exception_is_set (ex)) {
		g_mime_part_destroy (text_part);
		fprintf (stdout, "pgp_mime_part_sign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return;
	}
	g_mime_exception_free (ex);
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				    "Federico Mena-Quintero", "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_set_header (message, "X-Mailer", "main.c");
	g_mime_message_set_mime_part (message, text_part);
	
	text = g_mime_message_to_string (message);
	
	fprintf (stdout, "%s\n", text ? text : "(null)");
	g_free (text);
	
	/* get the body in text/plain */
	body = g_mime_message_get_body (message, TRUE, &is_html);
	fprintf (stdout, "Trying to get message body in plain text format:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	
	/* get the body in text/html */
	body = g_mime_message_get_body (message, FALSE, &is_html);
	fprintf (stdout, "Trying to get message body in html format:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	if (is_html)
		fprintf (stdout, "yep...got it in html format\n");
	
	/* okay, now to test our decrypt function... */
	ex = g_mime_exception_new ();
	decrypted_part = g_mime_pgp_mime_part_decrypt (ctx, text_part, ex);
	if (g_mime_exception_is_set (ex)) {
		fprintf (stdout, "failed to decrypt part.\n");
	} else {
		char *text;
		
		text = g_mime_part_to_string (decrypted_part);
		fprintf (stdout, "decrypted:\n%s\n", text ? text : "NULL");
		g_free (text);
		g_mime_part_destroy (decrypted_part);
	}
	g_mime_exception_free (ex);
	
	g_mime_message_destroy (message);
}

int main (int argc, char *argv[])
{
	GMimePgpContext *ctx;
	
	ctx = g_mime_pgp_context_new (type, path, get_passwd, NULL);
	
	test_multipart_signed (ctx);
	
	test_multipart_encrypted (ctx);
	
	g_mime_object_unref (GMIME_OBJECT (ctx));
	
	return 0;
}
