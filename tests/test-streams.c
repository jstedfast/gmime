/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x) 
#define v(x) if (verbose > 3) x;


static float
randf (int randfd)
{
	unsigned int value = 0;
	size_t nread = 0;
	ssize_t n;
	
	do {
		if ((n = read (randfd, ((char *) &value) + nread, sizeof (value) - nread)) > 0) {
			if ((nread += n) == sizeof (value))
				break;
		}
	} while (n == -1 && errno == EINTR);
	
	return (value * 1.0) / UINT_MAX;
}

static gboolean
streams_match (GMimeStream **streams, const char *filename)
{
	char buf[4096], dbuf[4096], errstr[1024];
	gint64 len, totalsize, totalread = 0;
	size_t nread, size;
	ssize_t n;
	
	v(fprintf (stdout, "Matching original stream (%" G_GINT64_FORMAT " -> %" G_GINT64_FORMAT ") with %s (%" G_GINT64_FORMAT ", %" G_GINT64_FORMAT ")... ",
		   streams[0]->position, streams[0]->bound_end, filename,
		   streams[1]->position, streams[1]->bound_end));
	
	if (streams[0]->bound_end != -1) {
		totalsize = streams[0]->bound_end - streams[0]->position;
	} else if ((len = g_mime_stream_length (streams[0])) == -1) {
		sprintf (errstr, "Error: Unable to get length of original stream: %s\n", g_strerror (errno));
		goto fail;
	} else if (len < (streams[0]->position - streams[0]->bound_start)) {
		sprintf (errstr, "Error: Overflow on original stream?\n");
		goto fail;
	} else {
		totalsize = len - (streams[0]->position - streams[0]->bound_start);
	}
	
	while (totalread <= totalsize) {
		if ((n = g_mime_stream_read (streams[0], buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %" G_GSIZE_FORMAT " bytes from stream[0]\n", size));
		
		do {
			if ((n = g_mime_stream_read (streams[1], dbuf + nread, size - nread)) <= 0) {
				d(fprintf (stderr, "stream[1] read() returned %" G_GSSIZE_FORMAT ", EOF\n", n));
				break;
			}
			d(fprintf (stderr, "read %" G_GSSIZE_FORMAT " bytes from stream[1]\n", n));
			nread += n;
		} while (nread < size);
		
		if (nread < size) {
			sprintf (errstr, "Error: `%s' appears to be truncated, short %" G_GSIZE_FORMAT "+ bytes\n",
				 filename, size - nread);
			goto fail;
		}
		
		if (memcmp (buf, dbuf, size) != 0) {
			sprintf (errstr, "Error: `%s': content does not match\n", filename);
			goto fail;
		} else {
			d(fprintf (stderr, "%" G_GSIZE_FORMAT " bytes identical\n", size));
		}
	}
	
	if (totalread < totalsize) {
		sprintf (errstr, "Error: expected more data from stream[0]\n");
		goto fail;
	}
	
	if ((n = g_mime_stream_read (streams[1], buf, sizeof (buf))) > 0) {
		sprintf (errstr, "Error: `%s' appears to contain extra content\n", filename);
		goto fail;
	}
	
	v(fputs ("passed\n", stdout));
	
	return TRUE;
	
 fail:
	
	v(fputs ("failed\n", stdout));
	v(fputs (errstr, stderr));
	
	return FALSE;
}

static void
test_stream_gets (GMimeStream *stream, const char *filename)
{
	char sbuf[100], rbuf[100];
	FILE *fp;
	
	/* '0x1a' character is treated as EOF (Ctrl+Z) on Windows if file is opened in text mode,
	 *  thus it's opened in binary mode.
	 */
	if (!(fp = fopen (filename, "r+b")))
		throw (exception_new ("could not open `%s': %s", filename, g_strerror (errno)));
	
	while (!g_mime_stream_eos (stream)) {
		rbuf[0] = '\0';
		g_mime_stream_buffer_gets (stream, sbuf, sizeof (sbuf));
		if (!fgets (rbuf, sizeof (rbuf), fp))
			break;
		
		if (strcmp (sbuf, rbuf) != 0)
			break;
	}
	
	fclose (fp);
	
	if (strcmp (sbuf, rbuf) != 0) {
		v(fprintf (stderr, "\tstream: \"%s\" (%" G_GSIZE_FORMAT ")\n", sbuf, strlen (sbuf)));
		v(fprintf (stderr, "\treal:   \"%s\" (%" G_GSIZE_FORMAT ")\n", rbuf, strlen (rbuf)));
		throw (exception_new ("streams did not match"));
	}
}

static void
test_stream_buffer_gets (const char *filename)
{
	GMimeStream *stream, *buffered = NULL;
	int fd;
	
	if ((fd = open (filename, O_RDONLY, 0)) == -1) {
		v(fprintf (stderr, "failed to open %s", filename));
		return;
	}
	
	stream = g_mime_stream_fs_new (fd);
	
	testsuite_check ("GMimeStreamBuffer::gets()");
	try {
		g_mime_stream_reset (stream);
		buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_BLOCK_READ);
		test_stream_gets (buffered, filename);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimeStreamBuffer::gets() failed: %s",
					ex->message);
	} finally {
		g_object_unref (buffered);
	}
	
	g_object_unref (stream);
}


#if 0
static void
test_stream_mem (const char *filename)
{
	/* Note: this also tests g_mime_stream_write_to_stream */
	GMimeStream *stream, *fstream;
	int fd;
	
	if ((fd = open (filename, O_RDONLY, 0)) == -1) {
		v(fprintf (stderr, "failed to open %s: %s\n", filename, g_strerror (errno)));
		return;
	}
	
	testsuite_start ("GMimeStreamMem");
	
	fstream = g_mime_stream_fs_new (fd);
	stream = g_mime_stream_mem_new ();
	
	testsuite_check ("GMimeStreamMem::read()");
	try {
		if (g_mime_stream_write_to_stream (fstream, stream) == -1)
			throw (exception_new ("g_mime_stream_write_to_stream() failed"));
		
		if (g_mime_stream_length (stream) != g_mime_stream_length (fstream))
			throw (exception_new ("stream lengths didn't match"));
		
		test_stream_read (stream, filename);
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimeStreamMem::read() failed: %s",
					ex->message);
	} finally;
	
	g_object_unref (fstream);
	g_object_unref (stream);
	
	testsuite_end ();
}
#endif


