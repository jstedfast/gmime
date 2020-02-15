/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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
#include <glib/gstdio.h>
#include <gmime/gmime.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static char *
basename (char *path)
{
	char *base;
	
	if ((base = strrchr (path, '/')))
		return base + 1;
	
	return path;
}

static char *
escape_string (const char *string)
{
	const char *start, *inptr;
	GString *str;
	
	str = g_string_new ("");
	
	inptr = string;
	
	while (*inptr) {
		start = inptr;
		while (*inptr && *inptr != '"')
			inptr++;
		
		g_string_append_len (str, start, inptr - start);
		if (*inptr == '"') {
			g_string_append (str, "\\\"");
			inptr++;
		}
	}
	
	return g_string_free (str, FALSE);
}

static void
write_part_bodystructure (GMimeObject *part, FILE *fp)
{
	GMimeContentType *content_type;
	GMimeParamList *params;
	const char *subtype;
	GMimeParam *param;
	int i, n;
	
	fputc ('(', fp);
	
	content_type = g_mime_object_get_content_type (part);
	
	fprintf (fp, "\"%s\" ", g_mime_content_type_get_media_type (content_type));
	
	if ((subtype = g_mime_content_type_get_media_subtype (content_type)))
		fprintf (fp, "\"%s\" ", subtype);
	else
		fputs ("\"\"", fp);
	
	/* Content-Type params */
	params = g_mime_content_type_get_parameters (content_type);
	if ((n = g_mime_param_list_length (params)) > 0) {
		fputc ('(', fp);
		for (i = 0; i < n; i++) {
			if (i > 0)
				fputc (' ', fp);
			
			param = g_mime_param_list_get_parameter_at (params, i);
			fprintf (fp, "\"%s\" \"%s\"", g_mime_param_get_name (param),
				 g_mime_param_get_value (param));
				
		}
		fputs (") ", fp);
	} else {
		fputs ("NIL ", fp);
	}
	
	if (GMIME_IS_MULTIPART (part)) {
		GMimeMultipart *multipart = (GMimeMultipart *) part;
		GMimeObject *subpart;
		
		n = g_mime_multipart_get_count (multipart);
		for (i = 0; i < n; i++) {
			subpart = g_mime_multipart_get_part (multipart, i);
			write_part_bodystructure (subpart, fp);
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		GMimeMessage *message;
		const char *str;
		char *nstring;
		
		message = GMIME_MESSAGE_PART (part)->message;
		
		/* print envelope */
		fputc ('(', fp);
		
		str = g_mime_object_get_header ((GMimeObject *) message, "Date");
		fprintf (fp, "\"%s\" ", str);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "Subject")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "From")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "Sender")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "Reply-To")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "To")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "Cc")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "Bcc")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_object_get_header ((GMimeObject *) message, "In-Reply-To")))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\" ", nstring);
		g_free (nstring);
		
		if ((str = g_mime_message_get_message_id (message)))
			nstring = escape_string (str);
		else
			nstring = g_strdup ("");
		fprintf (fp, "\"%s\"", nstring);
		g_free (nstring);
		
		fputs (") ", fp);
		
		/* print body */
		write_part_bodystructure ((GMimeObject *) message->mime_part, fp);
	} else if (GMIME_IS_PART (part)) {
		GMimeContentDisposition *disposition;
		
		disposition = g_mime_object_get_content_disposition (part);
		
		if (disposition) {
			fprintf (fp, "\"%s\" ", g_mime_content_disposition_get_disposition (disposition));
			
			params = g_mime_content_disposition_get_parameters (disposition);
			if ((n = g_mime_param_list_length (params)) > 0) {
				fputc ('(', fp);
				for (i = 0; i < n; i++) {
					if (i > 0)
						fputc (' ', fp);
					
					param = g_mime_param_list_get_parameter_at (params, i);
					fprintf (fp, "\"%s\" \"%s\"", g_mime_param_get_name (param),
						 g_mime_param_get_value (param));
				}
				fputs (") ", fp);
			} else {
				fputs ("NIL ", fp);
			}
		} else {
			fputs ("NIL NIL ", fp);
		}
		
		switch (g_mime_part_get_content_encoding ((GMimePart *) part)) {
		case GMIME_CONTENT_ENCODING_7BIT:
			fputs ("\"7bit\"", fp);
			break;
		case GMIME_CONTENT_ENCODING_8BIT:
			fputs ("\"8bit\"", fp);
			break;
		case GMIME_CONTENT_ENCODING_BINARY:
			fputs ("\"binary\"", fp);
			break;
		case GMIME_CONTENT_ENCODING_BASE64:
			fputs ("\"base64\"", fp);
			break;
		case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
			fputs ("\"quoted-printable\"", fp);
			break;
		case GMIME_CONTENT_ENCODING_UUENCODE:
			fputs ("\"x-uuencode\"", fp);
			break;
		default:
			fputs ("NIL", fp);
		}
	}
	
	fputc (')', fp);
}

