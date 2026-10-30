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
#include "gmime-object-private.h"
#endif

#include <errno.h>
#include <string.h>

#include "gmime-internal.h"
#include "gmime-message.h"
#include "gmime-message-part.h"
#include "gmime-stream-filter.h"

typedef struct {
	GMimeMultipart *multipart;
	GMimeFormatOptions *options;
	char *boundary;
	gboolean ensure_newline;
	guint index;
} MultipartFrame;

typedef struct {
	GMimeStream *stream;
	GArray *frames;
	gboolean verifying_signature;
	ssize_t written;
} GMimeWriter;

typedef struct {
	GMimeWriter *writer;
	GMimeEntity *entity;
	gboolean ensure_newline;
} WriterDispatch;

/* GObject writer overrides retain their existing virtual signature. While
 * calling one, let a chained parent writer inherit this writer's settings.
 * Scope this context to the callback and thread, not the formatting options. */
static GPrivate dispatch_writer = G_PRIVATE_INIT (NULL);

static gboolean
writer_add (GMimeWriter *writer, ssize_t written)
{
	if (written < 0)
		return FALSE;

	if (written > G_MAXSSIZE - writer->written) {
		errno = EOVERFLOW;
		return FALSE;
	}

	writer->written += written;

	return TRUE;
}

static gboolean
writer_write (GMimeWriter *writer, const char *data, gsize length)
{
	return writer_add (writer, _g_mime_stream_write_all (writer->stream, data, length));
}

static gboolean
writer_string (GMimeWriter *writer, const char *text)
{
	return writer_write (writer, text, strlen (text));
}

/* Write bytes, converting newlines to the requested format. */
static gboolean
writer_bytes (GMimeWriter *writer, GMimeFormatOptions *options, GBytes *bytes, gboolean ensure)
{
	GMimeStream *filtered = g_mime_stream_filter_new (writer->stream);
	GMimeFilter *filter = g_mime_format_options_create_newline_filter (options, ensure);
	gconstpointer data;
	gsize length;
	ssize_t written;
	int errsv;

	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);

	data = g_bytes_get_data (bytes, &length);
	written = _g_mime_stream_write_all (filtered, data, length);

	if (written >= 0 && g_mime_stream_flush (filtered) < 0)
		written = -1;

	if (written >= 0)
		written = _g_mime_stream_filter_get_written ((GMimeStreamFilter *) filtered);

	errsv = errno;
	g_object_unref (filtered);
	errno = errsv;

	return writer_add (writer, written);
}

/* A raw boundary marker may contain several lines for double boundaries.
 * Normally it is written byte-for-byte as parsed. When verifying a
 * signature, content must be canonicalized to CRLF (the newline format
 * is always DOS), so convert bare LF while retaining existing CRLF runs. */
static gboolean
writer_raw_boundary (GMimeWriter *writer, GMimeFormatOptions *options, GBytes *bytes)
{
	const guint8 *data, *lf;
	gsize length, start = 0, index = 0;

	data = g_bytes_get_data (bytes, &length);

	if (writer->verifying_signature) {
		while (index < length && (lf = memchr (data + index, '\n', length - index))) {
			index = lf - data;

			if (!index || data[index - 1] != '\r') {
				if (!writer_write (writer, (const char *) data + start, index - start) ||
				    !writer_string (writer, g_mime_format_options_get_newline (options)))
					return FALSE;
				start = index + 1;
			}

			index++;
		}
	}

	return start == length || writer_write (writer, (const char *) data + start, length - start);
}

static gboolean
multipart_is_truncated (GMimeMultipart *multipart)
{
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);

	return priv->raw_end_boundary && !g_bytes_get_size (priv->raw_end_boundary);
}

/* RFC 2046 assigns the newline before a delimiter to the delimiter, not
 * the preceding content. Write it unless that child's serialized form
 * already ends with it. Follow tail containers without recursion. */
