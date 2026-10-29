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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gmime/gmime.h>
#include "testsuite.h"

#ifdef GMIME_PRIVATE_TESTS
#error This test must compile against the public API only.
#endif

typedef struct {
	GMimeStreamMem parent_instance;
	guint writes;
	guint tells;
} PublicStream;

typedef GMimeStreamMemClass PublicStreamClass;

G_DEFINE_TYPE (PublicStream, public_stream, GMIME_TYPE_STREAM_MEM)

static ssize_t
public_stream_write (GMimeStream *stream, const char *buffer, size_t length)
{
	PublicStream *self = (PublicStream *) stream;
	gint64 start, end;

	g_mime_stream_get_bounds (stream, &start, &end);
	g_assert_cmpint (g_mime_stream_get_position (stream), >=, start);
	g_assert_true (end == -1 || g_mime_stream_get_position (stream) <= end);
	self->writes++;

	return GMIME_STREAM_CLASS (public_stream_parent_class)->write (stream, buffer, length);
}

static gint64
public_stream_tell (GMimeStream *stream)
{
	((PublicStream *) stream)->tells++;

	return g_mime_stream_get_position (stream);
}

static void
public_stream_class_init (PublicStreamClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	stream_class->write = public_stream_write;
	stream_class->tell = public_stream_tell;
}

static void
public_stream_init (PublicStream *self)
{
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (self), g_byte_array_new ());
	g_mime_stream_mem_set_owner (GMIME_STREAM_MEM (self), TRUE);
	self->writes = 0;
	self->tells = 0;
}

typedef struct {
	GMimeFilter parent_instance;
	guint calls;
} PublicFilter;

typedef GMimeFilterClass PublicFilterClass;

G_DEFINE_TYPE (PublicFilter, public_filter, GMIME_TYPE_FILTER)

static GMimeFilter *
public_filter_copy (GMimeFilter *filter)
{
	return g_object_new (public_filter_get_type (), NULL);
}

static void
public_filter_filter (GMimeFilter *filter, char *input, size_t length, size_t prespace,
		      char **output, size_t *outlen, size_t *outprespace)
{
	size_t capacity, i;

	((PublicFilter *) filter)->calls++;
	g_mime_filter_set_size (filter, length, FALSE);
	*output = g_mime_filter_get_output_buffer (filter, &capacity, outprespace);
	g_assert_cmpuint (capacity, >=, length);
	for (i = 0; i < length; i++)
		(*output)[i] = g_ascii_toupper (input[i]);
	*outlen = length;
}

static void
public_filter_reset (GMimeFilter *filter)
{
	((PublicFilter *) filter)->calls = 0;
}

static void
public_filter_class_init (PublicFilterClass *klass)
{
	klass->copy = public_filter_copy;
	klass->filter = public_filter_filter;
	klass->complete = public_filter_filter;
	klass->reset = public_filter_reset;
}

static void
public_filter_init (PublicFilter *self)
{
	self->calls = 0;
}

static void
test_public_stream (void)
{
	PublicStream *self = g_object_new (public_stream_get_type (), NULL);
	GMimeStream *stream = GMIME_STREAM (self);
	GByteArray *array = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (self));
	gint64 start, end;
	char buffer[4];

	g_assert_cmpint (g_mime_stream_write_string (stream, "abcdef"), ==, 6);
	g_assert_cmpuint (self->writes, ==, 1);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 6);
	g_assert_cmpuint (self->tells, ==, 1);
	g_assert_cmpmem (array->data, array->len, "abcdef", 6);

	g_mime_stream_set_bounds (stream, 1, 4);
	g_mime_stream_get_bounds (stream, &start, &end);
	g_assert_cmpint (start, ==, 1);
	g_assert_cmpint (end, ==, 4);
	g_mime_stream_get_bounds (stream, NULL, NULL);
	g_mime_stream_set_position (stream, 2);
	g_assert_cmpuint (self->tells, ==, 1);
	g_assert_cmpint (g_mime_stream_read (stream, buffer, sizeof (buffer)), ==, 2);
	g_assert_cmpmem (buffer, 2, "cd", 2);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 4);
	g_assert_cmpuint (self->tells, ==, 2);
	g_assert_cmpint (g_mime_stream_seek (stream, 1, GMIME_STREAM_SEEK_SET), ==, 1);
	g_assert_cmpint (g_mime_stream_write_string (stream, "WXYZ"), ==, 3);
	g_assert_cmpuint (self->writes, ==, 2);
	g_assert_cmpint (g_mime_stream_get_position (stream), ==, 4);
	g_assert_cmpmem (array->data, array->len, "aWXYef", 6);
	g_object_unref (stream);
}

