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

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>

#include <gmime/gmime.h>
#include "testsuite.h"

typedef struct {
	GMimeStream parent;
	const char *data;
	gsize length;
	gsize chunk_size;
	gsize fail_after;
	int error_number;
	gboolean seekable;
	gboolean fail_rewind;
	gboolean closed;
} TestInput;

typedef struct {
	GMimeStreamClass parent_class;
} TestInputClass;

G_DEFINE_TYPE (TestInput, test_input, GMIME_TYPE_STREAM)

static ssize_t
input_read (GMimeStream *stream, char *buffer, size_t length)
{
	TestInput *input = (TestInput *) stream;
	gsize position = stream->position;

	if (input->closed || position >= input->fail_after) {
		errno = input->closed ? EBADF : input->error_number;
		return -1;
	}
	length = MIN (length, input->length - position);
	length = MIN (length, input->chunk_size);
	length = MIN (length, input->fail_after - position);
	memcpy (buffer, input->data + position, length);
	stream->position += length;
	return length;
}

static gint64
input_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	TestInput *input = (TestInput *) stream;
	gint64 position;

	if (!input->seekable) {
		errno = ESPIPE;
		return -1;
	}
	if (input->fail_rewind && whence == GMIME_STREAM_SEEK_SET && offset == 0 && stream->position > 0) {
		errno = EIO;
		return -1;
	}
	position = whence == GMIME_STREAM_SEEK_CUR ? stream->position + offset :
		whence == GMIME_STREAM_SEEK_END ? (gint64) input->length + offset : offset;
	if (position < 0 || (guint64) position > input->length) {
		errno = EINVAL;
		return -1;
	}
	stream->position = position;
	return position;
}

static gint64
input_tell (GMimeStream *stream)
{
	if (!((TestInput *) stream)->seekable) {
		errno = ESPIPE;
		return -1;
	}
	return stream->position;
}

static gboolean
input_eos (GMimeStream *stream)
{
	return (guint64) stream->position == ((TestInput *) stream)->length;
}

static int
input_close (GMimeStream *stream)
{
	((TestInput *) stream)->closed = TRUE;
	return 0;
}

static void
test_input_class_init (TestInputClass *klass)
{
	GMimeStreamClass *stream_class = (GMimeStreamClass *) klass;

	stream_class->read = input_read;
	stream_class->seek = input_seek;
	stream_class->tell = input_tell;
	stream_class->eos = input_eos;
	stream_class->close = input_close;
}

static void
test_input_init (TestInput *input)
{
	g_mime_stream_construct ((GMimeStream *) input, 0, -1);
	input->chunk_size = 3;
	input->fail_after = G_MAXSIZE;
	input->error_number = EIO;
	input->seekable = TRUE;
}

static TestInput *
input_new (const char *data, gsize length)
{
	TestInput *input = g_object_new (test_input_get_type (), NULL);

	input->data = data;
	input->length = length;
	return input;
}

static void
check_content (GMimeEntity *entity, const char *expected, gsize length)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GByteArray *bytes;

	g_assert_cmpint (g_mime_content_decode_to_stream (g_mime_part_get_content (GMIME_PART (entity)), stream), >=, 0);
	bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
	g_assert_cmpuint (bytes->len, ==, length);
	if (length)
		g_assert_cmpint (memcmp (bytes->data, expected, length), ==, 0);
	g_object_unref (stream);
}

static void
check_metadata (GMimeEntity *entity, const char *type, const char *subtype, const char *name, gboolean linked)
{
	g_assert_true (g_mime_content_type_is_type (g_mime_entity_get_content_type (entity), type, subtype));
	g_assert_cmpstr (g_mime_entity_get_content_type_parameter (entity, "name"), ==, name);
	g_assert_cmpstr (g_mime_entity_get_content_disposition_parameter (entity, "filename"), ==, name);
	g_assert_cmpstr (g_mime_entity_get_disposition (entity), ==, linked ? "inline" : "attachment");
	g_assert_cmpstr (g_mime_entity_get_content_location (entity), ==, linked ? name : NULL);
}

static void
entity_finalized (gpointer data, GObject *object)
{
	*((gboolean *) data) = TRUE;
}