static gboolean
check_stream_fs (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	int fd[2];
	
	if ((fd[0] = open (input, O_RDONLY, 0)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY, 0)) == -1) {
		close (fd[0]);
		return FALSE;
	}
	
	stream = g_mime_stream_fs_new (fd[0]);
	if (!g_mime_stream_fs_get_owner ((GMimeStreamFs *) stream)) {
		ex = exception_new ("GMimeStreamFs is not the owner `%s'", filename);
		g_object_unref (stream);
		close (fd[1]);
		throw (ex);
	}
	
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	if (g_mime_stream_fs_get_owner ((GMimeStreamFs *) streams[0])) {
		ex = exception_new ("GMimeStreamFs substream should not be the owner `%s'", filename);
		g_object_unref (stream);
		close (fd[1]);
		throw (ex);
	}
	
	streams[1] = g_mime_stream_pipe_new (fd[1]);
	
	if (!streams_match (streams, filename)) {
		ex = exception_new ("GMimeStreamFs streams did not match for `%s'", filename);
		goto cleanup;
	}
	
	if (!g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamFs is not at the end-of-stream `%s'", filename);
		goto cleanup;
	}
	
	g_mime_stream_reset (streams[0]);
	if (g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamFs did not properly reset `%s'", filename);
		goto cleanup;
	}
	
cleanup:
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}