static void
test_public_filter (void)
{
	PublicFilter *self = g_object_new (public_filter_get_type (), NULL);
	GMimeFilter *filter = GMIME_FILTER (self), *copy;
	char *output, input[] = "cd";
	size_t length, prespace, capacity;

	g_assert_null (g_mime_filter_get_output_buffer (filter, &capacity, NULL));
	g_assert_cmpuint (capacity, ==, 0);
	g_mime_filter_backup (filter, "ab", 2);
	g_mime_filter_filter (filter, input, 2, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "ABCD", 4);
	g_assert_cmpuint (self->calls, ==, 1);

	g_mime_filter_set_size (filter, 4096, TRUE);
	output = g_mime_filter_get_output_buffer (filter, &capacity, &prespace);
	g_assert_cmpuint (capacity, >=, 4096);
	g_assert_cmpmem (output, 4, "ABCD", 4);
	g_assert_cmpuint (prespace, >, 0);
	output[-1] = '!';
	g_assert_cmpint (output[-1], ==, '!');
	g_mime_filter_complete (filter, input, 2, 0, &output, &length, &prespace);
	g_assert_cmpmem (output, length, "CD", 2);
	g_assert_cmpuint (self->calls, ==, 2);

	copy = g_mime_filter_copy (filter);
	g_assert_cmpuint (((PublicFilter *) copy)->calls, ==, 0);
	g_mime_filter_reset (filter);
	g_assert_cmpuint (self->calls, ==, 0);
	g_object_unref (copy);
	g_object_unref (filter);
}

static void
test_public_statistics (void)
{
	GMimeStream *stream = g_mime_stream_null_new ();
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);

	g_mime_stream_null_set_count_newlines (null, TRUE);
	g_assert_cmpint (g_mime_stream_write_string (stream, "a\nb\n"), ==, 4);
	g_assert_cmpint (g_mime_stream_seek (stream, 0, GMIME_STREAM_SEEK_SET), ==, 0);
	g_assert_cmpint (g_mime_stream_write_string (stream, "\n"), ==, 1);
	g_assert_cmpuint (g_mime_stream_null_get_written (null), ==, 5);
	g_assert_cmpuint (g_mime_stream_null_get_newlines (null), ==, 3);
	g_assert_cmpint (g_mime_stream_length (stream), ==, 4);
	g_assert_cmpint (g_mime_stream_reset (stream), ==, 0);
	g_assert_cmpuint (g_mime_stream_null_get_written (null), ==, 0);
	g_assert_cmpuint (g_mime_stream_null_get_newlines (null), ==, 0);
	g_object_unref (stream);
}

static void
assert_derivable (GType type, guint instance_size)
{
	GTypeQuery query;

	g_type_query (type, &query);
	g_assert_cmpuint (query.instance_size, ==, instance_size);
	g_assert_false (g_type_test_flags (type, G_TYPE_FLAG_FINAL));
}

