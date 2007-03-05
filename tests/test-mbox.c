/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

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
	
	print_depth (depth);
	type = g_mime_object_get_content_type (part);
	fprintf (stdout, "Content-Type: %s/%s\n", type->type, type->subtype);
	
	if (GMIME_IS_MULTIPART (part)) {
		GList *subpart;
		
		subpart = GMIME_MULTIPART (part)->subparts;
		while (subpart) {
			print_mime_struct (subpart->data, depth + 1);
			subpart = subpart->next;
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		GMimeMessagePart *mpart = (GMimeMessagePart *) part;
		
		if (mpart->message)
			print_mime_struct (mpart->message->mime_part, depth + 1);
	}
}

static void
header_cb (GMimeParser *parser, const char *header, const char *value, off_t offset, gpointer user_data)
{
	fprintf (stderr, "found \"%s:\" header at %ld with a value of \"%s\"\n", header, offset, value);
}

void
test_parser (GMimeStream *stream)
{
	GMimeParser *parser;
	GMimeMessage *message;
	char *from;
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_mime_parser_set_scan_from (parser, TRUE);
	/*g_mime_parser_set_respect_content_length (parser, TRUE);*/
	
	g_mime_parser_set_header_regex (parser, "^X-Evolution$", header_cb, NULL);
	
	while (!g_mime_parser_eos (parser)) {
		if (!(message = g_mime_parser_construct_message (parser)))
			continue;
		
		from = g_mime_parser_get_from (parser);
		fprintf (stdout, "%s\n", from);
		g_free (from);
		
		print_mime_struct (message->mime_part, 0);
		g_object_unref (message);
	}
	
	g_object_unref (parser);
}


/* you can only enable one of these at a time... */
/*#define STREAM_BUFFER*/
/*#define STREAM_MEM*/
/*#define STREAM_MMAP*/

int main (int argc, char **argv)
{
	char *filename = NULL;
	GMimeStream *stream;
	int fd;
	
	if (argc > 1)
		filename = argv[1];
	else
		return 0;
	
	if ((fd = open (filename, O_RDONLY)) == -1)
		return 0;
	
	g_mime_init (0);
	
#ifdef STREAM_MMAP
	stream = g_mime_stream_mmap_new (fd, PROT_READ, MAP_PRIVATE);
	g_assert (stream != NULL);
#else
	stream = g_mime_stream_fs_new (fd);
#endif /* STREAM_MMAP */
	
#ifdef STREAM_MEM
	istream = g_mime_stream_mem_new ();
	g_mime_stream_write_to_stream (stream, istream);
	g_mime_stream_reset (istream);
	g_object_unref (stream);
	stream = istream;
#endif
	
#ifdef STREAM_BUFFER
	istream = g_mime_stream_buffer_new (stream, GMIME_STREAM_BUFFER_BLOCK_READ);
	g_object_unref (stream);
	stream = istream;
#endif
	
	test_parser (stream);
	
	g_object_unref (stream);
	
	g_mime_shutdown ();
	
	return EXIT_SUCCESS;
}