static void
test_collection (void)
{
	GMimeAttachmentCollection *collection = g_mime_attachment_collection_new (TRUE);
	GMimeEntity *first = (GMimeEntity *) g_mime_text_part_new ();
	GMimeEntity *second = (GMimeEntity *) g_mime_part_new ();
	GMimeEntity *other = (GMimeEntity *) g_mime_part_new ();
	gboolean finalized = FALSE;

	g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 0);
	g_mime_attachment_collection_clear (collection);
	g_mime_attachment_collection_add (collection, first);
	g_mime_attachment_collection_add (collection, first);
	g_mime_attachment_collection_add (collection, second);
	g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 3);
	g_assert_null (g_mime_entity_get_content_disposition (first));
	g_assert_null (g_mime_entity_get_content_location (first));
	g_assert_true (g_mime_attachment_collection_contains (collection, first));
	g_assert_false (g_mime_attachment_collection_contains (collection, other));
	g_assert_cmpint (g_mime_attachment_collection_index_of (collection, first), ==, 0);
	g_assert_cmpint (g_mime_attachment_collection_index_of (collection, second), ==, 2);
	g_assert_cmpint (g_mime_attachment_collection_index_of (collection, other), ==, -1);
	g_assert_true (g_mime_attachment_collection_remove (collection, first));
	g_assert_true (g_mime_attachment_collection_get_attachment (collection, 0) == first);
	g_assert_false (g_mime_attachment_collection_remove (collection, other));
	g_mime_attachment_collection_set_attachment (collection, 0, first);
	g_mime_attachment_collection_set_attachment (collection, 0, second);
	g_mime_attachment_collection_insert (collection, 1, first);
	g_assert_true (g_mime_attachment_collection_get_attachment (collection, 0) == second);
	g_assert_true (g_mime_attachment_collection_get_attachment (collection, 1) == first);
	g_assert_true (g_mime_attachment_collection_get_attachment (collection, 2) == second);
	g_mime_attachment_collection_remove_at (collection, 2);
	g_object_weak_ref (G_OBJECT (second), entity_finalized, &finalized);
	g_object_unref (second);
	g_assert_false (finalized);
	g_mime_attachment_collection_clear (collection);
	g_assert_true (finalized);
	g_assert_true (GMIME_IS_TEXT_PART (first));
	g_object_unref (other);
	g_object_unref (first);
	g_object_unref (collection);
}

static void
test_data_and_metadata (void)
{
	static const char *paths[] = {
		"photo.JPG", "dir" G_DIR_SEPARATOR_S "photo.JPG",
		G_DIR_SEPARATOR_S "photo.JPG", "dir" G_DIR_SEPARATOR_S,
		"other\\photo.JPG", "other/photo.JPG", "quoted \"name\".bin", "caf\303\251.bin",
		"urn:photo.JPG", "escaped%20name.bin", "fragment#name.bin"
	};
	GMimeAttachmentCollection *collection;
	GMimeContentType *type;
	GMimeEntity *entity;
	GError *error = NULL;
	guint8 data[] = { 0xff, 0x00, 0xff };
	const char *separator, *name;
	guint i, linked, count;

	for (linked = 0; linked < 2; linked++) {
		collection = g_mime_attachment_collection_new (linked);
		for (i = 0; i < G_N_ELEMENTS (paths); i++) {
			separator = strrchr (paths[i], G_DIR_SEPARATOR);
			name = separator && separator > paths[i] ? separator + 1 : paths[i];
			count = g_mime_attachment_collection_get_count (collection);
			entity = g_mime_attachment_collection_add_data (collection, paths[i], data, sizeof (data), NULL, &error);
			if (linked && (strchr (name, '\\') || strchr (name, ' ') || !strcmp (name, "urn:photo.JPG"))) {
				g_assert_null (entity);
				g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
				g_clear_error (&error);
				g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, count);
				continue;
			}
			g_assert_no_error (error);
			g_assert_nonnull (entity);
			g_assert_true (G_OBJECT_TYPE (entity) == GMIME_TYPE_PART);
			g_assert_cmpstr (g_mime_entity_get_content_type_parameter (entity, "name"), ==, name);
			g_assert_cmpstr (g_mime_entity_get_content_disposition_parameter (entity, "filename"), ==, name);
			g_assert_cmpstr (g_mime_entity_get_disposition (entity), ==, linked ? "inline" : "attachment");
			g_assert_cmpstr (g_mime_entity_get_content_location (entity), ==, linked ? name : NULL);
			g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, GMIME_CONTENT_ENCODING_BASE64);
			check_content (entity, (const char *) data, sizeof (data));
			g_object_unref (entity);
		}
		g_object_unref (collection);
	}

	collection = g_mime_attachment_collection_new (FALSE);
	type = g_mime_content_type_new ("image", "gif");
	g_mime_content_type_set_parameter (type, "x-test", "retained");
	entity = g_mime_attachment_collection_add_data (collection, "image.jpg", data, sizeof (data), type, &error);
	g_assert_no_error (error);
	check_metadata (entity, "image", "gif", "image.jpg", FALSE);
	g_assert_true (g_mime_entity_get_content_type (entity) == type);
	g_assert_cmpstr (g_mime_entity_get_content_type_parameter (entity, "x-test"), ==, "retained");
	data[0] = 0;
	check_content (entity, "\377\0\377", 3);
	g_mime_attachment_collection_clear (collection);
	check_content (entity, "\377\0\377", 3);
	g_object_unref (entity);
	g_object_unref (type);

	entity = g_mime_attachment_collection_add_data (collection, "empty.txt", NULL, 0, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_TEXT_PART (entity));
	g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, GMIME_CONTENT_ENCODING_7BIT);
	check_content (entity, "", 0);
	g_object_unref (entity);
	type = g_mime_content_type_new ("application", "pkcs7-mime");
	entity = g_mime_attachment_collection_add_data (collection, "part.p7m", data, sizeof (data), type, &error);
	g_assert_no_error (error);
	g_assert_true (G_OBJECT_TYPE (entity) == GMIME_TYPE_PART);
	g_object_unref (entity);
	g_object_unref (type);
	type = g_mime_content_type_new ("message", "global");
	entity = g_mime_attachment_collection_add_data (collection, "message.eml", data, sizeof (data), type, &error);
	g_assert_no_error (error);
	g_assert_true (G_OBJECT_TYPE (entity) == GMIME_TYPE_PART);
	g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, GMIME_CONTENT_ENCODING_BASE64);
	check_content (entity, (const char *) data, sizeof (data));
	g_object_unref (entity);
	g_object_unref (type);
	g_object_unref (collection);
}

