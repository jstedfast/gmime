/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2009 Jeffrey Stedfast
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x) 
#define v(x) if (verbose > 3) x;


static gboolean
streams_match (GMimeStream **streams, const char *filename)
{
	char buf[4096], dbuf[4096], errstr[1024];
	size_t totalsize, totalread = 0;
	size_t nread, size;
	ssize_t n;
	
	v(fprintf (stdout, "Matching original stream (%lld -> %lld) with %s (%lld, %lld)... ",
		   streams[0]->position, streams[0]->bound_end, filename,
		   streams[1]->position, streams[1]->bound_end));
	
	if (streams[0]->bound_end != -1) {
		totalsize = streams[0]->bound_end - streams[0]->position;
	} else if ((n = g_mime_stream_length (streams[0])) == -1) {
		sprintf (errstr, "Error: Unable to get length of original stream: %s\n", g_strerror (errno));
		goto fail;
	} else if (n < (streams[0]->position - streams[0]->bound_start)) {
		sprintf (errstr, "Error: Overflow on original stream?\n");
		goto fail;
	} else {
		totalsize = n - (streams[0]->position - streams[0]->bound_start);
	}
	
	while (totalread < totalsize) {
		if ((n = g_mime_stream_read (streams[0], buf, sizeof (buf))) <= 0)
			break;
		
		size = n;
		nread = 0;
		totalread += n;
		
		d(fprintf (stderr, "read %zu bytes from stream[0]\n", size));
		
		do {
			if ((n = g_mime_stream_read (streams[1], dbuf + nread, size - nread)) <= 0) {
				d(fprintf (stderr, "stream[1] read() returned %zd, EOF\n", n));
				break;
			}
			d(fprintf (stderr, "read %zd bytes from stream[1]\n", n));
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
	ssize_t slen;
	FILE *fp;
	
	if (!(fp = fopen (filename, "r+")))
		throw (exception_new ("could not open `%s': %s", filename, g_strerror (errno)));
	
	while (!g_mime_stream_eos (stream)) {
		rbuf[0] = '\0';
		slen = g_mime_stream_buffer_gets (stream, sbuf, sizeof (sbuf));
		fgets (rbuf, sizeof (rbuf), fp);
		
		if (strcmp (sbuf, rbuf) != 0)
			break;
	}
	
	fclose (fp);
	
	if (strcmp (sbuf, rbuf) != 0) {
		v(fprintf (stderr, "\tstream: \"%s\" (%zu)\n", sbuf, strlen (sbuf)));
		v(fprintf (stderr, "\treal:   \"%s\" (%zu)\n", rbuf, strlen (rbuf)));
		throw (exception_new ("streams did not match"));
	}
}

static void
test_stream_buffer_gets (const char *filename)
{
	GMimeStream *stream, *buffered = NULL;
	int fd;
	
	if ((fd = open (filename, O_RDONLY)) == -1) {
		v(fprintf (stderr, "failed to open %s", filename));
		return;
	}
	
	stream = g_mime_stream_fs_new (fd);
	
	testsuite_check ("GMimeStreamBuffer::block gets");
	try {
		g_mime_stream_reset (stream);
		buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_BLOCK_READ);
		test_stream_gets (buffered, filename);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimeStreamBuffer::block gets() failed: %s",
					ex->message);
	} finally {
		g_object_unref (buffered);
	}
	
	testsuite_check ("GMimeStreamBuffer::cache gets");
	try {
		g_mime_stream_reset (stream);
		buffered = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_CACHE_READ);
		test_stream_gets (buffered, filename);
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("GMimeStreamBuffer::block gets() failed: %s",
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
	
	if ((fd = open (filename, O_RDONLY)) == -1) {
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
	
	if ((fd[0] = open (input, O_RDONLY)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY)) == -1) {
		close (fd[0]);
		return FALSE;
	}
	
	stream = g_mime_stream_fs_new (fd[0]);
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	
	streams[1] = g_mime_stream_fs_new (fd[1]);
	
	if (!streams_match (streams, filename))
		ex = exception_new ("GMimeStreamFs streams did not match for `%s'", filename);
	
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
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	
	streams[1] = g_mime_stream_file_new (fp[1]);
	
	if (!streams_match (streams, filename))
		ex = exception_new ("GMimeStreamFile streams did not match for `%s'", filename);
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}

static gboolean
check_stream_mmap (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	int fd[2];
	
	if ((fd[0] = open (input, O_RDONLY)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY)) == -1) {
		close (fd[0]);
		return FALSE;
	}
	
	stream = g_mime_stream_mmap_new (fd[0], PROT_READ, MAP_PRIVATE);
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	
	streams[1] = g_mime_stream_mmap_new (fd[1], PROT_READ, MAP_PRIVATE);
	
	if (!streams_match (streams, filename))
		ex = exception_new ("streams did not match");
	
	g_object_unref (streams[0]);
	g_object_unref (streams[1]);
	
	if (ex != NULL)
		throw (ex);
	
	return TRUE;
}

