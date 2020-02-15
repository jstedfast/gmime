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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x


//#define ENABLE_MBOX_MATCH


#define INDENT "   "

static void
print_depth (GMimeStream *stream, int depth)
{
	int i;
	
	for (i = 0; i < depth; i++)
		g_mime_stream_write (stream, INDENT, strlen (INDENT));
}

static void
print_mime_struct (GMimeStream *stream, GMimeObject *part, int depth)
{
	GMimeMultipart *multipart;
	GMimeMessagePart *mpart;
	GMimeContentType *type;
	GMimeObject *subpart;
	GMimeObject *body;
	GMimeMessage *msg;
	int i, n;
	
	print_depth (stream, depth);
	
	type = g_mime_object_get_content_type (part);
	
	g_mime_stream_printf (stream, "Content-Type: %s/%s\n",
			      g_mime_content_type_get_media_type (type),
			      g_mime_content_type_get_media_subtype (type));
	
	if (GMIME_IS_MULTIPART (part)) {
		multipart = (GMimeMultipart *) part;
		
		n = g_mime_multipart_get_count (multipart);
		for (i = 0; i < n; i++) {
			subpart = g_mime_multipart_get_part (multipart, i);
			print_mime_struct (stream, subpart, depth + 1);
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		mpart = (GMimeMessagePart *) part;
		msg = g_mime_message_part_get_message (mpart);
		
		if (msg != NULL) {
			body = g_mime_message_get_mime_part (msg);
			
			print_mime_struct (stream, body, depth + 1);
		}
	}
}

static void
xevcb (GMimeParser *parser, const char *header, const char *value, gint64 offset, gpointer user_data)
{
}

static void
test_parser (GMimeParser *parser, GMimeStream *mbox, GMimeStream *summary)
{
	gint64 message_begin, message_end, headers_begin, headers_end;
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	InternetAddressList *list;
	GMimeMessage *message;
	char *marker, *buf;
	const char *subject;
	GMimeObject *body;
	GDateTime *date;
	int nmsg = 0;
	
	while (!g_mime_parser_eos (parser)) {
		message_begin = g_mime_parser_tell (parser);
		if (!(message = g_mime_parser_construct_message (parser, NULL)))
			throw (exception_new ("failed to parse message #%d", nmsg));
		
		message_end = g_mime_parser_tell (parser);
		
		headers_begin = g_mime_parser_get_headers_begin (parser);
		headers_end = g_mime_parser_get_headers_end (parser);
		
		g_mime_stream_printf (summary, "message offsets: %" G_GINT64_FORMAT ", %" G_GINT64_FORMAT "\n",
				      message_begin, message_end);
		g_mime_stream_printf (summary, "header offsets: %" G_GINT64_FORMAT ", %" G_GINT64_FORMAT "\n",
				      headers_begin, headers_end);
		
		marker = g_mime_parser_get_mbox_marker (parser);
		g_mime_stream_printf (summary, "%s\n", marker);
		
		if ((list = g_mime_message_get_from (message)) != NULL &&
		    internet_address_list_length (list) > 0) {
			buf = internet_address_list_to_string (list, format, FALSE);
			g_mime_stream_printf (summary, "From: %s\n", buf);
			g_free (buf);
		}
		
		if ((list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO)) != NULL &&
		    internet_address_list_length (list) > 0) {
			buf = internet_address_list_to_string (list, format, FALSE);
			g_mime_stream_printf (summary, "To: %s\n", buf);
			g_free (buf);
		}
		
		if (!(subject = g_mime_message_get_subject (message)))
			subject = "";
		g_mime_stream_printf (summary, "Subject: %s\n", subject);
		
		if (!(date = g_mime_message_get_date (message))) {
			date = g_date_time_new_from_unix_utc (0);
		} else {
			g_date_time_ref (date);
		}
		buf = g_mime_utils_header_format_date (date);
		g_mime_stream_printf (summary, "Date: %s\n", buf);
		g_date_time_unref (date);
		g_free (buf);
		
		body = g_mime_message_get_mime_part (message);
		print_mime_struct (summary, body, 0);
		g_mime_stream_write (summary, "\n", 1);
		
		if (mbox) {
			if (nmsg > 0)
				g_mime_stream_write (mbox, "\n", 1);
			
			g_mime_stream_printf (mbox, "%s\n", marker);
			g_mime_object_write_to_stream ((GMimeObject *) message, format, mbox);
		}
		
		g_object_unref (message);
		g_free (marker);
		nmsg++;
	}
}

