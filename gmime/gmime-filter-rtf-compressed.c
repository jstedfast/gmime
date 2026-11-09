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

#include <config.h>
#include <errno.h>
#include <string.h>

#include "gmime-codec-private.h"
#include "gmime-filter-rtf-compressed.h"
#include "gmime-tnef-private.h"

static const guint8 dictionary_initializer[] =
	"{\\rtf1\\ansi\\mac\\deff0\\deftab720{\\fonttbl;}{\\f0\\fnil \\froman \\fswiss "
	"\\fmodern \\fscript \\fdecor MS Sans SerifSymbolArialTimes New RomanCourier"
	"{\\colortbl\\red0\\green0\\blue0\r\n\\par \\pard\\plain\\f0\\fs20\\b\\i\\u\\tab\\tx";

typedef enum {
	RTF_FLAGS,
	RTF_LITERAL,
	RTF_OFFSET,
	RTF_REFERENCE,
	RTF_COMPLETE
} RtfState;

struct _GMimeFilterRtfCompressed {
	GMimeFilter parent_instance;
	guint8 dictionary[4096], header[16];
	guint header_length, write_offset, read_offset, flags, flag_count;
	guint32 remaining, checksum, crc;
	GMimeRtfCompressionMode mode;
	RtfState state;
};

G_DEFINE_TYPE (GMimeFilterRtfCompressed, g_mime_filter_rtf_compressed, GMIME_TYPE_FILTER)

static void
rtf_filter_reset (GMimeFilter *base)
{
	GMimeFilterRtfCompressed *filter = GMIME_FILTER_RTF_COMPRESSED (base);

	memset (filter->dictionary, 0, sizeof filter->dictionary);
	memcpy (filter->dictionary, dictionary_initializer, sizeof dictionary_initializer - 1);
	filter->write_offset = sizeof dictionary_initializer - 1;
	filter->header_length = filter->read_offset = filter->flags = filter->flag_count = 0;
	filter->remaining = filter->checksum = filter->crc = 0;
	filter->mode = GMIME_RTF_COMPRESSION_MODE_UNKNOWN;
	filter->state = RTF_FLAGS;
}

static GMimeFilter *
rtf_filter_copy (GMimeFilter *base)
{
	(void) base;
	return g_mime_filter_rtf_compressed_new ();
}

static void
next_token (GMimeFilterRtfCompressed *filter)
{
	if (++filter->flag_count == 8) {
		filter->state = RTF_FLAGS;
	} else {
		filter->flags >>= 1;
		filter->state = filter->flags & 1 ? RTF_OFFSET : RTF_LITERAL;
	}
}