static gboolean
check_stream_file (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	FILE *fp[2];
	
	if (!(fp[0] = fopen (input, "r")))
		return FALSE;
	
	if (!(fp[1] = fopen (output, "r"))) {
		fclose (fp[0]);
		return FALSE;
	}
	
	stream = g_mime_stream_file_new (fp[0]);
	if (!g_mime_stream_file_get_owner ((GMimeStreamFile *) stream)) {
		ex = exception_new ("GMimeStreamFile is not the owner `%s'", filename);
		g_object_unref (stream);
		fclose (fp[1]);
		throw (ex);
	}
	
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	if (g_mime_stream_file_get_owner ((GMimeStreamFile *) streams[0])) {
		ex = exception_new ("GMimeStreamFile substream should not be the owner `%s'", filename);
		g_object_unref (stream);
		fclose (fp[1]);
		throw (ex);
	}
	
	streams[1] = g_mime_stream_file_new (fp[1]);
	
	if (!streams_match (streams, filename)) {
		ex = exception_new ("GMimeStreamFile streams did not match for `%s'", filename);
		goto cleanup;
	}
	
	if (!g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamFile is not at the end-of-stream `%s'", filename);
		goto cleanup;
	}
	
	g_mime_stream_reset (streams[0]);
	if (g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamFile did not properly reset `%s'", filename);
		goto cleanup;
	}
	
cleanup:
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}

#ifdef HAVE_MMAP
static gboolean
check_stream_mmap (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	int fd[2];
	
	if ((fd[0] = open (input, O_RDONLY, 0)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY, 0)) == -1) {
		close (fd[0]);
		return FALSE;
	}
	
	stream = g_mime_stream_mmap_new (fd[0], PROT_READ, MAP_PRIVATE);
	if (!g_mime_stream_mmap_get_owner ((GMimeStreamMmap *) stream)) {
		ex = exception_new ("GMimeStreamMmap is not the owner `%s'", filename);
		g_object_unref (stream);
		close (fd[1]);
		throw (ex);
	}
	
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	if (g_mime_stream_mmap_get_owner ((GMimeStreamMmap *) streams[0])) {
		ex = exception_new ("GMimeStreamMmap substream should not be the owner `%s'", filename);
		g_object_unref (stream);
		close (fd[1]);
		throw (ex);
	}
	
	streams[1] = g_mime_stream_mmap_new (fd[1], PROT_READ, MAP_PRIVATE);
	
	if (!streams_match (streams, filename)) {
		ex = exception_new ("GMimeStreamMmap streams did not match for `%s'", filename);
		goto cleanup;
	}
	
	if (!g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamMmap is not at the end-of-stream `%s'", filename);
		goto cleanup;
	}
	
	g_mime_stream_reset (streams[0]);
	if (g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamMmap did not properly reset `%s'", filename);
		goto cleanup;
	}
	
cleanup:
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}
#endif /* HAVE_MMAP */

static gboolean
check_stream_buffer (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	int fd[2];
	
	if ((fd[0] = open (input, O_RDONLY, 0)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY, 0)) == -1) {
		close (fd[0]);
		return FALSE;
	}
	
	streams[0] = g_mime_stream_fs_new (fd[0]);
	stream = g_mime_stream_buffer_new (streams[0], GMIME_STREAM_BUFFER_BLOCK_READ);
	g_object_unref (streams[0]);
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	
	streams[1] = g_mime_stream_fs_new (fd[1]);
	
	if (!streams_match (streams, filename))
		ex = exception_new ("GMimeStreamBuffer (Block Mode) streams did not match for `%s'", filename);
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}