static gboolean
check_stream_buffer_block (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	int fd[2];
	
	if ((fd[0] = open (input, O_RDONLY)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY)) == -1) {
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
check_stream_buffer_cache (const char *input, const char *output, const char *filename, gint64 start, gint64 end)
{
	GMimeStream *streams[2], *stream;
	Exception *ex = NULL;
	int fd[2];
	
	if ((fd[0] = open (input, O_RDONLY)) == -1)
		return FALSE;
	
	if ((fd[1] = open (output, O_RDONLY)) == -1) {
		close (fd[0]);
		return FALSE;
	}
	
	streams[0] = g_mime_stream_fs_new (fd[0]);
	stream = g_mime_stream_buffer_new (streams[0], GMIME_STREAM_BUFFER_CACHE_READ);
	g_object_unref (streams[0]);
	streams[0] = g_mime_stream_substream (stream, start, end);
	g_object_unref (stream);
	
	streams[1] = g_mime_stream_fs_new (fd[1]);
	
	if (!streams_match (streams, filename))
		ex = exception_new ("GMimeStreamBuffer (Cache Mode) streams did not match for `%s'", filename);
	
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
	{ "GMimeStreamFs",                  check_stream_fs           },
	{ "GMimeStreamFile",                check_stream_file         },
	{ "GMimeStreamMmap",                check_stream_mmap         },
	{ "GMimeStreamBuffer (block mode)", check_stream_buffer_block },
	{ "GMimeStreamBuffer (cache mode)", check_stream_buffer_cache },
};

static void
test_streams (DIR *dir, const char *datadir, const char *filename)
{
	char inpath[256], outpath[256], *p, *q, *o;
	struct dirent *dent;
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
	
	while ((dent = readdir (dir))) {
		if (strncmp (dent->d_name, filename, n) != 0 || dent->d_name[n] != ':')
			continue;
		
		p = dent->d_name + n + 1;
		if ((start = strtol (p, &o, 10)) < 0 || *o != ',')
			continue;
		
		p = o + 1;
		
		if ((((end = strtol (p, &o, 10)) < start) && end != -1) || *o != '\0')
			continue;
		
		strcpy (q, dent->d_name);
		
		for (i = 0; i < G_N_ELEMENTS (checks); i++) {
			testsuite_check ("%s on `%s'", checks[i].what, dent->d_name);
			try {
				if (!checks[i].check (inpath, outpath, dent->d_name, start, end)) {
					testsuite_check_warn ("%s could not open `%s'",
							      checks[i].what, dent->d_name);
				} else {
					testsuite_check_passed ();
				}
			} catch (ex) {
				testsuite_check_failed ("%s on `%s' failed: %s", checks[i].what,
							dent->d_name, ex->message);
			} finally;
		}
	}
	
	rewinddir (dir);
}


static void
gen_random_stream (GMimeStream *stream)
{
	size_t nwritten, buflen, total = 0, size, i;
	char buf[4096];
	ssize_t n;
	
	/* read between 4k and 14k bytes */
	size = 4096 + (size_t) (10240.0 * (rand () / (RAND_MAX + 1.0)));
	v(fprintf (stdout, "Generating %zu bytes of random data... ", size));
	v(fflush (stdout));
	
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
}

static int
gen_test_data (const char *datadir, char **stream_name)
{
	GMimeStream *istream, *ostream, *stream;
	char input[256], output[256], *name, *p;
	gint64 start, end;
	struct stat st;
	size_t len;
	int fd, i;
	
	srand (time (NULL));
	
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
	
	if ((fd = mkstemp (input)) == -1)
		return -1;
	
	*stream_name = g_strdup (name);
	
	*p++ = G_DIR_SEPARATOR;
	p = g_stpcpy (p, name);
	*p++ = ':';
	
	istream = g_mime_stream_fs_new (fd);
	gen_random_stream (istream);
	
	if (stat (input, &st) == -1 || !S_ISREG (st.st_mode)) {
		g_object_unref (istream);
		unlink (input);
		return -1;
	}
	
	for (i = 0; i < 64; i++) {
	retry:
		start = (gint64) (st.st_size * (rand () / (RAND_MAX + 1.0)));
		len = (size_t) (st.st_size * (rand () / (RAND_MAX + 1.0)));
		if (start + len > st.st_size) {
			len = st.st_size - start;
			end = -1;
		} else {
			end = start + len;
		}
		
		sprintf (p, "%lld,%lld", start, end);
		
		if ((fd = open (output, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, 0666)) == -1)
			goto retry;
		
		ostream = g_mime_stream_fs_new (fd);
		stream = g_mime_stream_substream (istream, start, end);
		g_mime_stream_write_to_stream (stream, ostream);
		g_mime_stream_flush (ostream);
		g_object_unref (ostream);
		g_object_unref (stream);
	}
	
	return 0;
}

int main (int argc, char **argv)
{
	const char *datadir = "data/streams";
	gboolean gen_data = TRUE;
	char *stream_name = NULL;
	struct dirent *dent;
	char path[256], *p;
	DIR *dir, *outdir;
	int i;
	
	g_mime_init (0);
	
	testsuite_init (argc, argv);
	
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
	
	if (!(outdir = opendir (path))) {
		if (gen_test_data (datadir, &stream_name) == -1 ||
		    !(outdir = opendir (path)))
			goto exit;
		
		gen_data = FALSE;
	}
	
	p = g_stpcpy (p, "input");
	
	if (!(dir = opendir (path))) {
		if (!gen_data || gen_test_data (datadir, &stream_name) == -1 ||
		    !(dir = opendir (path))) {
			closedir (outdir);
			goto exit;
		}
		
		gen_data = FALSE;
	}
	
	if (gen_data) {
		while ((dent = readdir (dir))) {
			if (dent->d_name[0] == '.' || !strcmp (dent->d_name, "README"))
				continue;
			
			gen_data = FALSE;
			break;
		}
		
		rewinddir (dir);
		
		if (gen_data && gen_test_data (datadir, &stream_name) == -1)
			goto exit;
	}
	
	*p++ = G_DIR_SEPARATOR;
	*p = '\0';
	
	while ((dent = readdir (dir))) {
		if (dent->d_name[0] == '.' || !strcmp (dent->d_name, "README"))
			continue;
		
		test_streams (outdir, datadir, dent->d_name);
		
		strcpy (p, dent->d_name);
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
		
		rewinddir (outdir);
		while ((dent = readdir (outdir))) {
			if (!strncmp (dent->d_name, stream_name, strlen (stream_name))) {
				strcpy (p, dent->d_name);
				unlink (path);
			}
		}
		
		g_free (stream_name);
	}
	
	closedir (outdir);
	closedir (dir);
	
exit:
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
