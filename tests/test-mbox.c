/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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
#include <dirent.h>
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
	const GMimeContentType *type;
	
	print_depth (stream, depth);
	
	type = g_mime_object_get_content_type (part);
	
	g_mime_stream_printf (stream, "Content-Type: %s/%s\n", type->type, type->subtype);
	
	if (GMIME_IS_MULTIPART (part)) {
		GList *subpart;
		
		subpart = GMIME_MULTIPART (part)->subparts;
		while (subpart) {
			print_mime_struct (stream, subpart->data, depth + 1);
			subpart = subpart->next;
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		GMimeMessagePart *mpart = (GMimeMessagePart *) part;
		
		if (mpart->message)
			print_mime_struct (stream, mpart->message->mime_part, depth + 1);
	}
}


static void
header_cb (GMimeParser *parser, const char *header, const char *value, gint64 offset, gpointer user_data)
{
	GMimeStream *stream = user_data;
	
	g_mime_stream_printf (stream, OFF_T ": %s: %s\n", offset, header, value);
}

static void
test_parser (GMimeParser *parser, GMimeStream *mbox, GMimeStream *summary)
{
	GMimeMessage *message;
	const char *exev;
	gint64 start, end;
	int nmsg = 0;
	char *from;
	
	while (!g_mime_parser_eos (parser)) {
		start = g_mime_parser_tell (parser);
		if (!(message = g_mime_parser_construct_message (parser)))
			throw (exception_new ("failed to parse message #%d", nmsg));
		
		end = g_mime_parser_tell (parser);
		
		g_mime_stream_printf (summary, "message offsets: " OFF_T ", " OFF_T "\n", start, end);
		
		from = g_mime_parser_get_from (parser);
		g_mime_stream_printf (summary, "%s\n", from);
		exev = g_mime_message_get_header (message, "X-Evolution");
		g_mime_stream_printf (summary, "X-Evolution: %s\n", exev ? exev : "None");
		print_mime_struct (summary, message->mime_part, 0);
		g_mime_stream_write (summary, "\n", 1);
		
		if (mbox) {
			g_mime_stream_printf (mbox, "%s%s\n", nmsg > 0 ? "\n" : "", from);
			g_mime_object_write_to_stream ((GMimeObject *) message, mbox);
		}
		
		g_object_unref (message);
		g_free (from);
		nmsg++;
	}
}