gboolean
_g_mime_entity_ends_with_delimiter_newline (GMimeEntity *entity, gboolean anonymize)
{
	while (entity) {
		if (GMIME_IS_MULTIPART (entity)) {
			GMimeMultipart *multipart = (GMimeMultipart *) entity;
			GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
			gsize length;
			const guint8 *data;

			if (!multipart_is_truncated (multipart) &&
			    !(anonymize && !priv->reader_model && !priv->write_end_boundary))
				return FALSE;

			if (priv->raw_trailing_boundary) {
				data = g_bytes_get_data (priv->raw_trailing_boundary, &length);
				return length && data[length - 1] == '\n';
			}

			if (!priv->children->len)
				return TRUE;

			entity = priv->children->pdata[priv->children->len - 1];
		} else if (GMIME_IS_MESSAGE_PART (entity)) {
			GMimeMessage *message = g_mime_message_part_get_message ((GMimeMessagePart *) entity);
			GMimeMessagePrivate *priv;

			if (!message)
				return TRUE;

			priv = _g_mime_message_get_private (message);
			if (!priv->mime_part)
				return TRUE;

			entity = priv->mime_part;
		} else {
			return GMIME_IS_PART (entity) && !g_mime_part_get_content ((GMimePart *) entity);
		}
	}

	return FALSE;
}

static void
multipart_frame_clear (gpointer data)
{
	MultipartFrame *frame = data;

	g_object_unref (frame->multipart);
	g_mime_format_options_free (frame->options);
	g_free (frame->boundary);
}

static void
writer_push (GMimeWriter *writer, GMimeMultipart *multipart, GMimeFormatOptions *options, gboolean ensure)
{
	GMimeEntityPrivate *entity_priv = _g_mime_entity_get_private ((GMimeEntity *) multipart);
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
	gboolean is_signed = g_mime_content_type_is_type (entity_priv->content_type, "multipart", "signed");
	/* Generating a boundary would discard an empty parsed multipart's
	 * truncation marker and invent a closing delimiter. */
	const char *boundary = priv->reader_model && (priv->children->len || !multipart_is_truncated (multipart)) ?
		g_mime_multipart_get_boundary (multipart) :
		g_mime_entity_get_content_type_parameter ((GMimeEntity *) multipart, "boundary");
	MultipartFrame frame;

	/* Don't reformat the headers or content of multipart/signed children. */
	frame.options = _g_mime_format_options_clone (options, !is_signed);
	if (is_signed)
		g_mime_format_options_set_allow_international (frame.options, FALSE);

	frame.multipart = multipart;
	frame.ensure_newline = ensure;
	frame.index = 0;
	frame.boundary = boundary ? g_strdup_printf ("--%s%s", boundary, g_mime_format_options_get_newline (frame.options)) : NULL;

	g_array_append_val (writer->frames, frame);
}

/* Write the marker and its preceding newline, if required, for this child. */
static gboolean
writer_boundary (GMimeWriter *writer, MultipartFrame *frame, guint index)
{
	GMimeMultipart *multipart = frame->multipart;
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
	GBytes *raw;

	if (priv->reader_model && index &&
	    !_g_mime_entity_ends_with_delimiter_newline (priv->children->pdata[index - 1], FALSE) &&
	    !writer_string (writer, g_mime_format_options_get_newline (frame->options)))
		return FALSE;

	raw = priv->raw_boundaries ? priv->raw_boundaries->pdata[index] : NULL;

	return raw ? writer_raw_boundary (writer, frame->options, raw) : writer_string (writer, frame->boundary);
}

