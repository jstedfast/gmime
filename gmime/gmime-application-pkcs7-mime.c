/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
#include <errno.h>
#include <zlib.h>

#include "gmime-application-pkcs7-mime.h"
#include "gmime-filter-dos2unix.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-parser.h"
#include "gmime-error.h"

#define _(x) x

/**
 * SECTION: gmime-application-pkcs7-mime
 * @title: GMimeApplicationPkcs7Mime
 * @short_description: Pkcs7 MIME parts
 * @see_also:
 *
 * A #GMimeApplicationPkcs7Mime represents the application/pkcs7-mime MIME part.
 **/

/* GObject class methods */
static void g_mime_application_pkcs7_mime_class_init (GMimeApplicationPkcs7MimeClass *klass);
static void g_mime_application_pkcs7_mime_init (GMimeApplicationPkcs7Mime *catpart, GMimeApplicationPkcs7MimeClass *klass);
static void g_mime_application_pkcs7_mime_finalize (GObject *object);

/* GMimeEntity class methods */
static void application_pkcs7_mime_set_content_type (GMimeEntity *object, GMimeContentType *content_type);


static GMimePartClass *parent_class = NULL;


GType
g_mime_application_pkcs7_mime_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeApplicationPkcs7MimeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_application_pkcs7_mime_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeApplicationPkcs7Mime),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_application_pkcs7_mime_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_PART, "GMimeApplicationPkcs7Mime", &info, 0);
	}
	
	return type;
}


static void
g_mime_application_pkcs7_mime_class_init (GMimeApplicationPkcs7MimeClass *klass)
{
	GMimeEntityClass *object_class = GMIME_ENTITY_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_PART);
	
	gobject_class->finalize = g_mime_application_pkcs7_mime_finalize;
	
	object_class->set_content_type = application_pkcs7_mime_set_content_type;
}

static void
g_mime_application_pkcs7_mime_init (GMimeApplicationPkcs7Mime *pkcs7_mime, GMimeApplicationPkcs7MimeClass *klass)
{
	pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
}

