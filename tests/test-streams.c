/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>

#include <gmime/gmime.h>


static GSList *test = NULL;

static void
test_abort (void)
{
	GSList *n;
	
	fprintf (stderr, "stream tests failed while in:\n\t");
	n = test;
	while (n) {
		fprintf (stderr, "%s", (char*)n->data);
		if (n->next)
			fprintf (stderr, ": ");
		n = n->next;
	}
	fprintf (stderr, "\n");
	
	abort ();
}

static void
test_assert_real (gboolean result, const char *file, const char *function, int line, const char *assertion)
{
	if (!result) {
		fprintf (stderr, "\nfile %s: line %d (%s): assertion failed: (%s)\n",
			 file, line, function, assertion);
		test_abort ();
	}
}

#define test_assert(assertion) test_assert_real (assertion, __FILE__, __FUNCTION__, __LINE__, #assertion)

static void
test_push (char *subtest)
{
	test = g_slist_append (test, subtest);
}

static void
test_pop (void)
{
	GSList *n;
	
	n = test;
	while (n && n->next)
		n = n->next;
	
	if (n) {
		test = g_slist_remove_link (test, n);
		g_slist_free_1 (n);
	} else
		g_warning ("nothing to pop");
}

static void
test_stream_read (GMimeStream *stream, const char *filename)
{
	char sbuf[1024], rbuf[1024];
	ssize_t slen, rlen;
	int fd;
	
	test_push ("stream_read");
	
	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("failed to open %s", filename);
		test_pop ();
		return;
	}
	
	slen = rlen = 0;
	while (!g_mime_stream_eos (stream)) {
		slen = g_mime_stream_read (stream, sbuf, sizeof (sbuf));
		rlen = read (fd, rbuf, sizeof (rbuf));
		if (slen != rlen || memcmp (sbuf, rbuf, slen))
			break;
	}
	
	close (fd);
	
	if (slen != rlen || memcmp (sbuf, rbuf, slen)) {
		fprintf (stderr, "\tstream: \"%.*s\" (%d)\n", slen, sbuf, slen);
		fprintf (stderr, "\treal:   \"%.*s\" (%d)\n", rlen, rbuf, rlen);
		test_abort ();
	}
	
	test_pop ();
}

static void
test_stream_gets (GMimeStream *stream, const char *filename)
{
	char sbuf[100], rbuf[100];
	ssize_t slen;
	FILE *fp;
	
	test_push ("stream_gets");
	
	fp = fopen (filename, "r+");
	if (fp == NULL) {
		g_warning ("failed to open %s", filename);
		test_pop ();
		return;
	}
	
	while (!g_mime_stream_eos (stream)) {
		rbuf[0] = '\0';
		slen = g_mime_stream_buffer_gets (stream, sbuf, sizeof (sbuf));
		fgets (rbuf, sizeof (rbuf), fp);
		
		if (strcmp (sbuf, rbuf))
			break;
	}
	
	fclose (fp);
	
	if (strcmp (sbuf, rbuf)) {
		fprintf (stderr, "\tstream: \"%s\" (%d)\n", sbuf, strlen (sbuf));
		fprintf (stderr, "\treal:   \"%s\" (%d)\n", rbuf, strlen (rbuf));
		test_abort ();
	}
	
	test_pop ();
}

static void
test_stream_buffer (const char *filename)
{
	GMimeStream *stream, *buffered;
	int fd;
	
	test_push ("GMimeStreamBuffer");
	
	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("failed to open %s", filename);
		test_pop ();
		return;
	}
	
	test_push ("block read");
	stream = g_mime_stream_fs_new (fd);
	buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_BLOCK_READ);
	test_stream_read (buffered, filename);
	g_mime_stream_unref (buffered);
	test_pop ();
	
	test_push ("cache read");
	g_mime_stream_reset (stream);
	buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_CACHE_READ);
	test_stream_read (buffered, filename);
	g_mime_stream_unref (buffered);
	test_pop ();
	
	test_push ("block read");
	g_mime_stream_reset (stream);
	buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_BLOCK_READ);
	test_stream_gets (buffered, filename);
	g_mime_stream_unref (buffered);
	test_pop ();
	
	test_push ("cache read");
	g_mime_stream_reset (stream);
	buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_CACHE_READ);
	test_stream_gets (buffered, filename);
	g_mime_stream_unref (buffered);
	test_pop ();
	
	g_mime_stream_unref (stream);
	
	test_pop ();
}

static void
test_stream_mem (const char *filename)
{
	/* Note: this also tests g_mime_stream_write_to_stream */
	GMimeStream *stream, *fstream;
	int fd;
	
	test_push ("GMimeStreamMem");
	
	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("failed to open %s", filename);
		test_pop ();
		return;
	}
	
	fstream = g_mime_stream_fs_new (fd);
	stream = g_mime_stream_mem_new ();
	
	g_mime_stream_write_to_stream (fstream, stream);
	test_assert (g_mime_stream_length (stream) == g_mime_stream_length (fstream));
	g_mime_stream_unref (fstream);
	
	test_stream_read (stream, filename);
	g_mime_stream_unref (stream);
	
	test_pop ();
}

static void
test_stream_fs (const char *filename)
{
	GMimeStream *stream;
	int fd;
	
	test_push ("GMimeStreamFs");
	
	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		g_warning ("failed to open %s", filename);
		test_pop ();
		return;
	}
	
	stream = g_mime_stream_fs_new (fd);
	test_stream_read (stream, filename);
	g_mime_stream_unref (stream);
	
	test_pop ();
}

static void
test_stream_file (const char *filename)
{
	GMimeStream *stream;
	FILE *fp;
	
	test_push ("GMimeStreamFile");
	
	fp = fopen (filename, "r+");
	if (fp == NULL) {
		g_warning ("failed to open %s", filename);
		test_pop ();
		return;
	}
	
	stream = g_mime_stream_file_new (fp);
	test_stream_read (stream, filename);
	g_mime_stream_unref (stream);
	
	test_pop ();
}


int main (int argc, char **argv)
{
	int i;
	
	g_mime_init (0);
	
	for (i = 1; i < argc; i++) {
		test_stream_file (argv[i]);
		test_stream_fs (argv[i]);
		test_stream_mem (argv[i]);
		test_stream_buffer (argv[i]);
	}
	
	return 0;
}