static gboolean
check_stream_gio (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2];
	Exception *ex = NULL;
	GMimeStream *stream;
	GFile *file;
	int fd;
	
	if (!(file = g_file_new_for_path (input)))
		return FALSE;
	
	if ((fd = open (output, O_RDONLY, 0)) == -1) {
		g_object_unref (file);
		return FALSE;
	}
	
	stream = g_mime_stream_gio_new (file);
	if (!g_mime_stream_gio_get_owner ((GMimeStreamGIO *) stream)) {
		ex = exception_new ("GMimeStreamGIO is not the owner `%s'", filename);
		g_object_unref (stream);
		close (fd);
		throw (ex);
	}
	
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	if (g_mime_stream_gio_get_owner ((GMimeStreamGIO *) streams[0])) {
		ex = exception_new ("GMimeStreamGIO substream should not be the owner `%s'", filename);
		g_object_unref (stream);
		close (fd);
		throw (ex);
	}
	
	streams[1] = g_mime_stream_fs_new (fd);
	
	if (!streams_match (streams, filename)) {
		ex = exception_new ("GMimeStreamGIO streams did not match for `%s'", filename);
		goto cleanup;
	}
	
	if (!g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamGIO is not at the end-of-stream `%s'", filename);
		goto cleanup;
	}
	
	g_mime_stream_reset (streams[0]);
	if (g_mime_stream_eos (streams[0])) {
		ex = exception_new ("GMimeStreamGIO did not properly reset `%s'", filename);
		goto cleanup;
	}
	
cleanup:
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}


typedef gboolean (* checkFunc) (const char *, const char *, const char *, gint64, gint64);

static struct {
	const char *what;
	checkFunc check;
} checks[] = {
	{ "GMimeStreamFs",     check_stream_fs     },
	{ "GMimeStreamFile",   check_stream_file   },
#ifdef HAVE_MMAP
	{ "GMimeStreamMmap",   check_stream_mmap   },
#endif /* HAVE_MMAP */
	{ "GMimeStreamBuffer", check_stream_buffer },
	{ "GMimeStreamGIO",    check_stream_gio    },
};

static void
test_streams (GDir *dir, const char *datadir, const char *filename)
{
	char inpath[256], outpath[256], *p, *q, *o;
	const char *dent, *d;
	gint64 start, end;
	size_t n;
	guint i;
	
	p = g_stpcpy (inpath, datadir);
	*p++ = G_DIR_SEPARATOR;
	p = g_stpcpy (p, "input");
	*p++ = G_DIR_SEPARATOR;
	strcpy (p, filename);
	
	q = g_stpcpy (outpath, datadir);
	*q++ = G_DIR_SEPARATOR;
	q = g_stpcpy (q, "output");
	*q++ = G_DIR_SEPARATOR;
	*q = '\0';
	
	n = strlen (filename);
	
	while ((dent = g_dir_read_name (dir))) {
		if (strncmp (dent, filename, n) != 0 || dent[n] != '_')
			continue;
		
		d = dent + n + 1;
		if ((start = strtol (d, &o, 10)) < 0 || *o != ',')
			continue;
		
		d = o + 1;
		
		if ((((end = strtol (d, &o, 10)) < start) && end != -1) || *o != '\0')
			continue;
		
		strcpy (q, dent);
		
		for (i = 0; i < G_N_ELEMENTS (checks); i++) {
			testsuite_check ("%s on `%s'", checks[i].what, dent);
			try {
				if (!checks[i].check (inpath, outpath, dent, start, end)) {
					testsuite_check_warn ("%s could not open `%s'",
							      checks[i].what, dent);
				} else {
					testsuite_check_passed ();
				}
			} catch (ex) {
				testsuite_check_failed ("%s on `%s' failed: %s", checks[i].what,
							dent, ex->message);
			} finally;
		}
	}
	
	g_dir_rewind (dir);
}


static size_t
gen_random_stream (int randfd, GMimeStream *stream)
{
	size_t nwritten, buflen, total = 0, size;
	char buf[4096];
	ssize_t n;
	
	/* read between 4k and 14k bytes */
	size = 4096 + (size_t) (10240 * randf (randfd));
	v(fprintf (stdout, "Generating %" G_GSIZE_FORMAT " bytes of random data... ", size));
	v(fflush (stdout));
	
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
	
	return size;
}

