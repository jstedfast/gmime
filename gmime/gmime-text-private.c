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

#include "gmime-text-private.h"
#include "gmime-error.h"
#include "gmime-iconv.h"
#include "gmime-internal.h"
#include "gmime-reader-simd-private.h"

static gboolean
text_io_error (GMimeStream *stream, GError **error, int code, const char *operation)
{
	if (!_g_mime_text_stream_propagate_error (stream, error))
		g_set_error (error, GMIME_ERROR, code ? code : GMIME_ERROR_GENERAL, "Text conversion %s failed: %s",
			     operation, code ? g_strerror (code) : "stream did not report an error");
	return FALSE;
}

gboolean
_g_mime_text_write (GMimeStream *stream, const char *text, gsize length, GError **error)
{
	errno = 0;
	if (_g_mime_stream_write_all (stream, text, length) < 0)
		return text_io_error (stream, error, errno, "write");

	return TRUE;
}

gboolean
_g_mime_text_reader_read (GMimeTextReader *reader, GError **error)
{
	gsize valid;

	if (reader->position < reader->end)
		return TRUE;

	reader->length -= reader->end;
	memmove (reader->buffer, reader->buffer + reader->end, reader->length);
	reader->position = reader->end = 0;

	while (!reader->eof) {
		ssize_t n;

		errno = 0;
		if ((n = g_mime_stream_read (reader->stream, reader->buffer + reader->length,
					     sizeof (reader->buffer) - reader->length)) < 0) {
			return text_io_error (reader->stream, error, errno, "read");
		} else if (n == 0) {
			if (!g_mime_stream_eos (reader->stream))
				return text_io_error (reader->stream, error, EIO, "read");

			reader->eof = TRUE;
		}

		reader->length += n;
		valid = _g_mime_reader_utf8_invalid_index ((const guint8 *) reader->buffer, reader->length);
		if (!reader->allow_nul && memchr (reader->buffer, '\0', reader->length) != NULL) {
			g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
					     "Text conversion input contains NUL");
			return FALSE;
		}

		if (valid < reader->length) {
			gunichar c = g_utf8_get_char_validated (reader->buffer + valid, reader->length - valid);

			if (c != (gunichar) -2 || reader->eof) {
				g_set_error_literal (error, G_CONVERT_ERROR,
						     c == (gunichar) -2 ? G_CONVERT_ERROR_PARTIAL_INPUT :
									  G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
						     "Text conversion input is not complete UTF-8");
				return FALSE;
			}
		}

		reader->end = valid;
		if (valid > 0 || reader->eof)
			return TRUE;
	}

	return TRUE;
}

gboolean
_g_mime_text_reader_read_line (GMimeTextReader *reader, GString *line, gboolean *eof, GError **error)
{
	static const GMimeReaderByteSet newlines = { { [10] = 1, [13] = 1 }, FALSE };
	gsize length;
	char c;

	g_string_truncate (line, 0);
	*eof = FALSE;

	for (;;) {
		if (!_g_mime_text_reader_read (reader, error))
			return FALSE;

		if (reader->position == reader->end) {
			*eof = line->len == 0;
			return TRUE;
		}

		if (reader->skip_lf) {
			reader->skip_lf = FALSE;
			if (reader->buffer[reader->position] == '\n') {
				reader->position++;
				continue;
			}
		}

		length = _g_mime_reader_find_byte ((const guint8 *) reader->buffer + reader->position,
						   reader->end - reader->position, &newlines);
		g_string_append_len (line, reader->buffer + reader->position, length);
		reader->position += length;

		if (reader->position < reader->end) {
			c = reader->buffer[reader->position++];
			reader->skip_lf = c == '\r';
			return TRUE;
		}
	}
}

char *
_g_mime_text_read_all (GMimeStream *stream, gsize max_chars, GError **error)
{
	GMimeTextReader reader = { .stream = stream };
	GString *text = g_string_new (NULL);
	gsize count = 0;

	for (;;) {
		gsize length, chars;

		if (!_g_mime_text_reader_read (&reader, error))
			break;

		if (reader.position == reader.end)
			return g_string_free (text, FALSE);

		length = reader.end - reader.position;
		chars = g_utf8_strlen (reader.buffer + reader.position, length);

		if (max_chars && chars > max_chars - count) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     "Text input length limit exceeded");
			break;
		}

		count += chars;
		g_string_append_len (text, reader.buffer + reader.position, length);
		reader.position = reader.end;
	}

	g_string_free (text, TRUE);

	return NULL;
}