static void
write_bodystructure (GMimeMessage *message, const char *uid)
{
	char *filename;
	FILE *fp;
	
	filename = g_strdup_printf ("%s/BODYSTRUCTURE", uid);
	fp = fopen (filename, "wt");
	g_free (filename);
	
	write_part_bodystructure (message->mime_part, fp);
	
	fclose (fp);
}

static void
write_header (GMimeMessage *message, const char *uid)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	char *buf;
	FILE *fp;
	
	buf = g_strdup_printf ("%s/HEADER", uid);
	fp = fopen (buf, "wt");
	g_free (buf);
	
	buf = g_mime_object_get_headers ((GMimeObject *) message, format);
	
	fwrite (buf, 1, strlen (buf), fp);
	g_free (buf);
	
	fclose (fp);
}

static void
write_part (GMimeObject *part, const char *uid, const char *spec)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeStream *istream, *ostream;
	GMimeMultipart *multipart;
	GMimeDataWrapper *content;
	GMimeMessage *message;
	GMimeObject *subpart;
	char *buf, *id;
	FILE *fp;
	int i, n;
	
	buf = g_strdup_printf ("%s/%s.HEADER", uid, spec);
	fp = fopen (buf, "wt");
	g_free (buf);
	
	buf = g_mime_object_get_headers (part, format);
	fwrite (buf, 1, strlen (buf), fp);
	g_free (buf);
	
	fclose (fp);
	
	if (GMIME_IS_MULTIPART (part)) {
		multipart = (GMimeMultipart *) part;
		buf = g_alloca (strlen (spec) + 14);
		id = g_stpcpy (buf, spec);
		*id++ = '.';
		
		n = g_mime_multipart_get_count (multipart);
		for (i = 0; i < n; i++) {
			subpart = g_mime_multipart_get_part (multipart, i);
			sprintf (id, "%d", i + 1);
			write_part (subpart, uid, buf);
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		buf = g_strdup_printf ("%s/%s.TEXT", uid, spec);
		fp = fopen (buf, "wt");
		g_free (buf);
		
		message = g_mime_message_part_get_message ((GMimeMessagePart *) part);
		
		ostream = g_mime_stream_file_new (fp);
		g_mime_object_write_to_stream ((GMimeObject *) message, format, ostream);
		g_object_unref (ostream);
	} else if (GMIME_IS_PART (part)) {
		buf = g_strdup_printf ("%s/%s.TEXT", uid, spec);
		fp = fopen (buf, "wt");
		g_free (buf);
		
		ostream = g_mime_stream_file_new (fp);
		content = g_mime_part_get_content ((GMimePart *) part);
		istream = g_mime_data_wrapper_get_stream (content);
		g_mime_stream_write_to_stream (istream, ostream);
		g_object_unref (ostream);
	}
}

static void
write_message (GMimeMessage *message, const char *uid)
{
	write_header (message, uid);
	write_bodystructure (message, uid);
	write_part (message->mime_part, uid, "1");
}

struct _envelope {
	char *date;
	char *subject;
	char *from;
	char *sender;
	char *reply_to;
	char *to;
	char *cc;
	char *bcc;
	char *in_reply_to;
	char *message_id;
};

struct _bodystruct {
	struct _bodystruct *next;
	