static int
gen_test_data (const char *datadir, char **stream_name)
{
	GMimeStream *istream, *ostream, *stream;
	char input[256], output[256], *name, *p;
	gint64 start, end, len, size;
	int randfd, fd, i;
	
	if ((randfd = open ("/dev/urandom", O_RDONLY)) == -1)
		return -1;
	
	name = g_stpcpy (input, datadir);
	*name++ = G_DIR_SEPARATOR;
	name = g_stpcpy (name, "input");
	
	p = g_stpcpy (output, datadir);
	*p++ = G_DIR_SEPARATOR;
	p = g_stpcpy (p, "output");
	
	g_mkdir_with_parents (input, 0755);
	g_mkdir_with_parents (output, 0755);
	
	*name++ = G_DIR_SEPARATOR;
	strcpy (name, "streamXXXXXX");
	
	if ((fd = g_mkstemp (input)) == -1) {
		close (randfd);
		return -1;
	}
	
	*stream_name = g_strdup (name);
	
	*p++ = G_DIR_SEPARATOR;
	p = g_stpcpy (p, name);
	*p++ = '_';
	
	istream = g_mime_stream_fs_new (fd);
	size = gen_random_stream (randfd, istream);
	
	for (i = 0; i < 64; i++) {
	retry:
		start = (gint64) (size * randf (randfd));
		len = (gint64) (size * randf (randfd));
		if (start + len > size) {
			end = -1;
		} else {
			end = start + len;
		}
		
		sprintf (p, "%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT, start, end);
		
		if ((fd = open (output, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, 0666)) == -1)
			goto retry;
		
		ostream = g_mime_stream_fs_new (fd);
		stream = g_mime_stream_substream (istream, start, end);
		g_mime_stream_write_to_stream (stream, ostream);
		g_mime_stream_flush (ostream);
		g_object_unref (ostream);
		g_object_unref (stream);
	}
	
	close (randfd);
	
	return 0;
}

static void
test_null_measure_write (void)
{
	GMimeStream *stream = g_mime_stream_null_new ();
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);
	char buf[4];

	testsuite_check ("Measuring writes retain a high-water length and cumulative statistics");
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_assert_false (g_mime_stream_null_get_count_newlines (null));
	g_mime_stream_null_set_count_newlines (null, TRUE);
	g_assert_cmpint (g_mime_stream_write (stream, "a\nbc", 4), ==, 4);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 4);
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_cmpint (g_mime_stream_write (stream, "Z\n", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 3);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 4);
	g_assert_cmpuint (null->written, ==, 6);
	g_assert_cmpuint (null->newlines, ==, 2);
	g_assert_cmpint (g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_END), ==, 4);
	g_assert_cmpint (g_mime_stream_write (stream, "\0X", 2), ==, 2);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 6);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 6);
	g_assert_cmpuint (null->written, ==, 8);
	g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);

	memset (buf, 0x5a, sizeof (buf));
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (stream, buf, sizeof (buf)), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpmem (buf, sizeof (buf), "ZZZZ", 4);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 6);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 6);
	g_assert_cmpint (g_mime_stream_read (stream, buf, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_write (stream, "", 0), ==, 0);
	g_mime_stream_null_set_count_newlines (null, FALSE);
	g_assert_cmpint (g_mime_stream_write (stream, "\n", 1), ==, 1);
	g_assert_cmpuint (null->newlines, ==, 2);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 7);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_null_measure_seek (void)
{
	GMimeStream *stream = g_mime_stream_null_new ();
	guint32 random = 1024;
	gint64 offset, position;
	guint i;

	testsuite_check ("Measuring seeks validate all origins without changing length or statistics");
	g_assert_cmpint (g_mime_stream_null_set_length (GMIME_STREAM_NULL (stream), 1024), ==, 0);
	for (i = 0; i < 128; i++) {
		random = random * 1664525 + 1013904223;
		position = random % 1025;
		g_assert_cmpint (g_mime_stream_seek (stream, position, GMIME_STREAM_SEEK_SET), ==, position);
		offset = -(position / 2);
		position += offset;
		g_assert_cmpint (g_mime_stream_seek (stream, offset, GMIME_STREAM_SEEK_CUR), ==, position);
		offset = (1024 - position) / 2;
		position += offset;
		g_assert_cmpint (g_mime_stream_seek (stream, offset, GMIME_STREAM_SEEK_CUR), ==, position);
		g_assert_cmpint (g_mime_stream_seek (stream, position - 1024, GMIME_STREAM_SEEK_END), ==, position);
		g_assert_cmpint (g_mime_stream_length (stream), ==, 1024);
	}
	position = g_mime_stream_tell (stream);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, -1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, 1025, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, -1025, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, -1025, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, 0, (GMimeSeekWhence) 99), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, position);
	g_assert_cmpuint (GMIME_STREAM_NULL (stream)->written, ==, 0);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_null_measure_length (void)
{
	GMimeStream *stream = g_mime_stream_null_new ();
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);

	testsuite_check ("Setting measured length clamps position without recording output");
	g_mime_stream_null_set_count_newlines (null, TRUE);
	g_assert_cmpint (g_mime_stream_write (stream, "1234\n", 5), ==, 5);
	g_assert_cmpint (g_mime_stream_seek (stream, 2, GMIME_STREAM_SEEK_SET), ==, 2);
	g_assert_cmpint (g_mime_stream_null_set_length (null, 10), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 2);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 10);
	g_assert_cmpint (g_mime_stream_seek (stream, 9, GMIME_STREAM_SEEK_SET), ==, 9);
	g_assert_cmpint (g_mime_stream_null_set_length (null, 3), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 3);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 3);
	g_assert_cmpuint (null->written, ==, 5);
	g_assert_cmpuint (null->newlines, ==, 1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_null_set_length (null, -1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 3);
	g_assert_cmpint (g_mime_stream_null_set_length (null, 0), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
	g_assert_cmpuint (null->written, ==, 5);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_assert_cmpuint (null->written, ==, 0);
	g_assert_cmpuint (null->newlines, ==, 0);
	g_assert_true (g_mime_stream_null_get_count_newlines (null));
	/* Native close remains a no-op rather than managed disposal. */
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_write (stream, "\n", 1), ==, 1);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 1);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_null_measure_bounds (void)
{
	GMimeStream *stream = g_mime_stream_null_new (), *sub;
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);

	testsuite_check ("Measuring streams retain native bounds and independent substream statistics");
	g_mime_stream_construct (stream, 10, 14);
	g_mime_stream_null_set_count_newlines (null, TRUE);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 4);
	sub = g_mime_stream_substream (stream, 11, -1);
	g_assert_nonnull (sub);
	g_assert_cmpint (g_mime_stream_length (sub), ==, 3);
	g_object_unref (sub);
	g_assert_cmpint (g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET), ==, 10);
	g_assert_cmpint (g_mime_stream_write (stream, "a\nb\n\n", 5), ==, 4);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 14);
	g_assert_cmpuint (null->written, ==, 4);
	g_assert_cmpuint (null->newlines, ==, 2);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, -1, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_null_set_length (null, 2), ==, 0);
	g_assert_cmpint (stream->bound_end, ==, 12);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 12);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 2);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 10);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 2);
	g_assert_cmpuint (null->written, ==, 0);
	g_assert_cmpint (g_mime_stream_null_set_length (null, 6), ==, 0);
	g_assert_cmpint (stream->bound_end, ==, 16);
	sub = g_mime_stream_substream (stream, 11, -1);
	g_assert_nonnull (sub);
	g_assert_cmpint (g_mime_stream_length (sub), ==, 5);
	g_assert_cmpint (g_mime_stream_write (sub, "abc", 3), ==, 3);
	g_assert_cmpuint (GMIME_STREAM_NULL (sub)->written, ==, 3);
	g_assert_cmpuint (null->written, ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 10);
	g_object_unref (sub);
	sub = g_mime_stream_substream (stream, 11, 13);
	g_assert_nonnull (sub);
	g_assert_cmpint (g_mime_stream_length (sub), ==, 2);
	g_assert_cmpint (g_mime_stream_write (sub, "abcd", 4), ==, 2);
	g_object_unref (sub);
	g_object_unref (stream);

	stream = g_mime_stream_null_new ();
	g_mime_stream_construct (stream, 10, -1);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET), ==, 10);
	g_assert_cmpint (g_mime_stream_write (stream, "abcd", 4), ==, 4);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 4);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 10);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_object_unref (stream);
	testsuite_check_passed ();
}

