/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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

#ifdef HAVE_GETOPT_H
#define _GNU_SOURCE
#include <getopt.h>
#else
#include "getopt.h"
#endif

#include <gmime/gmime.h>


#define DEFAULT_FILENAME "-"


static struct option longopts[] = {
	{ "help",        no_argument,       NULL, 'h' },
	{ "version",     no_argument,       NULL, 'v' },
	{ "output-file", required_argument, NULL, 'o' },
	{ NULL,          no_argument,       NULL,  0  }
};

static void
usage (const char *progname)
{
	printf ("Usage: %s [options] [ file ]...\n\n", progname);
	printf ("Options:\n");
	printf ("  -h, --help               display help and exit\n");
	printf ("  -v, --version            display version and exit\n");
	printf ("  -o, --output-file=FILE   output to FILE\n");
}

static void
version (const char *progname)
{
	printf ("%s - GMime %s\n", progname, VERSION);
}

static FILE *
uufopen (const char *filename, const char *rw, int flags, mode_t mode)
{
	int fd;
	
	if (strcmp (filename, "-") != 0) {
		if ((fd = open (filename, flags, mode)) == -1)
			return NULL;
	} else {
		fd = (flags & O_RDONLY) == O_RDONLY ? 0 : 1;
		if ((fd = dup (fd)) == -1)
			return NULL;
	}
	
	return fdopen (fd, rw);
}

static void
uudecode (const char *progname, int argc, char **argv)
{
	GMimeStream *istream, *ostream, *fstream;
	register const unsigned char *p;
	GMimeFilterBasicType encoding;
	int fd, opt, longindex = 0;
	unsigned char inbuf[4096];
	const unsigned char *q;
	GMimeFilter *filter;
	const char *infile;
	char *outfile;
	GString *str;
	mode_t mode;
	FILE *fp;
	
	outfile = NULL;
	while ((opt = getopt_long (argc, argv, "hvo:", longopts, &longindex)) != -1) {
		switch (opt) {
		case 'h':
			usage (progname);
			exit (0);
			break;
		case 'v':
			version (progname);
			exit (0);
			break;
		case 'o':
			if (!(outfile = optarg)) {
				usage (progname);
				exit (1);
			}
			break;
		default:
			printf ("Try `%s --help' for more information.\n", progname);
			exit (1);
		}
	}
	
	optarg = outfile;
	infile = optind < argc ? argv[optind] : DEFAULT_FILENAME;
	
	do {
		if ((fp = uufopen (infile, "r", O_RDONLY, 0)) == NULL) {
			fprintf (stderr, "%s: %s: uufopen %s\n", progname,
				 infile, strerror (errno));
			exit (1);
		}
		
		p = NULL;
		while ((fgets (inbuf, sizeof (inbuf), fp)) != NULL) {
			if (!strncmp (inbuf, "begin-base64 ", 13)) {
				encoding = GMIME_FILTER_BASIC_BASE64_DEC;
				p = inbuf + 13;
				break;
			} else if (!strncmp (inbuf, "begin ", 6)) {
				encoding = GMIME_FILTER_BASIC_UU_DEC;
				p = inbuf + 6;
				break;
			}
			
			while (!(strchr (inbuf, '\n'))) {
				if (!(fgets (inbuf, sizeof (inbuf), fp)))
					goto nexti;
			}
		}
		
		if (p == NULL) {
			fprintf (stderr, "%s: %s: No `begin' line\n", progname,
				 infile == DEFAULT_FILENAME ? "stdin" : infile);
			fclose (fp);
			str = NULL;
			goto nexti;
		}
		
		/* decode the mode */
		if ((mode = strtoul (p, (char **) &q, 8) & 0777) && *q != ' ')
			goto nexti;
		
		while (*q == ' ')
			q++;
		
		/* decode the 'name' */
		str = g_string_new ("");
		
		p = q;
		while (*p && *p != '\n')
			p++;
		
		g_string_append_len (str, q, p - q);
		if (*p != '\n') {
			while ((fgets (inbuf, sizeof (inbuf), fp)) != NULL) {
				p = inbuf;
				while (*p && *p != '\n')
					p++;
				
				g_string_append_len (str, inbuf, p - inbuf);
				if (*p == '\n')
					break;
			}
			
			if (*p != '\n')
				goto nexti;
		}
		
		if (!outfile || outfile != optarg)
			outfile = str->str;
		
		if ((fd = open (outfile, O_CREAT | O_TRUNC | O_WRONLY, mode)) == -1) {
			fprintf (stderr, "%s: %s: %s\n", progname,
				 outfile, strerror (errno));
			g_string_free (str, TRUE);
			fclose (fp);
			exit (1);
		}
		
		istream = g_mime_stream_file_new (fp);
		ostream = g_mime_stream_fs_new (fd);
		fstream = g_mime_stream_filter_new_with_stream (ostream);
		filter = g_mime_filter_basic_new_type (encoding);
		g_mime_stream_filter_add ((GMimeStreamFilter *) fstream, filter);
		g_object_unref (ostream);
		g_object_unref (filter);
		
		if (encoding == GMIME_FILTER_BASIC_UU_DEC) {
			/* Tell the filter that we've already read past the begin line */
			((GMimeFilterBasic *) filter)->state |= GMIME_UUDECODE_STATE_BEGIN;
		}
		
		if (g_mime_stream_write_to_stream (istream, fstream) == -1) {
			fprintf (stderr, "%s: %s: %s\n", progname, outfile, strerror (errno));
			g_object_unref (fstream);
			g_object_unref (istream);
			exit (1);
		}
		
		g_mime_stream_flush (fstream);
		g_object_unref (fstream);
		g_object_unref (istream);
		
	nexti:
		if (str)
			g_string_free (str, TRUE);
		
		infile = argv[++optind];
	} while (optind < argc);
}

int main (int argc, char **argv)
{
	const char *progname;
	
	g_type_init ();
	
	if (!(progname = strrchr (argv[0], '/')))
		progname = argv[0];
	else
		progname++;
	
	uudecode (progname, argc, argv);
	
	return 0;
}