	struct {
		char *type;
		char *subtype;
		GMimeParamList *params;
	} content;
	struct {
		char *type;
		GMimeParamList *params;
	} disposition;
	char *encoding;
	struct _envelope *envelope;
	struct _bodystruct *subparts;
};

static void
unescape_qstring (char *qstring)
{
	char *s, *d;
	
	d = s = qstring;
	while (*s != '\0') {
		if (*s != '\\')
			*d++ = *s++;
		else
			s++;
	}
	
	*d = '\0';
}

static char *
decode_qstring (unsigned char **in, unsigned char *inend)
{
	unsigned char *inptr, *start;
	char *qstring = NULL;
	
	inptr = *in;
	
	while (inptr < inend && *inptr == ' ')
		inptr++;
	
	if (inptr == inend)
		return NULL;
	
	if (strncmp ((const char *) inptr, "NIL", 3) != 0) {
		g_assert (*inptr == '"');
		inptr++;
		start = inptr;
		while (inptr < inend) {
			if (*inptr == '"' && inptr[-1] != '\\')
				break;
			inptr++;
		}
		
		qstring = g_strndup ((const char *) start, inptr - start);
		unescape_qstring (qstring);
		g_assert (*inptr == '"');
		inptr++;
	} else {
		inptr += 3;
	}
	
	*in = inptr;
	
	return qstring;
}

static gboolean
decode_param (unsigned char **in, unsigned char *inend, char **name, char **value)
{
	char *n, *v;
	
	if (!(n = decode_qstring (in, inend)))
		return FALSE;
	
	if (!(v = decode_qstring (in, inend))) {
		g_free (n);
		return FALSE;
	}
	
	*name = n;
	*value = v;
	
	return TRUE;
}

static GMimeParamList *
decode_params (unsigned char **in, unsigned char *inend)
{
	GMimeParamList *params;
	unsigned char *inptr;
	char *name, *value;
	
	inptr = *in;
	
	params = g_mime_param_list_new ();
	
	while (inptr < inend && *inptr == ' ')
		inptr++;
	
	if (inptr == inend) {
		g_assert_not_reached ();
		return params;
	}
	
	if (strncmp ((const char *) inptr, "NIL", 3) != 0) {
		g_assert (*inptr == '(');
		inptr++;
		
		while (decode_param (&inptr, inend, &name, &value)) {
			g_mime_param_list_set_parameter (params, name, value);
			g_free (value);
			g_free (name);
			
			while (inptr < inend && *inptr == ' ')
				inptr++;
			
			if (*inptr == ')')
				break;
		}
		
		g_assert (*inptr == ')');
		inptr++;
	} else {
		inptr += 3;
	}
	
	*in = inptr;
	
	return params;
}

static struct _envelope *
decode_envelope (unsigned char **in, unsigned char *inend)
{
	struct _envelope *envelope;
	unsigned char *inptr;
	
	inptr = *in;
	
	while (inptr < inend && *inptr == ' ')
		inptr++;
	
	if (inptr == inend || *inptr != '(') {
		g_assert_not_reached ();
		*in = inptr;
		return NULL;
	}
	
	inptr++;
	
	envelope = g_new (struct _envelope, 1);
	
	envelope->date = decode_qstring (&inptr, inend);
	envelope->subject = decode_qstring (&inptr, inend);
	envelope->from = decode_qstring (&inptr, inend);
	envelope->sender = decode_qstring (&inptr, inend);
	envelope->reply_to = decode_qstring (&inptr, inend);
	envelope->to = decode_qstring (&inptr, inend);
	envelope->cc = decode_qstring (&inptr, inend);
	envelope->bcc = decode_qstring (&inptr, inend);
	envelope->in_reply_to = decode_qstring (&inptr, inend);
	envelope->message_id = decode_qstring (&inptr, inend);
	
	while (inptr < inend && *inptr == ' ')
		inptr++;
	
	g_assert (*inptr == ')');
	inptr++;
	
	*in = inptr;
	
	return envelope;
}