static void
test_null_measure_integration (void)
{
	const char input[] = "a\nb\0cdefghijklmnopqrstuvwxyz0123456789";
	GMimeStream *outputs[2], *filtered, *cat, *tail;
	GMimeFilter *filter;
	GByteArray *bytes;
	guint i, newlines = 0;
	char buf[4];

	testsuite_check ("Measuring sinks match encoded output and support chained write extents");
	outputs[0] = g_mime_stream_null_new ();
	outputs[1] = g_mime_stream_mem_new ();
	g_mime_stream_null_set_count_newlines (GMIME_STREAM_NULL (outputs[0]), TRUE);
	for (i = 0; i < G_N_ELEMENTS (outputs); i++) {
		filtered = g_mime_stream_filter_new (outputs[i]);
		filter = g_mime_filter_basic_new (GMIME_CONTENT_ENCODING_BASE64, TRUE);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered), filter);
		g_object_unref (filter);
		g_assert_cmpint (g_mime_stream_write (filtered, input, sizeof (input) - 1), ==, sizeof (input) - 1);
		/* Include encoder completion output in the measured size. */
		g_assert_cmpint (g_mime_stream_flush (filtered), ==, 0);
		g_object_unref (filtered);
	}
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (outputs[1]));
	for (i = 0; i < bytes->len; i++)
		newlines += bytes->data[i] == '\n';
	g_assert_cmpint (g_mime_stream_length (outputs[0]), ==, bytes->len);
	g_assert_cmpuint (GMIME_STREAM_NULL (outputs[0])->written, ==, bytes->len);
	g_assert_cmpuint (GMIME_STREAM_NULL (outputs[0])->newlines, ==, newlines);
	g_object_unref (outputs[1]);

	g_assert_cmpint (g_mime_stream_reset (outputs[0]), ==, 0);
	g_assert_cmpint (g_mime_stream_null_set_length (GMIME_STREAM_NULL (outputs[0]), 2), ==, 0);
	tail = g_mime_stream_null_new ();
	cat = g_mime_stream_cat_new ();
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), outputs[0]);
	g_mime_stream_cat_add_source (GMIME_STREAM_CAT (cat), tail);
	g_assert_cmpint (g_mime_stream_write (cat, "abcde", 5), ==, 5);
	g_assert_cmpint (g_mime_stream_length (outputs[0]), ==, 2);
	g_assert_cmpint (g_mime_stream_length (tail), ==, 3);
	g_assert_cmpuint (GMIME_STREAM_NULL (outputs[0])->written, ==, 2);
	g_assert_cmpuint (GMIME_STREAM_NULL (tail)->written, ==, 3);
	g_assert_cmpint (g_mime_stream_flush (cat), ==, 0);
	errno = 0;
	g_assert_cmpint (g_mime_stream_read (cat, buf, sizeof (buf)), ==, -1);
	g_assert_cmpint (errno, ==, EBADF);
	g_assert_cmpint (g_mime_stream_tell (cat), ==, 5);
	g_object_unref (cat);
	g_object_unref (tail);
	g_object_unref (outputs[0]);
	testsuite_check_passed ();
}