static void
g_mime_application_pkcs7_mime_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
application_pkcs7_mime_set_content_type (GMimeEntity *object, GMimeContentType *content_type)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime = (GMimeApplicationPkcs7Mime *) object;
	const char *value;
	
	if ((value = g_mime_content_type_get_parameter (content_type, "smime-type")) != NULL) {
		if (!g_ascii_strcasecmp (value, "compressed-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA;
		else if (!g_ascii_strcasecmp (value, "enveloped-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA;
		else if (!g_ascii_strcasecmp (value, "authEnveloped-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA;
		else if (!g_ascii_strcasecmp (value, "signed-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_SIGNED_DATA;
		else if (!g_ascii_strcasecmp (value, "certs-only"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_CERTS_ONLY;
		else
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
	} else {
		pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
	}
	
	GMIME_ENTITY_CLASS (parent_class)->set_content_type (object, content_type);
}

/**
 * g_mime_application_pkcs7_mime_new:
 * @type: The type of S/MIME data contained within the part.
 *
 * Creates a new application/pkcs7-mime object.
 *
 * Returns: an empty application/pkcs7-mime object.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_new (GMimeSecureMimeType type)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeContentType *content_type;
	const char *name;
	
	g_return_val_if_fail (type != GMIME_SECURE_MIME_TYPE_UNKNOWN, NULL);
	
	pkcs7_mime = g_object_new (GMIME_TYPE_APPLICATION_PKCS7_MIME, NULL);
	content_type = g_mime_content_type_new ("application", "pkcs7-mime");
	
	switch (type) {
	case GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "compressed-data");
		name = "smime.p7z";
		break;
	case GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "enveloped-data");
		name = "smime.p7m";
		break;
	case GMIME_SECURE_MIME_TYPE_AUTH_ENVELOPED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "authEnveloped-data");
		name = "smime.p7m";
		break;
	case GMIME_SECURE_MIME_TYPE_SIGNED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "signed-data");
		name = "smime.p7m";
		break;
	case GMIME_SECURE_MIME_TYPE_CERTS_ONLY:
		g_mime_content_type_set_parameter (content_type, "smime-type", "certs-only");
		name = "smime.p7c";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	g_mime_entity_set_content_type ((GMimeEntity *) pkcs7_mime, content_type);
	g_object_unref (content_type);
	
	g_mime_part_set_filename ((GMimePart *) pkcs7_mime, name);
	g_mime_part_set_content_encoding ((GMimePart *) pkcs7_mime, GMIME_CONTENT_ENCODING_BASE64);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_get_smime_type:
 * @pkcs7_mime: A #GMimeApplicationPkcs7Mime object
 *
 * Gets the smime-type value of the Content-Type header.
 *
 * Returns: the smime-type value.
 **/
GMimeSecureMimeType
g_mime_application_pkcs7_mime_get_smime_type (GMimeApplicationPkcs7Mime *pkcs7_mime)
{
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), GMIME_SECURE_MIME_TYPE_UNKNOWN);
	
	return pkcs7_mime->smime_type;
}


static const unsigned char cms_compressed_oid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x01, 0x09 };
static const unsigned char cms_zlib_oid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x10, 0x03, 0x08 };
static const unsigned char cms_data_oid[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01 };

typedef struct {
	GMimeStream *stream;
	GError **error;
	guint64 position;
	struct {
		guint64 end;
		gboolean indefinite;
	} scopes[32];
	guint depth;
	int lookahead;
} CompressedDataReader;

static gboolean
compressed_data_invalid (GError **error)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     _("Invalid or truncated CMS CompressedData."));
	return FALSE;
}

static gboolean
compressed_data_io_error (GError **error)
{
	int errsv = errno;

	g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
		     _("CMS compression I/O failed: %s"),
		     errsv ? g_strerror (errsv) : "unknown I/O error");

	return FALSE;
}

static gboolean
compressed_data_write (GMimeStream *stream, const void *buffer, size_t length, GError **error)
{
	errno = 0;

	if (_g_mime_stream_write_all (stream, buffer, length) == -1)
		return compressed_data_io_error (error);

	return TRUE;
}

static guint64
compressed_data_limit (CompressedDataReader *reader)
{
	return reader->depth ? reader->scopes[reader->depth - 1].end : G_MAXUINT64;
}

static gboolean
compressed_data_read (CompressedDataReader *reader, unsigned char *buffer, size_t length)
{
	ssize_t nread;

	if (length > compressed_data_limit (reader) - reader->position)
		return compressed_data_invalid (reader->error);

	if (length && reader->lookahead != -1) {
		*buffer++ = reader->lookahead;
		reader->lookahead = -1;
		reader->position++;
		length--;
	}

	while (length) {
		errno = 0;
		nread = g_mime_stream_read (reader->stream, (char *) buffer, length);

		if (nread < 0)
			return compressed_data_io_error (reader->error);

		if (nread == 0)
			return compressed_data_invalid (reader->error);

		reader->position += nread;
		buffer += nread;
		length -= nread;
	}

	return TRUE;
}

static gboolean
compressed_data_header (CompressedDataReader *reader, unsigned char *tag, guint64 *length, gboolean *indefinite)
{
	unsigned char byte;
	guint count;

	if (!compressed_data_read (reader, tag, 1) ||
	    !compressed_data_read (reader, &byte, 1))
		return FALSE;

	*indefinite = byte == 0x80;
	*length = 0;

	if ((*tag & 0x1f) == 0x1f || (*indefinite && !(*tag & 0x20)))
		return compressed_data_invalid (reader->error);

	if (*indefinite)
		return TRUE;

	if (!(byte & 0x80)) {
		*length = byte;
	} else {
		count = byte & 0x7f;

		if (count > sizeof (*length))
			return compressed_data_invalid (reader->error);

		while (count--) {
			if (!compressed_data_read (reader, &byte, 1))
				return FALSE;

			*length = (*length << 8) | byte;
		}
	}

	if (*length > compressed_data_limit (reader) - reader->position)
		return compressed_data_invalid (reader->error);

	return TRUE;
}

static gboolean
compressed_data_push (CompressedDataReader *reader, guint64 length, gboolean indefinite)
{
	guint64 end = indefinite ? compressed_data_limit (reader) : reader->position + length;

	if (reader->depth == G_N_ELEMENTS (reader->scopes))
		return compressed_data_invalid (reader->error);

	reader->scopes[reader->depth].end = end;
	reader->scopes[reader->depth].indefinite = indefinite;
	reader->depth++;

	return TRUE;
}

static gboolean
compressed_data_begin (CompressedDataReader *reader, unsigned char expected)
{
	gboolean indefinite;
	guint64 length;
	unsigned char tag;

	if (!compressed_data_header (reader, &tag, &length, &indefinite))
		return FALSE;

	if (tag != expected)
		return compressed_data_invalid (reader->error);

	return compressed_data_push (reader, length, indefinite);
}

static gboolean
compressed_data_at_end (CompressedDataReader *reader, gboolean *at_end)
{
	unsigned char byte;
	ssize_t nread;

	if (!reader->scopes[reader->depth - 1].indefinite) {
		*at_end = reader->position == compressed_data_limit (reader);
		return TRUE;
	}

	if (reader->lookahead == -1) {
		if (reader->position == compressed_data_limit (reader))
			return compressed_data_invalid (reader->error);

		errno = 0;
		nread = g_mime_stream_read (reader->stream, (char *) &byte, 1);

		if (nread < 0)
			return compressed_data_io_error (reader->error);

		if (nread == 0)
			return compressed_data_invalid (reader->error);

		reader->lookahead = byte;
	}

	*at_end = reader->lookahead == 0;

	return TRUE;
}

static gboolean
compressed_data_end (CompressedDataReader *reader)
{
	unsigned char eoc[2];

	if (reader->scopes[reader->depth - 1].indefinite) {
		if (!compressed_data_read (reader, eoc, sizeof (eoc)))
			return FALSE;

		if (eoc[0] || eoc[1])
			return compressed_data_invalid (reader->error);
	} else if (reader->position != compressed_data_limit (reader)) {
		return compressed_data_invalid (reader->error);
	}

	reader->depth--;

	return TRUE;
}

static gboolean
compressed_data_value (CompressedDataReader *reader, unsigned char expected,
		       const unsigned char *value, size_t size, int error_code)
{
	unsigned char tag, buffer[sizeof (cms_compressed_oid)];
	gboolean indefinite;
	guint64 length;

	if (!compressed_data_header (reader, &tag, &length, &indefinite))
		return FALSE;

	if (tag != expected || indefinite)
		return compressed_data_invalid (reader->error);

	if (length == size) {
		if (!compressed_data_read (reader, buffer, size))
			return FALSE;

		if (!memcmp (buffer, value, size))
			return TRUE;
	}

	g_set_error_literal (reader->error, GMIME_ERROR, error_code,
			     error_code == GMIME_ERROR_NOT_SUPPORTED ?
			     _("Unsupported CMS compression algorithm or encapsulated content type.") :
			     _("Unexpected CMS CompressedData value."));

	return FALSE;
}

static gboolean
compressed_data_inflate (CompressedDataReader *reader, z_stream *zstream, GMimeStream *output,
			 size_t max_size, size_t *total, gboolean *finished)
{
	unsigned char tag, input[4096], buffer[4096];
	gboolean indefinite, at_end;
	guint64 length;
	size_t count;
	int status;

	if (!compressed_data_header (reader, &tag, &length, &indefinite))
		return FALSE;

	if (tag == 0x24) {
		if (!compressed_data_push (reader, length, indefinite))
			return FALSE;

		do {
			if (!compressed_data_at_end (reader, &at_end))
				return FALSE;

			if (at_end)
				break;

			if (!compressed_data_inflate (reader, zstream, output, max_size, total, finished))
				return FALSE;
		} while (TRUE);

		return compressed_data_end (reader);
	}

	if (tag != 0x04 || indefinite || (*finished && length))
		return compressed_data_invalid (reader->error);

	while (length) {
		count = (size_t) MIN (length, sizeof (input));

		if (!compressed_data_read (reader, input, count))
			return FALSE;

		length -= count;
		zstream->next_in = input;
		zstream->avail_in = (uInt) count;

		do {
			zstream->next_out = buffer;
			zstream->avail_out = sizeof (buffer);
			status = inflate (zstream, Z_NO_FLUSH);

			if (status == Z_MEM_ERROR) {
				g_set_error_literal (reader->error, GMIME_ERROR, GMIME_ERROR_GENERAL,
						     _("Could not allocate CMS decompression state."));
				return FALSE;
			}

			if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
				return compressed_data_invalid (reader->error);

			count = sizeof (buffer) - zstream->avail_out;

			if (count > max_size - *total) {
				g_set_error_literal (reader->error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
						     _("Decompressed CMS content exceeds the maximum size."));
				return FALSE;
			}

			if (!compressed_data_write (output, buffer, count, reader->error))
				return FALSE;

			*total += count;

			if (status == Z_STREAM_END) {
				*finished = TRUE;

				if (zstream->avail_in || length)
					return compressed_data_invalid (reader->error);

				break;
			}

			if (status == Z_BUF_ERROR) {
				if (zstream->avail_in)
					return compressed_data_invalid (reader->error);

				break;
			}
		} while (zstream->avail_in || !zstream->avail_out);
	}

	return TRUE;
}

static guint64
compressed_data_encoded_size (guint64 length)
{
	guint64 size = length + 2;

	if (length >= 128) {
		do {
			size++;
			length >>= 8;
		} while (length);
	}

	return size;
}

static gboolean
compressed_data_write_header (GMimeStream *stream, unsigned char tag, guint64 length, GError **error)
{
	unsigned char buffer[10];
	guint64 value = length;
	guint count = 0, i;

	buffer[0] = tag;

	if (length < 128) {
		buffer[1] = (unsigned char) length;
		return compressed_data_write (stream, buffer, 2, error);
	}

	do {
		count++;
		value >>= 8;
	} while (value);

	buffer[1] = 0x80 | count;

	for (i = 0; i < count; i++)
		buffer[2 + i] = (unsigned char) (length >> (8 * (count - i - 1)));

	return compressed_data_write (stream, buffer, count + 2, error);
}

static gboolean
compressed_data_write_oid (GMimeStream *stream, const unsigned char *oid, size_t length, GError **error)
{
	return compressed_data_write_header (stream, 0x06, length, error) &&
		compressed_data_write (stream, oid, length, error);
}

/**
 * g_mime_application_pkcs7_mime_compress:
 * @entity: a #GMimeEntity to compress
 * @err: return location for a #GError, or %NULL
 *
 * Serializes @entity using DOS newlines and compresses it as RFC 3274 CMS
 * CompressedData using zlib. No crypto context is required.
 *
 * Returns: (nullable) (transfer full): a new compressed S/MIME part, or %NULL
 * on failure, with @err set.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_compress (GMimeEntity *entity, GError **err)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime = NULL;
	GMimeStream *compressed, *stream, *encoded = NULL;
	GMimeFormatOptions *options;
	GMimeContent *content;
	GByteArray *bytes;
	z_stream zstream = { 0 };
	unsigned char buffer[4096];
	static const unsigned char version[] = { 0x02, 0x01, 0x00 };
	guint64 algorithm_size, econtent_size, content_size, wrapped_size;
	int status;

	g_return_val_if_fail (GMIME_IS_ENTITY (entity), NULL);

	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	stream = g_mime_stream_mem_new ();

	errno = 0;

	if (g_mime_entity_write_to_stream (entity, options, stream) == -1) {
		compressed_data_io_error (err);
		g_mime_format_options_free (options);
		g_object_unref (stream);
		return NULL;
	}

	g_mime_format_options_free (options);

	compressed = g_mime_stream_mem_new ();
	status = deflateInit (&zstream, Z_DEFAULT_COMPRESSION);

	if (status != Z_OK) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
				     _("Could not initialize CMS compression."));
		goto done;
	}

	bytes = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
	zstream.next_in = bytes->data;
	zstream.avail_in = bytes->len;

	do {
		zstream.next_out = buffer;
		zstream.avail_out = sizeof (buffer);
		status = deflate (&zstream, Z_FINISH);

		if (status != Z_OK && status != Z_STREAM_END) {
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
					     _("CMS compression failed."));
			break;
		}

		if (!compressed_data_write (compressed, buffer, sizeof (buffer) - zstream.avail_out, err)) {
			status = Z_ERRNO;
			break;
		}
	} while (status != Z_STREAM_END);

	deflateEnd (&zstream);

	if (status != Z_STREAM_END)
		goto done;

	g_object_unref (stream);
	stream = NULL;

	bytes = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) compressed);
	algorithm_size = compressed_data_encoded_size (sizeof (cms_zlib_oid));
	econtent_size = compressed_data_encoded_size (sizeof (cms_data_oid)) +
		compressed_data_encoded_size (compressed_data_encoded_size (bytes->len));
	content_size = sizeof (version) + compressed_data_encoded_size (algorithm_size) +
		compressed_data_encoded_size (econtent_size);
	wrapped_size = compressed_data_encoded_size (compressed_data_encoded_size (content_size));
	encoded = g_mime_stream_mem_new ();

	if (!compressed_data_write_header (encoded, 0x30,
					  compressed_data_encoded_size (sizeof (cms_compressed_oid)) + wrapped_size, err) ||
	    !compressed_data_write_oid (encoded, cms_compressed_oid, sizeof (cms_compressed_oid), err) ||
	    !compressed_data_write_header (encoded, 0xa0, compressed_data_encoded_size (content_size), err) ||
	    !compressed_data_write_header (encoded, 0x30, content_size, err) ||
	    !compressed_data_write (encoded, version, sizeof (version), err) ||
	    !compressed_data_write_header (encoded, 0x30, algorithm_size, err) ||
	    !compressed_data_write_oid (encoded, cms_zlib_oid, sizeof (cms_zlib_oid), err) ||
	    !compressed_data_write_header (encoded, 0x30, econtent_size, err) ||
	    !compressed_data_write_oid (encoded, cms_data_oid, sizeof (cms_data_oid), err) ||
	    !compressed_data_write_header (encoded, 0xa0, compressed_data_encoded_size (bytes->len), err) ||
	    !compressed_data_write_header (encoded, 0x04, bytes->len, err) ||
	    !compressed_data_write (encoded, bytes->data, bytes->len, err))
		goto done;

	g_mime_stream_reset (encoded);
	pkcs7_mime = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA);
	content = g_mime_content_new_with_stream (encoded, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content ((GMimePart *) pkcs7_mime, content);
	g_object_unref (content);

 done:
	g_clear_object (&encoded);
	g_object_unref (compressed);
	g_clear_object (&stream);

	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_decompress:
 * @pkcs7_mime: a #GMimeApplicationPkcs7Mime
 * @max_size: positive maximum decompressed size in bytes, including MIME headers
 * @err: return location for a #GError, or %NULL
 *
 * Decompresses RFC 3274 CMS CompressedData and parses its MIME content.
 * Accepts DER and BER, including constructed OCTET STRINGs, with at most
 * 32 nested ASN.1 containers. Only zlib compression and encapsulated id-data
 * content are supported. No crypto context is required. The entire zlib
 * stream, including its checksum, must be valid; truncated, trailing, or
 * oversized content is rejected.
 *
 * Returns: (nullable) (transfer full): the decompressed entity, or %NULL on
 * failure, with @err set. No partial entity is returned on failure.
 **/
GMimeEntity *
g_mime_application_pkcs7_mime_decompress (GMimeApplicationPkcs7Mime *pkcs7_mime, size_t max_size, GError **err)
{
	GMimeStream *filtered, *compressed, *stream;
	GMimeEntity *decompressed = NULL;
	GMimeContent *content;
	GMimeFilter *filter;
	CompressedDataReader reader = { 0 };
	z_stream zstream = { 0 };
	static const unsigned char zero = 0;
	gboolean at_end, finished = FALSE, valid;
	size_t total = 0;
	ssize_t nread;
	unsigned char byte;

	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), NULL);
	g_return_val_if_fail (max_size > 0, NULL);

	if (pkcs7_mime->smime_type != GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA &&
	    pkcs7_mime->smime_type != GMIME_SECURE_MIME_TYPE_UNKNOWN) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("The S/MIME part does not contain compressed data."));
		return NULL;
	}

	if (!(content = g_mime_part_get_content ((GMimePart *) pkcs7_mime)) ||
	    !g_mime_content_get_stream (content)) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("The compressed S/MIME part has no content stream."));
		return NULL;
	}

	errno = 0;
	if (!(compressed = g_mime_content_open (content))) {
		compressed_data_io_error (err);
		return NULL;
	}

	reader.stream = compressed;
	reader.error = err;
	reader.lookahead = -1;

	if (!compressed_data_begin (&reader, 0x30) ||
	    !compressed_data_value (&reader, 0x06, cms_compressed_oid, sizeof (cms_compressed_oid), GMIME_ERROR_PARSE_ERROR) ||
	    !compressed_data_begin (&reader, 0xa0) ||
	    !compressed_data_begin (&reader, 0x30) ||
	    !compressed_data_value (&reader, 0x02, &zero, 1, GMIME_ERROR_PARSE_ERROR) ||
	    !compressed_data_begin (&reader, 0x30) ||
	    !compressed_data_value (&reader, 0x06, cms_zlib_oid, sizeof (cms_zlib_oid), GMIME_ERROR_NOT_SUPPORTED) ||
	    !compressed_data_at_end (&reader, &at_end))
		goto done;

	if (!at_end && !compressed_data_value (&reader, 0x05, &zero, 0, GMIME_ERROR_PARSE_ERROR))
		goto done;

	if (!compressed_data_end (&reader) ||
	    !compressed_data_begin (&reader, 0x30) ||
	    !compressed_data_value (&reader, 0x06, cms_data_oid, sizeof (cms_data_oid), GMIME_ERROR_NOT_SUPPORTED) ||
	    !compressed_data_begin (&reader, 0xa0))
		goto done;

	if (inflateInit (&zstream) != Z_OK) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
				     _("Could not initialize CMS decompression."));
		goto done;
	}

	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);

	valid = compressed_data_inflate (&reader, &zstream, filtered, max_size, &total, &finished);
	inflateEnd (&zstream);

	if (valid && !finished)
		valid = compressed_data_invalid (err);

	while (valid && reader.depth)
		valid = compressed_data_end (&reader);

	if (valid) {
		errno = 0;
		nread = g_mime_stream_read (compressed, (char *) &byte, 1);

		if (nread < 0 || (!nread && !g_mime_stream_eos (compressed)))
			valid = compressed_data_io_error (err);
		else if (nread)
			valid = compressed_data_invalid (err);
	}

	if (valid) {
		errno = 0;

		if (g_mime_stream_flush (filtered) == -1)
			valid = compressed_data_io_error (err);
	}

	if (valid) {
		g_mime_stream_reset (stream);
		decompressed = g_mime_entity_load (NULL, stream, err);
	}

	g_object_unref (filtered);
	g_object_unref (stream);

 done:
	g_object_unref (compressed);

	return decompressed;
}