static void
rtf_filter (GMimeFilter *base, char *input, size_t length, size_t prespace, char **output, size_t *output_length,
	    size_t *output_prespace)
{
	GMimeFilterRtfCompressed *filter = GMIME_FILTER_RTF_COMPRESSED (base);
	gsize index = 0, capacity;
	guint64 extra, estimate;
	guint count, i;
	guint8 value;
	gint32 size;

	*output = input;
	*output_length = 0;
	*output_prespace = prespace;

	while (index < length && filter->header_length < 16) {
		filter->header[filter->header_length++] = input[index++];

		if (filter->header_length == 12)
			filter->mode = _g_mime_tnef_get_uint32 (filter->header + 8);

		if (filter->header_length == 16) {
			/* COMPSIZE includes the 12 following header bytes; smaller values describe empty content. */
			size = (gint32) _g_mime_tnef_get_uint32 (filter->header);
			filter->remaining = size >= 12 ? size - 12 : 0;
			filter->checksum = _g_mime_tnef_get_uint32 (filter->header + 12);
		}
	}

	if (filter->header_length < 16)
		return;

	if (filter->mode != GMIME_RTF_COMPRESSION_MODE_COMPRESSED &&
	    filter->mode != GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED) {
		/* Decompression is not defined for unknown COMPTYPE values. Passing them through would label arbitrary bytes as RTF. */
		return;
	}

	if (filter->mode == GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED) {
		/* The uncompressed CRC must be zero; no checksum is computed over uncompressed content. */
		*output = input ? input + index : NULL;
		*output_length = MIN (length - index, filter->remaining);
		filter->remaining -= *output_length;
		return;
	}

	/* Both size fields are untrusted: use 64-bit arithmetic and cap speculative allocation at 1 MiB. */
	extra = ABS ((gint64) (gint32) _g_mime_tnef_get_uint32 (filter->header + 4) - (gint64) filter->remaining);
	estimate = MIN ((guint64) (length - index) + extra, 1024 * 1024);

	g_mime_filter_set_size (base, MAX (estimate, 4096), FALSE);

	*output = g_mime_filter_get_output_buffer (base, &capacity, output_prespace);

	/* Stop at COMPSIZE or the end marker; following data must not enter the output or CRC. */
	while (index < length && filter->remaining && filter->state != RTF_COMPLETE) {
		value = (guint8) input[index++];
		filter->crc = codec_crc32_update (filter->crc, value);
		filter->remaining--;

		if (*output_length + 17 > capacity) {
			g_mime_filter_set_size (base, *output_length + 17, TRUE);
			*output = g_mime_filter_get_output_buffer (base, &capacity, output_prespace);
		}

		switch (filter->state) {
		case RTF_FLAGS:
			filter->flags = value;
			filter->flag_count = 0;
			filter->state = value & 1 ? RTF_OFFSET : RTF_LITERAL;
			break;
		case RTF_LITERAL:
			(*output)[(*output_length)++] = value;
			filter->dictionary[filter->write_offset] = value;
			filter->write_offset = (filter->write_offset + 1) & 4095;
			next_token (filter);
			break;
		case RTF_OFFSET:
			filter->read_offset = value;
			filter->state = RTF_REFERENCE;
			break;
		case RTF_REFERENCE:
			filter->read_offset = (filter->read_offset << 4) | (value >> 4);
			if (filter->read_offset == filter->write_offset) {
				filter->state = RTF_COMPLETE;
				break;
			}

			count = (value & 15) + 2;

			for (i = 0; i < count; i++) {
				value = filter->dictionary[filter->read_offset++ & 4095];
				(*output)[(*output_length)++] = value;
				filter->dictionary[filter->write_offset] = value;
				filter->write_offset = (filter->write_offset + 1) & 4095;
			}

			next_token (filter);
			break;
		default:
			break;
		}
	}
}

static void
g_mime_filter_rtf_compressed_class_init (GMimeFilterRtfCompressedClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);

	filter_class->filter = rtf_filter;
	filter_class->complete = rtf_filter;
	filter_class->copy = rtf_filter_copy;
	filter_class->reset = rtf_filter_reset;
}

static void
g_mime_filter_rtf_compressed_init (GMimeFilterRtfCompressed *filter)
{
	rtf_filter_reset (GMIME_FILTER (filter));
}

/**
 * g_mime_filter_rtf_compressed_new:
 *
 * Creates a filter for compressed or uncompressed [MS-OXRTFCP] data.
 *
 * Returns: (transfer full): the filter
 **/
GMimeFilter *
g_mime_filter_rtf_compressed_new (void)
{
	return g_object_new (GMIME_TYPE_FILTER_RTF_COMPRESSED, NULL);
}

/**
 * g_mime_filter_rtf_compressed_get_compression_mode:
 * @filter: an RTF decompression filter
 *
 * Gets the raw COMPTYPE field after at least twelve header bytes have
 * been processed. Unknown wire values are preserved.
 *
 * Returns: the compression mode, or Unknown before the field is available
 **/
GMimeRtfCompressionMode
g_mime_filter_rtf_compressed_get_compression_mode (GMimeFilterRtfCompressed *filter)
{
	g_return_val_if_fail (GMIME_IS_FILTER_RTF_COMPRESSED (filter), GMIME_RTF_COMPRESSION_MODE_UNKNOWN);

	return filter->mode;
}

/**
 * g_mime_filter_rtf_compressed_is_valid_crc32:
 * @filter: an RTF decompression filter
 *
 * Checks the CRC once the header and content have been processed.
 * Uncompressed content requires a zero CRC. Unknown modes are invalid.
 * A matching CRC alone does not validate the RTF document or its raw size.
 *
 * Returns: whether the completed content has the required CRC
 **/
