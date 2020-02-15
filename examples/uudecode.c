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

#include <gmime/gmime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DEFAULT_FILENAME "-"

static gboolean version;
static const char *outfile;

static GOptionEntry options[] = {
	{ "version",     'v', 0, G_OPTION_ARG_NONE,     &version, "display version and exit", NULL   },
	{ "output-file", 'o', 0, G_OPTION_ARG_FILENAME, &outfile, "output to FILE",           "FILE" },
	{ NULL, }
};

static FILE *
uufopen (const char *filename, const char *rw, int flags, mode_t mode)
{
	int fd;
	
	if (strcmp (filename, "-") != 0) {
		if ((fd = open (filename, flags, mode)) == -1)
			return NULL;
	} else {
		fd = (flags & O_WRONLY) == O_WRONLY ? 1 : 0;
		if ((fd = dup (fd)) == -1)
			return NULL;
	}
	
	return fdopen (fd, rw);
}

typedef size_t (* Decoder) (const unsigned char *in, size_t inlen, unsigned char *out, int *state, guint32 *save);

static int
uudecode (const char *progname, int argc, char **argv)
{
	gboolean midline = FALSE, base64 = FALSE;
	GOptionContext *context;
	register const char *p;
	char inbuf[4096], *q;
	GString *str = NULL;
	GError *err = NULL;
	const char *infile;
	char outbuf[4100];
	FILE *fin, *fout;
	guint32 save = 0;
	Decoder decode;
	int state = 0;
	mode_t mode;
	size_t n;
#ifdef WIN32
	int optind = 1;
#endif
	
	context = g_option_context_new ("[FILE]...");
	g_option_context_add_main_entries (context, options, progname);
	g_option_context_set_help_enabled (context, TRUE);
	
	if (!g_option_context_parse (context, &argc, &argv, &err)) {
		printf ("Try `%s --help' for more information.\n", progname);
		g_option_context_free (context);
		return -1;
	}
	
	g_option_context_free (context);
	
	if (version) {
		printf ("%s - GMime %u.%u.%u\n", progname, GMIME_MAJOR_VERSION, GMIME_MINOR_VERSION, GMIME_MICRO_VERSION);
		return 0;
	}
	
	if (argc == 1) {
		printf ("Try `%s --help' for more information.\n", progname);
		return -1;
	}
	
	infile = argc > 1 ? argv[1] : DEFAULT_FILENAME;
	
	do {
		if ((fin = uufopen (infile, "rt", O_RDONLY, 0)) == NULL) {
			fprintf (stderr, "%s: %s: %s\n", progname,
				 infile, g_strerror (errno));
			return -1;
		}
		
		p = NULL;
		while ((fgets (inbuf, sizeof (inbuf), fin)) != NULL) {
			if (!strncmp (inbuf, "begin-base64 ", 13)) {
				decode = g_mime_encoding_base64_decode_step;
				p = inbuf + 13;
				base64 = TRUE;
				break;
			} else if (!strncmp (inbuf, "begin ", 6)) {
				decode = g_mime_encoding_uudecode_step;
				state = GMIME_UUDECODE_STATE_BEGIN;
				p = inbuf + 6;
				break;
			}
			
			while (!(strchr (inbuf, '\n'))) {
				if (!(fgets (inbuf, sizeof (inbuf), fin)))
					goto nexti;
			}
		}
		
		if (p == NULL) {
			fprintf (stderr, "%s: %s: No `begin' line\n", progname,
				 (!strcmp (infile, DEFAULT_FILENAME)) ? "stdin" : infile);
			goto nexti;
		}
		
		/* decode the mode */
		if ((mode = strtoul (p, &q, 8) & 0777) && *q != ' ')
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
			while ((fgets (inbuf, sizeof (inbuf), fin)) != NULL) {
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
		
		if (!outfile)
			outfile = g_strchomp (str->str);
		
		if (!(fout = fopen (outfile, "wb"))) {
			fprintf (stderr, "%s: %s: %s\n", progname,
				 outfile, g_strerror (errno));
			g_string_free (str, TRUE);
			fclose (fin);
			
			return -1;
		}
		
		while ((fgets (inbuf, sizeof (inbuf), fin))) {
			if (!midline) {
				if (base64) {
					if (!strncmp (inbuf, "====", 4) &&
					    (inbuf[4] == '\r' || inbuf[4] == '\n'))
						break;
				} else {
					if ((state & GMIME_UUDECODE_STATE_END) &&
					    !strncmp (inbuf, "end", 3) &&
					    (inbuf[3] == '\r' || inbuf[3] == '\n'))
						break;
				}
			}
			
			n = strlen (inbuf);
			midline = n > 0 && inbuf[n - 1] != '\n';
			
			n = decode ((const unsigned char *) inbuf, n, (unsigned char *) outbuf, &state, &save);
			if (fwrite (outbuf, 1, n, fout) != n) {
				fprintf (stderr, "%s: %s: %s\n", progname, outfile,
					 g_strerror (ferror (fout)));
				fclose (fout);
				fclose (fin);
				return -1;
			}
		}
		
		fflush (fout);
		fclose (fout);
		
	nexti:
		fclose (fin);
		
		if (str) {
			g_string_free (str, TRUE);
			str = NULL;
		}
		
		infile = argv[++optind];
	} while (optind < argc);
	
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
	
	if (uudecode (progname, argc, argv) == -1)
		return EXIT_FAILURE;
	
	return EXIT_SUCCESS;
}