/**
 * g_mime_application_pkcs7_mime_encrypt:
 * @entity: a #GMimeEntity to encrypt
 * @flags: a #GMimeEncryptFlags
 * @recipients: (element-type utf8): an array of recipients to encrypt to
 * @err: a #GError
 *
 * Attempts to encrypt the @entity MIME part to the public keys of @recipients
 * using S/MIME. If successful, a new application/pkcs7-mime object is returned.
 *
 * Returns: (nullable) (transfer full): a new #GMimeApplicationPkcs7Mime object on success
 * or %NULL on fail. If encrypting fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_encrypt (GMimeEntity *entity, GMimeEncryptFlags flags, GPtrArray *recipients, GError **err)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeStream *ciphertext, *stream;
	GMimeFormatOptions *options;
	GMimeContent *content;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_ENTITY (entity), NULL);
	g_return_val_if_fail (recipients != NULL, NULL);
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot encrypt application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream (entity, options, stream);
	g_mime_format_options_free (options);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* encrypt the content stream */
	ciphertext = g_mime_stream_mem_new ();

	if (g_mime_crypto_context_encrypt (ctx, FALSE, NULL, flags, recipients, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		g_object_unref (ctx);
		return NULL;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	g_object_unref (ctx);
	
	/* construct the application/pkcs7-mime part */
	pkcs7_mime = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA);
	content = g_mime_content_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content ((GMimePart *) pkcs7_mime, content);
	g_object_unref (ciphertext);
	g_object_unref (content);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_decrypt:
 * @pkcs7_mime: a #GMimeApplicationPkcs7Mime
 * @flags: a #GMimeDecryptFlags
 * @session_key: session key to use or %NULL
 * @result: the decryption result
 * @err: a #GError
 *
 * Attempts to decrypt the encrypted application/pkcs7-mime part.
 *
 * When non-%NULL, @session_key should be a %NULL-terminated string,
 * such as the one returned by g_mime_decrypt_result_get_session_key()
 * from a previous decryption. If the @session_key is not valid, decryption
 * will fail.
 *
 * If @result is non-%NULL, then on a successful decrypt operation, it will be
 * updated to point to a newly-allocated #GMimeDecryptResult with signature
 * status information as well as a list of recipients that the part was
 * encrypted to.
 *
 * Returns: (nullable) (transfer full): the decrypted MIME part on success or
 * %NULL on fail. If the decryption fails, an exception will be set on
 * @err to provide information as to why the failure occurred.
 **/
GMimeEntity *
g_mime_application_pkcs7_mime_decrypt (GMimeApplicationPkcs7Mime *pkcs7_mime,
				       GMimeDecryptFlags flags, const char *session_key,
				       GMimeDecryptResult **result, GError **err)
{
	GMimeStream *filtered, *ciphertext, *stream;
	GMimeContent *content;
	GMimeCryptoContext *ctx;
	GMimeDecryptResult *res;
	GMimeEntity *decrypted;
	GMimeFilter *filter;
	GMimeParser *parser;
	
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), NULL);
	
	if (result)
		*result = NULL;
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot decrypt application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	content = g_mime_part_get_content ((GMimePart *) pkcs7_mime);
	ciphertext = g_mime_stream_mem_new ();
	g_mime_content_decode_to_stream (content, ciphertext);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* decrypt the content stream */
	if (!(res = g_mime_crypto_context_decrypt (ctx, flags, session_key, ciphertext, filtered, err))) {
		g_object_unref (ciphertext);
		g_object_unref (filtered);
		g_object_unref (stream);
		g_object_unref (ctx);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (ciphertext);
	g_object_unref (filtered);
	g_object_unref (ctx);
	
	g_mime_stream_reset (stream);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	decrypted = g_mime_parser_construct_part (parser, NULL, err);
	g_object_unref (parser);
	
	if (!decrypted) {
		if (err == NULL || *err == NULL)
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     _("Cannot decrypt application/pkcs7-mime part: failed to parse decrypted content."));
		
		g_object_unref (res);
		
		return NULL;
	}
	
	if (!result)
		g_object_unref (res);
	else
		*result = res;
	
	return decrypted;
}