static void
test_encoding_and_streams (void)
{
	static const struct {
		const char *data;
		gsize length;
		GMimeContentEncoding encoding;
	} cases[] = {
		{ "", 0, GMIME_CONTENT_ENCODING_7BIT },
		{ "plain\r\n", 7, GMIME_CONTENT_ENCODING_7BIT },
		{ "From \n", 6, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE },
		{ "From ", 5, GMIME_CONTENT_ENCODING_7BIT },
		{ "\303\251", 2, GMIME_CONTENT_ENCODING_BASE64 },
		{ "a\0b", 3, GMIME_CONTENT_ENCODING_BASE64 },
	};
	GMimeAttachmentCollection *collection = g_mime_attachment_collection_new (FALSE);
	GMimeContentType *type = g_mime_content_type_new ("text", "html");
	GMimeEntity *entity;
	GMimeStream *stream;
	TestInput *input;
	GError *error = NULL;
	guint i;
	char line[80];

	g_mime_content_type_set_charset (type, "utf-8");
	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		input = input_new (cases[i].data, cases[i].length);
		input->seekable = FALSE;
		entity = g_mime_attachment_collection_add_stream (collection, "text.txt", (GMimeStream *) input, type, &error);
		g_assert_no_error (error);
		g_assert_nonnull (entity);
		g_assert_false (input->closed);
		g_object_unref (input);
		g_assert_true (GMIME_IS_TEXT_PART (entity));
		check_metadata (entity, "text", "html", "text.txt", FALSE);
		g_assert_cmpstr (g_mime_content_type_get_charset (g_mime_entity_get_content_type (entity)), ==, "utf-8");
		g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, cases[i].encoding);
		check_content (entity, cases[i].data, cases[i].length);
		g_object_unref (entity);
	}

	memset (line, 'x', 78);
	line[78] = '\r';
	line[79] = '\n';
	input = input_new (line, sizeof (line));
	input->chunk_size = 1;
	entity = g_mime_attachment_collection_add_stream (collection, "line.txt", (GMimeStream *) input, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_part_get_content_encoding (GMIME_PART (entity)), ==, GMIME_CONTENT_ENCODING_7BIT);
	g_object_unref (input);
	check_content (entity, line, sizeof (line));
	g_object_unref (entity);

	stream = g_mime_stream_mem_new_with_buffer ("skipremaining", 13);
	g_assert_cmpint (g_mime_stream_seek (stream, 4, GMIME_STREAM_SEEK_SET), ==, 4);
	entity = g_mime_attachment_collection_add_stream (collection, "bytes.bin", stream, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_stream_tell (stream), ==, 13);
	g_assert_cmpint (g_mime_stream_close (stream), ==, 0);
	g_object_unref (stream);
	check_content (entity, "remaining", 9);
	g_object_unref (entity);
	g_object_unref (type);
	g_object_unref (collection);
}