gboolean
g_mime_filter_rtf_compressed_is_valid_crc32 (GMimeFilterRtfCompressed *filter)
{
	g_return_val_if_fail (GMIME_IS_FILTER_RTF_COMPRESSED (filter), FALSE);

	return filter->header_length == 16 &&
	       (filter->mode == GMIME_RTF_COMPRESSION_MODE_COMPRESSED ||
		filter->mode == GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED) &&
	       (filter->remaining == 0 || filter->state == RTF_COMPLETE) && filter->crc == filter->checksum;
}

typedef struct {
	GMimeStream parent_instance;
	GMimeStream *output;
	GObject *owner;
	GMimeTnefStreamClosedFunc completed;
	GError *fault;
	gboolean compress, full, closed;
	gint64 header_offset;
	guint32 raw_size, compressed_size, crc;
	guint8 dictionary[4096], input[4096], group[17];
	gint16 head[4096], previous[4096];
	guint write_offset, input_index, input_end, group_length, token_count;
} RtfWriteStream;

typedef struct {
	GMimeStreamClass parent_class;
} RtfWriteStreamClass;

static GType rtf_write_stream_get_type (void);
G_DEFINE_TYPE (RtfWriteStream, rtf_write_stream, GMIME_TYPE_STREAM)

static guint
dictionary_hash (guint8 first, guint8 second)
{
	return ((first << 4) ^ second) & 4095;
}

/* Adds the dictionary position to the hash chain of the two bytes beginning there. */
static void
dictionary_insert (RtfWriteStream *stream, guint position)
{
	guint hash = dictionary_hash (stream->dictionary[position], stream->dictionary[(position + 1) & 4095]);

	stream->previous[position] = stream->head[hash];
	stream->head[hash] = position;
}

/* Adds a byte exactly as the decoder does. */
static void
dictionary_add (RtfWriteStream *stream, guint8 value)
{
	stream->dictionary[stream->write_offset] = value;

	/* Now that the following byte is known, the previous position can begin a match. */
	dictionary_insert (stream, (stream->write_offset - 1) & 4095);

	stream->write_offset = (stream->write_offset + 1) & 4095;

	if (!stream->write_offset)
		stream->full = TRUE;
}

static guint
match_length (RtfWriteStream *stream, guint offset, guint maximum)
{
	guint length = 0, position, distance;
	guint8 value;

	while (length < maximum) {
		position = (offset + length) & 4095;
		distance = (position - stream->write_offset) & 4095;

		/* Decoder copies are added one byte at a time: a match may overlap the bytes it produces. */
		value = distance < length ? stream->input[stream->input_index + distance] :
					    stream->dictionary[position];

		if (value != stream->input[stream->input_index + length])
			break;

		length++;
	}

	return length;
}

static void
find_match (RtfWriteStream *stream, guint available, guint *offset, guint *length)
{
	guint maximum = MIN (available, 17), age, candidate_length, match_age = G_MAXUINT;
	guint start = stream->full ? (stream->write_offset + 1) & 4095 : 0;
	int chain = 64;
	int candidate = stream->head[dictionary_hash (stream->input[stream->input_index],
						      stream->input[stream->input_index + 1])];

	*offset = *length = 0;

	/* [MS-OXRTFCP] scans from the oldest position and keeps the first longest match, so prefer the oldest tie. */
	while (candidate >= 0 && chain-- > 0) {
		/* A reference to the current write offset marks the end of the stream. */
		if ((guint) candidate != stream->write_offset) {
			candidate_length = match_length (stream, candidate, maximum);
			age = (candidate - start) & 4095;

			if (candidate_length > *length ||
			    (candidate_length == *length && candidate_length && age < match_age)) {
				*offset = candidate;
				*length = candidate_length;
				match_age = age;
			}
		}

		candidate = stream->previous[candidate];
	}
}

static gboolean
rtf_output (RtfWriteStream *stream, const guint8 *buffer, gsize length)
{
	gint64 position = g_mime_stream_tell (stream->output);

	if (position < 0 || position > G_MAXINT32 || length > (gsize) (G_MAXINT32 - position)) {
		if (position >= 0)
			errno = EOVERFLOW;

		return _g_mime_tnef_io_error (&stream->fault);
	}

	return _g_mime_tnef_write_all (stream->output, buffer, length, &stream->fault);
}

