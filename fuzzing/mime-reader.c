/* Adapted from MimeKit.Fuzzing.Targets.MimeReaderTarget and
 * Helpers.RecordingMimeReader. Copyright (c) 2013-2026 .NET Foundation
 * and Contributors. Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"

typedef struct {
	GMimeReader parent_instance;
	GString *trace;
	gsize length, pending_length;
	guint64 pending_hash;
	gint pending_kind;
	guint entities;
} FuzzReader;

typedef GMimeReaderClass FuzzReaderClass;

G_DEFINE_TYPE (FuzzReader, fuzz_reader, GMIME_TYPE_READER)

static void
flush_reads (FuzzReader *reader)
{
	if (!reader->pending_kind)
		return;
	g_string_append_printf (reader->trace, "R:%d:%" G_GSIZE_FORMAT ":%" G_GUINT64_FORMAT "\n",
				reader->pending_kind, reader->pending_length, reader->pending_hash);
	reader->pending_kind = 0;
}

static void
record_read (FuzzReader *reader, gint kind, const guint8 *buffer, gsize length)
{
	FUZZ_CHECK (buffer != NULL || length == 0, "NULL callback buffer");
	FUZZ_CHECK (length <= reader->length && reader->pending_length <= reader->length, "Oversized callback buffer");
	if (reader->pending_kind != kind) {
		flush_reads (reader);
		reader->pending_kind = kind;
		reader->pending_length = 0;
		reader->pending_hash = FUZZ_HASH_INIT;
	}
	reader->pending_hash = fuzz_hash (buffer, length, reader->pending_hash);
	reader->pending_length += length;
	FUZZ_CHECK (reader->pending_length <= reader->length, "Read callbacks exceed input length");
}

static void
check_range (FuzzReader *reader, gint64 begin, guint64 line, gint64 end)
{
	FUZZ_CHECK (begin >= 0 && begin <= end && (guint64) end <= reader->length && line >= 1,
		    "Invalid callback range: %" G_GINT64_FORMAT ":%" G_GINT64_FORMAT " line %" G_GUINT64_FORMAT,
		    begin, end, line);
}

static gboolean
record_begin (GMimeReader *base, const char *kind, gint64 begin, guint64 line)
{
	FuzzReader *reader = (FuzzReader *) base;

	check_range (reader, begin, line, begin);
	flush_reads (reader);
	g_string_append_printf (reader->trace, "%s:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT "\n", kind, begin, line);
	return TRUE;
}

static gboolean
record_end (GMimeReader *base, const char *kind, gint64 begin, guint64 line, gint64 end)
{
	FuzzReader *reader = (FuzzReader *) base;

	check_range (reader, begin, line, end);
	flush_reads (reader);
	g_string_append_printf (reader->trace, "%s:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%" G_GINT64_FORMAT "\n",
				kind, begin, line, end);
	return TRUE;
}

static gboolean
headers_begin (GMimeReader *reader, gint64 begin, guint64 line, GError **error)
{
	return record_begin (reader, "HB", begin, line);
}

static gboolean
header_read (GMimeReader *base, const guint8 *buffer, gsize length, gsize name_length, gsize field_length,
	     gboolean invalid, gint64 begin, guint64 line, GError **error)
{
	FuzzReader *reader = (FuzzReader *) base;

	check_range (reader, begin, line, begin);
	FUZZ_CHECK (length <= reader->length - begin && name_length <= field_length && field_length <= length,
		    "Invalid header callback lengths");
	FUZZ_CHECK (invalid || (field_length < length && buffer[field_length] == ':'), "Missing header colon");
	flush_reads (reader);
	g_string_append_printf (reader->trace, "H:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT ":%" G_GSIZE_FORMAT
				":%" G_GSIZE_FORMAT ":%" G_GSIZE_FORMAT ":%d:%" G_GUINT64_FORMAT "\n",
				begin, line, length, name_length, field_length, invalid, fuzz_hash (buffer, length, FUZZ_HASH_INIT));
	return TRUE;
}

static gboolean
headers_end (GMimeReader *base, gint64 begin, guint64 line, gint64 end, guint64 end_line, GError **error)
{
	FuzzReader *reader = (FuzzReader *) base;

	record_end (base, "HE", begin, line, end);
	FUZZ_CHECK (end_line >= line, "Header line numbers moved backwards");
	g_string_append_printf (reader->trace, "L:%" G_GUINT64_FORMAT "\n", end_line);
	return TRUE;
}

static gboolean
body_separator (GMimeReader *reader, gint64 begin, guint64 line, gint64 end, GError **error)
{
	return record_end (reader, "S", begin, line, end);
}

static void
record_content_type (FuzzReader *reader, GMimeContentType *content_type)
{
	char *mime;

	FUZZ_CHECK (GMIME_IS_CONTENT_TYPE (content_type), "Missing callback content type");
	mime = g_mime_content_type_get_mime_type (content_type);
	g_string_append_printf (reader->trace, "T:%s\n", mime);
	g_free (mime);
}

static gboolean
entity_begin (GMimeReader *base, GMimeReaderEntityType type, GMimeContentType *content_type,
	      gint64 begin, guint64 line, GError **error)
{
	FuzzReader *reader = (FuzzReader *) base;

	FUZZ_CHECK (type >= GMIME_READER_ENTITY_PART && type <= GMIME_READER_ENTITY_MESSAGE_PART, "Invalid entity kind");
	record_begin (base, "EB", begin, line);
	record_content_type (reader, content_type);
	g_string_append_printf (reader->trace, "K:%d\n", type);
	reader->entities++;
	return TRUE;
}

static void
record_entity_end (GMimeReader *base, const char *kind, gint64 begin, guint64 line,
		   gint64 headers_end, gint64 end, guint64 lines)
{
	FuzzReader *reader = (FuzzReader *) base;

	record_end (base, kind, begin, line, end);
	FUZZ_CHECK (headers_end >= begin && headers_end <= end, "Headers extend outside entity");
	g_string_append_printf (reader->trace, "Z:%" G_GINT64_FORMAT ":%" G_GUINT64_FORMAT "\n", headers_end, lines);
}

static gboolean
entity_end (GMimeReader *base, GMimeReaderEntityType type, GMimeContentType *content_type,
	    gint64 begin, guint64 line, gint64 headers_end, gint64 end, guint64 lines, GError **error)
{
	FuzzReader *reader = (FuzzReader *) base;

	FUZZ_CHECK (reader->entities > 0, "Unmatched entity end");
	reader->entities--;
	record_entity_end (base, "EE", begin, line, headers_end, end, lines);
	record_content_type (reader, content_type);
	g_string_append_printf (reader->trace, "K:%d\n", type);
	return TRUE;
}

static gboolean
content_begin (GMimeReader *reader, GMimeReaderContentType type, gint64 begin, guint64 line, GError **error)
{
	record_begin (reader, "CB", begin, line);
	g_string_append_printf (((FuzzReader *) reader)->trace, "K:%d\n", type);
	return TRUE;
}

static gboolean
content_read (GMimeReader *reader, GMimeReaderContentType type, const guint8 *buffer, gsize length, GError **error)
{
	FUZZ_CHECK (type >= GMIME_READER_CONTENT_PART && type <= GMIME_READER_CONTENT_EPILOGUE, "Invalid content kind");
	record_read ((FuzzReader *) reader, type + 1, buffer, length);
	return TRUE;
}

static gboolean
content_end (GMimeReader *base, GMimeReaderContentType type, gint64 begin, guint64 line, gint64 end,
	     guint64 lines, GMimeReaderNewLineFormat newline, GError **error)
{
	FuzzReader *reader = (FuzzReader *) base;

	record_end (base, "CE", begin, line, end);
	FUZZ_CHECK (newline >= GMIME_READER_NEWLINE_UNKNOWN && newline <= GMIME_READER_NEWLINE_MIXED, "Invalid newline kind");
	g_string_append_printf (reader->trace, "C:%d:%" G_GUINT64_FORMAT ":%d\n", type, lines, newline);
	return TRUE;
}

static gboolean
boundary_begin (GMimeReader *reader, gboolean final, gint64 begin, guint64 line, GError **error)
{
	return record_begin (reader, final ? "FB" : "BB", begin, line);
}

static gboolean
boundary_read (GMimeReader *base, gboolean final, const guint8 *buffer, gsize length,
	       gint64 begin, guint64 line, GError **error)
{
	FuzzReader *reader = (FuzzReader *) base;

	check_range (reader, begin, line, begin);
	FUZZ_CHECK (length <= reader->length - begin, "Boundary read outside input");
	record_read (reader, final ? 5 : 4, buffer, length);
	return TRUE;
}

static gboolean
boundary_end (GMimeReader *reader, gboolean final, gint64 begin, guint64 line, gint64 end, GError **error)
{
	return record_end (reader, final ? "FE" : "BE", begin, line, end);
}

static gboolean
message_begin (GMimeReader *reader, gint64 begin, guint64 line, GError **error)
{
	return record_begin (reader, "MB", begin, line);
}

static gboolean
message_end (GMimeReader *reader, gint64 begin, guint64 line, gint64 headers_end, gint64 end, guint64 lines, GError **error)
{
	record_entity_end (reader, "ME", begin, line, headers_end, end, lines);
	return TRUE;
}

static gboolean
marker_begin (GMimeReader *reader, gint64 begin, guint64 line, GError **error)
{
	return record_begin (reader, "XB", begin, line);
}

static gboolean
marker_read (GMimeReader *reader, const guint8 *buffer, gsize length, GError **error)
{
	record_read ((FuzzReader *) reader, 6, buffer, length);
	return TRUE;
}

static gboolean
marker_end (GMimeReader *reader, gint64 begin, guint64 line, gint64 end, GError **error)
{
	return record_end (reader, "XE", begin, line, end);
}

static void
fuzz_reader_finalize (GObject *object)
{
	g_string_free (((FuzzReader *) object)->trace, TRUE);
	G_OBJECT_CLASS (fuzz_reader_parent_class)->finalize (object);
}

static void
fuzz_reader_class_init (FuzzReaderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = fuzz_reader_finalize;
	klass->headers_begin = headers_begin;
	klass->header_read = header_read;
	klass->headers_end = headers_end;
	klass->body_separator = body_separator;
	klass->entity_begin = entity_begin;
	klass->entity_end = entity_end;
	klass->content_begin = content_begin;
	klass->content_read = content_read;
	klass->content_end = content_end;
	klass->boundary_begin = boundary_begin;
	klass->boundary_read = boundary_read;
	klass->boundary_end = boundary_end;
	klass->message_begin = message_begin;
	klass->message_end = message_end;
	klass->marker_begin = marker_begin;
	klass->marker_read = marker_read;
	klass->marker_end = marker_end;
}

static void
fuzz_reader_init (FuzzReader *reader)
{
	reader->trace = g_string_new (NULL);
}

static GString *
read_input (const guint8 *data, gsize length, GMimeFormat format, gsize chunk, FuzzCompliance *checker)
{
	GMimeParserOptions *options = fuzz_parser_options ();
	GMimeStream *stream = fuzz_stream_new (data, length, chunk);
	FuzzReader *reader = g_object_new (fuzz_reader_get_type (), NULL);
	GMimeReader *base = (GMimeReader *) reader;
	GString *trace;
	guint count = 0;

	reader->length = length;
	g_mime_reader_set_options (base, options);
	g_mime_reader_set_stream_with_format (base, stream, format);
	if (checker)
		fuzz_compliance_attach (checker, base);
	do {
		gint64 before = g_mime_reader_tell (base), after;
		GError *error = NULL;
		gboolean success;

		if (checker)
			fuzz_compliance_begin (checker);
		success = g_mime_reader_read_message (base, &error);
		if (checker)
			fuzz_compliance_end (checker);
		flush_reads (reader);
		after = g_mime_reader_tell (base);
		FUZZ_CHECK (after >= before && (guint64) after <= length, "Reader position outside input");
		/* Read-ahead at a fatal error can leave different buffered tails.
		 * Compare the error and callbacks, not that unspecified position. */
		g_string_append_printf (reader->trace, "RESULT:%d:%" G_GINT64_FORMAT ":%d\n",
					success, success ? after : -1, success && g_mime_reader_eos (base));
		if (!success) {
			fuzz_check_error (error);
			g_string_append_printf (reader->trace, "ERROR:%d:%s\n", error->code, error->message);
			g_clear_error (&error);
			break;
		}
		FUZZ_CHECK (error == NULL && reader->entities == 0, "Incomplete successful reader operation");
		FUZZ_CHECK (after > before || g_mime_reader_eos (base), "Reader did not advance");
	} while (format != GMIME_FORMAT_MESSAGE && !g_mime_reader_eos (base) && ++count < FUZZ_MAX_MESSAGES);
	/* A work budget is not a malformed-input failure. All compared runs use it. */
	if (count == FUZZ_MAX_MESSAGES)
		g_string_append (reader->trace, "BUDGET\n");
	trace = g_string_new_len (reader->trace->str, reader->trace->len);
	g_object_unref (reader);
	g_object_unref (stream);
	g_mime_parser_options_unref (options);
	return trace;
}