/**
 * g_mime_application_pkcs7_mime_sign:
 * @entity: a #GMimeEntity
 * @userid: the user id to sign with
 * @err: a #GError
 *
 * Attempts to sign the @entity MIME part with @userid's private key using
 * S/MIME. If successful, a new application/pkcs7-mime object is returned.
 *
 * Returns: (nullable) (transfer full): a new #GMimeApplicationPkcs7Mime object on success
 * or %NULL on fail. If signing fails, an exception will be set on @err to provide
 * information as to why the failure occurred.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_sign (GMimeEntity *entity, const char *userid, GError **err)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeStream *ciphertext, *stream;
	GMimeFormatOptions *options;
	GMimeContent *content;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_ENTITY (entity), NULL);
	g_return_val_if_fail (userid != NULL, NULL);
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot sign application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	options = _g_mime_format_options_clone (NULL, FALSE);
	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
	
	/* get the cleartext */
	stream = g_mime_stream_mem_new ();
	g_mime_entity_write_to_stream (entity, options, stream);
	g_mime_format_options_free (options);
	
	/* reset the content stream */
	g_mime_stream_reset (stream);
	
	/* sign the content stream */
	ciphertext = g_mime_stream_mem_new ();
	if (g_mime_crypto_context_sign (ctx, FALSE, userid, stream, ciphertext, err) == -1) {
		g_object_unref (ciphertext);
		g_object_unref (stream);
		g_object_unref (ctx);
		return NULL;
	}
	
	g_object_unref (stream);
	g_mime_stream_reset (ciphertext);
	g_object_unref (ctx);
	
	/* construct the application/pkcs7-mime part */
	pkcs7_mime = g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_SIGNED_DATA);
	content = g_mime_content_new_with_stream (ciphertext, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content ((GMimePart *) pkcs7_mime, content);
	g_object_unref (ciphertext);
	g_object_unref (content);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_verify:
 * @pkcs7_mime: a #GMimeApplicationPkcs7Mime
 * @flags: a #GMimeVerifyFlags
 * @entity: (out) (transfer full): the extracted entity
 * @err: a #GError
 *
 * Attempts to verify the signed @pkcs7_mime part and extract the original
 * MIME entity.
 *
 * Returns: (nullable) (transfer full): a new #GMimeSignatureList object on
 * success or %NULL on fail. If the verification fails, an exception
 * will be set on @err to provide information as to why the failure
 * occurred.
 **/
GMimeSignatureList *
g_mime_application_pkcs7_mime_verify (GMimeApplicationPkcs7Mime *pkcs7_mime, GMimeVerifyFlags flags, GMimeEntity **entity, GError **err)
{
	GMimeStream *filtered, *ciphertext, *stream;
	GMimeSignatureList *signatures;
	GMimeContent *content;
	GMimeCryptoContext *ctx;
	GMimeFilter *filter;
	GMimeParser *parser;
	
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), NULL);
	g_return_val_if_fail (entity != NULL, NULL);
	
	*entity = NULL;
	
	if (!(ctx = g_mime_crypto_context_new ("application/pkcs7-mime"))) {
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PROTOCOL_ERROR,
			     _("Cannot verify application/pkcs7-mime part: no crypto context registered for this type."));
		
		return NULL;
	}
	
	/* get the ciphertext stream */
	content = g_mime_part_get_content ((GMimePart *) pkcs7_mime);
	ciphertext = g_mime_stream_mem_new ();
	g_mime_content_decode_to_stream (content, ciphertext);
	g_mime_stream_reset (ciphertext);
	
	stream = g_mime_stream_mem_new ();
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_filter_dos2unix_new (FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	/* decrypt the content stream */
	if (!(signatures = g_mime_crypto_context_verify (ctx, flags, ciphertext, NULL, filtered, err))) {
		g_object_unref (ciphertext);
		g_object_unref (filtered);
		g_object_unref (stream);
		g_object_unref (ctx);
		
		return NULL;
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (ciphertext);
	g_object_unref (filtered);
	g_object_unref (ctx);
	
	g_mime_stream_reset (stream);
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	g_object_unref (stream);
	
	*entity = g_mime_parser_construct_part (parser, NULL, err);
	g_object_unref (parser);
	
	if (*entity == NULL) {
		if (err == NULL || *err == NULL)
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     _("Cannot verify application/pkcs7-mime part: failed to parse extracted content."));
		
		g_object_unref (signatures);
		
		return NULL;
	}
	
	return signatures;
}