static gboolean
write_group (RtfWriteStream *stream)
{
	guint i;

	if (!rtf_output (stream, stream->group, stream->group_length))
		return FALSE;

	for (i = 0; i < stream->group_length; i++)
		stream->crc = codec_crc32_update (stream->crc, stream->group[i]);

	stream->compressed_size += stream->group_length;
	stream->group_length = 1;
	stream->token_count = stream->group[0] = 0;

	return TRUE;
}

static gboolean
write_token (RtfWriteStream *stream, gboolean reference, guint8 first, guint8 second)
{
	if (reference)
		stream->group[0] |= 1 << stream->token_count;

	stream->group[stream->group_length++] = first;

	if (reference)
		stream->group[stream->group_length++] = second;

	return ++stream->token_count < 8 || write_group (stream);
}

static gboolean
write_reference (RtfWriteStream *stream, guint offset, guint length)
{
	/* A reference is a 12-bit offset followed by a 4-bit length (minus 2), big-endian. */
	return write_token (stream, TRUE, offset >> 4, ((offset & 15) << 4) | (length - 2));
}

/* Leave at least 17 bytes unencoded unless the input is complete. */
static gboolean
encode_rtf (RtfWriteStream *stream, gboolean final)
{
	guint minimum = final ? 0 : 17, available, offset, length, i, remaining;
	guint8 value;

	while (stream->input_end - stream->input_index > minimum) {
		available = stream->input_end - stream->input_index;
		offset = length = 0;

		if (available >= 2)
			find_match (stream, available, &offset, &length);

		if (length >= 2) {
			if (!write_reference (stream, offset, length))
				return FALSE;

			for (i = 0; i < length; i++)
				dictionary_add (stream, stream->input[stream->input_index++]);
		} else {
			value = stream->input[stream->input_index++];

			if (!write_token (stream, FALSE, value, 0))
				return FALSE;

			dictionary_add (stream, value);
		}
	}

	/* Move the remaining input to the start of the buffer. */
	remaining = stream->input_end - stream->input_index;
	memmove (stream->input, stream->input + stream->input_index, remaining);
	stream->input_index = 0;
	stream->input_end = remaining;

	return TRUE;
}

static ssize_t
rtf_write (GMimeStream *base, const char *buffer, size_t length)
{
	RtfWriteStream *stream = (RtfWriteStream *) base;
	gsize count = length, copied;

	if (stream->closed || stream->fault) {
		errno = stream->closed ? EBADF : EIO;
		return -1;
	}

	if (length > G_MAXINT32 - stream->raw_size) {
		errno = EOVERFLOW;
		_g_mime_tnef_io_error (&stream->fault);
		return -1;
	}

	if (!stream->compress) {
		if (!rtf_output (stream, (const guint8 *) buffer, length))
			return -1;

		stream->compressed_size += length;
	} else {
		while (count) {
			copied = MIN (count, sizeof stream->input - stream->input_end);
			memcpy (stream->input + stream->input_end, buffer, copied);
			stream->input_end += copied;
			buffer += copied;
			count -= copied;

			if (stream->input_end == sizeof stream->input && !encode_rtf (stream, FALSE))
				return -1;
		}
	}

	stream->raw_size += length;
	g_mime_stream_set_position (base, stream->raw_size);

	return length;
}

static gboolean
complete_rtf (RtfWriteStream *stream)
{
	guint8 header[16];
	gint64 end;

	if (stream->compress) {
		if (!encode_rtf (stream, TRUE) || !write_reference (stream, stream->write_offset, 2))
			return FALSE;

		if (stream->token_count && !write_group (stream))
			return FALSE;
	}

	/* COMPSIZE includes the twelve header bytes following it. */
	_g_mime_tnef_put_uint32 (header, stream->compressed_size + 12);
	_g_mime_tnef_put_uint32 (header + 4, stream->raw_size);
	_g_mime_tnef_put_uint32 (header + 8, stream->compress ? GMIME_RTF_COMPRESSION_MODE_COMPRESSED :
								GMIME_RTF_COMPRESSION_MODE_UNCOMPRESSED);
	_g_mime_tnef_put_uint32 (header + 12, stream->compress ? stream->crc : 0);

	end = g_mime_stream_tell (stream->output);

	if (end < 0 ||
	    g_mime_stream_seek (stream->output, stream->header_offset, GMIME_STREAM_SEEK_SET) != stream->header_offset)
		return _g_mime_tnef_io_error (&stream->fault);

	if (!rtf_output (stream, header, sizeof header))
		return FALSE;

	if (g_mime_stream_seek (stream->output, end, GMIME_STREAM_SEEK_SET) != end)
		return _g_mime_tnef_io_error (&stream->fault);

	return TRUE;
}

