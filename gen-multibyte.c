/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#include <glib.h>
#include <stdio.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <iconv.h>


#if G_BYTE_ORDER == G_BIG_ENDIAN
#define UCS "UCS-4BE"
#else
#define UCS "UCS-4LE"
#endif

static struct {
	char *charset;
	iconv_t cd;
	FILE *fp;
} charsets[] = {
	/* Japanese - in order of preference */
	{ "iso-2022-jp", (iconv_t) -1, NULL },
	{ "Shift-JIS", (iconv_t) -1, NULL },
	{ "euc-jp", (iconv_t) -1, NULL },
	
	/* Korean - in order of preference */
	{ "euc-kr", (iconv_t) -1, NULL },
	{ "iso-2022-kr", (iconv_t) -1, NULL },
	
	/* Simplified Chinese */
	{ "gb2312", (iconv_t) -1, NULL },
	
	/* Traditional Chinese - in order of preference */
	{ "Big5", (iconv_t) -1, NULL },
	{ "euc-tw", (iconv_t) -1, NULL },
};

static int num_multibyte_charsets = sizeof (charsets) / sizeof (charsets[0]);

#define MAX_UNICODE_CHAR_THEORETICAL 0x10ffff
#define MAX_UNICODE_CHAR_REAL        0x0e007f

#define MAX_UNICODE_CHAR  MAX_UNICODE_CHAR_THEORETICAL

int main (int argc, char **argv)
{
	char out[10], *outbuf, *inbuf;
	size_t inleft, outleft, ret;
	guint32 in;
	iconv_t cd;
	int i;
	
	for (i = 0; i < num_multibyte_charsets; i++) {
		char filename[50];
		
		sprintf (filename, "%s.dat", charsets[i].charset);
		charsets[i].cd = iconv_open (charsets[i].charset, UCS);
		charsets[i].fp = fopen (filename, "w");
	}
	
	for (in = 128; in < MAX_UNICODE_CHAR; in++) {
		inbuf = (char *) &in;
		outbuf = out;
		inleft = sizeof (in);
		outleft = sizeof (out);
		
		for (i = 0; i < num_multibyte_charsets; i++) {
			cd = charsets[i].cd;
			
			inbuf = (char *) &in;
			outbuf = out;
			inleft = sizeof (in);
			outleft = sizeof (out);
			
			ret = iconv (cd, &inbuf, &inleft, &outbuf, &outleft);
			if (ret != (size_t) -1) {
				/* this is a legal iso-2022-jp character */
				iconv (cd, NULL, NULL, &outbuf, &outleft);
				fwrite (out, sizeof (out) - outleft, 1, charsets[i].fp);
			} else {
				/* reset the iconv descriptor */
				iconv (cd, NULL, NULL, NULL, NULL);
			}
		}
	}
	
	for (i = 0; i < num_multibyte_charsets; i++) {
		iconv_close (charsets[i].cd);
		fclose (charsets[i].fp);
	}
	
	return 0;
}