static gboolean
streams_match (GMimeStream *istream, GMimeStream *ostream)
{
	char buf[4096], dbuf[4096], errstr[1024], *bufptr, *bufend, *dbufptr;
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	gint64 offset = 0;
	ssize_t n;
	
	v(fprintf (stdout, "Checking if streams match... "));
	
	if (istream->bound_end != -1) {
		totalsize = istream->bound_end - istream->position;
	} else if ((len = g_mime_stream_length (istream)) == -1) {
		sprintf (errstr, "Error: Unable to get length of original stream\n");
		goto fail;
	} else if (len < (istream->position - istream->bound_start)) {
		sprintf (errstr, "Error: Overflow on original stream?\n");
		goto fail;
	} else {
		totalsize = len - (istream->position - istream->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (istream, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %zu bytes from istream\n", size));
		
		do {
			if ((n = g_mime_stream_read (ostream, dbuf + nread, size - nread)) <= 0) {
				fprintf (stderr, "ostream's read(%p, dbuf + %zu, %zu) returned %zd, EOF\n", ostream, nread, size - nread, n);
				break;
			}
			d(fprintf (stderr, "read %zd bytes from ostream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: ostream appears to be truncated, short %zu+ bytes\n",
				 size - nread);
			goto fail;
		}
		
		bufend = buf + size;
		dbufptr = dbuf;
		bufptr = buf;
		
		while (bufptr < bufend) {
			if (*bufptr != *dbufptr)
				break;
			
			dbufptr++;
			bufptr++;
		}
		
		if (bufptr < bufend) {
			sprintf (errstr, "Error: content does not match at offset %" G_GINT64_FORMAT "\n",
				 offset + (bufptr - buf));
			/*fprintf (stderr, "-->'%.*s'<--\nvs\n-->'%.*s'<--\n",
			  bufend - bufptr, bufptr, bufend - bufptr, dbufptr);*/
			goto fail;
		} else {
			d(fprintf (stderr, "%zu bytes identical\n", size));
		}
		
		offset += size;
	}
	
	if (totalread < totalsize) {
		strcpy (errstr, "Error: expected more data from istream\n");
		goto fail;
	}
	
	if ((n = g_mime_stream_read (ostream, buf, sizeof (buf))) > 0) {
		strcpy (errstr, "Error: ostream appears to contain extra content\n");
		goto fail;
	}
	
	v(fputs ("passed\n", stdout));
	
	return TRUE;
	
 fail:
	
	v(fputs ("failed\n", stdout));
	v(fputs (errstr, stderr));
	
	return FALSE;
}

int main (int argc, char **argv)
{
	const char *datadir = "data/mbox";
	char input[256], output[256], *tmp, *p, *q;
	GMimeStream *istream, *ostream, *mstream, *pstream;
	GMimeParser *parser;
	const char *dent;
	const char *path;
	struct stat st;
	GDir *dir;
	int i;
#ifdef ENABLE_MBOX_MATCH
	int fd;

	if (mkdir ("./tmp", 0755) == -1 && errno != EEXIST)
		return 0;
#endif
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	path = datadir;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			path = argv[i];
			break;
		}
	}
	
	testsuite_start ("Mbox parser");
	
	if (stat (path, &st) == -1)
		goto exit;
	
	if (S_ISDIR (st.st_mode)) {
		/* automated testsuite */
		p = g_stpcpy (input, path);
		*p++ = G_DIR_SEPARATOR;
		p = g_stpcpy (p, "input");
		
		if (!(dir = g_dir_open (input, 0, NULL)))
			goto exit;
		
		*p++ = G_DIR_SEPARATOR;
		*p = '\0';
		
		q = g_stpcpy (output, path);
		*q++ = G_DIR_SEPARATOR;
		q = g_stpcpy (q, "output");
		*q++ = G_DIR_SEPARATOR;
		*q = '\0';
		
		while ((dent = g_dir_read_name (dir))) {
			if (!g_str_has_suffix (dent, ".mbox"))
				continue;
			
			strcpy (p, dent);
			strcpy (q, dent);
			
			tmp = NULL;
			parser = NULL;
			istream = NULL;
			ostream = NULL;
			mstream = NULL;
			pstream = NULL;
			
			testsuite_check ("%s", dent);
			try {
				if (!(istream = g_mime_stream_fs_open (input, O_RDONLY, 0, NULL))) {
					throw (exception_new ("could not open `%s': %s",
							      input, g_strerror (errno)));
				}
				
				if (!(ostream = g_mime_stream_fs_open (output, O_RDONLY, 0, NULL))) {
					throw (exception_new ("could not open `%s': %s",
							      output, g_strerror (errno)));
				}
				
#ifdef ENABLE_MBOX_MATCH
				tmp = g_strdup_printf ("./tmp/%s", dent);
				if ((fd = open (tmp, O_CREAT | O_RDWR | O_TRUNC, 0644)) == -1) {
					throw (exception_new ("could not open `%s': %s",
							      tmp, g_strerror (errno)));
				}
				
				mstream = g_mime_stream_fs_new (fd);
#endif
				
				parser = g_mime_parser_new_with_stream (istream);
				g_mime_parser_set_persist_stream (parser, TRUE);
				g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
				
				if (!g_mime_parser_get_persist_stream (parser))
					throw (exception_new ("persist stream check failed"));
				
				if (g_mime_parser_get_format (parser) != GMIME_FORMAT_MBOX)
					throw (exception_new ("format check failed"));
				
				if (strstr (dent, "content-length") != NULL) {
					g_mime_parser_set_respect_content_length (parser, TRUE);
					
					if (!g_mime_parser_get_respect_content_length (parser))
						throw (exception_new ("respect content-length check failed"));
				} else {
					g_mime_parser_set_respect_content_length (parser, FALSE);
					
					if (g_mime_parser_get_respect_content_length (parser))
						throw (exception_new ("respect content-length check failed"));
				}
				
				g_mime_parser_set_header_regex (parser, "^X-Evolution", xevcb, NULL);
				
				pstream = g_mime_stream_mem_new ();
				test_parser (parser, mstream, pstream);
				
#ifdef ENABLE_MBOX_MATCH
				g_mime_stream_flush (mstream);
				g_mime_stream_reset (istream);
				g_mime_stream_reset (mstream);
				if (!streams_match (istream, mstream))
					throw (exception_new ("mboxes do not match for `%s'", dent));
#endif
				
				g_mime_stream_reset (ostream);
				g_mime_stream_reset (pstream);
				if (!streams_match (ostream, pstream))
					throw (exception_new ("summaries do not match for `%s'", dent));
				
				testsuite_check_passed ();
				
#ifdef ENABLE_MBOX_MATCH
				unlink (tmp);
#endif
			} catch (ex) {
				if (parser != NULL)
					testsuite_check_failed ("%s: %s", dent, ex->message);
				else
					testsuite_check_warn ("%s: %s", dent, ex->message);
			} finally;
			
			if (mstream != NULL)
				g_object_unref (mstream);
			
			if (pstream != NULL)
				g_object_unref (pstream);
			
			if (istream != NULL)
				g_object_unref (istream);
			
			if (ostream != NULL)
				g_object_unref (ostream);
			
			if (parser != NULL)
				g_object_unref (parser);
			
			g_free (tmp);
		}
		
		g_dir_close (dir);
	} else if (S_ISREG (st.st_mode)) {
		/* manually run test on a single file */
		if (!(istream = g_mime_stream_fs_open (path, O_RDONLY, 0, NULL)))
			goto exit;
		
		parser = g_mime_parser_new_with_stream (istream);
		g_mime_parser_set_format (parser, GMIME_FORMAT_MBOX);
		
#ifdef ENABLE_MBOX_MATCH
		tmp = g_strdup ("./tmp/mbox-test.XXXXXX");
		if ((fd = g_mkstemp (tmp)) == -1) {
			g_object_unref (istream);
			g_object_unref (parser);
			g_free (tmp);
			goto exit;
		}
		
		mstream = g_mime_stream_fs_new (fd);
#else
		mstream = NULL;
#endif
		
		ostream = g_mime_stream_file_new (stdout);
		g_mime_stream_file_set_owner ((GMimeStreamFile *) ostream, FALSE);
		
		testsuite_check ("user-input mbox: `%s'", path);
		try {
			test_parser (parser, mstream, ostream);
			
#ifdef ENABLE_MBOX_MATCH
			g_mime_stream_reset (istream);
			g_mime_stream_reset (mstream);
			if (!streams_match (istream, mstream))
				throw (exception_new ("`%s' does not match `%s'", tmp, path));
			
			unlink (tmp);
#endif
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("user-input mbox `%s': %s", path, ex->message);
		} finally;
		
		g_object_unref (istream);
		g_object_unref (ostream);
		g_object_unref (parser);
		
#ifdef ENABLE_MBOX_MATCH
		g_object_unref (mstream);
		g_free (tmp);
#endif
	} else {
		goto exit;
	}
	
 exit:
	
#ifdef ENABLE_MBOX_MATCH
	//if ((dir = g_dir_open ("./tmp", 0, NULL))) {
	//	p = g_stpcpy (input, "./tmp");
	//	*p++ = G_DIR_SEPARATOR;
	//	
	//	while ((dent = g_dir_read_name (dir))) {
	//		strcpy (p, dent);
	//		unlink (input);
	//	}
	//	
	//	g_dir_close (dir);
	//}
	
	//rmdir ("./tmp");
#endif
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
