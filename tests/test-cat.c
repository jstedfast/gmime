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
#include <errno.h>
#include <time.h>

#include <gmime/gmime.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-stream-fs.h>
#include <gmime/gmime-stream-cat.h>
#include <gmime/gmime-stream-mem.h>

#include "testsuite.h"

/* Note: this test suite assumes StreamFs and StreamMem are correct */

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x;

static int randfd;

static unsigned char
randc (void)
{
	unsigned char c;
	ssize_t n;
	
	do {
		if ((n = read (randfd, &c, 1)) > 0)
		    break;
	} while (n == -1 && errno == EINTR);
	
	return c;
}

static float
randf (void)
{
	size_t nread = 0;
	unsigned int v;
	ssize_t n;
	
	do {
		if ((n = read (randfd, ((char *) &v) + nread, sizeof (v) - nread)) > 0) {
			if ((nread += n) == sizeof (v))
				break;
		}
	} while (n == -1 && errno == EINTR);
	
	return (v * 1.0) / UINT_MAX;
}


static GMimeStream *
random_whole_stream (const char *datadir, char **filename)
{
	size_t nwritten, buflen, total = 0, size;
	GMimeStream *stream;
	char buf[4096];
	ssize_t n;
	int fd;
	
	/* read between 4k and 14k bytes */
	size = 4096 + (size_t) (10240.0 * randf ());
	v(fprintf (stdout, "Generating %zu bytes of random data... ", size));
	v(fflush (stdout));
	
	g_mkdir_with_parents (datadir, 0755);
	
	g_snprintf (buf, sizeof (buf), "%s%cstream.%u", datadir, G_DIR_SEPARATOR, getpid ());
	if ((fd = open (buf, O_CREAT | O_TRUNC | O_RDWR, 0666)) == -1) {
		fprintf (stderr, "Error: Cannot create `%s': %s\n", buf, g_strerror (errno));
		exit (EXIT_FAILURE);
	}
	
	*filename = g_strdup (buf);
	
	stream = g_mime_stream_fs_new (fd);
	
	while (total < size) {
		buflen = size - total > sizeof (buf) ? sizeof (buf) : size - total;
		
		nwritten = 0;
		do {
			if ((n = read (randfd, buf + nwritten, buflen - nwritten)) > 0)
				nwritten += n;
		} while (nwritten < buflen);
		
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
	gint64 pstart, pend;  /* start/end offsets of the part stream */
	gint64 wstart, wend;  /* corresponding start/end offsets of the whole stream */
	char filename[256];
};


static int
check_streams_match (GMimeStream *orig, GMimeStream *dup, const char *filename, int check_overflow)
{
	char buf[4096], dbuf[4096], errstr[1024];
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	ssize_t n;
	
	v(fprintf (stdout, "Matching original stream (%" G_GINT64_FORMAT " -> %" G_GINT64_FORMAT ") with %s (%" G_GINT64_FORMAT ", %" G_GINT64_FORMAT ")... ",
		   orig->position, orig->bound_end, filename, dup->position, dup->bound_end));
	
	if (orig->bound_end != -1) {
		totalsize = orig->bound_end - orig->position;
	} else if ((len = g_mime_stream_length (orig)) == -1) {
		sprintf (errstr, "Error: Unable to get length of original stream\n");
		goto fail;
	} else if (len < (orig->position - orig->bound_start)) {
		sprintf (errstr, "Error: Overflow on original stream?\n");
		goto fail;
	} else {
		totalsize = len - (orig->position - orig->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (orig, buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %zu bytes from original stream\n", size));
		
		do {
			if ((n = g_mime_stream_read (dup, dbuf + nread, size - nread)) <= 0) {
				d(fprintf (stderr, "dup read() returned %zd, EOF\n", n));
				break;
			}
			d(fprintf (stderr, "read %zd bytes from dup stream\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: `%s' appears to be truncated, short %zu+ bytes\n",
				 filename, size - nread);
			goto fail;
		}
		
		if (memcmp (buf, dbuf, size) != 0) {
			sprintf (errstr, "Error: `%s': content does not match\n", filename);
			goto fail;
		} else {
			d(fprintf (stderr, "%zu bytes identical\n", size));
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
	v(fputs (errstr, stderr));
	
	return -1;
}

static void
test_cat_write (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *sub, *cat;
	Exception *ex;
	int fd;
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1) {
			ex = exception_new ("could not create `%s': %s", part->filename, g_strerror (errno));
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, part->pend);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	g_mime_stream_reset (whole);
	if (g_mime_stream_write_to_stream (whole, (GMimeStream *) cat) == -1) {
		ex = exception_new ("%s", g_strerror (errno));
		g_object_unref (cat);
		throw (ex);
	}
	
	g_object_unref (cat);
	
	/* now lets check that the content matches */
	d(fprintf (stderr, "checking all part streams have correct data...\n"));
	part = parts;
	while (part != NULL) {
		d(fprintf (stderr, "checking substream %s\n", part->filename));
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			throw (ex);
		}
		
		if (!(sub = g_mime_stream_substream (whole, part->wstart, part->wend))) {
			ex = exception_new ("could not substream original stream");
			close (fd);
			throw (ex);
		}
		
		if (!(stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, -1))) {
			ex = exception_new ("could not instantiate stream for `%s'", part->filename);
			close (fd);
			throw (ex);
		}
		
		d(fprintf (stderr, "checking substream %s matches...\n", part->filename));
		if (check_streams_match (sub, stream, part->filename, TRUE) == -1) {
			ex = exception_new ("streams did not match");
			g_object_unref (stream);
			g_object_unref (sub);
			throw (ex);
		}
		
		g_object_unref (stream);
		g_object_unref (sub);
		
		part = part->next;
	}
}

static void
test_cat_read (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *cat;
	Exception *ex;
	int fd;
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			g_object_unref (cat);
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	g_mime_stream_reset (whole);
	if (check_streams_match (whole, cat, "stream.part*", TRUE) == -1) {
		ex = exception_new ("streams do not match");
		g_object_unref (cat);
		throw (ex);
	}
}

static void
test_cat_seek (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	struct _StreamPart *part = parts;
	GMimeStream *stream, *cat;
	gint64 offset, len;
	Exception *ex;
	int fd;
	
	if (whole->bound_end != -1) {
		len = whole->bound_end - whole->bound_start;
	} else if ((len = g_mime_stream_length (whole)) == -1) {
		ex = exception_new ("unable to get original stream length");
		throw (ex);
	}
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			g_object_unref (cat);
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	/* calculate a random seek offset to compare at */
	offset = (gint64) (len * randf ());
	
	if (g_mime_stream_seek (whole, offset, GMIME_STREAM_SEEK_SET) == -1) {
		ex = exception_new ("could not seek to %lld in original stream: %s",
				    (long long) offset, g_strerror (errno));
		throw (ex);
	}
	
	if (g_mime_stream_seek (cat, offset, GMIME_STREAM_SEEK_SET) == -1) {
		ex = exception_new ("could not seek to %lld: %s",
				    (long long) offset, g_strerror (errno));
		throw (ex);
	}
	
	if (check_streams_match (whole, cat, "stream.part*", TRUE) == -1) {
		ex = exception_new ("streams did not match");
		g_object_unref (cat);
		throw (ex);
	}
}

static void
test_cat_substream (GMimeStream *whole, struct _StreamPart *parts, int bounded)
{
	GMimeStream *stream, *cat, *sub1, *sub2;
	struct _StreamPart *part = parts;
	gint64 start, end, len;
	Exception *ex;
	int fd;
	
	if (whole->bound_end != -1) {
		len = whole->bound_end - whole->bound_start;
	} else if ((len = g_mime_stream_length (whole)) == -1) {
		ex = exception_new ("unable to get original stream length");
		throw (ex);
	}
	
	cat = g_mime_stream_cat_new ();
	
	while (part != NULL) {
		d(fprintf (stderr, "adding %s start=%lld, end=%lld...\n",
			   part->filename, part->pstart, part->pend));
		
		if ((fd = open (part->filename, O_RDONLY, 0)) == -1) {
			ex = exception_new ("could not open `%s': %s", part->filename, g_strerror (errno));
			g_object_unref (cat);
			throw (ex);
		}
		
		stream = g_mime_stream_fs_new_with_bounds (fd, part->pstart, bounded ? part->pend : -1);
		g_mime_stream_cat_add_source ((GMimeStreamCat *) cat, stream);
		g_object_unref (stream);
		
		part = part->next;
	}
	
	/* calculate a random start/end offsets */
	start = (gint64) (len * randf ());
	if (randc () % 2)
		end = start + (gint64) ((len - start) * randf ());
	else
		end = -1;
	
	if (!(sub1 = g_mime_stream_substream (whole, start, end))) {
		ex = exception_new ("could not substream the original stream: %s",
				    g_strerror (errno));
		g_object_unref (cat);
		throw (ex);
	}
	
	if (!(sub2 = g_mime_stream_substream (cat, start, end))) {
		ex = exception_new ("%s", g_strerror (errno));
		g_object_unref (sub1);
		g_object_unref (cat);
		throw (ex);
	}
	
	g_object_unref (cat);
	
	if (check_streams_match (sub1, sub2, "stream.part*", TRUE) == -1) {
		ex = exception_new ("streams did not match");
		g_object_unref (sub1);
		g_object_unref (sub2);
		throw (ex);
	}
	
	g_object_unref (sub1);
	g_object_unref (sub2);
}


typedef void (* checkFunc) (GMimeStream *stream, struct _StreamPart *parts, int bounded);

struct {
	const char *what;
	checkFunc check;
	int bounded;
} checks[] = {
	{ "GMimeStreamCat::write()",            test_cat_write,     FALSE },
	{ "GMimeStreamCat::read(bound)",        test_cat_read,      TRUE  },
	{ "GMimeStreamCat::read(unbound)",      test_cat_read,      FALSE },
	{ "GMimeStreamCat::seek(bound)",        test_cat_seek,      TRUE  },
	{ "GMimeStreamCat::seek(unbound)",      test_cat_seek,      FALSE },
	{ "GMimeStreamCat::substream(bound)",   test_cat_substream, TRUE  },
	{ "GMimeStreamCat::substream(unbound)", test_cat_substream, FALSE },
};

int main (int argc, char **argv)
{
	const char *datadir = "data/cat";
	struct _StreamPart *list, *tail, *n;
	gboolean failed = FALSE;
	gint64 wholelen, left;
	GMimeStream *whole;
	guint32 part = 0;
	gint64 start = 0;
	char *filename;
	struct stat st;
	gint64 len;
	int fd, i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	if ((randfd = open ("/dev/urandom", O_RDONLY)) == -1)
		return EXIT_FAILURE;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc) {
		if (stat (datadir, &st) == -1) {
			if (errno == ENOENT) {
				g_mkdir_with_parents (datadir, 0755);
				if (stat (datadir, &st) == -1) {
					close (randfd);
					
					return EXIT_FAILURE;
				}
			} else {
				close (randfd);
				
				return EXIT_FAILURE;
			}
		}
		
		if (S_ISREG (st.st_mode)) {
			/* test a particular input file */
			if ((fd = open (argv[i], O_RDONLY, 0)) == -1) {
				close (randfd);
				
				return EXIT_FAILURE;
			}
			
			filename = g_strdup (argv[i]);
			whole = g_mime_stream_fs_new (fd);
		} else if (S_ISDIR (st.st_mode)) {
			/* use path as test suite data dir */
			whole = random_whole_stream (argv[i], &filename);
		} else {
			close (randfd);
			
			return EXIT_FAILURE;
		}
	} else {
		whole = random_whole_stream (datadir, &filename);
	}
	
	if ((wholelen = g_mime_stream_length (whole)) == -1) {
		fprintf (stderr, "Error: length of test stream unknown\n");
		g_object_unref (whole);
		close (randfd);
		
		return EXIT_FAILURE;
	} else if (wholelen == 64) {
		fprintf (stderr, "Error: length of test stream is unsuitable for testing\n");
		g_object_unref (whole);
		close (randfd);
		
		return EXIT_FAILURE;
	}
	
	list = NULL;
	tail = (struct _StreamPart *) &list;
	
	left = wholelen;
	while (left > 0) {
		len = 1 + (gint64) (left * randf ());
		n = g_new (struct _StreamPart, 1);
		sprintf (n->filename, "%s.%u", filename, part++);
		n->pstart = (gint64) 0; /* FIXME: we could make this a random offset */
		n->pend = n->pstart + len;
		n->wend = start + len;
		n->wstart = start;
		tail->next = n;
		tail = n;
		
		start += len;
		left -= len;
	}
	
	tail->next = NULL;
	
	testsuite_start ("GMimeStreamCat");
	
	for (i = 0; i < G_N_ELEMENTS (checks) && !failed; i++) {
		testsuite_check ("%s", checks[i].what);
		try {
			checks[i].check (whole, list, checks[i].bounded);
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s failed: %s", checks[i].what,
						ex->message);
			failed = TRUE;
		} finally;
	}
	
	testsuite_end ();
	
	while (list != NULL) {
		n = list->next;
		if (!failed)
			unlink (list->filename);
		g_free (list);
		list = n;
	}
	
	g_object_unref (whole);
	
	if (!failed)
		unlink (filename);
	
	g_free (filename);
	close (randfd);
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
