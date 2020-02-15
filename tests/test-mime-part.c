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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x) 
#define v(x) if (verbose > 3) x;

static GByteArray *
read_all_bytes (const char *path, gboolean is_text)
{
	GMimeStream *filtered, *stream, *mem;
	GMimeFilter *filter;
	GByteArray *buffer;
	
	buffer = g_byte_array_new ();
	
	if (!(stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL)))
		return buffer;
	
	mem = g_mime_stream_mem_new_with_byte_array (buffer);
	g_mime_stream_mem_set_owner ((GMimeStreamMem *) mem, FALSE);
	
	if (is_text) {
		filtered = g_mime_stream_filter_new (mem);
		filter = g_mime_filter_dos2unix_new (FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		g_object_unref (mem);
	} else {
		filtered = mem;
	}
	
	g_mime_stream_write_to_stream (stream, filtered);
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	g_object_unref (stream);
	
	return buffer;
}

static void
test_content_disposition (GMimePart *mime_part, const char *filename)
{
	const char *what = "Content-Disposition";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_filename (mime_part))) {
		testsuite_check_failed ("%s failed: filename and name parameters were NULL", what);
		return;
	}
	
	if (strcmp (value, filename) != 0) {
		testsuite_check_failed ("%s failed: filenames do not match: %s", what, value);
		return;
	}
	
	if (!g_mime_part_is_attachment (mime_part)) {
		testsuite_check_failed ("%s failed: mime part is not an attachment", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_description (GMimePart *mime_part, const char *description)
{
	const char *what = "Content-Description";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_description (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_description() returned NULL", what);
		return;
	}
	
	if (strcmp (value, description) != 0) {
		testsuite_check_failed ("%s failed: descriptions do not match: %s", what, value);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_location (GMimePart *mime_part, const char *location)
{
	const char *what = "Content-Location";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_location (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_location() returned NULL", what);
		return;
	}
	
	if (strcmp (value, location) != 0) {
		testsuite_check_failed ("%s failed: locationss do not match: %s", what, value);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_id (GMimePart *mime_part, const char *id)
{
	const char *what = "Content-Id";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_id (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_id() returned NULL", what);
		return;
	}
	
	if (strcmp (value, id) != 0) {
		testsuite_check_failed ("%s failed: ids do not match: %s", what, value);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_md5 (GMimePart *mime_part, const char *md5sum)
{
	const char *what = "Content-Md5";
	const char *value;
	
	testsuite_check ("%s", what);
	
	if (!(value = g_mime_part_get_content_md5 (mime_part))) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_md5() returned NULL", what);
		return;
	}
	
	if (strcmp (value, md5sum) != 0) {
		testsuite_check_failed ("%s failed: md5's do not match: %s", what, value);
		return;
	}
	
	if (!g_mime_part_verify_content_md5 (mime_part)) {
		testsuite_check_failed ("%s failed: md5sum did not verify", what);
		return;
	}
	
	testsuite_check_passed ();
}

static const char *constraints[] = {
	"7bit", "8bit", "binary"
};

static void
test_content_transfer_encoding (GMimePart *mime_part, GMimeEncodingConstraint constraint, GMimeContentEncoding encoding)
{
	const char *what = "Content-Transfer-Encoding";
	GMimeContentEncoding value;
	
	testsuite_check ("%s (constraint: %s)", what, constraints[constraint]);
	
	value = g_mime_part_get_best_content_encoding (mime_part, constraint);
	
	if (value != encoding) {
		testsuite_check_failed ("%s failed: g_mime_part_get_best_encoding() returned %s",
					what, g_mime_content_encoding_to_string (value));
		return;
	}
	
	g_mime_part_set_content_encoding (mime_part, encoding);
	value = g_mime_part_get_content_encoding (mime_part);
	
	if (value != encoding) {
		testsuite_check_failed ("%s failed: g_mime_part_get_content_encoding() returned %s",
					what, g_mime_content_encoding_to_string (value));
		return;
	}
	
	testsuite_check_passed ();
}

static GMimePart *
create_mime_part (const char *type, const char *subtype, const char *datadir, const char *filename)
{
	GMimeDataWrapper *content;
	GMimePart *mime_part;
	GMimeStream *stream;
	char *path;
	
	path = g_build_filename (datadir, filename, NULL);
	stream = g_mime_stream_fs_open (path, O_RDONLY, 0644, NULL);
	content = g_mime_data_wrapper_new_with_stream (stream, GMIME_CONTENT_ENCODING_DEFAULT);
	g_object_unref (stream);
	g_free (path);
	
	mime_part = g_mime_part_new_with_type (type, subtype);
	g_mime_part_set_content (mime_part, content);
	g_object_unref (content);
	
	return mime_part;
}

static void
test_clearing_headers (GMimePart *mime_part)
{
	const char *what = "GMimePart::HeaderList::clear()";
	GMimeHeaderList *list;
	
	testsuite_check ("%s", what);
	
	list = g_mime_object_get_header_list ((GMimeObject *) mime_part);
	g_mime_header_list_clear (list);
	
	if (g_mime_object_get_content_disposition ((GMimeObject *) mime_part) != NULL) {
		testsuite_check_failed ("%s: failed: Content-Disposition still set", what);
		return;
	}
	
	if (g_mime_part_get_content_description (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Description still set", what);
		return;
	}
	
	if (g_mime_part_get_content_location (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Location still set", what);
		return;
	}
	
	if (g_mime_part_get_content_id (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Id still set", what);
		return;
	}
	
	if (g_mime_part_get_content_md5 (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Md5 still set", what);
		return;
	}
	
	if (g_mime_part_get_content_encoding (mime_part) != GMIME_CONTENT_ENCODING_DEFAULT) {
		testsuite_check_failed ("%s failed: Content-Transfer-Encoding still set", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_removing_headers (GMimePart *mime_part, const char *filename)
{
	const char *what = "GMimePart::HeaderList::remove()";
	GMimeHeaderList *list;
	const char *value;
	
	testsuite_check ("%s", what);
	
	list = g_mime_object_get_header_list ((GMimeObject *) mime_part);
	
	if (!g_mime_header_list_remove (list, "Content-Disposition")) {
		testsuite_check_failed ("%s failed: could not remove Content-Disposition header", what);
		return;
	}
	
	if ((value = g_mime_part_get_filename (mime_part)) == NULL) {
		testsuite_check_failed ("%s failed: filename should still work (via name parameter)", what);
		return;
	}
	
	if (strcmp (value, filename) != 0) {
		testsuite_check_failed ("%s failed: names do not match: %s", what, value);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Description")) {
		testsuite_check_failed ("%s failed: could not remove Content-Description header", what);
		return;
	}
	
	if (g_mime_part_get_content_description (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Description still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Location")) {
		testsuite_check_failed ("%s failed: could not remove Content-Location header", what);
		return;
	}
	
	if (g_mime_part_get_content_location (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Location still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Id")) {
		testsuite_check_failed ("%s failed: could not remove Content-Id header", what);
		return;
	}
	
	if (g_mime_part_get_content_id (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Id still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Md5")) {
		testsuite_check_failed ("%s failed: could not remove Content-Md5 header", what);
		return;
	}
	
	if (g_mime_part_get_content_md5 (mime_part) != NULL) {
		testsuite_check_failed ("%s failed: Content-Md5 still set", what);
		return;
	}
	
	if (!g_mime_header_list_remove (list, "Content-Transfer-Encoding")) {
		testsuite_check_failed ("%s failed: could not remove Content-Transfer-Encoding header", what);
		return;
	}
	
	if (g_mime_part_get_content_encoding (mime_part) != GMIME_CONTENT_ENCODING_DEFAULT) {
		testsuite_check_failed ("%s failed: Content-Transfer-Encoding still set", what);
		return;
	}
	
	testsuite_check_passed ();
}

static void
test_content_headers (const char *datadir)
{
	const char *description = "I was such a Jurassic Park fanboy as a kid...";
	const char *location = "http://jurassic-park.com/raptors.png";
	const char *id = "raptors@jurassic-park.com";
	const char *md5 = "Av+KQAT/2KFlDYeGHib8kQ==";
	const char *filename = "raptors.png";
	GMimePart *mime_part;
	
	mime_part = create_mime_part ("image", "png", datadir, filename);
	
	g_mime_part_set_filename (mime_part, filename);
	test_content_disposition (mime_part, filename);
	
	g_mime_part_set_content_description (mime_part, description);
	test_content_description (mime_part, description);
	
	g_mime_part_set_content_location (mime_part, location);
	test_content_location (mime_part, location);
	
	g_mime_part_set_content_id (mime_part, id);
	test_content_id (mime_part, id);
	
	g_mime_part_set_content_md5 (mime_part, NULL);
	test_content_md5 (mime_part, md5);
	
	test_content_transfer_encoding (mime_part, GMIME_ENCODING_CONSTRAINT_BINARY, GMIME_CONTENT_ENCODING_BINARY);
	test_content_transfer_encoding (mime_part, GMIME_ENCODING_CONSTRAINT_8BIT, GMIME_CONTENT_ENCODING_BASE64);
	test_content_transfer_encoding (mime_part, GMIME_ENCODING_CONSTRAINT_7BIT, GMIME_CONTENT_ENCODING_BASE64);
	
	test_clearing_headers (mime_part);
	
	/* re-set the headers */
	g_mime_part_set_filename (mime_part, filename);
	g_mime_part_set_content_description (mime_part, description);
	g_mime_part_set_content_location (mime_part, location);
	g_mime_part_set_content_id (mime_part, id);
	g_mime_part_set_content_md5 (mime_part, md5);
	g_mime_part_set_content_encoding (mime_part, GMIME_CONTENT_ENCODING_BASE64);
	
	test_removing_headers (mime_part, filename);
	
	g_object_unref (mime_part);
}

static void
test_write_to_stream (const char *datadir, const char *output, GMimeContentEncoding encoding)
{
	const char *description = "I was such a Jurassic Park fanboy as a kid...";
	const char *location = "http://jurassic-park.com/raptors.png";
	const char *id = "raptors@jurassic-park.com";
	const char *md5 = "Av+KQAT/2KFlDYeGHib8kQ==";
	const char *filename = "raptors.png";
	const char *what = "GMimePart::write_to_stream()";
	GMimeFormatOptions *options;
	GMimePart *mime_part;
	GMimeStream *stream;
	GByteArray *expected;
	GByteArray *actual;
	char *path;
	
	testsuite_check ("%s (%s)", what, output);
	
	options = g_mime_format_options_clone (NULL);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	
	path = g_build_filename (datadir, output, NULL);
	expected = read_all_bytes (path, TRUE);
	
	mime_part = create_mime_part ("image", "png", datadir, filename);
	g_mime_part_set_filename (mime_part, filename);
	g_mime_part_set_content_description (mime_part, description);
	g_mime_part_set_content_location (mime_part, location);
	g_mime_part_set_content_id (mime_part, id);
	g_mime_part_set_content_md5 (mime_part, md5);
	
	if (encoding == GMIME_CONTENT_ENCODING_DEFAULT)
		g_mime_object_encode ((GMimeObject *) mime_part, GMIME_ENCODING_CONSTRAINT_7BIT);
	else
		g_mime_part_set_content_encoding (mime_part, encoding);
	
	actual = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array ((GMimeStreamMem *) stream, actual);
	g_mime_object_write_to_stream ((GMimeObject *) mime_part, options, stream);
	g_object_unref (mime_part);
	g_object_unref (stream);
	
	if (actual->len != expected->len) {
		testsuite_check_failed ("%s failed: lengths did not match (%u vs %u)", what, actual->len, expected->len);
		if (expected->len == 0) {
			stream = g_mime_stream_fs_open (path, O_WRONLY | O_CREAT, 0644, NULL);
			g_mime_stream_write (stream, (const char *) actual->data, actual->len);
			g_mime_stream_flush (stream);
			g_object_unref (stream);
		}
		goto error;
	}
	
	if (memcmp (actual->data, expected->data, actual->len) != 0) {
		testsuite_check_failed ("%s failed: streams did not match", what);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	g_byte_array_free (expected, TRUE);
	g_byte_array_free (actual, TRUE);
	g_free (path);
}

static char *openpgp_data_types[] = {
	"GMIME_OPENPGP_DATA_NONE",
	"GMIME_OPENPGP_DATA_ENCRYPTED",
	"GMIME_OPENPGP_DATA_SIGNED",
	"GMIME_OPENPGP_DATA_PUBLIC_KEY",
	"GMIME_OPENPGP_DATA_PRIVATE_KEY"
};

static void
test_openpgp_data (const char *datadir, const char *filename, GMimeOpenPGPData expected)
{
	const char *what = "GMimePart::get_openpgp_data()";
	GMimeOpenPGPData actual;
	GMimePart *mime_part;
	
	testsuite_check ("%s (%s)", what, filename);
	
	mime_part = create_mime_part ("application", "octet-stream", datadir, filename);
	
	if ((actual = g_mime_part_get_openpgp_data (mime_part)) != expected) {
		testsuite_check_failed ("%s failed: expected=%s; actual=%s", what,
					openpgp_data_types[expected], openpgp_data_types[actual]);
		goto error;
	}
	
	testsuite_check_passed ();
	
error:
	g_object_unref (mime_part);
}

static void
test_text_part (const char *datadir, const char *filename, const char *expected_charset)
{
	const char *what = "GMimeTextPart";
	GByteArray *expected;
	GMimeTextPart *part;
	const char *charset;
	char *text, *path;
	
	testsuite_check ("%s (%s)", what, filename);
	
	path = g_build_filename (datadir, filename, NULL);
	expected = read_all_bytes (path, TRUE);
	g_byte_array_append (expected, (guint8 *) "", 1);
	g_free (path);
	
	part = g_mime_text_part_new ();
	g_mime_text_part_set_text (part, (const char *) expected->data);
	
	charset = g_mime_text_part_get_charset (part);
	if (charset == NULL) {
		testsuite_check_failed ("%s failed: charset is NULL", what);
		goto cleanup;
	}
	
	if (strcmp (charset, expected_charset) != 0) {
		testsuite_check_failed ("%s failed: charsets do not match: expected=%s; actual=%s",
					what, expected_charset, charset);
		goto cleanup;
	}
	
	text = g_mime_text_part_get_text (part);
	if (text == NULL) {
		testsuite_check_failed ("%s failed: text is NULL", what);
		goto cleanup;
	}
	
	if (strcmp (text, (const char *) expected->data) != 0) {
		testsuite_check_failed ("%s failed: text does not match: expected=%s; actual=%s",
					what, (const char *) expected->data, text);
		g_free (text);
		goto cleanup;
	}
	
	testsuite_check_passed ();
	
cleanup:
	
	g_byte_array_free (expected, TRUE);
	g_object_unref (part);
}

int main (int argc, char **argv)
{
	const char *datadir = "data/mime-part";
	struct stat st;
	int i;
	
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	//verbose = 4;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			datadir = argv[i];
			break;
		}
	}
	
	if (i < argc && (stat (datadir, &st) == -1 || !S_ISDIR (st.st_mode)))
		return 0;
	
	testsuite_start ("GMimePart");
	
	test_content_headers (datadir);
	
	test_write_to_stream (datadir, "raptors.b64.txt", GMIME_CONTENT_ENCODING_DEFAULT);
	test_write_to_stream (datadir, "raptors.uu.txt", GMIME_CONTENT_ENCODING_UUENCODE);
	
	test_openpgp_data (datadir, "raptors.png", GMIME_OPENPGP_DATA_NONE);
	test_openpgp_data (datadir, "signed-body.txt", GMIME_OPENPGP_DATA_SIGNED);
	test_openpgp_data (datadir, "encrypted-body.txt", GMIME_OPENPGP_DATA_ENCRYPTED);
	test_openpgp_data (datadir, "pubkey-body.txt", GMIME_OPENPGP_DATA_PUBLIC_KEY);
	test_openpgp_data (datadir, "privkey-body.txt", GMIME_OPENPGP_DATA_PRIVATE_KEY);
	
	test_text_part (datadir, "french-fable.txt", "iso-8859-1");
	
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