static void
test_messages_and_failures (void)
{
	static const char message[] =
		"Subject: Attached message\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\nSGVsbG8sIHdvcmxkIQ==\r\n";
	const char invalid[] = "\377\0\377";
	GMimeAttachmentCollection *collection = g_mime_attachment_collection_new (TRUE);
	GMimeContentType *type = g_mime_content_type_new ("message", "rfc822");
	GMimeEntity *entity;
	GMimeMessage *nested;
	TestInput *input;
	GMimeStream *stream;
	GError *error = NULL;
	guint mode;

	g_mime_content_type_set_parameter (type, "x-test", "not-copied");
	for (mode = 0; mode < 3; mode++) {
		input = input_new (message, sizeof (message) - 1);
		input->seekable = mode != 0;
		entity = g_mime_attachment_collection_add_stream (collection, "message.eml", (GMimeStream *) input,
								mode == 2 ? type : NULL, &error);
		g_assert_no_error (error);
		g_assert_true (GMIME_IS_MESSAGE_PART (entity));
		g_assert_false (input->closed);
		g_object_unref (input);
		check_metadata (entity, "message", "rfc822", "message.eml", TRUE);
		g_assert_null (g_mime_entity_get_content_type_parameter (entity, "x-test"));
		g_assert_null (g_mime_content_type_get_parameter (type, "name"));
		nested = g_mime_message_part_get_message (GMIME_MESSAGE_PART (entity));
		g_assert_cmpstr (g_mime_message_get_subject (nested), ==, "Attached message");
		check_content (g_mime_message_get_mime_part (nested), "Hello, world!", 13);
		g_object_unref (entity);
	}
	g_mime_attachment_collection_clear (collection);
	entity = g_mime_attachment_collection_add_data (collection, "message.eml", (const guint8 *) message,
						      sizeof (message) - 1, type, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_MESSAGE_PART (entity));
	g_object_unref (entity);
	g_mime_attachment_collection_clear (collection);

	/* Failed autodetection must rewind to the original position, not to zero. */
	stream = g_mime_stream_mem_new_with_buffer ("skip\377\0\377", 7);
	g_mime_stream_seek (stream, 4, GMIME_STREAM_SEEK_SET);
	entity = g_mime_attachment_collection_add_stream (collection, "message.eml", stream, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (G_OBJECT_TYPE (entity) == GMIME_TYPE_PART);
	check_metadata (entity, "application", "octet-stream", "message.eml", TRUE);
	check_content (entity, invalid, sizeof (invalid) - 1);
	g_object_unref (stream);
	g_object_unref (entity);
	g_mime_attachment_collection_clear (collection);

	for (mode = 0; mode < 4; mode++) {
		input = input_new (invalid, sizeof (invalid) - 1);
		if (mode == 0)
			input->seekable = FALSE;
		else if (mode == 2)
			input->fail_after = 1;
		else if (mode == 3)
			input->fail_rewind = TRUE;
		entity = g_mime_attachment_collection_add_stream (collection, "message.eml", (GMimeStream *) input,
								mode == 1 ? type : NULL, &error);
		g_assert_null (entity);
		g_assert_error (error, GMIME_ERROR, (mode < 2 ? GMIME_ERROR_PARSE_ERROR : EIO));
		g_clear_error (&error);
		g_assert_false (input->closed);
		g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 0);
		g_object_unref (input);
	}

	for (mode = 0; mode < 2; mode++) {
		entity = g_mime_attachment_collection_add_data (collection, "empty.eml", NULL, 0, mode ? type : NULL, &error);
		if (mode) {
			g_assert_null (entity);
			g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
			g_clear_error (&error);
		} else {
			g_assert_no_error (error);
			g_assert_true (G_OBJECT_TYPE (entity) == GMIME_TYPE_PART);
			check_content (entity, "", 0);
			g_object_unref (entity);
			g_mime_attachment_collection_clear (collection);
		}
	}

	for (mode = 0; mode < 3; mode++) {
		input = input_new ("some attachment bytes", 21);
		input->fail_after = 4;
		input->error_number = mode == 2 ? 0 : EIO;
		entity = g_mime_attachment_collection_add_stream (collection, mode == 0 ? "file.bin" : "file.txt",
								(GMimeStream *) input, NULL, &error);
		g_assert_null (entity);
		g_assert_error (error, GMIME_ERROR, (mode == 2 ? GMIME_ERROR_GENERAL : EIO));
		g_clear_error (&error);
		g_assert_false (input->closed);
		g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 0);
		g_object_unref (input);
	}

	g_object_unref (type);
	g_object_unref (collection);
}

