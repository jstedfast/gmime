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

#include <string.h>

#include "gmime.h"
#include "gmime-filter-html.h"

/* 1 = non-email-address chars: ()<>@,;:\\\"[]`'|  */
/* 2 = trailing url garbage:    ,.!?;:>)]}\\`'-_|  */
static unsigned short special_chars[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /*  nul - 0x0f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x10 - 0x1f */
	1, 2, 1, 0, 0, 0, 0, 3, 1, 3, 0, 0, 3, 2, 2, 0,    /*   sp - /    */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 1, 0, 3, 2,    /*    0 - ?    */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /*    @ - O    */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3, 0, 2,    /*    P - _    */
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /*    ` - o    */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 0, 0     /*    p - del  */
};

#define IS_NON_ADDR   (1 << 0)
#define IS_NON_URL    (1 << 1)
#define IS_GARBAGE    (1 << 2)

#define NON_EMAIL_CHARS         "()<>@,;:\\\"/[]`'|\n\t "
#define NON_URL_CHARS           "()<>,;\\\"[]`'|\n\t "
#define TRAILING_URL_GARBAGE    ",.!?;:>)}\\`'-_|\n\t "

static void
table_init (void)
{
	char *c;
	int i;
	
	memset (special_chars, 0, sizeof (special_chars));
	for (c = NON_EMAIL_CHARS; *c; c++)
		special_chars[(int) *c] |= IS_NON_ADDR;
	for (c = NON_URL_CHARS; *c; c++)
		special_chars[(int) *c] |= IS_NON_URL;
	for (c = TRAILING_URL_GARBAGE; *c; c++)
		special_chars[(int) *c] |= IS_GARBAGE;
	
	fprintf (stderr, "static unsigned short special_chars[256] = {");
	for (i = 0; i < 256; i++) {
		fprintf (stderr, "%s%2d%s", (i % 16) ? "" : "\n\t",
			 special_chars[i], i != 255 ? "," : "\n");
	}
	fprintf (stderr, "};\n");
}


int main (int argc, char **argv)
{
	GMimeStream *istream, *ostream, *fstream;
	GMimeFilter *html;
	int i;
	
	if (argc == 1) {
		table_init ();
		return 0;
	}
	
	fstream = g_mime_stream_file_new (stdout);
	ostream = g_mime_stream_filter_new_with_stream (fstream);
	g_mime_stream_unref (fstream);
	html = g_mime_filter_html_new (GMIME_FILTER_HTML_CONVERT_NL |
				       GMIME_FILTER_HTML_CONVERT_SPACES |
				       GMIME_FILTER_HTML_CONVERT_URLS |
				       GMIME_FILTER_HTML_MARK_CITATION |
				       GMIME_FILTER_HTML_CONVERT_ADDRESSES |
				       GMIME_FILTER_HTML_ESCAPE_8BIT |
				       GMIME_FILTER_HTML_CITE, 0);
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (ostream), html);
	g_object_unref (html);
	
	for (i = 1; i < argc; i++) {
		FILE *fp;
		
		fp = fopen (argv[i], "r");
		if (!fp) {
			fprintf (stderr, "failed to open %s\n", argv[i]);
			continue;
		}
		
		istream = g_mime_stream_file_new (fp);
		g_mime_stream_write_to_stream (istream, ostream);
		g_mime_stream_unref (istream);
	}
	
	g_mime_stream_unref (ostream);
	
	return 0;
}