static void
test_null_measure_overflow (void)
{
	GMimeStream *stream = g_mime_stream_null_new ();
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);

	testsuite_check ("Measuring position and counter overflow fail without changing state");
	g_assert_cmpint (g_mime_stream_null_set_length (null, G_MAXINT64), ==, 0);
	g_assert_cmpint (g_mime_stream_seek (stream, -1, GMIME_STREAM_SEEK_END), ==, G_MAXINT64 - 1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "xy", 2), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, G_MAXINT64 - 1);
	g_assert_cmpuint (null->written, ==, 0);
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, 1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "\n", 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, G_MAXINT64);
	g_assert_cmpint (g_mime_stream_length (stream), ==, G_MAXINT64);
	g_assert_cmpuint (null->written, ==, 1);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_CUR), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, G_MININT64, GMIME_STREAM_SEEK_END), ==, -1);
	g_assert_cmpint (errno, ==, EINVAL);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	null->written = G_MAXSIZE;
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "x", 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	g_assert_cmpuint (null->written, ==, G_MAXSIZE);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "", (size_t) G_MAXSSIZE + 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 0);
	null->newlines = G_MAXSIZE;
	g_mime_stream_null_set_count_newlines (null, TRUE);
	errno = 0;
	g_assert_cmpint (g_mime_stream_write (stream, "\n", 1), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpuint (null->written, ==, 0);
	g_assert_cmpuint (null->newlines, ==, G_MAXSIZE);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 0);
	g_mime_stream_construct (stream, G_MAXINT64 - 1, -1);
	g_assert_cmpint (g_mime_stream_null_set_length (null, 1), ==, 0);
	errno = 0;
	g_assert_cmpint (g_mime_stream_null_set_length (null, 2), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 1);
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_SET), ==, G_MAXINT64);
	errno = 0;
	g_assert_cmpint (g_mime_stream_seek (stream, 2, GMIME_STREAM_SEEK_SET), ==, -1);
	g_assert_cmpint (errno, ==, EOVERFLOW);
	g_object_unref (stream);
	testsuite_check_passed ();
}