static void
test_files_and_registry (void)
{
	GMimeAttachmentCollection *collection = g_mime_attachment_collection_new (FALSE);
	GMimeContentType *type;
	GMimeEntity *entity;
	GError *error = NULL;
	char *directory, *path;
	char data[8193];

	memset (data, 'x', sizeof (data));
	data[0] = '\0';
	directory = g_dir_make_tmp ("gmime-attachments-XXXXXX", &error);
	g_assert_no_error (error);
	path = g_build_filename (directory, "payload.custom-attachment", NULL);
	g_assert_true (g_file_set_contents (path, data, sizeof (data), &error));
	g_assert_no_error (error);
	g_mime_types_register ("application/x-gmime-attachment", ".custom-attachment");
	entity = g_mime_attachment_collection_add_file (collection, path, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (entity);
	check_metadata (entity, "application", "x-gmime-attachment", "payload.custom-attachment", FALSE);
	check_content (entity, data, sizeof (data));
	g_object_unref (entity);
	type = g_mime_content_type_new ("text", "html");
	g_mime_content_type_set_charset (type, "utf-8");
	entity = g_mime_attachment_collection_add_file (collection, path, type, &error);
	g_assert_no_error (error);
	g_assert_true (GMIME_IS_TEXT_PART (entity));
	g_assert_true (g_mime_entity_get_content_type (entity) == type);
	g_assert_cmpstr (g_mime_content_type_get_charset (type), ==, "utf-8");
	g_assert_cmpint (g_remove (path), ==, 0);
	check_content (entity, data, sizeof (data));
	check_content (g_mime_attachment_collection_get_attachment (collection, 0), data, sizeof (data));
	g_object_unref (entity);
	g_object_unref (type);
	entity = g_mime_attachment_collection_add_file (collection, path, NULL, &error);
	g_assert_null (entity);
	g_assert_error (error, GMIME_ERROR, ENOENT);
	g_clear_error (&error);
	g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 2);
	g_mime_types_register ("not-a-mime-type", ".invalid-attachment-type");
	entity = g_mime_attachment_collection_add_data (collection, "file.invalid-attachment-type",
						      (const guint8 *) "x", 1, NULL, &error);
	g_assert_null (entity);
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 2);
	g_assert_cmpint (g_rmdir (directory), ==, 0);
	g_free (directory);
	g_free (path);
	g_object_unref (collection);
}

static void
test_preconditions (void)
{
	GMimeAttachmentCollection *collection = g_mime_attachment_collection_new (FALSE);
	GMimeEntity *entity = (GMimeEntity *) g_mime_part_new ();
	GMimeStream *stream = g_mime_stream_mem_new ();
	GError *error = NULL;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_attachment_collection_insert (collection, 0, entity);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_assert_null (g_mime_attachment_collection_add_data (collection, "\377", NULL, 0, NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_assert_null (g_mime_attachment_collection_add_stream (collection, "\377", stream, NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*g_utf8_validate*");
	g_assert_null (g_mime_attachment_collection_add_file (collection, "\377", NULL, &error));
	g_test_assert_expected_messages ();
	g_mime_attachment_collection_add (collection, entity);
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_mime_attachment_collection_insert (collection, 1, entity);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*index >= 0*");
	g_assert_null (g_mime_attachment_collection_get_attachment (collection, -1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_ENTITY*");
	g_mime_attachment_collection_set_attachment (collection, 0, NULL);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*file_name != NULL*");
	g_assert_null (g_mime_attachment_collection_add_data (collection, "", NULL, 0, NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*data != NULL*");
	g_assert_null (g_mime_attachment_collection_add_data (collection, "file", NULL, 1, NULL, &error));
	g_test_assert_expected_messages ();
	if (G_MAXSIZE > G_MAXUINT) {
		gsize length = G_MAXUINT;

		length++;
		g_assert_null (g_mime_attachment_collection_add_data (collection, "large", (const guint8 *) "x", length, NULL, &error));
		g_assert_error (error, GMIME_ERROR, EOVERFLOW);
		g_clear_error (&error);
	}
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_attachment_collection_get_count (collection), ==, 1);
	g_object_unref (stream);
	g_object_unref (entity);
	g_object_unref (collection);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MimeKit attachment collection");
	test_collection ();
	test_data_and_metadata ();
	test_encoding_and_streams ();
	test_messages_and_failures ();
	test_files_and_registry ();
	test_preconditions ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
