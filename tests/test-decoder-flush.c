/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include <gmime/gmime.h>
#include <string.h>
#include "testsuite.h"

static void
test_headers (void)
{
	static const struct {
		const char *input, *expected;
	} words[] = {
		{ "=?utf-8?b?TQ?=", "M" },
		{ "=?utf-8?b?TWE?=", "Ma" },
		{ "=?utf-8?b?TQ=?=", "M" },
		{ "=?utf-8?q?abc=?=", "abc=" },
		{ "=?utf-8?q?abc=4?=", "abc=4" },
		{ "=?utf-8?q?=4?= =?utf-8?q?1?=", "A" },
		{ "=?utf-8?b?TW?= =?utf-8?b?Fu?=", "Man" },
		{ "=?utf-8?q?abc=?= =?utf-8?b?TQ?=", "abc=M" },
		{ "=?utf-8?b?TQ?= =?iso-8859-1?b?TWE?=", "MMa" },
		{ "=?utf-8?q?abc=?= text", "abc= text" },
		{ "prefix =?utf-8?b?TQ?= suffix", "prefix M suffix" }
	}, params[] = {
		{ "name*=utf-8''file%", "file%" },
		{ "name*=utf-8''file%2", "file%2" },
		{ "name*0*=utf-8''file%; name*1*=2Ezip", "file.zip" },
		{ "name*0*=utf-8''file%2; name*1*=Ezip", "file.zip" },
		{ "name*0*=utf-8''file%2; name*1=Ezip", "file%2Ezip" },
		{ "name*0*=utf-8''file; name*1*=%2", "file%2" }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeContentType *type;
	GError *error = NULL;
	char *text, *input;
	guint mode, i;

	for (mode = 0; mode < 2; mode++) {
		g_mime_parser_options_set_rfc2047_compliance_mode (options, mode);
		g_mime_parser_options_set_parameter_compliance_mode (options, mode);
		for (i = 0; i < G_N_ELEMENTS (words); i++) {
			text = g_mime_utils_header_decode_text (options, words[i].input);
			g_assert_cmpstr (text, ==, words[i].expected);
			g_free (text);
			text = g_mime_utils_header_decode_phrase (options, words[i].input);
			g_assert_cmpstr (text, ==, words[i].expected);
			g_free (text);
		}
		for (i = 0; i < G_N_ELEMENTS (params); i++) {
			input = g_strconcat ("application/octet-stream; ", params[i].input, NULL);
			type = g_mime_content_type_parse_full (options, input, strlen (input), &error);
			g_assert_no_error (error);
			g_assert_nonnull (type);
			g_assert_cmpstr (g_mime_content_type_get_parameter (type, "name"), ==, params[i].expected);
			g_object_unref (type);
			g_free (input);
		}
	}
	g_mime_parser_options_unref (options);
}

static void
test_streams (void)
{
	static const struct {
		GMimeContentEncoding encoding;
		const char *input, *expected;
	} cases[] = {
		{ GMIME_CONTENT_ENCODING_BASE64, "TQ", "M" },
		{ GMIME_CONTENT_ENCODING_BASE64, "TWE", "Ma" },
		{ GMIME_CONTENT_ENCODING_BASE64, "TQ=", "M" },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "abc=", "abc=" },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "abc=4", "abc=4" },
		{ GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE, "abc=\r", "abc=\r" },
		{ GMIME_CONTENT_ENCODING_UUENCODE, "begin 644 file\n#36%", "Ma" },
		{ GMIME_CONTENT_ENCODING_BINARY, "a\r\n=", "a\r\n=" },
		{ GMIME_CONTENT_ENCODING_UNSPECIFIED, "=J", "\340" }
	};
	GMimeStream *memory, *stream, *destination, *input;
	GMimeDecoder *decoder;
	GMimeFilter *filter;
	GByteArray *bytes;
	char *output, byte;
	size_t output_length, prespace;
	gsize i, split, length;
	guint read;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		length = strlen (cases[i].input);
		for (split = 0; split <= length; split++) {
			for (read = 0; read < 3; read++) {
				switch (cases[i].encoding) {
				case GMIME_CONTENT_ENCODING_BASE64:
					decoder = GMIME_DECODER (g_mime_base64_decoder_new ());
					break;
				case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
					decoder = GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
					break;
				case GMIME_CONTENT_ENCODING_UUENCODE:
					decoder = GMIME_DECODER (g_mime_uu_decoder_new (FALSE));
					break;
				case GMIME_CONTENT_ENCODING_BINARY:
					decoder = GMIME_DECODER (g_mime_pass_through_decoder_new (cases[i].encoding));
					break;
				default:
					decoder = GMIME_DECODER (g_mime_yenc_decoder_new (TRUE));
					break;
				}
				filter = g_mime_filter_basic_new_with_decoder (decoder);
				memory = read ? g_mime_stream_mem_new_with_buffer (cases[i].input, length) :
					g_mime_stream_mem_new ();
				input = read == 2 ? test_stream_onebyte_new (memory) : g_object_ref (memory);
				stream = g_mime_stream_filter_new (input);
				g_object_unref (input);
				g_mime_stream_filter_add (GMIME_STREAM_FILTER (stream), filter);
				if (read) {
					destination = g_mime_stream_mem_new ();
					g_assert_cmpint (g_mime_stream_write_to_stream (stream, destination), ==,
							 strlen (cases[i].expected));
					g_assert_cmpint (g_mime_stream_read (stream, &byte, 1), ==, 0);
					bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (destination));
				} else {
					destination = g_object_ref (memory);
					g_assert_cmpint (g_mime_stream_write (stream, cases[i].input, split), ==, split);
					g_assert_cmpint (g_mime_stream_write (stream, cases[i].input + split, length - split),
							 ==, length - split);
					g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
					g_assert_cmpint (g_mime_stream_flush (stream), ==, 0);
					g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
					bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (memory));
				}
				g_assert_cmpmem (bytes->data, bytes->len, cases[i].expected, strlen (cases[i].expected));
				g_mime_filter_reset (filter);
				g_byte_array_set_size (bytes, 0);
				g_mime_filter_filter (filter, (char *) cases[i].input, split, 0,
						      &output, &output_length, &prespace);
				g_byte_array_append (bytes, (guint8 *) output, output_length);
				g_mime_filter_complete (filter, (char *) cases[i].input + split, length - split, 0,
							&output, &output_length, &prespace);
				g_byte_array_append (bytes, (guint8 *) output, output_length);
				g_assert_cmpmem (bytes->data, bytes->len, cases[i].expected, strlen (cases[i].expected));
				g_mime_filter_complete (filter, "", 0, 0, &output, &output_length, &prespace);
				g_assert_cmpuint (output_length, ==, 0);
				g_object_unref (destination);
				g_object_unref (stream);
				g_object_unref (memory);
				g_object_unref (filter);
				g_object_unref (decoder);
			}
		}
	}
}

int
main (int argc, char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/decoder-flush/headers", test_headers);
	g_test_add_func ("/decoder-flush/streams", test_streams);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
