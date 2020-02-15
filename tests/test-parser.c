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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <gmime/gmime.h>

#if !defined (G_OS_WIN32) || defined (__MINGW32__)
//#define ENABLE_ZENTIMER
#endif
#include "zentimer.h"

#define TEST_RAW_HEADER
#define TEST_PRESERVE_HEADERS
//#define PRINT_MIME_STRUCT
#define PRINT_MIME_STRUCT_ITER
#define TEST_WRITE_TO_STREAM

#ifdef PRINT_MIME_STRUCT
static void
print_depth (int depth)
{
	int i;
	
	for (i = 0; i < depth; i++)
		fprintf (stdout, "   ");
}

static void
print_mime_struct (GMimeObject *part, int depth)
{
	const GMimeContentType *type;
	GMimeMultipart *multipart;
	GMimeMessagePart *mpart;
	GMimeObject *subpart;
	gboolean has_md5;
	int i, n;
	
	print_depth (depth);
	type = g_mime_object_get_content_type (part);
	has_md5 = g_mime_object_get_header (part, "Content-Md5") != NULL;
	fprintf (stdout, "Content-Type: %s/%s%s", type->type, type->subtype,
		 has_md5 ? "; md5sum=" : "\n");
	
	if (GMIME_IS_MULTIPART (part)) {
		multipart = (GMimeMultipart *) part;
		
		n = g_mime_multipart_get_count (multipart);
		for (i = 0; i < n; i++) {
			subpart = g_mime_multipart_get_part (multipart, i);
			print_mime_struct (subpart, depth + 1);
		}
	} else if (GMIME_IS_MESSAGE_PART (part)) {
		mpart = (GMimeMessagePart *) part;
		
		if (mpart->message)
			print_mime_struct (mpart->message->mime_part, depth + 1);
	} else if (has_md5) {
		/* validate the Md5 sum */
		if (g_mime_part_verify_content_md5 ((GMimePart *) part))
			fprintf (stdout, "GOOD\n");
		else
			fprintf (stdout, "BAD\n");
	}
}
#endif

#ifdef PRINT_MIME_STRUCT_ITER
static void
print_mime_part_info (const char *path, GMimeObject *object)
{
	const GMimeContentType *type;
	gboolean has_md5;
	
	type = g_mime_object_get_content_type (object);
	
	if (GMIME_IS_PART (object))
		has_md5 = g_mime_object_get_header (object, "Content-Md5") != NULL;
	else
		has_md5 = FALSE;
	
	fprintf (stdout, "%s\tContent-Type: %s/%s%s", path,
		 type->type, type->subtype, has_md5 ? "; md5sum=" : "\n");
	
	if (has_md5) {
		/* validate the Md5 sum */
		if (g_mime_part_verify_content_md5 ((GMimePart *) object))
			fprintf (stdout, "GOOD\n");
		else
			fprintf (stdout, "BAD\n");
	}
}

static void
print_mime_struct_iter (GMimeMessage *message)
{
	//const char *jump_to = "4.2.2.2";
	GMimePartIter *iter;
	GMimeObject *parent;
	GMimeObject *part;
	char *path;
	
	iter = g_mime_part_iter_new ((GMimeObject *) message);
	if (g_mime_part_iter_is_valid (iter)) {
		if ((parent = g_mime_part_iter_get_parent (iter)))
			print_mime_part_info ("TEXT", parent);
		
		do {
			part = g_mime_part_iter_get_current (iter);
			path = g_mime_part_iter_get_path (iter);
			print_mime_part_info (parent ? path : "TEXT", part);
			g_free (path);
		} while (g_mime_part_iter_next (iter));
		
#if 0
		fprintf (stdout, "Jumping to %s\n", jump_to);
		if (g_mime_part_iter_jump_to (iter, jump_to)) {
			part = g_mime_part_iter_get_current (iter);
			path = g_mime_part_iter_get_path (iter);
			print_mime_part_info (path, part);
			g_free (path);
		} else {
			fprintf (stdout, "Failed to jump to %s\n", jump_to);
		}
#endif
	}
	
	g_mime_part_iter_free (iter);
}
#endif /* PRINT_MIME_STRUCT_ITER */

static void
test_parser (GMimeStream *stream)
{
	GMimeFormatOptions *format = g_mime_format_options_get_default ();
	GMimeParser *parser;
	GMimeMessage *message;
	char *text;
	
	fprintf (stdout, "\nTesting MIME parser...\n\n");
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	
	ZenTimerStart (NULL);
	message = g_mime_parser_construct_message (parser, NULL);
	ZenTimerStop (NULL);
	ZenTimerReport (NULL, "gmime::parser_construct_message");
	
	g_object_unref (parser);
	
	ZenTimerStart (NULL);
	text = g_mime_object_to_string ((GMimeObject *) message, format);
	ZenTimerStop (NULL);
	ZenTimerReport (NULL, "gmime::message_to_string");
	/*fprintf (stdout, "Result should match previous MIME message dump\n\n%s\n", text);*/
	g_free (text);

#ifdef TEST_RAW_HEADER
	{
		char *raw;
		
		raw = g_mime_object_get_headers ((GMimeObject *) message, format);
		fprintf (stdout, "\nTesting raw headers...\n\n%s\n", raw);
		g_free (raw);
	}
#endif
	
#ifdef TEST_PRESERVE_HEADERS
	{
		GMimeStream *output;
		
		fprintf (stdout, "\nTesting preservation of headers...\n\n");
		output = g_mime_stream_file_new (stdout);
		g_mime_stream_file_set_owner ((GMimeStreamFile *) output, FALSE);
		g_mime_header_list_write_to_stream (GMIME_OBJECT (message)->headers, format, output);
		g_mime_stream_flush (output);
		g_object_unref (output);
		fprintf (stdout, "\n");
	}
#endif
	
#ifdef TEST_WRITE_TO_STREAM
	stream = g_mime_stream_pipe_new (2);
	g_mime_stream_pipe_set_owner ((GMimeStreamPipe *) stream, FALSE);
	g_mime_object_write_to_stream ((GMimeObject *) message, format, stream);
	g_mime_stream_flush (stream);
	g_object_unref (stream);
#endif
	
#ifdef PRINT_MIME_STRUCT
	print_mime_struct (message->mime_part, 0);
#elif defined (PRINT_MIME_STRUCT_ITER)
	print_mime_struct_iter (message);
#endif
	
	g_object_unref (message);
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
	
	g_mime_init ();
	
	if (argc > 1)
		filename = argv[1];
	else
		return 0;
	
	if ((fd = open (filename, O_RDONLY, 0)) == -1)
		return 0;
	
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
	
	return 0;
}
