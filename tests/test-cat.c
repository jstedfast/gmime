/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <gmime/gmime.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-stream-fs.h>
#include <gmime/gmime-stream-cat.h>
#include <gmime/gmime-stream-mem.h>

/* Note: this test suite assumes StreamFs and StreamMem are correct */

static int verbose = 0;

#define d(x) 
#define v(x) if (verbose) x;


static GMimeStream *
random_whole_stream (void)
{
	size_t nwritten, buflen, total = 0, size, i;
	GMimeStream *stream;
	char buf[4096];
	ssize_t n;
	int fd;
	
	/* read between 4k and 14k bytes */
	size = 4096 + (size_t) (10240.0 * (rand () / (RAND_MAX + 1.0)));
	v(fprintf (stdout, "Generating %lld bytes of random data... ", size));
	v(fflush (stdout));
	
	if ((fd = open ("stream.whole", O_CREAT | O_TRUNC | O_RDWR, 0666)) == -1) {
		fprintf (stderr, "Error: Cannot create `stream.whole': %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}
	
	stream = g_mime_stream_fs_new (fd);
	
	while (total < size) {
		buflen = size - total > sizeof (buf) ? sizeof (buf) : size - total;
		for (i = 0; i < buflen; i++)
			buf[i] = (char) (255 * (rand () / (RAND_MAX + 1.0)));
		
		nwritten = 0;
		do {
			if ((n = g_mime_stream_write (stream, buf + nwritten, buflen - nwritten)) <= 0)
				break;
			nwritten += n;
			total += n;
		} while (nwritten < buflen);
		
		if (nwritten < buflen)
			break;
	}
	
	g_mime_stream_flush (stream);
	g_mime_stream_reset (stream);
	
	v(fputs ("done\n", stdout));
	
	return stream;
}

struct _StreamPart {
	struct _StreamPart *next;
	off_t pstart, pend;  /* start/end offsets of the part stream */
	off_t wstart, wend;  /* corresponding start/end offsets of the whole stream */
	char filename[64];
};


static int
check_streams_match (GMimeStream *orig, GMimeStream *dup, const char *filename, int check_overflow)
{
	char buf[4096], dbuf[4096], errstr[1024];
	size_t totalsize, totalread = 0;
	size_t nread, size;
	ssize_t n;
	
	v(fprintf (stdout, "Matching original stream (%ld -> %ld) with %s (%ld, %ld)... ",
		   orig->position, orig->bound_end, filename, dup->position, dup->bound_end));
	
	if (orig->bound_end != -1) {
		totalsize = orig->bound_end - orig->position;
	} else if ((n = g_mime_stream_length (orig)) == -1) {
		sprintf (errstr, "Error: Unable to get length of original stream\n");
		goto fail;
	} else if (n < (orig->position - orig->bound_start)) {
		sprintf (errstr, "Error: Overflow on original stream?\n");
		goto fail;
	} else {
		totalsize = n - (orig->position - orig->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (orig, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %u bytes from original stream\n", size));
		
		do {
			if ((n = g_mime_stream_read (dup, dbuf + nread, size - nread)) <= 0) {
				fprintf (stderr, "dup read() returned %d, EOF\n", n);
				break;
			}
			d(fprintf (stderr, "read %d bytes from dup stream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: `%s' appears to be truncated, short %u+ bytes\n",
				 filename, size - nread);
			goto fail;
		}
		
		if (memcmp (buf, dbuf, size) != 0) {
			sprintf (errstr, "Error: `%s': content does not match\n", filename);
			goto fail;
		} else {
			d(fprintf (stderr, "%u bytes identical\n", size));
		}
	}
	
	if (totalread < totalsize) {
		sprintf (errstr, "Error: expected more data from original stream\n");
		goto fail;
	}
	
	if (check_overflow && (n = g_mime_stream_read (dup, buf, sizeof (buf))) > 0) {
		sprintf (errstr, "Error: `%s' appears to contain extra content\n", filename);
		goto fail;
	}
	
	v(fputs ("passed\n", stdout));
	
	return 0;
	
 fail:
	
	v(fputs ("failed\n", stdout));
	fputs (errstr, stderr);
	
	return -1;
}

static int
test_cat_write (GMimeStream *whole, struct _StreamPart *parts)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *sub, *cat;
	int fd;
	
	v(fprintf (stdout, "\nTesting GMimeStreamCat::write()...\n"));
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%ld, end=%ld...\n",
			   part->filename, part->pstart, part->pend));
		if ((fd = open (part->filename, O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1) {
			fprintf (stderr, "Error: Failed to create `%s': %s\n",
				 part->filename, strerror (errno));
			g_object_unref (cat);
			return -1;
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, part->pend);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	g_mime_stream_reset (whole);
	if (g_mime_stream_write_to_stream (whole, (GMimeStream *) cat) == -1) {
		fprintf (stderr, "Error: failed to write to cat stream: %s\n", strerror (errno));
		g_object_unref (cat);
		return -1;
	}
	
	g_object_unref (cat);
	
	/* now lets check that the content matches */
	d(fprintf (stderr, "checking all part streams have correct data...\n"));
	part = parts;
	while (part != NULL) {
		d(fprintf (stderr, "checking substream %s\n", part->filename));
		if ((fd = open (part->filename, O_RDONLY)) == -1) {
			fprintf (stderr, "Error: failed to check contents of `%s': %s\n",
				 part->filename, strerror (errno));
			return -1;
		}
		
		if (!(sub = g_mime_stream_substream (whole, part->wstart, part->wend))) {
			fprintf (stderr, "Error: failed to substream original stream\n");
			close (fd);
			return -1;
		}
		
		if (!(stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, -1))) {
			fprintf (stderr, "Error: failed to create stream for %s\n", part->filename);
			close (fd);
			return -1;
		}
		
		d(fprintf (stderr, "checking substream %s matches...\n", part->filename));
		if (check_streams_match (sub, stream, part->filename, TRUE) == -1) {
			g_object_unref (stream);
			g_object_unref (sub);
			return -1;
		}
		
		g_object_unref (stream);
		g_object_unref (sub);
		
		part = part->next;
	}
	
	v(fprintf (stdout, "Test of GMimeStreamCat::write() successful.\n"));
	
	return 0;
}

static int
test_cat_read (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *cat;
	int fd;
	
	v(fprintf (stdout, "\nTesting GMimeStreamCat::read()%s...\n",
		   bounded ? "" : " with unbound sources"));
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%ld, end=%ld...\n",
			   part->filename, part->pstart, part->pend));
		if ((fd = open (part->filename, O_RDONLY)) == -1) {
			fprintf (stderr, "Error: Failed to open `%s': %s\n",
				 part->filename, strerror (errno));
			g_object_unref (cat);
			return -1;
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	g_mime_stream_reset (whole);
	if (check_streams_match (whole, cat, "stream.part*", TRUE) == -1) {
		g_object_unref (cat);
		return -1;
	}
	
	v(fprintf (stdout, "Test of GMimeStreamCat::read()%s successful.\n",
		   bounded ? "" : " with unbound sources"));
	
	return 0;
}

static int
test_cat_seek (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *cat;
	off_t offset;
	size_t size;
	ssize_t n;
	int fd;
	
	v(fprintf (stdout, "\nTesting GMimeStreamCat::seek()%s...\n",
		   bounded ? "" : " with unbound sources"));
	
	if (whole->bound_end != -1) {
		size = whole->bound_end - whole->bound_start;
	} else if ((n = g_mime_stream_length (whole)) == -1) {
		fprintf (stderr, "Error: Unable to get length of original stream\n");
		return -1;
	} else {
		size = n;
	}
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%ld, end=%ld...\n",
			   part->filename, part->pstart, part->pend));
		if ((fd = open (part->filename, O_RDONLY)) == -1) {
			fprintf (stderr, "Error: Failed to open `%s': %s\n",
				 part->filename, strerror (errno));
			g_object_unref (cat);
			return -1;
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	/* calculate a random seek offset to compare at */
	offset = (off_t) (size * (rand () / (RAND_MAX + 1.0)));
	
	if (g_mime_stream_seek (whole, offset, GMIME_STREAM_SEEK_SET) == -1) {
		fprintf (stderr, "Error: failed to seek to %d in original stream: %s\n",
			 offset, strerror (errno));
		return -1;
	}
	
	if (g_mime_stream_seek (cat, offset, GMIME_STREAM_SEEK_SET) == -1) {
		fprintf (stderr, "Error: failed to seek to %d in cat stream: %s\n",
			 offset, strerror (errno));
		return -1;
	}
	
	if (check_streams_match (whole, cat, "stream.part*", TRUE) == -1) {
		g_object_unref (cat);
		return -1;
	}
	
	v(fprintf (stdout, "Test of GMimeStreamCat::seek()%s successful.\n",
		   bounded ? "" : " with unbound sources"));
	
	return 0;
}

static int
test_cat_substream (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	GMimeStream *stream, *cat, *sub1, *sub2;
	struct _StreamPart *part = parts;
	off_t start, end;
	size_t size;
	ssize_t n;
	int fd;
	
	v(fprintf (stdout, "\nTesting GMimeStreamCat::substream()%s...\n",
		   bounded ? "" : " with unbound sources"));
	
	if (whole->bound_end != -1) {
		size = whole->bound_end - whole->bound_start;
	} else if ((n = g_mime_stream_length (whole)) == -1) {
		fprintf (stderr, "Error: Unable to get length of original stream\n");
		return -1;
	} else {
		size = n;
	}
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%ld, end=%ld...\n",
			   part->filename, part->pstart, part->pend));
		if ((fd = open (part->filename, O_RDONLY)) == -1) {
			fprintf (stderr, "Error: Failed to open `%s': %s\n",
				 part->filename, strerror (errno));
			g_object_unref (cat);
			return -1;
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	/* calculate a random start/end offsets */
	start = (off_t) (size * (rand () / (RAND_MAX + 1.0)));
	if (rand () % 2)
		end = start + (off_t) ((size - start) * (rand () / (RAND_MAX + 1.0)));
	else
		end = -1;
	
	if (!(sub1 = g_mime_stream_substream (whole, start, end))) {
		fprintf (stderr, "Error: failed to substream original stream: %s\n",
			 strerror (errno));
		g_object_unref (cat);
		return -1;
	}
	
	if (!(sub2 = g_mime_stream_substream (cat, start, end))) {
		fprintf (stderr, "Error: failed to substream cat stream: %s\n",
			 strerror (errno));
		g_object_unref (sub1);
		g_object_unref (cat);
		return -1;
	}
	
	g_object_unref (cat);
	
	if (check_streams_match (sub1, sub2, "stream.part*", TRUE) == -1) {
		g_object_unref (sub1);
		g_object_unref (sub2);
		return -1;
	}
	
	g_object_unref (sub1);
	g_object_unref (sub2);
	
	v(fprintf (stdout, "Test of seeking in a GMimeStreamCat%s successful.\n",
		   bounded ? "" : " with unbound sources"));
	
	return 0;
}

