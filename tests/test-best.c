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
#include <unistd.h>
#include <fcntl.h>

#include <gmime/gmime.h>


int main (int argc, char **argv)
{
	GMimeStream *istream, *stream, *null;
	GMimeFilterBest *best;
	int fd;
	
	if ((fd = open (argv[1], O_RDONLY, 0)) == -1)
		return 0;
	
	g_mime_init ();
	
	stream = g_mime_stream_fs_new (fd);
	istream = g_mime_stream_filter_new (stream);
	g_object_unref (stream);
	best = (GMimeFilterBest *) g_mime_filter_best_new (GMIME_FILTER_BEST_CHARSET |
							   GMIME_FILTER_BEST_ENCODING);
	
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (istream), GMIME_FILTER (best));
	g_object_unref (best);
	
	null = g_mime_stream_null_new ();
	
	g_mime_stream_write_to_stream (istream, null);
	
	printf ("summary for %s:\n", argv[1]);
	printf ("\tbest charset: %s\n", g_mime_filter_best_charset (best));
	printf ("\tbest encoding (7bit): %s\n",
		g_mime_content_encoding_to_string (
			g_mime_filter_best_encoding (best, GMIME_ENCODING_CONSTRAINT_7BIT)));
	printf ("\tbest encoding (8bit): %s\n",
		g_mime_content_encoding_to_string (
			g_mime_filter_best_encoding (best, GMIME_ENCODING_CONSTRAINT_8BIT)));
	printf ("\tbest encoding (binary): %s\n",
		g_mime_content_encoding_to_string (
			g_mime_filter_best_encoding (best, GMIME_ENCODING_CONSTRAINT_BINARY)));
	
	g_object_unref (istream);
	g_object_unref (null);
	
	return 0;
}
