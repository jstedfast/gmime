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
#include "pgp-mime.h"


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

void
test_multipart_signed (void)
{
	GMimeMessage *message;
	GMimeContentType *mime_type;
	GMimePart *text_part, *signed_part;
	GMimeException *ex;
	gchar *body;
	gchar *text;
	gboolean is_html;
	
	text_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, mime_type);
	g_mime_part_set_content_id (text_part, "1");
	g_mime_part_set_content (text_part, "This is a test of the emergency broadcast system with an md5 clearsign.\n",
				 strlen ("This is a test of the emergency broadcast system with an md5 clearsign.\n"));
	
	/* sign the part */
	ex = g_mime_exception_new ();
	pgp_mime_part_sign (&text_part, userid, PGP_HASH_TYPE_MD5, ex);
	if (g_mime_exception_is_set (ex)) {
		g_mime_part_destroy (text_part);
		fprintf (stderr, "pgp_mime_part_sign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return;
	}
	
	message = g_mime_message_new ();
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				    "Federico Mena-Quintero", "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_add_arbitrary_header (message, "X-Mailer", "main.c");
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
	
	g_mime_message_destroy (message);
}

void
test_multipart_encrypted (void)
{
	GMimeMessage *message;
	GMimeContentType *mime_type;
	GMimePart *text_part;
	GMimeException *ex;
	GPtrArray *recipients;
	gchar *body;
	gchar *text;
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
	pgp_mime_part_encrypt (&text_part, recipients, ex);
	g_ptr_array_free (recipients, TRUE);
	if (g_mime_exception_is_set (ex)) {
		g_mime_part_destroy (text_part);
		fprintf (stderr, "pgp_mime_part_sign failed: %s\n",
			 g_mime_exception_get_description (ex));
		g_mime_exception_free (ex);
		return;
	}
	
	message = g_mime_message_new ();
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				    "Federico Mena-Quintero", "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_add_arbitrary_header (message, "X-Mailer", "main.c");
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
	
	g_mime_message_destroy (message);
}

int main (int argc, char *argv[])
{
	pgp_mime_init (path, type, pgp_get_passphrase, NULL);
	
	test_multipart_signed ();
	
	test_multipart_encrypted ();
	
	return 0;
}
