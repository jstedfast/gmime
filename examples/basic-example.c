/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gmime/gmime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef G_OS_WIN32
static const char *path = "/usr/bin/gpg";
static const char *passphrase = "no.secret";

static gboolean
request_passwd (GMimeCryptoContext *ctx, const char *user_id, const char *prompt_ctx, gboolean reprompt, GMimeStream *response, GError **err)
{
	if (g_mime_stream_write_string (response, passphrase) == -1 ||
	    g_mime_stream_write (response, "\n", 1) == -1) {
		g_set_error (err, GMIME_ERROR, errno, "%s", g_strerror (errno));
		return FALSE;
	}
	
	return TRUE;
}
#endif /* ! G_OS_WIN32 */

static GMimeMessage *
parse_message (int fd)
{
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	
	/* create a stream to read from the file descriptor */
	stream = g_mime_stream_fs_new (fd);
	
	/* create a new parser object to parse the stream */
	parser = g_mime_parser_new_with_stream (stream);
	
	/* unref the stream (parser owns a ref, so this object does not actually get free'd until we destroy the parser) */
	g_object_unref (stream);
	
	/* parse the message from the stream */
	message = g_mime_parser_construct_message (parser);
	
	/* free the parser (and the stream) */
	g_object_unref (parser);
	
	return message;
}


static void
count_foreach_callback (GMimeObject *parent, GMimeObject *part, gpointer user_data)
{
	int *count = user_data;
	
	(*count)++;
	
	/* 'part' points to the current part node that
	 * g_mime_message_foreach() is iterating over */
	
	/* find out what class 'part' is... */
	if (GMIME_IS_MESSAGE_PART (part)) {
		/* message/rfc822 or message/news */
		GMimeMessage *message;
		
		/* g_mime_message_foreach() won't descend into
                   child message parts, so if we want to count any
                   subparts of this child message, we'll have to call
                   g_mime_message_foreach() again here. */
		
		message = g_mime_message_part_get_message ((GMimeMessagePart *) part);
		g_mime_message_foreach (message, count_foreach_callback, count);
	} else if (GMIME_IS_MESSAGE_PARTIAL (part)) {
		/* message/partial */
		
		/* this is an incomplete message part, probably a
                   large message that the sender has broken into
                   smaller parts and is sending us bit by bit. we
                   could save some info about it so that we could
                   piece this back together again once we get all the
                   parts? */
	} else if (GMIME_IS_MULTIPART (part)) {
		/* multipart/mixed, multipart/alternative,
		 * multipart/related, multipart/signed,
		 * multipart/encrypted, etc... */
		
		/* we'll get to finding out if this is a
		 * signed/encrypted multipart later... */
	} else if (GMIME_IS_PART (part)) {
		/* a normal leaf part, could be text/plain or
		 * image/jpeg etc */
	} else {
		g_assert_not_reached ();
	}
}

static void
count_parts_in_message (GMimeMessage *message)
{
	int count = 0;
	
	/* count the number of parts (recursively) in the message
	 * including the container multiparts */
	g_mime_message_foreach (message, count_foreach_callback, &count);
	
	printf ("There are %d parts in the message\n", count);
}

#ifndef G_OS_WIN32
static GMimeSignatureStatus
sig_status (GMimeSignatureList *signatures)
{
	GMimeSignatureStatus status = GMIME_SIGNATURE_STATUS_GOOD;
	GMimeSignature *sig;
	int i;
	
	if (!signatures || signatures->array->len == 0)
		return GMIME_SIGNATURE_STATUS_ERROR;
	
	for (i = 0; i < g_mime_signature_list_length (signatures); i++) {
		sig = g_mime_signature_list_get_signature (signatures, i);
		status = MAX (status, sig->status);
	}
	
	return status;
}

static void
verify_foreach_callback (GMimeObject *parent, GMimeObject *part, gpointer user_data)
{
	GMimeCryptoContext *ctx = user_data;
	
	if (GMIME_IS_MULTIPART_SIGNED (part)) {
		/* this is a multipart/signed part, so we can verify the pgp signature */
		GMimeMultipartSigned *mps = (GMimeMultipartSigned *) part;
		GMimeSignatureList *signatures;
		GError *err = NULL;
		const char *str;
		
		if (!(signatures = g_mime_multipart_signed_verify (mps, ctx, &err))) {
			/* an error occured - probably couldn't start gpg? */
			
			/* for more information about GError, see:
			 * http://developer.gnome.org/doc/API/2.0/glib/glib-Error-Reporting.html
			 */
			
			fprintf (stderr, "Failed to verify signed part: %s\n", err->message);
			g_error_free (err);
		} else {
			/* print out validity info - GOOD vs BAD and "why" */
			switch (sig_status (signatures)) {
			case GMIME_SIGNATURE_STATUS_GOOD:
				str = "Good";
				break;
			case GMIME_SIGNATURE_STATUS_BAD:
				str = "Bad";
				break;
			case GMIME_SIGNATURE_STATUS_ERROR:
				str = "Error";
				break;
			default:
				str = NULL;
				break;
			}
			
			g_object_unref (signatures);
		}
	}
}