typedef struct {
	GMimeStream parent;
	GMimeStream *source;

	iconv_t cd;
	GError *error;
	char *buffer;
	char converted[4096];
	gsize capacity, read_size, length, position, available;
	const char *preamble;
	gsize preamble_length;
	gboolean decoding, detect_bom, initialized, eof, finished;
} TextStream;

typedef GMimeStreamClass TextStreamClass;
static GType text_stream_get_type (void);
G_DEFINE_FINAL_TYPE (TextStream, text_stream, GMIME_TYPE_STREAM)

static gboolean
text_stream_error (TextStream *stream, int code)
{
	g_set_error_literal (&stream->error, G_CONVERT_ERROR,
			     code == EINVAL ? G_CONVERT_ERROR_PARTIAL_INPUT : G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
			     code == EINVAL ? "Incomplete character at end of text" :
					      "Text cannot be represented in the selected charset");
	errno = code;
	return FALSE;
}

static gboolean
text_stream_fill (TextStream *stream, gsize maximum)
{
	ssize_t n;

	errno = 0;
	n = g_mime_stream_read (stream->source, stream->buffer + stream->length,
				MIN (maximum, stream->capacity - stream->length));
	if (n < 0) {
		text_io_error (stream->source, &stream->error, errno, "read");
		return FALSE;
	}

	if (n == 0) {
		if (!g_mime_stream_eos (stream->source)) {
			text_io_error (stream->source, &stream->error, EIO, "read");
			errno = EIO;
			return FALSE;
		}

		stream->eof = TRUE;
	}

	stream->length += n;
	return TRUE;
}

static gboolean
text_stream_detect_bom (TextStream *stream)
{
	const char *charset = NULL;
	gsize skip = 0;

	iconv_t cd;

	/* StreamReader recognizes UTF-32 before UTF-16, unlike MIME charset detection. */
	while (stream->length < 4 && !stream->eof) {
		if (!text_stream_fill (stream, 4 - stream->length))
			return FALSE;
	}

	if (stream->length >= 4 && !memcmp (stream->buffer, "\xff\xfe\0\0", 4)) {
		charset = "UTF-32LE";
		skip = 4;
	} else if (stream->length >= 4 && !memcmp (stream->buffer, "\0\0\xfe\xff", 4)) {
		charset = "UTF-32BE";
		skip = 4;
	} else if (stream->length >= 3 && !memcmp (stream->buffer, "\xef\xbb\xbf", 3)) {
		charset = "UTF-8";
		skip = 3;
	} else if (stream->length >= 2 && !memcmp (stream->buffer, "\xff\xfe", 2)) {
		charset = "UTF-16LE";
		skip = 2;
	} else if (stream->length >= 2 && !memcmp (stream->buffer, "\xfe\xff", 2)) {
		charset = "UTF-16BE";
		skip = 2;
	}

	if (charset) {
		cd = _g_mime_charset_open_text_decoder (charset, NULL, &stream->error);
		if (cd == (iconv_t) -1)
			return FALSE;

		g_mime_iconv_close (stream->cd);

		stream->cd = cd;
		stream->length -= skip;
		memmove (stream->buffer, stream->buffer + skip, stream->length);
	}

	return TRUE;
}

