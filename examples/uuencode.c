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

/*
 * For the hell of it, I built gmime's uuencode with gcc -O2 on my
 * redhat 7.2 box and compared it with the system's uuencode:
 *
 * file: gmime/tests/large.eml
 * size: 38657516 bytes
 *
 * uuencode - GNU sharutils 4.2.1
 * real	0m9.066s
 * user	0m8.930s
 * sys  0m0.220s
 *
 * uuencode - GMime 1.90.4
 * real	0m3.563s
 * user	0m3.240s
 * sys 	0m0.200s
 *
 * And here are the results of base64 encoding the same file:
 *
 * uuencode - GNU sharutils 4.2.1
 * real	0m8.920s
 * user	0m8.750s
 * sys	0m0.160s
 *
 * uuencode - GMime 1.90.4
 * real	0m1.173s
 * user	0m0.940s
 * sys	0m0.180s
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gmime/gmime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef HAVE_GETOPT_H
#define _GNU_SOURCE
#include <getopt.h>
#else
#include "getopt.h"
#endif


static struct option longopts[] = {
	{ "help",        no_argument,       NULL, 'h' },
	{ "version",     no_argument,       NULL, 'v' },
	{ "base64",      no_argument,       NULL, 'm' },
	{ NULL,          no_argument,       NULL,  0  }
};


static void
usage (const char *progname)
{
	printf ("Usage: %s [options] [ file ] name\n\n", progname);
	printf ("Options:\n");
	printf ("  -h, --help               display help and exit\n");
	printf ("  -v, --version            display version and exit\n");
	printf ("  -m, --base64             use RFC1521 base64 encoding\n");
}

static void
version (const char *progname)
{
	printf ("%s - GMime %s\n", progname, GMIME_VERSION);
}

static int
uuencode (const char *progname, int argc, char **argv)
{
	GMimeStream *istream, *ostream, *fstream;
	GMimeContentEncoding encoding;
	const char *filename, *name;
	GMimeFilter *filter;
	gboolean base64;
	struct stat st;
	int fd, opt;
	
	base64 = FALSE;
	encoding = GMIME_CONTENT_ENCODING_UUENCODE;
	while ((opt = getopt_long (argc, argv, "hvm", longopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage (progname);
			return 0;
		case 'v':
			version (progname);
			return 0;
		case 'm':
			base64 = TRUE;
			encoding = GMIME_CONTENT_ENCODING_BASE64;
			break;
		default:
			printf ("Try `%s --help' for more information.\n", progname);
			return -1;
		}
	}
	
	if (optind >= argc) {
		printf ("Try `%s --help' for more information.\n", progname);
		return -1;
	}
	
	if (optind + 1 < argc)
		filename = argv[optind++];
	else
		filename = NULL;
	
	name = argv[optind];
	
	/* open our input file... */
	if ((fd = filename ? open (filename, O_RDONLY, 0) : dup (0)) == -1) {
		fprintf (stderr, "%s: %s: %s\n", progname,
			 filename ? filename : "stdin",
			 g_strerror (errno));
		return -1;
	}
	
	/* stat() our input file for file mode permissions */
	if (fstat (fd, &st) == -1) {
		fprintf (stderr, "%s: %s: %s\n", progname,
			 filename ? filename : "stdin",
			 g_strerror (errno));
		close (fd);
		return -1;
	}
	
	printf ("begin%s %.3o %s\n", base64 ? "-base64" : "", st.st_mode & 0777, name);
	fflush (stdout);
	
	istream = g_mime_stream_fs_new (fd);
	
	/* open our output stream */
	ostream = g_mime_stream_fs_new (1);
	g_mime_stream_fs_set_owner ((GMimeStreamFs *) ostream, FALSE);
	
	fstream = g_mime_stream_filter_new (ostream);
	
	/* attach an encode filter */
	filter = g_mime_filter_basic_new (encoding, TRUE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) fstream, filter);
	g_object_unref (filter);
	
	if (g_mime_stream_write_to_stream (istream, fstream) == -1) {
		fprintf (stderr, "%s: %s\n", progname, g_strerror (errno));
		g_object_unref (fstream);
		g_object_unref (istream);
		g_object_unref (ostream);
		return -1;
	}
	
	g_mime_stream_flush (fstream);
	g_object_unref (fstream);
	g_object_unref (istream);
	
	if (g_mime_stream_write_string (ostream, base64 ? "====\n" : "end\n") == -1) {
		fprintf (stderr, "%s: %s\n", progname, g_strerror (errno));
		g_object_unref (ostream);
		return -1;
	}
	
	g_object_unref (ostream);
	
	return 0;
}

int main (int argc, char **argv)
{
	const char *progname;
	
#if !GLIB_CHECK_VERSION(2, 35, 1)
	g_type_init ();
#endif
	
	if (!(progname = strrchr (argv[0], '/')))
		progname = argv[0];
	else
		progname++;
	
	if (uuencode (progname, argc, argv) == -1)
		return EXIT_FAILURE;
	
	return EXIT_SUCCESS;
}