static struct _bodystruct *
bodystruct_part_decode (unsigned char **in, unsigned char *inend)
{
	struct _bodystruct *part, *list, *tail, *n;
	unsigned char *inptr;
	
	inptr = *in;
	
	while (inptr < inend && *inptr == ' ')
		inptr++;
	
	if (inptr == inend || *inptr != '(') {
		g_assert_not_reached ();
		*in = inptr;
		return NULL;
	}
	
	inptr++;
	
	part = g_new (struct _bodystruct, 1);
	part->next = NULL;
	
	part->content.type = decode_qstring (&inptr, inend);
	part->content.subtype = decode_qstring (&inptr, inend);
	part->content.params = decode_params (&inptr, inend);
	part->disposition.type = NULL;
	part->disposition.params = NULL;
	part->encoding = NULL;
	part->envelope = NULL;
	part->subparts = NULL;
	
	if (!g_ascii_strcasecmp (part->content.type, "multipart")) {
		list = NULL;
		tail = (struct _bodystruct *) &list;
		
		while ((n = bodystruct_part_decode (&inptr, inend)) != NULL) {
			tail->next = n;
			tail = n;
			
			while (inptr < inend && *inptr == ' ')
				inptr++;
			
			if (*inptr == ')')
				break;
		}
		
		part->subparts = list;
	} else if (!g_ascii_strcasecmp (part->content.type, "message") && !g_ascii_strcasecmp (part->content.subtype, "rfc822")) {
		part->envelope = decode_envelope (&inptr, inend);
		part->subparts = bodystruct_part_decode (&inptr, inend);
	} else {
		part->disposition.type = decode_qstring (&inptr, inend);
		part->disposition.params = decode_params (&inptr, inend);
		part->encoding = decode_qstring (&inptr, inend);
	}
	
	while (inptr < inend && *inptr == ' ')
		inptr++;
	
	g_assert (*inptr == ')');
	inptr++;
	
	*in = inptr;
	
	return part;
}

struct _bodystruct *
bodystruct_parse (unsigned char *inbuf, int inlen)
{
	return bodystruct_part_decode (&inbuf, inbuf + inlen);
}

static void
bodystruct_dump (struct _bodystruct *part, int depth)
{
	GMimeParam *param;
	int i, n;
	
	for (i = 0; i < depth; i++)
		fputs ("  ", stderr);
	
	fprintf (stderr, "Content-Type: %s/%s", part->content.type,
		 part->content.subtype);
	
	if (part->content.params) {
		n = g_mime_param_list_length (part->content.params);
		for (i = 0; i < n; i++) {
			param = g_mime_param_list_get_parameter_at (part->content.params, i);
			fprintf (stderr, "; %s=%s", g_mime_param_get_name (param),
				 g_mime_param_get_value (param));
		}
	}
	
	fputc ('\n', stderr);
	
	if (!g_ascii_strcasecmp (part->content.type, "multipart")) {
		part = part->subparts;
		while (part != NULL) {
			bodystruct_dump (part, depth + 1);
			part = part->next;
		}
	} else if (!g_ascii_strcasecmp (part->content.type, "message") && !g_ascii_strcasecmp (part->content.subtype, "rfc822")) {
		depth++;
		
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Date: %s\n", part->envelope->date);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Subject: %s\n", part->envelope->subject);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "From: %s\n", part->envelope->from);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Sender: %s\n", part->envelope->sender);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Reply-To: %s\n", part->envelope->reply_to);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "To: %s\n", part->envelope->to);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Cc: %s\n", part->envelope->cc);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Bcc: %s\n", part->envelope->bcc);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "In-Reply-To: %s\n", part->envelope->in_reply_to);
		for (i = 0; i < depth; i++)
			fputs ("  ", stderr);
		fprintf (stderr, "Message-Id: %s\n", part->envelope->message_id);
		bodystruct_dump (part->subparts, depth);
		depth--;
	} else {
		if (part->disposition.type) {
			for (i = 0; i < depth; i++)
				fputs ("  ", stderr);
			fprintf (stderr, "Content-Disposition: %s", part->disposition.type);
			if (part->disposition.params) {
				n = g_mime_param_list_length (part->disposition.params);
				for (i = 0; i < n; i++) {
					param = g_mime_param_list_get_parameter_at (part->disposition.params, i);
					fprintf (stderr, "; %s=%s", g_mime_param_get_name (param),
						 g_mime_param_get_value (param));
				}
			}
			
			fputc ('\n', stderr);
		}
		
		if (part->encoding) {
			for (i = 0; i < depth; i++)
				fputs ("  ", stderr);
			fprintf (stderr, "Content-Transfer-Encoding: %s\n", part->encoding);
		}
	}
	
	fputc ('\n', stderr);
}