/* Write everything following the last child: the end boundary and epilogue. */
static gboolean
writer_end (GMimeWriter *writer, MultipartFrame *frame)
{
	GMimeMultipart *multipart = frame->multipart;
	GMimeFormatOptions *options = frame->options;
	GMimeMultipartPrivate *priv = _g_mime_multipart_get_private (multipart);
	const char *newline = g_mime_format_options_get_newline (options);
	char *boundary;
	gboolean success;

	if (!priv->reader_model) {
		const char *value = g_mime_entity_get_content_type_parameter ((GMimeEntity *) multipart, "boundary");

		if (priv->write_end_boundary && value) {
			boundary = g_strdup_printf ("--%s--%s", value, priv->raw_epilogue ? "" : newline);
			success = writer_string (writer, boundary);
			g_free (boundary);

			if (!success)
				return FALSE;
		}

		if (priv->raw_epilogue)
			return writer_bytes (writer, options, priv->raw_epilogue, frame->ensure_newline);

		return !priv->epilogue || writer_string (writer, priv->epilogue);
	}

	if (priv->children->len && !_g_mime_entity_ends_with_delimiter_newline (
	    priv->children->pdata[priv->children->len - 1], FALSE) &&
	    (!multipart_is_truncated (multipart) || priv->raw_trailing_boundary) &&
	    !writer_string (writer, newline))
		return FALSE;

	if (multipart_is_truncated (multipart)) {
		/* There is no closing delimiter or epilogue after truncation, but
		 * an opening marker at EOF may not have acquired a child. */
		if (priv->raw_trailing_boundary) {
			gsize length;
			const guint8 *data = g_bytes_get_data (priv->raw_trailing_boundary, &length);

			if (!writer_raw_boundary (writer, options, priv->raw_trailing_boundary))
				return FALSE;

			if (frame->ensure_newline && (!length || data[length - 1] != '\n'))
				return writer_string (writer, newline);
		}

		return TRUE;
	}

	if (priv->raw_end_boundary) {
		if (!writer_raw_boundary (writer, options, priv->raw_end_boundary))
			return FALSE;
	} else {
		/* A generated end marker includes a newline only without an epilogue. */
		boundary = g_strdup_printf ("--%s--%s", g_mime_multipart_get_boundary (multipart),
					   priv->raw_epilogue ? "" : newline);
		success = writer_string (writer, boundary);
		g_free (boundary);

		if (!success)
			return FALSE;
	}

	if (priv->raw_epilogue && g_bytes_get_size (priv->raw_epilogue))
		return writer_bytes (writer, options, priv->raw_epilogue, frame->ensure_newline);

	/* The end marker owns a newline only if generated without an epilogue. */
	return !frame->ensure_newline || (!priv->raw_end_boundary && !priv->raw_epilogue) ||
		writer_string (writer, newline);
}

static gboolean
writer_message_marker (GMimeWriter *writer, GMimeMessage *message, GMimeFormatOptions *options)
{
	const char *newline = g_mime_format_options_get_newline (options);
	GMimeMessagePrivate *priv = _g_mime_message_get_private (message);
	gconstpointer data;
	gsize length;

	if (priv->raw_marker) {
		data = g_bytes_get_data (priv->raw_marker, &length);
		return !length || writer_write (writer, data, length);
	}

	if (!priv->marker || !(length = strlen (priv->marker)))
		return TRUE;

	if (priv->marker[length - 1] == '\n') {
		length--;
		if (length && priv->marker[length - 1] == '\r')
			length--;
	}

	return writer_write (writer, priv->marker, length) && writer_string (writer, newline);
}

static gboolean
entity_has_custom_writer (GMimeEntity *entity)
{
	GType type;

	if (GMIME_IS_MULTIPART (entity))
		type = GMIME_TYPE_MULTIPART;
	else if (GMIME_IS_MESSAGE_PART (entity))
		type = GMIME_TYPE_MESSAGE_PART;
	else if (GMIME_IS_MESSAGE (entity))
		type = GMIME_TYPE_MESSAGE;
	else if (GMIME_IS_PART (entity))
		type = GMIME_TYPE_PART;
	else
		return TRUE;

	return GMIME_ENTITY_GET_CLASS (entity)->write_to_stream !=
		GMIME_ENTITY_CLASS (g_type_class_peek (type))->write_to_stream;
}

