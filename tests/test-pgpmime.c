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
#include <string.h>
#include <glib.h>

#include <gmime/gmime.h>

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
			     GError **err);


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
request_passwd (GMimeSession *session, const char *prompt, gboolean secret, const char *item, GError **err)
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
test_multipart_signed (GMimeCipherContext *ctx)
{
	GMimeMessage *message;
	GMimeContentType *mime_type;
	GMimeMultipartSigned *mps;
	GMimePart *text_part;
	GMimeCipherValidity *validity;
	GMimeDataWrapper *content;
	GMimeStream *stream;
	GMimeParser *parser;
	GError *err = NULL;
	
	text_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, mime_type);
	
	stream = g_mime_stream_mem_new ();
	g_mime_stream_write_string (
		stream,
		"This is a test of the emergency broadcast system with an sha1 detach-sign.\n\n"
		"From now on, there will be text to try and break     \t  \n"
		"various things. For example, the F in \"From\" in the previous line...\n"
		"...and the first dot of this line have been pre-encoded in the QP encoding "
		"in order to test that GMime properly treats MIME part content as opaque.\n"
		"If this still verifies okay, then we have ourselves a winner I guess...\n");
	
	g_mime_stream_reset (stream);
	content = g_mime_data_wrapper_new_with_stream (stream, GMIME_PART_ENCODING_DEFAULT);
	g_mime_stream_unref (stream);
	
	g_mime_part_set_content_object (text_part, content);
	g_object_unref (content);
	
	/* create the multipart/signed container part */
	mps = g_mime_multipart_signed_new ();
	
	/* sign the part */
	g_mime_multipart_signed_sign (mps, GMIME_OBJECT (text_part), ctx, userid,
				      GMIME_CIPHER_HASH_SHA1, &err);
	g_mime_object_unref (GMIME_OBJECT (text_part));
	
	if (err != NULL) {
		g_mime_object_unref (GMIME_OBJECT (mps));
		fprintf (stdout, "pgp_mime_part_sign failed: %s\n", err->message);
		g_error_free (err);
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
	g_mime_message_set_mime_part (message, GMIME_OBJECT (mps));
	g_mime_object_unref (GMIME_OBJECT (mps));
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (GMIME_OBJECT (message), stream);
	g_mime_stream_reset (stream);
	
	fprintf (stdout, "%.*s\n", GMIME_STREAM_MEM (stream)->buffer->len,
		 GMIME_STREAM_MEM (stream)->buffer->data);
	
	g_mime_object_unref (GMIME_OBJECT (message));
	
#if 0
	/* Note: the use of get_body() destroys the ability to verify MIME parts, so DON'T USE IT!! */
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
#endif
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_mime_stream_unref (stream);
	
	message = g_mime_parser_construct_message (parser);
	g_object_unref (parser);
	
	if (!GMIME_IS_MULTIPART_SIGNED (message->mime_part)) {
		fprintf (stdout, "*** error: toplevel mime part is not a multipart/signed object\n");
		g_mime_object_unref (GMIME_OBJECT (message));
		return;
	}
	
	mps = (GMimeMultipartSigned *) message->mime_part;
	
	validity = g_mime_multipart_signed_verify (mps, ctx, &err);
	fprintf (stdout, "Trying to verify signature...%s\n",
		 g_mime_cipher_validity_get_valid (validity) ? "valid" : "invalid");
	fprintf (stdout, "Validity diagnostics: \n%s\n",
		 g_mime_cipher_validity_get_description (validity));
	g_mime_cipher_validity_free (validity);
	
	if (err != NULL) {
		fprintf (stdout, "error: %s\n", err->message);
		g_error_free (err);
	}
	
	g_mime_object_unref (GMIME_OBJECT (message));
}

void
test_multipart_encrypted (GMimeCipherContext *ctx)
{
	GMimeMessage *message;
	GMimeMultipartEncrypted *mpe;
	GMimeObject *decrypted_part;
	GMimePart *text_part;
	GMimeContentType *mime_type;
	GPtrArray *recipients;
	GError *err = NULL;
	char *text;
	
	text_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, mime_type);
	g_mime_part_set_content (text_part, "This is a test of multipart/encrypted.\n",
				 strlen ("This is a test of multipart/encrypted.\n"));
	
	/* create the multipart/encrypted container part */
	mpe = g_mime_multipart_encrypted_new ();
	
	/* encrypt the part */
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, userid);
	
	g_mime_multipart_encrypted_encrypt (mpe, GMIME_OBJECT (text_part), ctx, recipients, &err);
	g_mime_object_unref (GMIME_OBJECT (text_part));
	g_ptr_array_free (recipients, TRUE);
	
	if (err != NULL) {
		g_mime_object_unref (GMIME_OBJECT (text_part));
		fprintf (stdout, "pgp_mime_part_sign failed: %s\n", err->message);
		g_error_free (err);
		return;
	}
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				    "Federico Mena-Quintero", "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_set_header (message, "X-Mailer", "main.c");
	g_mime_message_set_mime_part (message, GMIME_OBJECT (mpe));
	
	text = g_mime_message_to_string (message);
	fprintf (stdout, "%s\n", text ? text : "(null)");
	g_free (text);
	
	/* okay, now to test our decrypt function... */
	decrypted_part = g_mime_multipart_encrypted_decrypt (mpe, ctx, &err);
	if (!decrypted_part || err != NULL) {
		fprintf (stdout, "failed to decrypt part: %s\n", err->message);
		g_error_free (err);
	} else {
		text = g_mime_object_to_string (decrypted_part);
		fprintf (stdout, "decrypted:\n%s\n", text ? text : "NULL");
		g_free (text);
		g_mime_object_unref (GMIME_OBJECT (decrypted_part));
	}
	
	g_mime_object_unref (GMIME_OBJECT (mpe));
	g_mime_object_unref (GMIME_OBJECT (message));
}

int main (int argc, char *argv[])
{
	GMimeSession *session;
	GMimeCipherContext *ctx;
	
	g_mime_init (0);
	
	session = g_object_new (test_session_get_type (), NULL, NULL);
	ctx = g_mime_gpg_context_new (session, path);
	g_mime_gpg_context_set_always_trust ((GMimeGpgContext *) ctx, TRUE);
	
	test_multipart_signed (ctx);
	
	test_multipart_encrypted (ctx);
	
	g_object_unref (ctx);
	g_object_unref (session);
	
	return 0;
}