static void
test_type_contracts (void)
{
	GType final_types[] = {
		GMIME_TYPE_ANONYMIZER, GMIME_TYPE_ATTACHMENT_COLLECTION,
		GMIME_TYPE_AUTOCRYPT_HEADER, GMIME_TYPE_AUTOCRYPT_HEADER_LIST,
		GMIME_TYPE_BODY_BUILDER, GMIME_TYPE_CERTIFICATE, GMIME_TYPE_CERTIFICATE_LIST,
		GMIME_TYPE_CONTENT_TYPE, GMIME_TYPE_DECRYPT_RESULT, GMIME_TYPE_CONTENT_DISPOSITION,
		GMIME_TYPE_DOMAIN_LIST, GMIME_TYPE_HEADER_LIST_COLLECTION,
		GMIME_TYPE_HEADER, GMIME_TYPE_HEADER_LIST, GMIME_TYPE_INTERNET_ADDRESS_LIST,
		GMIME_TYPE_ITERATOR, GMIME_TYPE_MESSAGE_ID_LIST, GMIME_TYPE_PARAM, GMIME_TYPE_PARAM_LIST,
		GMIME_TYPE_PARSER, GMIME_TYPE_RECEIVED_CLAUSE, GMIME_TYPE_RECEIVED,
		GMIME_TYPE_SIGNATURE, GMIME_TYPE_SIGNATURE_LIST,
		GMIME_TYPE_GPG_CONTEXT, GMIME_TYPE_PKCS7_CONTEXT,
		GMIME_TYPE_YENC_HEADER, GMIME_TYPE_YENC_READER, GMIME_TYPE_YENC_WRITER,
		GMIME_TYPE_BASE64_ENCODER, GMIME_TYPE_BASE64_DECODER,
		GMIME_TYPE_QUOTED_PRINTABLE_ENCODER, GMIME_TYPE_QUOTED_PRINTABLE_DECODER,
		GMIME_TYPE_UU_ENCODER, GMIME_TYPE_UU_DECODER,
		GMIME_TYPE_YENC_ENCODER, GMIME_TYPE_YENC_DECODER,
		GMIME_TYPE_PASS_THROUGH_ENCODER, GMIME_TYPE_PASS_THROUGH_DECODER,
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (final_types); i++)
		g_assert_true (g_type_test_flags (final_types[i], G_TYPE_FLAG_FINAL));

	assert_derivable (GMIME_TYPE_ENTITY, sizeof (GObject));
	assert_derivable (GMIME_TYPE_PART, sizeof (GMimeEntity));
	assert_derivable (GMIME_TYPE_TEXT_PART, sizeof (GMimePart));
	assert_derivable (GMIME_TYPE_MULTIPART, sizeof (GMimeEntity));
	assert_derivable (GMIME_TYPE_MESSAGE, sizeof (GMimeEntity));
	assert_derivable (GMIME_TYPE_CONTENT, sizeof (GObject));
	assert_derivable (GMIME_TYPE_STREAM, sizeof (GObject));
	assert_derivable (GMIME_TYPE_STREAM_MEM, sizeof (GMimeStream));
	assert_derivable (GMIME_TYPE_FILTER, sizeof (GObject));
	assert_derivable (GMIME_TYPE_FILTER_BASIC, sizeof (GMimeFilter));
	assert_derivable (GMIME_TYPE_INTERNET_ADDRESS, sizeof (GObject));
	assert_derivable (GMIME_TYPE_MAILBOX_ADDRESS, sizeof (GMimeInternetAddress));
	assert_derivable (GMIME_TYPE_GROUP_ADDRESS, sizeof (GMimeInternetAddress));
	assert_derivable (GMIME_TYPE_CRYPTO_CONTEXT, sizeof (GObject));
	assert_derivable (GMIME_TYPE_READER, sizeof (GObject));
	assert_derivable (GMIME_TYPE_VISITOR, sizeof (GObject));
}

int
main (int argc, char **argv)
{
	testsuite_init (argc, argv);
	g_mime_init ();
	testsuite_start ("Public instance encapsulation and subclassing");
	testsuite_check ("Opaque final types and parent-only derivable instances");
	test_type_contracts ();
	testsuite_check_passed ();
	testsuite_check ("Stream subclassing with public bookkeeping");
	test_public_stream ();
	testsuite_check_passed ();
	testsuite_check ("Filter subclassing with a borrowed output buffer");
	test_public_filter ();
	testsuite_check_passed ();
	testsuite_check ("Null-stream statistics accessors");
	test_public_statistics ();
	testsuite_check_passed ();
	testsuite_end ();
	g_mime_shutdown ();

	return testsuite_exit ();
}
