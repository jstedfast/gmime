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
	
	if ((fd = open (argv[1], O_RDONLY)) == -1)
		return 0;
	
	g_mime_init (0);
	
	stream = g_mime_stream_fs_new (fd);
	istream = g_mime_stream_filter_new_with_stream (stream);
	g_mime_stream_unref (stream);
	best = (GMimeFilterBest *) g_mime_filter_best_new (GMIME_FILTER_BEST_CHARSET |
							   GMIME_FILTER_BEST_ENCODING);
	
	g_mime_stream_filter_add (GMIME_STREAM_FILTER (istream), GMIME_FILTER (best));
	g_object_unref (best);
	
	null = g_mime_stream_null_new ();
	
	g_mime_stream_write_to_stream (istream, null);
	
	printf ("summary for %s:\n", argv[1]);
	printf ("\tbest charset: %s\n", g_mime_filter_best_charset (best));
	printf ("\tbest encoding (7bit): %s\n",
		g_mime_part_encoding_to_string (
			g_mime_filter_best_encoding (best, GMIME_BEST_ENCODING_7BIT)));
	printf ("\tbest encoding (8bit): %s\n",
		g_mime_part_encoding_to_string (
			g_mime_filter_best_encoding (best, GMIME_BEST_ENCODING_8BIT)));
	printf ("\tbest encoding (binary): %s\n",
		g_mime_part_encoding_to_string (
			g_mime_filter_best_encoding (best, GMIME_BEST_ENCODING_BINARY)));
	
	g_mime_stream_unref (istream);
	g_mime_stream_unref (null);
	
	return 0;
}
