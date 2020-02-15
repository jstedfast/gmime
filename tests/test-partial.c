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


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x

static gboolean
streams_match (GMimeStream *expected, GMimeStream *actual)
{
	char buf[4096], dbuf[4096], errstr[1024], *bufptr, *bufend, *dbufptr;
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	gint64 offset = 0;
	ssize_t n;
	
	v(fprintf (stdout, "Checking if streams match... "));
	
	if (expected->bound_end != -1) {
		totalsize = expected->bound_end - expected->position;
	} else if ((len = g_mime_stream_length (expected)) == -1) {
		sprintf (errstr, "Error: Unable to get length of expected stream\n");
		goto fail;
	} else if (len < (expected->position - expected->bound_start)) {
		sprintf (errstr, "Error: Overflow on expected stream?\n");
		goto fail;
	} else {
		totalsize = len - (expected->position - expected->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (expected, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %zu bytes from expected stream\n", size));
		
		do {
			if ((n = g_mime_stream_read (actual, dbuf + nread, size - nread)) <= 0) {
				fprintf (stderr, "actual stream's read(%p, dbuf + %zu, %zu) returned %zd, EOF\n", actual, nread, size - nread, n);
				break;
			}
			d(fprintf (stderr, "read %zd bytes from actual stream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: actual stream appears to be truncated, short %zu+ bytes\n",
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
		strcpy (errstr, "Error: expected more data from input stream\n");
		goto fail;
	}
	
	if ((n = g_mime_stream_read (actual, buf, sizeof (buf))) > 0) {
		strcpy (errstr, "Error: actual stream appears to contain extra content\n");
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
	const char *datadir = "data/partial";
	GMimeStream *stream, *combined, *expected;
	GMimeMessage *message, **messages;
	GMimeMessagePartial **parts;
	GMimeFormatOptions *format;
	GString *input, *output;
	GMimeParser *parser;
	GPtrArray *partials;
	int inlen, outlen;
	GDir *data, *dir;
	const char *dent;
	size_t i, n;
	char *path;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	testsuite_start ("message/partial");
	
	format = g_mime_format_options_get_default ();
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	output = g_string_new (datadir);
	g_string_append_c (output, G_DIR_SEPARATOR);
	g_string_append (output, "output");
	
	input = g_string_new (datadir);
	g_string_append_c (input, G_DIR_SEPARATOR);
	g_string_append (input, "input");
	
	if (!(data = g_dir_open (input->str, 0, NULL)))
		goto exit;
	
	g_string_append_c (output, G_DIR_SEPARATOR);
	g_string_append_c (input, G_DIR_SEPARATOR);
	outlen = output->len;
	inlen = input->len;
	
	while ((dent = g_dir_read_name (data))) {
		g_string_append (output, dent);
		g_string_append (input, dent);
		
		parser = g_mime_parser_new ();
		partials = g_ptr_array_new ();
		dir = NULL;
		
		testsuite_check ("%s", dent);
		try {
			if (!(dir = g_dir_open (input->str, 0, NULL)))
				throw (exception_new ("Failed to open `%s'", input->str));
			
			while ((dent = g_dir_read_name (dir))) {
				path = g_build_filename (input->str, dent, NULL);
				
				if (!(stream = g_mime_stream_file_open (path, "r", NULL)))
					throw (exception_new ("Failed to open `%s'", path));
				
				g_mime_parser_init_with_stream (parser, stream);
				g_object_unref (stream);
				
				if (!(message = g_mime_parser_construct_message (parser, NULL)))
					throw (exception_new ("Failed to parse `%s'", path));
				
				if (!GMIME_IS_MESSAGE_PARTIAL (message->mime_part)) {
					g_object_unref (message);
					throw (exception_new ("`%s' is not a message/partial", path));
				}
				
				g_ptr_array_add (partials, message->mime_part);
				g_object_ref (message->mime_part);
				g_object_unref (message);
				g_free (path);
				path = NULL;
			}
			
			parts = (GMimeMessagePartial **) partials->pdata;
			n = partials->len;
			
			if (!(message = g_mime_message_partial_reconstruct_message (parts, n))) {
				for (i = 0; i < n; i++)
					g_object_unref (parts[i]);
				
				throw (exception_new ("Failed to recombine message/partial `%s'", input->str));
			}
			
			for (i = 0; i < n; i++)
				g_object_unref (parts[i]);
			
			combined = g_mime_stream_mem_new ();
			g_mime_object_write_to_stream (GMIME_OBJECT (message), format, combined);
			g_mime_stream_reset (combined);
			
			if (!(expected = g_mime_stream_file_open (output->str, "r", NULL))) {
				expected = g_mime_stream_file_open (output->str, "w", NULL);
				g_mime_stream_write_to_stream (combined, expected);
				g_mime_stream_flush (expected);
				g_object_unref (expected);
				g_object_unref (combined);
				g_object_unref (message);
				
				throw (exception_new ("Failed to open `%s'", output->str));
			}
			
			if (!streams_match (expected, combined)) {
				g_object_unref (combined);
				g_object_unref (expected);
				g_object_unref (message);
				
				throw (exception_new ("messages do not match for `%s'", input->str));
			}
			
			g_object_unref (combined);
			
			if (!(messages = g_mime_message_partial_split_message (message, 4096, &n))) {
				g_object_unref (expected);
				g_object_unref (message);
				
				throw (exception_new ("Failed to split message `%s'", input->str));
			}
			
			g_object_unref (message);
			
			g_ptr_array_set_size (partials, 0);
			for (i = 0; i < n; i++) {
				g_ptr_array_add (partials, messages[i]->mime_part);
				g_object_ref (messages[i]->mime_part);
				g_object_unref (messages[i]);
			}
			g_free (messages);
			
			parts = (GMimeMessagePartial **) partials->pdata;
			n = partials->len;
			
			if (!(message = g_mime_message_partial_reconstruct_message (parts, n))) {
				for (i = 0; i < n; i++)
					g_object_unref (parts[i]);
				g_object_unref (expected);
				
				throw (exception_new ("Failed to recombine split message/partial `%s'", input->str));
			}
			
			combined = g_mime_stream_mem_new ();
			g_mime_object_write_to_stream (GMIME_OBJECT (message), format, combined);
			g_mime_stream_reset (combined);
			g_mime_stream_reset (expected);
			g_object_unref (message);
			
			if (!streams_match (expected, combined)) {
				g_object_unref (combined);
				g_object_unref (expected);
				
				throw (exception_new ("re-split/combined messages do not match for `%s'", input->str));
			}
			
			g_object_unref (combined);
			g_object_unref (expected);
			g_dir_close (dir);
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s: %s", input->str, ex->message);
			
			if (dir != NULL)
				g_dir_close (dir);
			
			g_free (path);
		} finally;
		
		g_string_truncate (output, outlen);
		g_string_truncate (input, inlen);
		g_ptr_array_free (partials, TRUE);
		g_object_unref (parser);
	}
	
	g_dir_close (data);
	
 exit:
	g_string_free (output, TRUE);
	g_string_free (input, TRUE);
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