void
fuzz_mime_reader (const guint8 *data, gsize length)
{
	static const gsize chunks[] = { 1, 7, 31, 4096 };
	guint32 selector = fuzz_selector (data, length);
	GMimeFormat format;

	for (format = GMIME_FORMAT_MESSAGE; format <= GMIME_FORMAT_MBOX; format++) {
		FuzzCompliance first, second;
		GString *fast, *logged, *chunked;

		fuzz_compliance_init (&first, length, selector);
		fuzz_compliance_init (&second, length, selector);
		fast = read_input (data, length, format, 4096, NULL);
		logged = read_input (data, length, format, 4096, &first);
		chunked = read_input (data, length, format, chunks[(selector >> 3) & 3], &second);
		fuzz_equal ((guint8 *) fast->str, fast->len, (guint8 *) logged->str, logged->len, "Reader fast vs logged callbacks");
		fuzz_equal ((guint8 *) logged->str, logged->len, (guint8 *) chunked->str, chunked->len, "Reader chunked callbacks");
		fuzz_equal ((guint8 *) first.trace->str, first.trace->len, (guint8 *) second.trace->str, second.trace->len,
			    "Reader chunked compliance issues");
		g_string_free (fast, TRUE);
		g_string_free (logged, TRUE);
		g_string_free (chunked, TRUE);
		g_string_free (first.trace, TRUE);
		g_string_free (second.trace, TRUE);
	}
}