static void
bodystruct_free (struct _bodystruct *node)
{
	struct _bodystruct *next;
	
	while (node != NULL) {
		g_free (node->content.type);
		g_free (node->content.subtype);
		if (node->content.params)
			g_object_unref (node->content.params);
		
		g_free (node->disposition.type);
		if (node->disposition.params)
			g_object_unref (node->disposition.params);
		
		g_free (node->encoding);
		
		if (node->envelope) {
			g_free (node->envelope->date);
			g_free (node->envelope->subject);
			g_free (node->envelope->from);
			g_free (node->envelope->sender);
			g_free (node->envelope->reply_to);
			g_free (node->envelope->to);
			g_free (node->envelope->cc);
			g_free (node->envelope->bcc);
			g_free (node->envelope->in_reply_to);
			g_free (node->envelope->message_id);
			g_free (node->envelope);
		}
		
		if (node->subparts)
			bodystruct_free (node->subparts);
		
		next = node->next;
		g_free (node);
		node = next;
	}
}

static void
reconstruct_part_content (GMimePart *part, const char *uid, const char *spec)
{
	GMimeDataWrapper *content;
	GMimeStream *stream;
	char *filename;
	int fd;
	
	filename = g_strdup_printf ("%s/%s.TEXT", uid, spec);
	if ((fd = open (filename, O_RDONLY, 0)) == -1) {
		g_free (filename);
		return;
	}
	
	g_free (filename);
	
	stream = g_mime_stream_fs_new (fd);
	
	content = g_mime_data_wrapper_new_with_stream (stream, part->encoding);
	g_object_unref (stream);
	
	g_mime_part_set_content (part, content);
}

static void
reconstruct_message_part (GMimeMessagePart *msgpart, const char *uid, const char *spec)
{
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	char *filename;
	int fd;
	
	filename = g_strdup_printf ("%s/%s.TEXT", uid, spec);
	if ((fd = open (filename, O_RDONLY, 0)) == -1) {
		g_mime_message_part_set_message (msgpart, NULL);
		g_free (filename);
		return;
	}
	
	g_free (filename);
	
	stream = g_mime_stream_fs_new (fd);
	parser = g_mime_parser_new_with_stream (stream);
	g_object_unref (stream);
	
	message = g_mime_parser_construct_message (parser, NULL);
	g_object_unref (parser);
	
	g_mime_message_part_set_message (msgpart, message);
	g_object_unref (message);
}

static void
reconstruct_multipart (GMimeMultipart *multipart, struct _bodystruct *body,
		       const char *uid, const char *spec)
{
	struct _bodystruct *part;
	GMimeObject *subpart;
	GMimeParser *parser;
	GMimeStream *stream;
	char *subspec, *id;
	char *filename;
	int fd, i = 1;
	
	subspec = g_alloca (strlen (spec) + 14);
	id = g_stpcpy (subspec, spec);
	*id++ = '.';
	
	part = body->subparts;
	
	while (part != NULL) {
		sprintf (id, "%d", i++);
		
		fprintf (stderr, "reconstructing a %s/%s part (%s)\n", part->content.type,
			 part->content.subtype, subspec);
		
		/* NOTE: if we didn't want to necessarily construct
		   the full part, we could use the BODYSTRUCTURE info
		   to create a 'fake' MIME part of the correct
		   type/subtype and even fill in some other useful
		   Content-* headers (like Content-Disposition and
		   Content-Transfer-Encoding) so that our UI could
		   actually use that info. We could then go out and
		   fetch the content "on demand"... but this example
		   is just to show you *how* to construct MIME parts
		   manually rather than to do uber-fancy stuff */
		
		filename = g_strdup_printf ("%s/%s.HEADER", uid, subspec);
		if ((fd = open (filename, O_RDONLY, 0)) == -1) {
			g_free (filename);
			return;
		}
		
		g_free (filename);
		
		stream = g_mime_stream_fs_new (fd);
		parser = g_mime_parser_new_with_stream (stream);
		g_object_unref (stream);
		
		subpart = g_mime_parser_construct_part (parser, NULL);
		g_object_unref (parser);
		
		if (GMIME_IS_MULTIPART (subpart)) {
			reconstruct_multipart ((GMimeMultipart *) subpart, part, uid, subspec);
		} else if (GMIME_IS_MESSAGE_PART (subpart)) {
			reconstruct_message_part ((GMimeMessagePart *) subpart, uid, subspec);
		} else if (GMIME_IS_PART (subpart)) {
			reconstruct_part_content ((GMimePart *) subpart, uid, subspec);
		}
		
		g_mime_multipart_add (multipart, subpart);
		g_object_unref (subpart);
		
		part = part->next;
	}
}