int main (int argc, char **argv)
{
	struct _StreamPart *list, *tail, *n;
	ssize_t wholelen, left;
	GMimeStream *whole;
	guint32 part = 0;
	off_t start = 0;
	int fd, i, j;
	size_t len;
	
	srand (time (NULL));
	
	g_mime_init (0);
	
	/* get our verbose level */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			break;
		
		for (j = 1; argv[i][j]; j++) {
			if (argv[i][j] == 'v')
				verbose++;
		}
	}
	
	if (i < argc) {
		if ((fd = open (argv[i], O_RDONLY)) == -1)
			return EXIT_FAILURE;
		
		whole = g_mime_stream_fs_new (fd);
	} else {
		whole = random_whole_stream ();
	}
	
	if ((wholelen = g_mime_stream_length (whole)) == -1) {
		fprintf (stderr, "Error: length of test stream unknown\n");
		g_object_unref (whole);
		return EXIT_FAILURE;
	} else if (wholelen == 64) {
		fprintf (stderr, "Error: length of test stream is unsuitable for testing\n");
		g_object_unref (whole);
		return EXIT_FAILURE;
	}
	
	list = NULL;
	tail = (struct _StreamPart *) &list;
	
	left = wholelen;
	while (left > 0) {
		len = 1 + (size_t) (left * (rand() / (RAND_MAX + 1.0)));
		n = g_new (struct _StreamPart, 1);
		sprintf (n->filename, "stream.part%u", part++);
		n->pstart = (off_t) 0; /* FIXME: we could make this a random offset */
		n->pend = n->pstart + len;
		n->wend = start + len;
		n->wstart = start;
		tail->next = n;
		tail = n;
		
		start += len;
		left -= len;
	}
	
	tail->next = NULL;
	
	if (test_cat_write (whole, list) == -1)
		goto fail;
	
	if (test_cat_read (whole, list, TRUE) == -1)
		goto fail;
	
	if (test_cat_read (whole, list, FALSE) == -1)
		goto fail;
	
	if (test_cat_seek (whole, list, TRUE) == -1)
		goto fail;
	
	if (test_cat_seek (whole, list, FALSE) == -1)
		goto fail;
	
	if (test_cat_substream (whole, list, TRUE) == -1)
		goto fail;
	
	if (test_cat_substream (whole, list, FALSE) == -1)
		goto fail;
	
	while (list != NULL) {
		n = list->next;
		g_free (list);
		list = n;
	}
	
	g_object_unref (whole);
	
	return EXIT_SUCCESS;
	
 fail:
	
	while (list != NULL) {
		n = list->next;
		g_free (list);
		list = n;
	}
	
	g_object_unref (whole);
	
	return EXIT_FAILURE;
}
