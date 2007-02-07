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



static void
test_multipart_signed (GMimeCipherContext *ctx)
{
	GMimeSignatureValidity *validity;
	GMimeContentType *content_type;
	GMimeMultipartSigned *mps;
	GMimeDataWrapper *content;
	GMimeMessage *message;
	GMimePart *text_part;
	GMimeStream *stream;
	GMimeParser *parser;
	GMimeSigner *signer;
	GError *err = NULL;
	
	text_part = g_mime_part_new ();
	content_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, content_type);
	
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
	g_object_unref (text_part);
	
	if (err != NULL) {
		g_object_unref (mps);
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
	g_object_unref (mps);
	
	stream = g_mime_stream_mem_new ();
	g_mime_object_write_to_stream (GMIME_OBJECT (message), stream);
	g_mime_stream_reset (stream);
	
	fprintf (stdout, "%.*s\n", GMIME_STREAM_MEM (stream)->buffer->len,
		 GMIME_STREAM_MEM (stream)->buffer->data);
	
	g_object_unref (message);
	
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
		g_object_unref (message);
		return;
	}
	
	mps = (GMimeMultipartSigned *) message->mime_part;
	
	fputs ("Trying to verify signature... ", stdout);
	validity = g_mime_multipart_signed_verify (mps, ctx, &err);
	switch (validity->status) {
	case GMIME_SIGNATURE_STATUS_NONE:
		fputs ("Unset\n", stdout);
	case GMIME_SIGNATURE_STATUS_GOOD:
		fputs ("GOOD\n", stdout);
		break;
	case GMIME_SIGNATURE_STATUS_BAD:
		fputs ("BAD\n", stdout);
		break;
	case GMIME_SIGNATURE_STATUS_UNKNOWN:
		fputs ("Unknown status\n", stdout);
		break;
	default:
		fputs ("Unknown enum value\n", stdout);
		break;
	}
	
	fputs ("\nSigners:\n", stdout);
	signer = validity->signers;
	while (signer != NULL) {
		fprintf (stdout, "\tName: %s\n", signer->name ? signer->name : "(null)");
		fprintf (stdout, "\tKeyId: %s\n", signer->keyid ? signer->keyid : "(null)");
		fprintf (stdout, "\tFingerprint: %s\n", signer->fingerprint ? signer->fingerprint : "(null)");
		fprintf (stdout, "\tTrust: ");
		switch (signer->trust) {
		case GMIME_SIGNER_TRUST_NONE:
			fputs ("None\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_NEVER:
			fputs ("Never\n", stdout);
		case GMIME_SIGNER_TRUST_UNDEFINED:
			fputs ("Undefined\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_MARGINAL:
			fputs ("Marginal\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_FULLY:
			fputs ("Fully\n", stdout);
			break;
		case GMIME_SIGNER_TRUST_ULTIMATE:
			fputs ("Ultimate\n", stdout);
			break;
		}
		fprintf (stdout, "\tStatus: ");
		switch (signer->status) {
		case GMIME_SIGNER_STATUS_NONE:
			fputs ("None\n", stdout);
			break;
		case GMIME_SIGNER_STATUS_GOOD:
			fputs ("GOOD\n", stdout);
			break;
		case GMIME_SIGNER_STATUS_BAD:
			fputs ("BAD\n", stdout);
			break;
		case GMIME_SIGNER_STATUS_ERROR:
			fputs ("ERROR\n", stdout);
			break;
		}
		fprintf (stdout, "\tSignature made on %s", ctime (&signer->sig_created));
		if (signer->sig_expire != (time_t) 0)
			fprintf (stdout, "\tSignature expires on %s", ctime (&signer->sig_expire));
		else
			fprintf (stdout, "\tSignature never expires\n");
		if (signer->errors) {
			fprintf (stdout, "\tErrors: ");
			if (signer->errors & GMIME_SIGNER_ERROR_EXPSIG)
				fputs ("Expired, ", stdout);
			if (signer->errors & GMIME_SIGNER_ERROR_NO_PUBKEY)
				fputs ("No Pub Key, ", stdout);
			if (signer->errors & GMIME_SIGNER_ERROR_EXPKEYSIG)
				fputs ("Key Expired, ", stdout);
			if (signer->errors & GMIME_SIGNER_ERROR_REVKEYSIG)
				fputs ("Key Revoked", stdout);
			fputc ('\n', stdout);
		} else {
			fprintf (stdout, "\tNo errors for this signer\n");
		}
		
		if ((signer = signer->next))
			fputc ('\n', stdout);
	}
	
	fprintf (stdout, "\nValidity diagnostics: \n%s\n",
		 g_mime_signature_validity_get_details (validity));
	g_mime_signature_validity_free (validity);
	
	if (err != NULL) {
		fprintf (stdout, "error: %s\n", err->message);
		g_error_free (err);
	}
	
	g_object_unref (message);
}

void
test_multipart_encrypted (GMimeCipherContext *ctx)
{
	GMimeMessage *message;
	GMimeMultipartEncrypted *mpe;
	GMimeObject *decrypted_part;
	GMimePart *text_part;
	GMimeContentType *content_type;
	GPtrArray *recipients;
	GError *err = NULL;
	char *text;
	
	text_part = g_mime_part_new ();
	content_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, content_type);
	g_mime_part_set_content (text_part, "This is a test of multipart/encrypted.\n",
				 strlen ("This is a test of multipart/encrypted.\n"));
	
	/* create the multipart/encrypted container part */
	mpe = g_mime_multipart_encrypted_new ();
	
	/* encrypt the part */
	recipients = g_ptr_array_new ();
	g_ptr_array_add (recipients, userid);
	
	g_mime_multipart_encrypted_encrypt (mpe, GMIME_OBJECT (text_part), ctx, recipients, &err);
	g_object_unref (text_part);
	g_ptr_array_free (recipients, TRUE);
	
	if (err != NULL) {
		g_object_unref (text_part);
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
		g_object_unref (decrypted_part);
	}
	
	g_object_unref (mpe);
	g_object_unref (message);
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
	
	g_mime_shutdown ();
	
	return 0;
}
