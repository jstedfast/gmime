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


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <gmime/gmime.h>


void
print_depth (int depth)
{
	int i;
	
	for (i = 0; i < depth; i++)
		fprintf (stdout, "   ");
}

void
print_mime_struct (GMimeObject *part, int depth)
{
	const GMimeContentType *type;
	GMimeMultipart *multipart;
	GMimeObject *subpart;
	int i, n;
	
	print_depth (depth);
	type = g_mime_object_get_content_type (part);
	fprintf (stdout, "Content-Type: %s/%s\n", type->type, type->subtype);
		
	if (GMIME_IS_MULTIPART (part)) {
		multipart = (GMimeMultipart *) part;
		
		n = g_mime_multipart_get_count (multipart);
		for (i = 0; i < n; i++) {
			subpart = g_mime_multipart_get_part (multipart, i);
			print_mime_struct (subpart, depth + 1);
			g_object_unref (subpart);
		}
	}
}


int main (int argc, char **argv)
{
	GMimeMessage *message;
	GMimeParser *parser;
	GMimeStream *stream;
	GPtrArray *partials;
	int fd, i;
	
	if (argc < 2)
		return 0;
	
	g_mime_init (0);
	
	parser = g_mime_parser_new ();
	
	partials = g_ptr_array_new ();
	for (i = 1; i < argc; i++) {
		if ((fd = open (argv[i], O_RDONLY, 0)) == -1)
			return -1;
		
		stream = g_mime_stream_fs_new (fd);
		g_mime_parser_init_with_stream (parser, stream);
		g_object_unref (stream);
		
		if (!(message = g_mime_parser_construct_message (parser)))
			return -2;
		
		if (!GMIME_IS_MESSAGE_PARTIAL (message->mime_part))
			return -3;
		
		g_ptr_array_add (partials, message->mime_part);
		g_object_ref (message->mime_part);
		g_object_unref (message);
	}
	
	g_object_unref (parser);
	
	message = g_mime_message_partial_reconstruct_message ((GMimeMessagePartial **) partials->pdata,
							      partials->len);
	g_ptr_array_free (partials, TRUE);
	if (!message)
		return -4;
	
	stream = g_mime_stream_fs_new (STDERR_FILENO);
	g_mime_object_write_to_stream (GMIME_OBJECT (message), stream);
	
	print_mime_struct (message->mime_part, 0);
	
	g_object_unref (message);
	g_object_unref (stream);
	
	g_mime_shutdown ();
	
	return 0;
}