static int
rtf_close (GMimeStream *base)
{
	RtfWriteStream *stream = (RtfWriteStream *) base;
	GError *error = NULL;

	if (!stream->closed) {
		stream->closed = TRUE;

		if (!stream->fault)
			complete_rtf (stream);

		if (!stream->completed (stream->owner, base, stream->fault, &error)) {
			if (!stream->fault)
				stream->fault = g_steal_pointer (&error);

			g_clear_error (&error);
		}
	}

	if (stream->fault) {
		errno = stream->fault->code > 0 ? stream->fault->code : EINVAL;
		return -1;
	}

	return 0;
}

static int
rtf_flush (GMimeStream *base)
{
	RtfWriteStream *stream = (RtfWriteStream *) base;

	if (stream->closed || stream->fault) {
		errno = stream->closed ? EBADF : EIO;
		return -1;
	}

	return 0;
}

static void
rtf_write_finalize (GObject *object)
{
	RtfWriteStream *stream = (RtfWriteStream *) object;

	if (!stream->closed && stream->completed) {
		if (!stream->fault)
			_g_mime_tnef_invalid_operation (&stream->fault,
							"The compressed RTF stream was abandoned without being closed.");
		stream->completed (stream->owner, GMIME_STREAM (stream), stream->fault, NULL);
	}

	g_clear_object (&stream->output);
	g_clear_object (&stream->owner);
	g_clear_error (&stream->fault);

	G_OBJECT_CLASS (rtf_write_stream_parent_class)->finalize (object);
}

static void
rtf_write_stream_class_init (RtfWriteStreamClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = rtf_write_finalize;

	stream_class->read = _g_mime_tnef_stream_no_read;
	stream_class->reset = _g_mime_tnef_stream_no_reset;
	stream_class->seek = _g_mime_tnef_stream_no_seek;
	stream_class->substream = _g_mime_tnef_stream_no_substream;
	stream_class->write = rtf_write;
	stream_class->close = rtf_close;
	stream_class->flush = rtf_flush;
}

static void
rtf_write_stream_init (RtfWriteStream *stream)
{
	g_mime_stream_construct (GMIME_STREAM (stream), 0, -1);
	stream->group_length = 1;
}

GMimeStream *
_g_mime_tnef_rtf_write_stream_new (GMimeStream *output, gboolean compress, GObject *owner,
				   GMimeTnefStreamClosedFunc completed, GError **error)
{
	static const guint8 placeholder[16];
	RtfWriteStream *stream = g_object_new (rtf_write_stream_get_type (), NULL);
	guint i;

	stream->output = g_object_ref (output);
	stream->owner = g_object_ref (owner);
	stream->compress = compress;
	stream->header_offset = g_mime_stream_tell (output);

	if (!rtf_output (stream, placeholder, sizeof placeholder)) {
		g_propagate_error (error, g_steal_pointer (&stream->fault));
		g_object_unref (stream);
		return NULL;
	}

	if (compress) {
		memcpy (stream->dictionary, dictionary_initializer, sizeof dictionary_initializer - 1);
		stream->write_offset = sizeof dictionary_initializer - 1;

		for (i = 0; i < 4096; i++)
			stream->head[i] = stream->previous[i] = -1;

		/* Every initializer position followed by another initializer byte can begin a match. */
		for (i = 0; i < stream->write_offset - 1; i++)
			dictionary_insert (stream, i);
	}

	stream->completed = completed;

	return GMIME_STREAM (stream);
}