static gboolean
streams_match (GMimeStream *istream, GMimeStream *ostream)
{
	char buf[4096], dbuf[4096], errstr[1024], *bufptr, *bufend, *dbufptr;
	size_t totalsize, totalread = 0;
	size_t nread, size;
	gint64 offset = 0;
	ssize_t n;
	
	v(fprintf (stdout, "Checking if streams match... "));
	
	if (istream->bound_end != -1) {
		totalsize = istream->bound_end - istream->position;
	} else if ((n = g_mime_stream_length (istream)) == -1) {
		sprintf (errstr, "Error: Unable to get length of original stream\n");
		goto fail;
	} else if (n < (istream->position - istream->bound_start)) {
		sprintf (errstr, "Error: Overflow on original stream?\n");
		goto fail;
	} else {
		totalsize = n - (istream->position - istream->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (istream, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read " SIZE_T " bytes from istream\n", size));
		
		do {
			if ((n = g_mime_stream_read (ostream, dbuf + nread, size - nread)) <= 0) {
				fprintf (stderr, "ostream's read() returned " SSIZE_T ", EOF\n", n);
				break;
			}
			d(fprintf (stderr, "read " SSIZE_T " bytes from ostream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: ostream appears to be truncated, short %u+ bytes\n",
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
			sprintf (errstr, "Error: content does not match at offset " OFF_T "\n",
				 offset + (bufptr - buf));
			/*fprintf (stderr, "-->'%.*s'<--\nvs\n-->'%.*s'<--\n",
			  bufend - bufptr, bufptr, bufend - bufptr, dbufptr);*/
			goto fail;
		} else {
			d(fprintf (stderr, SIZE_T " bytes identical\n", size));
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
	struct dirent *dent;
	const char *path;
	struct stat st;
	int fd, i;
	DIR *dir;
	
#ifdef ENABLE_MBOX_MATCH
	if (g_mkdir ("./tmp", 0755) == -1 && errno != EEXIST)
		return 0;
#endif
	
	g_mime_init (0);
	
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
		
		if (!(dir = opendir (input)))
			goto exit;
		
		*p++ = G_DIR_SEPARATOR;
		*p = '\0';
		
		q = g_stpcpy (output, path);
		*q++ = G_DIR_SEPARATOR;
		q = g_stpcpy (q, "output");
		*q++ = G_DIR_SEPARATOR;
		*q = '\0';
		
		while ((dent = readdir (dir))) {
			if (!g_str_has_suffix (dent->d_name, ".mbox"))
				continue;
			
			strcpy (p, dent->d_name);
			strcpy (q, dent->d_name);
			
			tmp = NULL;
			parser = NULL;
			istream = NULL;
			ostream = NULL;
			mstream = NULL;
			pstream = NULL;
			
			testsuite_check ("%s", dent->d_name);
			try {
				if ((fd = open (input, O_RDONLY)) == -1) {
					throw (exception_new ("could not open `%s': %s",
							      input, strerror (errno)));
				}
				
				istream = g_mime_stream_fs_new (fd);
				
				if ((fd = open (output, O_RDONLY)) == -1) {
					throw (exception_new ("could not open `%s': %s",
							      output, strerror (errno)));
				}
				
				ostream = g_mime_stream_fs_new (fd);
				
#ifdef ENABLE_MBOX_MATCH
				tmp = g_strdup_printf ("./tmp/%s.XXXXXX", dent->d_name);
				if ((fd = g_mkstemp (tmp)) == -1) {
					throw (exception_new ("could not open `%s': %s",
							      tmp, strerror (errno)));
				}
				
				mstream = g_mime_stream_fs_new (fd);
#endif
				
				parser = g_mime_parser_new_with_stream (istream);
				g_mime_parser_set_persist_stream (parser, TRUE);
				g_mime_parser_set_scan_from (parser, TRUE);
				
				if (strstr (dent->d_name, "content-length") != NULL)
					g_mime_parser_set_respect_content_length (parser, TRUE);
				
				pstream = g_mime_stream_mem_new ();
				g_mime_parser_set_header_regex (parser, "^Subject$", header_cb, pstream);
				test_parser (parser, mstream, pstream);
				
#ifdef ENABLE_MBOX_MATCH
				g_mime_stream_reset (istream);
				g_mime_stream_reset (mstream);
				if (!streams_match (istream, mstream))
					throw (exception_new ("mboxes do not match for `%s'", dent->d_name));
#endif
				
				g_mime_stream_reset (ostream);
				g_mime_stream_reset (pstream);
				if (!streams_match (ostream, pstream))
					throw (exception_new ("summaries do not match for `%s'", dent->d_name));
				
				testsuite_check_passed ();
				
#ifdef ENABLE_MBOX_MATCH
				unlink (tmp);
#endif
			} catch (ex) {
				if (parser != NULL)
					testsuite_check_failed ("%s: %s", dent->d_name, ex->message);
				else
					testsuite_check_warn ("%s: %s", dent->d_name, ex->message);
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
		
		closedir (dir);
	} else if (S_ISREG (st.st_mode)) {
		/* manually run test on a single file */
		if ((fd = open (path, O_RDONLY)) == -1)
			goto exit;
		
		istream = g_mime_stream_fs_new (fd);
		parser = g_mime_parser_new_with_stream (istream);
		g_mime_parser_set_scan_from (parser, TRUE);
		
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
		
		ostream = g_mime_stream_fs_new (dup (1));
		g_mime_parser_set_header_regex (parser, "^Subject$", header_cb, ostream);
		
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
		
#ifdef ENABLE_MBOX_MATCH
		g_object_unref (mstream);
		g_free (tmp);
#endif
	} else {
		goto exit;
	}
	
 exit:
	
#ifdef ENABLE_MBOX_MATCH
	g_rmdir ("./tmp");
#endif
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