int main (int argc, char **argv)
{
	const char *datadir = "data/streams";
	gboolean gen_data = TRUE;
	char *stream_name = NULL;
	char path[256], *p;
	GDir *dir, *outdir;
	const char *dent;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);

	testsuite_start ("MeasuringStream port");
	test_null_measure_write ();
	test_null_measure_seek ();
	test_null_measure_length ();
	test_null_measure_bounds ();
	test_null_measure_integration ();
	test_null_measure_overflow ();
	testsuite_end ();
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	testsuite_start ("Stream tests");
	
	p = g_stpcpy (path, datadir);
	*p++ = G_DIR_SEPARATOR;
	strcpy (p, "output");
	
	if (!(outdir = g_dir_open (path, 0, NULL))) {
		if (gen_test_data (datadir, &stream_name) == -1 ||
		    !(outdir = g_dir_open (path, 0, NULL)))
			goto exit;
		
		gen_data = FALSE;
	}
	
	p = g_stpcpy (p, "input");
	
	if (!(dir = g_dir_open (path, 0, NULL))) {
		if (!gen_data || gen_test_data (datadir, &stream_name) == -1 ||
		    !(dir = g_dir_open (path, 0, NULL))) {
			g_dir_close (outdir);
			goto exit;
		}
		
		gen_data = FALSE;
	}
	
	if (gen_data) {
		while ((dent = g_dir_read_name (dir))) {
			if (dent[0] == '.' || !strcmp (dent, "README"))
				continue;
			
			gen_data = FALSE;
			break;
		}
		
		g_dir_rewind (dir);
		
		if (gen_data && gen_test_data (datadir, &stream_name) == -1)
			goto exit;
	}
	
	*p++ = G_DIR_SEPARATOR;
	*p = '\0';
	
	while ((dent = g_dir_read_name (dir))) {
		if (dent[0] == '.' || !strcmp (dent, "README"))
			continue;
		
		test_streams (outdir, datadir, dent);
		
		strcpy (p, dent);
		test_stream_buffer_gets (path);
	}
	
	if (gen_data && stream_name && testsuite_total_errors () == 0) {
		/* since all tests were successful, unlink the generated test data */
		strcpy (p, stream_name);
		unlink (path);
		
		p = g_stpcpy (path, datadir);
		*p++ = G_DIR_SEPARATOR;
		p = g_stpcpy (p, "output");
		*p++ = G_DIR_SEPARATOR;
		
		g_dir_rewind (outdir);
		while ((dent = g_dir_read_name (outdir))) {
			if (!strncmp (dent, stream_name, strlen (stream_name))) {
				strcpy (p, dent);
				unlink (path);
			}
		}
		
		g_free (stream_name);
	}
	
	g_dir_close (outdir);
	g_dir_close (dir);
	
exit:
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
