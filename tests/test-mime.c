/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

static int
test_parser (const char *Path)
{
	GMimeStream *Source, *Stream;
	GMimeMessage *Message;
	GMimeFilter *Filter;
	GMimeParser *Parser;
	int FD;
	
	if ((FD = open(Path,O_RDONLY)) < 0)
	{
		return errno;
	}
	
	Source = g_mime_stream_fs_new(FD);
	Stream = g_mime_stream_filter_new_with_stream(Source);
	g_mime_stream_unref(Source);
	Filter = 
		g_mime_filter_crlf_new(GMIME_FILTER_CRLF_DECODE,GMIME_FILTER_CRLF_MODE_CRLF_DOTS);
	g_mime_stream_filter_add(GMIME_STREAM_FILTER(Stream),Filter);
	
	Parser = g_mime_parser_new();
	g_mime_parser_init_with_stream(Parser,Stream);
	Message = g_mime_parser_construct_message(Parser);
	/*g_mime_message_foreach_part(Message,VCheckPart,M);*/
	
	g_object_unref(Filter);
	g_object_unref(Parser);
	g_mime_object_unref(GMIME_OBJECT(Message));
	g_mime_stream_unref(Stream);
	
	return 0;
}

int main (int argc, char **argv)
{
	g_mime_init (0);
	
	while (1)
		test_parser (argv[1]);
	
	return 0;
}