static gboolean
writer_custom (GMimeWriter *writer, GMimeEntity *entity, GMimeFormatOptions *options,
	       gboolean content_only, gboolean ensure)
{
	GMimeFormatOptions *format = _g_mime_format_options_clone (options, TRUE);
	WriterDispatch *previous = g_private_get (&dispatch_writer);
	WriterDispatch context = { writer, entity, ensure };
	ssize_t written;
	int errsv;

	g_mime_format_options_set_ensure_newline (format, ensure);
	g_private_set (&dispatch_writer, &context);

	written = GMIME_ENTITY_GET_CLASS (entity)->write_to_stream (entity, format, content_only, writer->stream);
	errsv = errno;

	g_private_set (&dispatch_writer, previous);
	g_mime_format_options_free (format);
	errno = errsv;

	return writer_add (writer, written);
}

/* Only multipart containers require frames: remaining children, closing
 * delimiters and epilogues follow their descendants. Embedded messages and
 * message bodies are tails, so continue with them instead of recursing. */
static ssize_t
writer_run (GMimeEntity *root, GMimeFormatOptions *options, gboolean content_only,
	    GMimeStream *stream, gboolean verifying, gboolean dispatch, gboolean ensure)
{
	GMimeWriter writer = { stream, g_array_new (FALSE, FALSE, sizeof (MultipartFrame)), verifying, 0 };
	GMimeEntity *entity = g_object_ref (root);
	GMimeFormatOptions *temporary = NULL;
	int errsv;

	if (!options)
		options = g_mime_format_options_get_default ();

	g_array_set_clear_func (writer.frames, multipart_frame_clear);

	while (TRUE) {
		GMimeMultipartPrivate *multipart_priv;
		GMimeEntityPrivate *entity_priv;
		GMimeMultipart *multipart;
		MultipartFrame *frame;
		guint index;

		if (entity) {
			GMimeEntity *next = NULL;

			if (dispatch && entity_has_custom_writer (entity)) {
				if (!writer_custom (&writer, entity, options, content_only, ensure))
					goto fail;
			} else if (GMIME_IS_MESSAGE (entity)) {
				GMimeMessage *message = (GMimeMessage *) entity;
				GMimeMessagePrivate *message_priv;

				if (!content_only) {
					if (!writer_add (&writer, g_mime_message_write_headers_to_stream (message, options, stream)))
						goto fail;
				}

				message_priv = _g_mime_message_get_private (message);
				entity_priv = _g_mime_entity_get_private (entity);

				/* Get the message body and whether it must end with a newline. */
				ensure = entity_priv->ensure_newline || g_mime_format_options_get_ensure_newline (options);

				if (message_priv->mime_part)
					next = g_object_ref (message_priv->mime_part);

				content_only = TRUE;
			} else {
				if (!content_only) {
					GMimeEntityPrivate *priv = _g_mime_entity_get_private (entity);

					if (!writer_add (&writer, g_mime_header_list_write_to_stream (priv->headers, options, stream)))
						goto fail;
				}

				if (GMIME_IS_MULTIPART (entity)) {
					GMimeMultipart *multipart = (GMimeMultipart *) entity;
					GMimeMultipartPrivate *multipart_priv = _g_mime_multipart_get_private (multipart);

					if (multipart_priv->reader_model || multipart_priv->raw_preamble) {
						if (multipart_priv->raw_preamble && g_bytes_get_size (multipart_priv->raw_preamble) &&
						    !writer_bytes (&writer, options, multipart_priv->raw_preamble, multipart_priv->children->len > 0 || ensure))
							goto fail;
					} else if (multipart_priv->preamble &&
						   (!writer_string (&writer, multipart_priv->preamble) ||
						    !writer_string (&writer, g_mime_format_options_get_newline (options)))) {
						goto fail;
					}

					writer_push (&writer, multipart, options, ensure);
					entity = NULL;
				} else if (GMIME_IS_MESSAGE_PART (entity)) {
					GMimeMessage *message = g_mime_message_part_get_message ((GMimeMessagePart *) entity);

					if (message) {
						if (!writer_message_marker (&writer, message, options))
							goto fail;

						/* Only ensure an embedded message ends in a newline
						 * when its enclosing message part must end in one. */
						if (g_mime_format_options_get_ensure_newline (options) != ensure) {
							GMimeFormatOptions *format = _g_mime_format_options_clone (options, TRUE);

							g_mime_format_options_set_ensure_newline (format, ensure);
							g_clear_pointer (&temporary, g_mime_format_options_free);
							options = temporary = format;
						}

						next = g_object_ref ((GMimeEntity *) message);
						content_only = FALSE;
					}
				} else if (GMIME_IS_PART (entity)) {
					GMimePart *part = (GMimePart *) entity;
					GMimeContent *content = g_mime_part_get_content (part);
					GMimeNewLineFormat newline;

					/* Preserve mixed line endings when verifying signatures,
					 * but never bypass a requested transcode.
					 * See https://github.com/jstedfast/MimeKit/issues/569. */
					if (writer.verifying_signature && content && g_mime_content_get_newline_format (content, &newline) &&
					    newline == GMIME_NEWLINE_FORMAT_MIXED &&
					    g_mime_content_get_encoding (content) == g_mime_part_get_content_encoding (part)) {
						if (!writer_add (&writer, g_mime_content_write_to_stream (content, stream)))
							goto fail;
					} else if (!writer_add (&writer, _g_mime_part_write_content (part, options, stream, ensure))) {
						goto fail;
					}
				}
			}

			g_clear_object (&entity);
			entity = next;
			dispatch = TRUE;

			continue;
		}

		g_clear_pointer (&temporary, g_mime_format_options_free);

		if (!writer.frames->len)
			break;

		frame = &g_array_index (writer.frames, MultipartFrame, writer.frames->len - 1);
		multipart = frame->multipart;
		index = frame->index++;

		multipart_priv = _g_mime_multipart_get_private (multipart);

		if (!multipart_priv->reader_model && index && index <= multipart_priv->children->len) {
			GMimeEntity *previous = multipart_priv->children->pdata[index - 1];
			gboolean write_end_boundary = TRUE;

			if (GMIME_IS_MULTIPART (previous)) {
				GMimeMultipartPrivate *priv = _g_mime_multipart_get_private ((GMimeMultipart *) previous);

				write_end_boundary = priv->write_end_boundary;
			}

			if (write_end_boundary &&
			    !_g_mime_entity_ends_with_delimiter_newline (previous, FALSE) &&
			    !writer_string (&writer, g_mime_format_options_get_newline (frame->options)))
				goto fail;
		}

		if (index >= multipart_priv->children->len) {
			if (!writer_end (&writer, frame))
				goto fail;

			g_array_set_size (writer.frames, writer.frames->len - 1);
			continue;
		}

		if (!writer_boundary (&writer, frame, index))
			goto fail;

		if (index >= multipart_priv->children->len) {
			errno = EINVAL;
			goto fail;
		}

		entity = g_object_ref (multipart_priv->children->pdata[index]);
		entity_priv = _g_mime_entity_get_private (entity);
		options = frame->options;

		ensure = entity_priv->ensure_newline || (frame->ensure_newline && index + 1 == multipart_priv->children->len &&
			multipart_is_truncated (multipart) && !multipart_priv->raw_trailing_boundary);
		content_only = FALSE;
	}

	g_array_unref (writer.frames);
	return writer.written;

fail:
	errsv = errno;
	g_clear_object (&entity);
	g_clear_pointer (&temporary, g_mime_format_options_free);
	g_array_unref (writer.frames);
	errno = errsv;

	return -1;
}

ssize_t
_g_mime_writer_write (GMimeEntity *entity, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream)
{
	WriterDispatch *parent = g_private_get (&dispatch_writer);
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (entity);
	gboolean ensure = parent && parent->entity == entity ? parent->ensure_newline :
		priv->ensure_newline;

	return writer_run (entity, options, content_only, stream, parent && parent->writer->verifying_signature, FALSE, ensure);
}

ssize_t
_g_mime_writer_write_for_signature (GMimeEntity *entity, GMimeFormatOptions *options, GMimeStream *stream)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (entity);

	return writer_run (entity, options, FALSE, stream, TRUE, TRUE,
			   priv->ensure_newline);
}