static ssize_t
text_stream_read (GMimeStream *base, char *buffer, size_t length)
{
	TextStream *stream = (TextStream *) base;

	if (!stream->decoding) {
		errno = EBADF;
		return -1;
	}

	if (stream->error) {
		errno = EIO;
		return -1;
	}

	if (!stream->initialized) {
		stream->initialized = TRUE;
		if (stream->detect_bom && !text_stream_detect_bom (stream))
			return -1;
	}

	while (stream->position == stream->available && !stream->finished) {
		char *inbuf = stream->buffer, *outbuf = stream->converted;
		gsize inleft = stream->length, outleft = sizeof (stream->converted);
		gsize result;
		int code;

		stream->position = stream->available = 0;
		if (stream->length == 0 && !stream->eof) {
			if (!text_stream_fill (stream, stream->read_size))
				return -1;

			inleft = stream->length;
		}

		errno = 0;
		if (stream->length > 0)
			result = g_mime_iconv (stream->cd, &inbuf, &inleft, &outbuf, &outleft);
		else
			result = g_mime_iconv (stream->cd, NULL, NULL, &outbuf, &outleft);

		code = errno;
		stream->length = inleft;
		memmove (stream->buffer, inbuf, inleft);
		stream->available = sizeof (stream->converted) - outleft;

		if (result != (gsize) -1 && result > 0) {
			text_stream_error (stream, EILSEQ);
			return -1;
		}

		if (result == (gsize) -1 && code != E2BIG) {
			if (code != EINVAL || stream->eof) {
				text_stream_error (stream, code);
				return -1;
			}

			if (stream->length == stream->capacity) {
				text_stream_error (stream, EILSEQ);
				return -1;
			}

			if (!text_stream_fill (stream, stream->read_size))
				return -1;
		} else if (result != (gsize) -1 && stream->eof && stream->length == 0) {
			/* Flush decoder state in a separate call after the last input bytes. */
			if (inbuf == stream->buffer)
				stream->finished = TRUE;
		}
	}

	length = MIN (length, stream->available - stream->position);
	memcpy (buffer, stream->converted + stream->position, length);
	stream->position += length;

	return length;
}

static gboolean
text_stream_preamble (TextStream *stream)
{
	if (stream->initialized)
		return TRUE;

	stream->initialized = TRUE;

	return !stream->preamble_length ||
	       _g_mime_text_write (stream->source, stream->preamble, stream->preamble_length, &stream->error);
}

static gboolean
text_stream_encode (TextStream *stream, gboolean flush)
{
	for (;;) {
		char *inbuf = stream->buffer, *outbuf = stream->converted;
		gsize inleft = stream->length, outleft = sizeof (stream->converted), result;
		int code;

		errno = 0;
		if (flush)
			result = g_mime_iconv (stream->cd, NULL, NULL, &outbuf, &outleft);
		else
			result = g_mime_iconv (stream->cd, &inbuf, &inleft, &outbuf, &outleft);

		code = errno;
		stream->length = inleft;
		memmove (stream->buffer, inbuf, inleft);

		if (result != (gsize) -1 && result > 0)
			return text_stream_error (stream, EILSEQ);

		if (result == (gsize) -1 && code != E2BIG && (code != EINVAL || flush))
			return text_stream_error (stream, code);

		if (!_g_mime_text_write (stream->source, stream->converted, sizeof (stream->converted) - outleft,
					 &stream->error))
			return FALSE;

		if (result != (gsize) -1 || code == EINVAL)
			return TRUE;
	}
}

static ssize_t
text_stream_write (GMimeStream *base, const char *buffer, size_t length)
{
	TextStream *stream = (TextStream *) base;
	gsize total = length;

	if (stream->decoding || stream->finished || length > G_MAXSSIZE) {
		errno = stream->decoding || stream->finished ? EBADF : EOVERFLOW;
		return -1;
	}

	if (stream->error || !text_stream_preamble (stream)) {
		errno = EIO;
		return -1;
	}

	while (length > 0) {
		gsize n = MIN (length, stream->capacity - stream->length);

		if (n == 0) {
			text_stream_error (stream, EILSEQ);
			return -1;
		}

		memcpy (stream->buffer + stream->length, buffer, n);
		stream->length += n;
		buffer += n;
		length -= n;

		if (!text_stream_encode (stream, FALSE))
			return -1;
	}

	return total;
}

static int
text_stream_flush (GMimeStream *base)
{
	TextStream *stream = (TextStream *) base;

	if (stream->decoding)
		return 0;

	if (stream->error || !text_stream_preamble (stream))
		return -1;

	if (stream->length) {
		text_stream_error (stream, EINVAL);
		return -1;
	}

	if (!text_stream_encode (stream, TRUE))
		return -1;

	errno = 0;
	if (g_mime_stream_flush (stream->source) < 0) {
		text_io_error (stream->source, &stream->error, errno, "flush");
		return -1;
	}

	return 0;
}