static void
verify_signed_parts (GMimeMessage *message, GMimeCryptoContext *ctx)
{
	/* descend the mime tree and verify any signed parts */
	g_mime_message_foreach (message, verify_foreach_callback, ctx);
}
#endif

static void
write_message_to_screen (GMimeMessage *message)
{
	GMimeStream *stream;
	
	/* create a new stream for writing to stdout */
	stream = g_mime_stream_file_new (stdout);
	g_mime_stream_file_set_owner ((GMimeStreamFile *) stream, FALSE);
	
	/* write the message to the stream */
	g_mime_object_write_to_stream ((GMimeObject *) message, stream);
	
	/* flush the stream (kinda like fflush() in libc's stdio) */
	g_mime_stream_flush (stream);
	
	/* free the output stream */
	g_object_unref (stream);
}

#define PART_CONTENT "Hello, this is the new text/plain part's content text."

static void
add_a_mime_part (GMimeMessage *message)
{
	GMimeMultipart *multipart;
	GMimeDataWrapper *content;
	GMimePart *mime_part;
	GMimeStream *stream;
	
	/* create the new part that we are going to add... */
	mime_part = g_mime_part_new_with_type ("text", "plain");
	
	/* create the parts' content stream */
	stream = g_mime_stream_mem_new_with_buffer (PART_CONTENT, strlen (PART_CONTENT));
	
	/* create the content object - since the stream is not base64
	   or QP encoded or anything, we can use
	   GMIME_CONTENT_ENCODING_DEFAULT as the encoding type (_DEFAULT
	   is the same as saying "nothing specified") */
	content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
	g_object_unref (stream);
	
	/* set the content object on the new mime part */
	g_mime_part_set_content_object (mime_part, content);
	g_object_unref (content);
	
	/* if we want, we can tell GMime that the content should be base64 encoded when written to disk... */
	g_mime_part_set_content_encoding (mime_part, GMIME_CONTENT_ENCODING_BASE64);
	
	/* the "polite" way to modify a mime structure that we didn't
	   create is to create a new toplevel multipart/mixed part and
	   add the previous toplevel part as one of the subparts as
	   well as our text part that we just created... */
	
	/* create a multipart/mixed part */
	multipart = g_mime_multipart_new_with_subtype ("mixed");
	
	/* add our new text part to it */
	g_mime_multipart_add (multipart, (GMimeObject *) mime_part);
	g_object_unref (mime_part);
	
	/* now append the message's toplevel part to our multipart */
	g_mime_multipart_add (multipart, message->mime_part);
	
	/* now replace the message's toplevel mime part with our new multipart */
	g_mime_message_set_mime_part (message, (GMimeObject *) multipart);
	g_object_unref (multipart);
}

static void
remove_a_mime_part (GMimeMessage *message)
{
	GMimeMultipart *multipart;
	
	/* since we know the toplevel part is a multipart (we added it
	   in add_a_mime_part() earlier) and we know that the first
	   part of that multipart is our text part, lets remove the
	   first part of the toplevel mime part... */
	
	multipart = (GMimeMultipart *) message->mime_part;
	
	/* subpart indexes start at 0 */
	g_mime_multipart_remove_at (multipart, 0);
	
	/* now we should be left with a toplevel multipart/mixed which
	   contains the mime parts of the original message */
}

int main (int argc, char **argv)
{
#ifndef G_OS_WIN32
	GMimeCryptoContext *ctx;
#endif
	GMimeMessage *message;
	int fd;
	
	if (argc < 2) {
		printf ("Usage: a.out <message file>\n");
		return 0;
	} else {
		if ((fd = open (argv[1], O_RDONLY, 0)) == -1) {
			fprintf (stderr, "Cannot open message `%s': %s\n", argv[1], g_strerror (errno));
			return 0;
		}
	}
	
	/* init the gmime library */
	g_mime_init (0);
	
	/* parse the message */
	message = parse_message (fd);
	
	/* count the number of parts in the message */
	count_parts_in_message (message);
	
#ifndef G_OS_WIN32
#ifdef ENABLE_CRYPTOGRAPHY
	/* create our crypto context */
	ctx = g_mime_gpg_context_new (request_passwd, path);
	
	/* don't allow auto key-retrival */
	g_mime_gpg_context_set_auto_key_retrieve ((GMimeGpgContext *) ctx, FALSE);
	
	/* set the always_trust flag so that gpg will be spawned with `gpg --always-trust` */
	g_mime_gpg_context_set_always_trust ((GMimeGpgContext *) ctx, TRUE);
	
	/* verify any signed parts */
	verify_signed_parts (message, ctx);
	g_object_unref (ctx);
#endif
#endif
	
	/* add and remove parts */
	add_a_mime_part (message);
	write_message_to_screen (message);
	
	remove_a_mime_part (message);
	write_message_to_screen (message);
	
	/* free the mesage */
	g_object_unref (message);
	
	return 0;
}