static void
reconstruct_message (const char *uid)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeObject *mime_part;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	char *filename;
	int fd;
	
	filename = g_strdup_printf ("%s/HEADER", uid);
	if ((fd = open (filename, O_RDONLY, 0)) == -1) {
		g_free (filename);
		return;
	}
	
	g_free (filename);
	
	stream = g_mime_stream_fs_new (fd);
	parser = g_mime_parser_new_with_stream (stream);
	g_object_unref (stream);
	
	/* constructs message object and toplevel mime part (although
	   the toplevel mime part will not have any content... */
	message = g_mime_parser_construct_message (parser, NULL);
	g_object_unref (parser);
	
	mime_part = g_mime_message_get_mime_part (message);
	
	if (GMIME_IS_MULTIPART (mime_part)) {
		struct _bodystruct *body;
		GByteArray *buffer;
		GMimeStream *mem;
		
		filename = g_strdup_printf ("%s/BODYSTRUCTURE", uid);
		if ((fd = open (filename, O_RDONLY, 0)) == -1) {
			g_free (filename);
			return;
		}
		
		g_free (filename);
		
		stream = g_mime_stream_fs_new (fd);
		mem = g_mime_stream_mem_new ();
		
		g_mime_stream_write_to_stream (stream, mem);
		g_object_unref (stream);
		
		buffer = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) mem);
		body = bodystruct_parse (buffer->data, buffer->len);
		g_object_unref (mem);
		
		bodystruct_dump (body, 0);
		
		reconstruct_multipart ((GMimeMultipart *) mime_part, body, uid, "1");
		bodystruct_free (body);
	} else if (GMIME_IS_PART (mime_part)) {
		reconstruct_part_content ((GMimePart *) mime_part, uid, "1");
	}
	
	filename = g_strdup_printf ("%s/MESSAGE", uid);
	if ((fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		g_free (filename);
		return;
	}
	
	g_free (filename);
	stream = g_mime_stream_fs_new (fd);
	g_mime_object_write_to_stream ((GMimeObject *) message, format, stream);
	g_object_unref (message);
	g_object_unref (stream);
}


int main (int argc, char **argv)
{
	GMimeFormat format = GMIME_FORMAT_MESSAGE;
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	int fd, i = 1;
	char *uid;
	
	if (argc < 2)
		return 0;
	
	g_mime_init ();
	
	if (!strcmp (argv[i], "--mbox")) {
		format = GMIME_FORMAT_MBOX;
		i++;
	}
	
	if ((fd = open (argv[i], O_RDONLY, 0)) == -1)
		return 0;
	
	stream = g_mime_stream_fs_new (fd);
	
	parser = g_mime_parser_new_with_stream (stream);
	g_mime_parser_set_format (parser, format);
	g_object_unref (stream);
	
	message = g_mime_parser_construct_message (parser, NULL);
	g_object_unref (parser);
	
	if (message) {
		uid = g_strdup (message->message_id ? message->message_id : basename (argv[i]));
		g_mkdir (uid, 0777);
		write_message (message, uid);
		g_object_unref (message);
		
		reconstruct_message (uid);
		g_free (uid);
	}
	
	return 0;
}