static int
text_stream_close (GMimeStream *base)
{
	/* The wrapper never closes or flushes a caller-owned stream on disposal. */
	((TextStream *) base)->finished = TRUE;
	return 0;
}

static gboolean
text_stream_eos (GMimeStream *base)
{
	TextStream *stream = (TextStream *) base;

	return stream->finished && stream->position == stream->available;
}

static void
text_stream_finalize (GObject *object)
{
	TextStream *stream = (TextStream *) object;

	if (stream->cd != (iconv_t) -1)
		g_mime_iconv_close (stream->cd);

	g_clear_object (&stream->source);
	g_clear_error (&stream->error);
	g_free (stream->buffer);

	G_OBJECT_CLASS (text_stream_parent_class)->finalize (object);
}

static void
text_stream_class_init (TextStreamClass *klass)
{
	klass->read = text_stream_read;
	klass->write = text_stream_write;
	klass->flush = text_stream_flush;
	klass->close = text_stream_close;
	klass->eos = text_stream_eos;

	G_OBJECT_CLASS (klass)->finalize = text_stream_finalize;
}

static void
text_stream_init (TextStream *stream)
{
	stream->cd = (iconv_t) -1;
	g_mime_stream_construct ((GMimeStream *) stream, 0, -1);
}

gboolean
_g_mime_text_stream_propagate_error (GMimeStream *base, GError **error)
{
	TextStream *stream;

	if (!G_TYPE_CHECK_INSTANCE_TYPE (base, text_stream_get_type ()))
		return FALSE;

	stream = (TextStream *) base;
	if (!stream->error)
		return FALSE;

	g_clear_error (error);
	g_propagate_error (error, g_error_copy (stream->error));

	return TRUE;
}

GMimeStream *
_g_mime_text_stream_new_decoder (GMimeStream *source, const char *charset, gboolean detect_bom, gsize buffer_size,
				 GError **error)
{
	TextStream *stream;

	iconv_t cd;

	if ((cd = _g_mime_charset_open_text_decoder (charset, NULL, error)) == (iconv_t) -1)
		return NULL;

	stream = g_object_new (text_stream_get_type (), NULL);
	stream->source = g_object_ref (source);
	stream->cd = cd;
	stream->decoding = TRUE;
	stream->detect_bom = detect_bom;
	stream->capacity = MAX (buffer_size, 32);
	stream->read_size = buffer_size;
	stream->buffer = g_malloc (stream->capacity);

	return (GMimeStream *) stream;
}

GMimeStream *
_g_mime_text_stream_new_encoder (GMimeStream *destination, const char *charset, gboolean write_bom, gsize buffer_size,
				 GError **error)
{
	TextStream *stream;
	const char *encoding;
	char *name;

	iconv_t cd;

	if (!(name = _g_mime_charset_get_text_name (charset, error)))
		return NULL;

	encoding = _g_mime_charset_get_text_converter (name);
	cd = g_mime_iconv_open (encoding, "UTF-8");
	g_free (name);

	if (cd == (iconv_t) -1) {
		g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
			     "Cannot encode text using charset '%s'", charset);
		return NULL;
	}

	stream = g_object_new (text_stream_get_type (), NULL);
	stream->source = g_object_ref (destination);
	stream->cd = cd;
	stream->capacity = MAX (buffer_size, 32);
	stream->buffer = g_malloc (stream->capacity);

	if (write_bom) {
		if (!g_ascii_strcasecmp (encoding, "UTF-8")) {
			stream->preamble = "\xef\xbb\xbf";
			stream->preamble_length = 3;
		} else if (!g_ascii_strcasecmp (encoding, "UTF-16LE")) {
			stream->preamble = "\xff\xfe";
			stream->preamble_length = 2;
		} else if (!g_ascii_strcasecmp (encoding, "UTF-16BE")) {
			stream->preamble = "\xfe\xff";
			stream->preamble_length = 2;
		} else if (!g_ascii_strcasecmp (encoding, "UTF-32LE")) {
			stream->preamble = "\xff\xfe\0\0";
			stream->preamble_length = 4;
		} else if (!g_ascii_strcasecmp (encoding, "UTF-32BE")) {
			stream->preamble = "\0\0\xfe\xff";
			stream->preamble_length = 4;
		}
	}

	return (GMimeStream *) stream;
}
