/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include "gmime-part.h"
#include "gmime-error.h"
#include "gmime-utils.h"
#include "gmime-common.h"
#include "gmime-internal.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-null.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-filter-best.h"
#include "gmime-filter-checksum.h"
#include "gmime-filter-unix2dos.h"
#include "gmime-table-private.h"

#define _(x) x
#define d(x)


/**
 * SECTION: gmime-part
 * @title: GMimePart
 * @short_description: MIME parts
 * @see_also:
 *
 * A #GMimePart represents any MIME leaf part (meaning it has no
 * sub-parts).
 **/

/* GObject class methods */
static void g_mime_part_class_init (GMimePartClass *klass);
static void g_mime_part_init (GMimePart *mime_part, GMimePartClass *klass);
static void g_mime_part_finalize (GObject *object);

/* GMimeObject class methods */
static void mime_part_header_added    (GMimeObject *object, GMimeHeader *header);
static void mime_part_header_changed  (GMimeObject *object, GMimeHeader *header);
static void mime_part_header_removed  (GMimeObject *object, GMimeHeader *header);
static void mime_part_headers_cleared (GMimeObject *object);

static ssize_t mime_part_write_to_stream (GMimeObject *object, GMimeFormatOptions *options,
					  gboolean content_only, GMimeStream *stream);
static void mime_part_encode (GMimeObject *object, GMimeEncodingConstraint constraint);

/* GMimePart class methods */
static void set_content (GMimePart *mime_part, GMimeDataWrapper *content);


static GMimeObjectClass *parent_class = NULL;


GType
g_mime_part_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimePartClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_part_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimePart),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_part_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_OBJECT, "GMimePart", &info, 0);
	}
	
	return type;
}


static void
g_mime_part_class_init (GMimePartClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_OBJECT);
	
	gobject_class->finalize = g_mime_part_finalize;
	
	object_class->header_added = mime_part_header_added;
	object_class->header_changed = mime_part_header_changed;
	object_class->header_removed = mime_part_header_removed;
	object_class->headers_cleared = mime_part_headers_cleared;
	object_class->write_to_stream = mime_part_write_to_stream;
	object_class->encode = mime_part_encode;
	
	klass->set_content = set_content;
}

static void
g_mime_part_init (GMimePart *mime_part, GMimePartClass *klass)
{
	mime_part->encoding = GMIME_CONTENT_ENCODING_DEFAULT;
	mime_part->content_description = NULL;
	mime_part->content_location = NULL;
	mime_part->content_md5 = NULL;
	mime_part->content = NULL;
	mime_part->openpgp = (GMimeOpenPGPData) -1;
}

static void
g_mime_part_finalize (GObject *object)
{
	GMimePart *mime_part = (GMimePart *) object;
	
	g_free (mime_part->content_description);
	g_free (mime_part->content_location);
	g_free (mime_part->content_md5);
	
	if (mime_part->content)
		g_object_unref (mime_part->content);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


enum {
	HEADER_CONTENT_TRANSFER_ENCODING,
	HEADER_CONTENT_DESCRIPTION,
	HEADER_CONTENT_LOCATION,
	HEADER_CONTENT_MD5,
	HEADER_UNKNOWN
};

static const char *content_headers[] = {
	"Content-Transfer-Encoding",
	"Content-Description",
	"Content-Location",
	"Content-Md5",
};


static gboolean
process_header (GMimeObject *object, GMimeHeader *header)
{
	GMimePart *mime_part = (GMimePart *) object;
	const char *name, *value;
	guint i;
	
	name = g_mime_header_get_name (header);
	
	if (g_ascii_strncasecmp (name, "Content-", 8) != 0)
		return FALSE;
	
	for (i = 0; i < G_N_ELEMENTS (content_headers); i++) {
		if (!g_ascii_strcasecmp (content_headers[i] + 8, name + 8))
			break;
	}
	
	switch (i) {
	case HEADER_CONTENT_TRANSFER_ENCODING:
		value = g_mime_header_get_value (header);
		mime_part->encoding = g_mime_content_encoding_from_string (value);
		break;
	case HEADER_CONTENT_DESCRIPTION:
		value = g_mime_header_get_value (header);
		g_free (mime_part->content_description);
		mime_part->content_description = g_strdup (value);
		break;
	case HEADER_CONTENT_LOCATION:
		value = g_mime_header_get_value (header);
		g_free (mime_part->content_location);
		mime_part->content_location = g_strdup (value);
		break;
	case HEADER_CONTENT_MD5:
		value = g_mime_header_get_value (header);
		g_free (mime_part->content_md5);
		mime_part->content_md5 = g_strdup (value);
		break;
	default:
		return FALSE;
	}
	
	return TRUE;
}

static void
mime_part_header_added (GMimeObject *object, GMimeHeader *header)
{
	if (process_header (object, header))
		return;
	
	GMIME_OBJECT_CLASS (parent_class)->header_added (object, header);
}

static void
mime_part_header_changed (GMimeObject *object, GMimeHeader *header)
{
	if (process_header (object, header))
		return;
	
	GMIME_OBJECT_CLASS (parent_class)->header_changed (object, header);
}

static void
mime_part_header_removed (GMimeObject *object, GMimeHeader *header)
{
	GMimePart *mime_part = (GMimePart *) object;
	const char *name;
	guint i;
	
	name = g_mime_header_get_name (header);
	
	if (!g_ascii_strncasecmp (name, "Content-", 8)) {
		for (i = 0; i < G_N_ELEMENTS (content_headers); i++) {
			if (!g_ascii_strcasecmp (content_headers[i] + 8, name + 8))
				break;
		}
		
		switch (i) {
		case HEADER_CONTENT_TRANSFER_ENCODING:
			mime_part->encoding = GMIME_CONTENT_ENCODING_DEFAULT;
			break;
		case HEADER_CONTENT_DESCRIPTION:
			g_free (mime_part->content_description);
			mime_part->content_description = NULL;
			break;
		case HEADER_CONTENT_LOCATION:
			g_free (mime_part->content_location);
			mime_part->content_location = NULL;
			break;
		case HEADER_CONTENT_MD5:
			g_free (mime_part->content_md5);
			mime_part->content_md5 = NULL;
			break;
		default:
			break;
		}
	}
	
	GMIME_OBJECT_CLASS (parent_class)->header_removed (object, header);
}

static void
mime_part_headers_cleared (GMimeObject *object)
{
	GMimePart *mime_part = (GMimePart *) object;
	
	mime_part->encoding = GMIME_CONTENT_ENCODING_DEFAULT;
	g_free (mime_part->content_description);
	mime_part->content_description = NULL;
	g_free (mime_part->content_location);
	mime_part->content_location = NULL;
	g_free (mime_part->content_md5);
	mime_part->content_md5 = NULL;
	
	GMIME_OBJECT_CLASS (parent_class)->headers_cleared (object);
}


static ssize_t
write_content (GMimePart *part, GMimeFormatOptions *options, GMimeStream *stream)
{
	GMimeObject *object = (GMimeObject *) part;
	ssize_t nwritten, total = 0;
	GMimeStream *filtered;
	GMimeFilter *filter;
	
	if (!part->content)
		return 0;
	
	/* Evil Genius's "slight" optimization: Since GMimeDataWrapper::write_to_stream()
	 * decodes its content stream to the raw format, we can cheat by requesting its
	 * content stream and not doing any encoding on the data if the source and
	 * destination encodings are identical.
	 */
	
	if (part->encoding != g_mime_data_wrapper_get_encoding (part->content)) {
		const char *newline = g_mime_format_options_get_newline (options);
		const char *filename;
		
		filtered = g_mime_stream_filter_new (stream);
		
		switch (part->encoding) {
		case GMIME_CONTENT_ENCODING_UUENCODE:
			if (!(filename = g_mime_part_get_filename (part)))
				filename = "unknown";
			
			if ((nwritten = g_mime_stream_printf (stream, "begin 0644 %s%s", filename, newline)) == -1)
				return -1;
			
			total += nwritten;
			
			/* fall thru... */
		case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
		case GMIME_CONTENT_ENCODING_BASE64:
			filter = g_mime_filter_basic_new (part->encoding, TRUE);
			g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
			g_object_unref (filter);
			break;
		default:
			break;
		}
		
		if (part->encoding != GMIME_CONTENT_ENCODING_BINARY) {
			filter = g_mime_format_options_create_newline_filter (options, object->ensure_newline);
			g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
			g_object_unref (filter);
		}
		
		nwritten = g_mime_data_wrapper_write_to_stream (part->content, filtered);
		g_mime_stream_flush (filtered);
		g_object_unref (filtered);
		
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
		
		if (part->encoding == GMIME_CONTENT_ENCODING_UUENCODE) {
			if ((nwritten = g_mime_stream_printf (stream, "end%s", newline)) == -1)
				return -1;
			
			total += nwritten;
		}
	} else {
		GMimeStream *content;
		
		content = g_mime_data_wrapper_get_stream (part->content);
		g_mime_stream_reset (content);
		
		filtered = g_mime_stream_filter_new (stream);
		
		if (part->encoding != GMIME_CONTENT_ENCODING_BINARY) {
			filter = g_mime_format_options_create_newline_filter (options, object->ensure_newline);
			g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
			g_object_unref (filter);
		}
		
		nwritten = g_mime_stream_write_to_stream (content, filtered);
		g_mime_stream_flush (filtered);
		g_mime_stream_reset (content);
		g_object_unref (filtered);
		
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
	}
	
	return total;
}

static ssize_t
mime_part_write_to_stream (GMimeObject *object, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream)
{
	GMimePart *mime_part = (GMimePart *) object;
	ssize_t nwritten, total = 0;
	const char *newline;
	
	if (!content_only) {
		/* write the content headers */
		if ((nwritten = g_mime_header_list_write_to_stream (object->headers, options, stream)) == -1)
			return -1;
		
		total += nwritten;
		
		/* terminate the headers */
		newline = g_mime_format_options_get_newline (options);
		if ((nwritten = g_mime_stream_write_string (stream, newline)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	if ((nwritten = write_content (mime_part, options, stream)) == -1)
		return -1;
	
	total += nwritten;
	
	return total;
}

static void
mime_part_encode (GMimeObject *object, GMimeEncodingConstraint constraint)
{
	GMimePart *part = (GMimePart *) object;
	GMimeContentEncoding encoding;
	GMimeStream *stream, *null;
	GMimeFilter *filter;
	
	switch (part->encoding) {
	case GMIME_CONTENT_ENCODING_BINARY:
		/* This encoding is only safe if the constraint is binary. */
		if (constraint == GMIME_ENCODING_CONSTRAINT_BINARY)
			return;
		break;
	case GMIME_CONTENT_ENCODING_BASE64:
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
	case GMIME_CONTENT_ENCODING_UUENCODE:
		/* These encodings are always safe. */
		return;
	default:
		break;
	}
	
	filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	
	null = g_mime_stream_null_new ();
	stream = g_mime_stream_filter_new (null);
	g_mime_stream_filter_add ((GMimeStreamFilter *) stream, filter);
	g_object_unref (null);
	
	g_mime_data_wrapper_write_to_stream (part->content, stream);
	g_object_unref (stream);
	
	encoding = g_mime_filter_best_encoding ((GMimeFilterBest *) filter, constraint);
	
	switch (part->encoding) {
	case GMIME_CONTENT_ENCODING_DEFAULT:
		g_mime_part_set_content_encoding (part, encoding);
		break;
	case GMIME_CONTENT_ENCODING_7BIT:
		/* This encoding is generally safe, but we may need to encode From-lines. */
		if (((GMimeFilterBest *) filter)->hadfrom)
			g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
		break;
	case GMIME_CONTENT_ENCODING_8BIT:
		if (constraint == GMIME_ENCODING_CONSTRAINT_7BIT)
			g_mime_part_set_content_encoding (part, encoding);
		else if (((GMimeFilterBest *) filter)->hadfrom)
			g_mime_part_set_content_encoding (part, GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE);
		break;
	default:
		break;
	}
	
	g_object_unref (filter);
}


/**
 * g_mime_part_new:
 *
 * Creates a new MIME Part object with a default content-type of
 * application/octet-stream.
 *
 * Returns: an empty MIME Part object with a default content-type of
 * application/octet-stream.
 **/
GMimePart *
g_mime_part_new (void)
{
	return g_mime_part_new_with_type ("application", "octet-stream");
}


/**
 * g_mime_part_new_with_type:
 * @type: content-type string
 * @subtype: content-subtype string
 *
 * Creates a new MIME Part with a sepcified type.
 *
 * Returns: an empty MIME Part object with the specified content-type.
 **/
GMimePart *
g_mime_part_new_with_type (const char *type, const char *subtype)
{
	GMimeContentType *content_type;
	GMimePart *mime_part;
	
	mime_part = g_object_new (GMIME_TYPE_PART, NULL);
	
	content_type = g_mime_content_type_new (type, subtype);
	g_mime_object_set_content_type ((GMimeObject *) mime_part, content_type);
	g_object_unref (content_type);
	
	return mime_part;
}


/**
 * g_mime_part_set_content_description:
 * @mime_part: a #GMimePart object
 * @description: content description
 *
 * Set the content description for the specified mime part.
 **/
void
g_mime_part_set_content_description (GMimePart *mime_part, const char *description)
{
	GMimeObject *object = (GMimeObject *) mime_part;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->content_description == description)
		return;
	
	g_free (mime_part->content_description);
	mime_part->content_description = g_strdup (description);
	
	_g_mime_object_block_header_list_changed (object);
	g_mime_header_list_set (object->headers, "Content-Description", description, NULL);
	_g_mime_object_unblock_header_list_changed (object);
}


/**
 * g_mime_part_get_content_description:
 * @mime_part: a #GMimePart object
 *
 * Gets the value of the Content-Description for the specified mime
 * part if it exists or %NULL otherwise.
 *
 * Returns: the content description for the specified mime part.
 **/
const char *
g_mime_part_get_content_description (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content_description;
}


/**
 * g_mime_part_set_content_id:
 * @mime_part: a #GMimePart object
 * @content_id: content id
 *
 * Set the content id for the specified mime part.
 **/
void
g_mime_part_set_content_id (GMimePart *mime_part, const char *content_id)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	g_mime_object_set_content_id ((GMimeObject *) mime_part, content_id);
}


/**
 * g_mime_part_get_content_id:
 * @mime_part: a #GMimePart object
 *
 * Gets the content-id of the specified mime part if it exists, or
 * %NULL otherwise.
 *
 * Returns: the content id for the specified mime part.
 **/
const char *
g_mime_part_get_content_id (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return g_mime_object_get_content_id ((GMimeObject *) mime_part);
}


/**
 * g_mime_part_set_content_md5:
 * @mime_part: a #GMimePart object
 * @content_md5: content md5 or %NULL to generate the md5 digest.
 *
 * Set the content md5 for the specified mime part.
 **/
void
g_mime_part_set_content_md5 (GMimePart *mime_part, const char *content_md5)
{
	GMimeObject *object = (GMimeObject *) mime_part;
	unsigned char digest[16], b64digest[32];
	GMimeContentType *content_type;
	GMimeStream *filtered, *stream;
	GMimeFilter *filter;
	guint32 save = 0;
	int state = 0;
	size_t len;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	g_free (mime_part->content_md5);
	
	if (!content_md5) {
		/* compute a md5sum */
		stream = g_mime_stream_null_new ();
		filtered = g_mime_stream_filter_new (stream);
		g_object_unref (stream);
		
		content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
		if (g_mime_content_type_is_type (content_type, "text", "*")) {
			filter = g_mime_filter_unix2dos_new (FALSE);
			g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
			g_object_unref (filter);
		}
		
	        filter = g_mime_filter_checksum_new (G_CHECKSUM_MD5);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		
		g_mime_data_wrapper_write_to_stream (mime_part->content, filtered);
		g_object_unref (filtered);
		
		memset (digest, 0, 16);
		g_mime_filter_checksum_get_digest ((GMimeFilterChecksum *) filter, digest, 16);
		g_object_unref (filter);
		
		len = g_mime_encoding_base64_encode_close (digest, 16, b64digest, &state, &save);
		b64digest[len] = '\0';
		g_strstrip ((char *) b64digest);
		
		content_md5 = (const char *) b64digest;
	}
	
	mime_part->content_md5 = g_strdup (content_md5);
	
	_g_mime_object_block_header_list_changed (object);
	g_mime_header_list_set (object->headers, "Content-Md5", content_md5, NULL);
	_g_mime_object_unblock_header_list_changed (object);
}


/**
 * g_mime_part_verify_content_md5:
 * @mime_part: a #GMimePart object
 *
 * Verify the content md5 for the specified mime part.
 *
 * Returns: %TRUE if the md5 is valid or %FALSE otherwise. Note: will
 * return %FALSE if the mime part does not contain a Content-MD5.
 **/
gboolean
g_mime_part_verify_content_md5 (GMimePart *mime_part)
{
	unsigned char digest[16], b64digest[32];
	GMimeContentType *content_type;
	GMimeStream *filtered, *stream;
	GMimeFilter *filter;
	guint32 save = 0;
	int state = 0;
	size_t len;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	g_return_val_if_fail (GMIME_IS_DATA_WRAPPER (mime_part->content), FALSE);
	
	if (!mime_part->content_md5)
		return FALSE;
	
	stream = g_mime_stream_null_new ();
	filtered = g_mime_stream_filter_new (stream);
	g_object_unref (stream);
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
	if (g_mime_content_type_is_type (content_type, "text", "*")) {
		filter = g_mime_filter_unix2dos_new (FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
	}
	
	filter = g_mime_filter_checksum_new (G_CHECKSUM_MD5);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	
	g_mime_data_wrapper_write_to_stream (mime_part->content, filtered);
	g_object_unref (filtered);
	
	memset (digest, 0, 16);
	g_mime_filter_checksum_get_digest ((GMimeFilterChecksum *) filter, digest, 16);
	g_object_unref (filter);
	
	len = g_mime_encoding_base64_encode_close (digest, 16, b64digest, &state, &save);
	b64digest[len] = '\0';
	g_strstrip ((char *) b64digest);
	
	return !strcmp ((char *) b64digest, mime_part->content_md5);
}


/**
 * g_mime_part_get_content_md5:
 * @mime_part: a #GMimePart object
 *
 * Gets the md5sum contained in the Content-Md5 header of the
 * specified mime part if it exists, or %NULL otherwise.
 *
 * Returns: the content md5 for the specified mime part.
 **/
const char *
g_mime_part_get_content_md5 (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content_md5;
}


/**
 * g_mime_part_set_content_location:
 * @mime_part: a #GMimePart object
 * @content_location: content location
 *
 * Set the content location for the specified mime part.
 **/
void
g_mime_part_set_content_location (GMimePart *mime_part, const char *content_location)
{
	GMimeObject *object = (GMimeObject *) mime_part;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->content_location == content_location)
		return;
	
	g_free (mime_part->content_location);
	mime_part->content_location = g_strdup (content_location);

	_g_mime_object_block_header_list_changed (object);
	g_mime_header_list_set (object->headers, "Content-Location", content_location, NULL);
	_g_mime_object_unblock_header_list_changed (object);
}


/**
 * g_mime_part_get_content_location:
 * @mime_part: a #GMimePart object
 *
 * Gets the value of the Content-Location header if it exists, or
 * %NULL otherwise.
 *
 * Returns: the content location for the specified mime part.
 **/
const char *
g_mime_part_get_content_location (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content_location;
}


/**
 * g_mime_part_set_content_encoding:
 * @mime_part: a #GMimePart object
 * @encoding: a #GMimeContentEncoding
 *
 * Set the content encoding for the specified mime part.
 **/
void
g_mime_part_set_content_encoding (GMimePart *mime_part, GMimeContentEncoding encoding)
{
	GMimeObject *object = (GMimeObject *) mime_part;
	const char *value;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	value = g_mime_content_encoding_to_string (encoding);
	mime_part->encoding = encoding;
	
	_g_mime_object_block_header_list_changed (object);
	if (value != NULL)
		g_mime_header_list_set (object->headers, "Content-Transfer-Encoding", value, NULL);
	else
		g_mime_header_list_remove (object->headers, "Content-Transfer-Encoding");
	_g_mime_object_unblock_header_list_changed (object);
}


/**
 * g_mime_part_get_content_encoding:
 * @mime_part: a #GMimePart object
 *
 * Gets the content encoding of the mime part.
 *
 * Returns: the content encoding for the specified mime part.
 **/
GMimeContentEncoding
g_mime_part_get_content_encoding (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), GMIME_CONTENT_ENCODING_DEFAULT);
	
	return mime_part->encoding;
}


/**
 * g_mime_part_get_best_content_encoding:
 * @mime_part: a #GMimePart object
 * @constraint: a #GMimeEncodingConstraint
 *
 * Calculates the most efficient content encoding for the @mime_part
 * given the @constraint.
 *
 * Returns: the best content encoding for the specified mime part.
 **/
GMimeContentEncoding
g_mime_part_get_best_content_encoding (GMimePart *mime_part, GMimeEncodingConstraint constraint)
{
	GMimeStream *filtered, *stream;
	GMimeContentEncoding encoding;
	GMimeFilterBest *best;
	GMimeFilter *filter;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), GMIME_CONTENT_ENCODING_DEFAULT);
	
	stream = g_mime_stream_null_new ();
	filtered = g_mime_stream_filter_new (stream);
	g_object_unref (stream);
	
	filter = g_mime_filter_best_new (GMIME_FILTER_BEST_ENCODING);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	best = (GMimeFilterBest *) filter;
	
	g_mime_data_wrapper_write_to_stream (mime_part->content, filtered);
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	
	encoding = g_mime_filter_best_encoding (best, constraint);
	g_object_unref (best);
	
	return encoding;
}


/**
 * g_mime_part_is_attachment:
 * @mime_part: a #GMimePart object
 *
 * Determines whether or not the part is an attachment based on the
 * value of the Content-Disposition header.
 *
 * Returns: %TRUE if the part is an attachment, otherwise %FALSE.
 **/
gboolean
g_mime_part_is_attachment (GMimePart *mime_part)
{
	GMimeContentDisposition *disposition;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	
	disposition = g_mime_object_get_content_disposition ((GMimeObject *) mime_part);
	
	return disposition != NULL && g_mime_content_disposition_is_attachment (disposition);
}


/**
 * g_mime_part_set_filename:
 * @mime_part: a #GMimePart object
 * @filename: the file name
 *
 * Sets the "filename" parameter on the Content-Disposition and also sets the
 * "name" parameter on the Content-Type.
 *
 * Note: The @filename string should be in UTF-8.
 **/
void
g_mime_part_set_filename (GMimePart *mime_part, const char *filename)
{
	GMimeObject *object = (GMimeObject *) mime_part;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	g_mime_object_set_content_disposition_parameter (object, "filename", filename);
	g_mime_object_set_content_type_parameter (object, "name", filename);
}


/**
 * g_mime_part_get_filename:
 * @mime_part: a #GMimePart object
 *
 * Gets the filename of the specificed mime part, or %NULL if the
 * @mime_part does not have the filename or name parameter set.
 *
 * Returns: the filename of the specified @mime_part or %NULL if
 * neither of the parameters is set. If a file name is set, the
 * returned string will be in UTF-8.
 **/
const char *
g_mime_part_get_filename (GMimePart *mime_part)
{
	GMimeObject *object = (GMimeObject *) mime_part;
	const char *filename = NULL;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if ((filename = g_mime_object_get_content_disposition_parameter (object, "filename")))
		return filename;
	
	/* check the "name" param in the content-type */
	return g_mime_object_get_content_type_parameter (object, "name");
}


static void
set_content (GMimePart *mime_part, GMimeDataWrapper *content)
{
	if (mime_part->content)
		g_object_unref (mime_part->content);
	
	mime_part->openpgp = (GMimeOpenPGPData) -1;
	
	mime_part->content = content;
	g_object_ref (content);
}


/**
 * g_mime_part_set_content:
 * @mime_part: a #GMimePart object
 * @content: a #GMimeDataWrapper content object
 *
 * Sets the content on the mime part.
 **/
void
g_mime_part_set_content (GMimePart *mime_part, GMimeDataWrapper *content)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->content == content)
		return;
	
	GMIME_PART_GET_CLASS (mime_part)->set_content (mime_part, content);
}


/**
 * g_mime_part_get_content:
 * @mime_part: a #GMimePart object
 *
 * Gets the internal data-wrapper of the specified mime part, or %NULL
 * on error.
 *
 * Returns: (transfer none): the data-wrapper for the mime part's
 * contents.
 **/
GMimeDataWrapper *
g_mime_part_get_content (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content;
}


/**
 * g_mime_part_set_openpgp_data:
 * @mime_part: a #GMimePart
 * @data: a #GMimeOpenPGPData
 *
 * Sets whether or not (and what type) of OpenPGP data is contained
 * within the #GMimePart.
 **/
void
g_mime_part_set_openpgp_data (GMimePart *mime_part, GMimeOpenPGPData data)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	mime_part->openpgp = data;
}


/**
 * g_mime_part_get_openpgp_data:
 * @mime_part: a #GMimePart
 *
 * Gets whether or not (and what type) of OpenPGP data is contained
 * within the #GMimePart.
 *
 * Returns: a #GMimeOpenPGPData.
 **/
GMimeOpenPGPData
g_mime_part_get_openpgp_data (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), GMIME_OPENPGP_DATA_NONE);
	
	if (mime_part->content == NULL)
		return GMIME_OPENPGP_DATA_NONE;
	
	if (mime_part->openpgp == (GMimeOpenPGPData) -1) {
		GMimeStream *filtered, *stream;
		GMimeFilterOpenPGP *openpgp;
		
		stream = g_mime_stream_null_new ();
		filtered = g_mime_stream_filter_new (stream);
		g_object_unref (stream);
		
		openpgp = (GMimeFilterOpenPGP *) g_mime_filter_openpgp_new ();
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, (GMimeFilter *) openpgp);
		g_mime_data_wrapper_write_to_stream (mime_part->content, filtered);
		g_mime_stream_flush (filtered);
		g_object_unref (filtered);
		
		mime_part->openpgp = g_mime_filter_openpgp_get_data_type (openpgp);
		g_object_unref (openpgp);
	}
	
	return mime_part->openpgp;
}


/**
 * g_mime_part_openpgp_encrypt:
 * @mime_part: a #GMimePart
 * @sign: %TRUE if the content should also be signed; otherwise, %FALSE
 * @userid: (nullable): the key id (or email address) to use when signing (assuming @sign is %TRUE)
 * @flags: a set of #GMimeEncryptFlags
 * @recipients: (element-type utf8): an array of recipient key ids and/or email addresses
 * @err: a #GError
 *
 * Encrypts (and optionally signs) the content of the @mime_part and then replaces
 * the content with the new, encrypted, content.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
gboolean
g_mime_part_openpgp_encrypt (GMimePart *mime_part, gboolean sign, const char *userid,
			     GMimeEncryptFlags flags, GPtrArray *recipients, GError **err)
{
	GMimeStream *istream, *encrypted;
	GMimeCryptoContext *ctx;
	int rv;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	
	if (mime_part->content == NULL) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("No content set on the MIME part."));
		return FALSE;
	}
	
	if (!(ctx = g_mime_crypto_context_new ("application/pgp-encrypted"))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("No crypto context registered for application/pgp-encrypted."));
		return FALSE;
	}
	
	encrypted = g_mime_stream_mem_new ();
	istream = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (mime_part->content, istream);
	g_mime_stream_reset (istream);
	
	rv = g_mime_crypto_context_encrypt (ctx, sign, userid, flags, recipients, istream, encrypted, err);
	g_object_unref (istream);
	g_object_unref (ctx);
	
	if (rv == -1) {
		g_object_unref (encrypted);
		return FALSE;
	}
	
	g_mime_stream_reset (encrypted);
	
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_data_wrapper_set_stream (mime_part->content, encrypted);
	mime_part->encoding = GMIME_CONTENT_ENCODING_7BIT;
	mime_part->openpgp = GMIME_OPENPGP_DATA_ENCRYPTED;
	g_object_unref (encrypted);
	
	return TRUE;
}


/**
 * g_mime_part_openpgp_decrypt:
 * @mime_part: a #GMimePart
 * @flags: a set of #GMimeDecryptFlags
 * @session_key: (nullable): the session key to use or %NULL
 * @err: a #GError
 *
 * Decrypts the content of the @mime_part and then replaces the content with
 * the new, decrypted, content.
 *
 * Returns: (nullable) (transfer full): a #GMimeDecryptResult on success or %NULL on error.
 **/
GMimeDecryptResult *
g_mime_part_openpgp_decrypt (GMimePart *mime_part, GMimeDecryptFlags flags, const char *session_key, GError **err)
{
	GMimeStream *istream, *decrypted;
	GMimeDecryptResult *result;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	
	if (mime_part->content == NULL) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("No content set on the MIME part."));
		return NULL;
	}
	
	if (!(ctx = g_mime_crypto_context_new ("application/pgp-encrypted"))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("No crypto context registered for application/pgp-encrypted."));
		return NULL;
	}
	
	decrypted = g_mime_stream_mem_new ();
	istream = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (mime_part->content, istream);
	g_mime_stream_reset (istream);
	
	result = g_mime_crypto_context_decrypt (ctx, flags, session_key, istream, decrypted, err);
	g_object_unref (istream);
	g_object_unref (ctx);
	
	if (result == NULL) {
		g_object_unref (decrypted);
		return NULL;
	}
	
	g_mime_stream_reset (decrypted);
	
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_data_wrapper_set_stream (mime_part->content, decrypted);
	mime_part->openpgp = GMIME_OPENPGP_DATA_NONE;
	g_object_unref (decrypted);
	
	return result;
}


/**
 * g_mime_part_openpgp_sign:
 * @mime_part: a #GMimePart
 * @userid: the key id (or email address) to use for signing
 * @err: a #GError
 *
 * Signs the content of the @mime_part and then replaces the content with
 * the new, signed, content.
 *
 * Returns: %TRUE on success or %FALSE on error.
 **/
gboolean
g_mime_part_openpgp_sign (GMimePart *mime_part, const char *userid, GError **err)
{
	GMimeStream *istream, *ostream;
	GMimeCryptoContext *ctx;
	int rv;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	
	if (mime_part->content == NULL) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("No content set on the MIME part."));
		return FALSE;
	}
	
	if (!(ctx = g_mime_crypto_context_new ("application/pgp-signature"))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("No crypto context registered for application/pgp-signature."));
		return FALSE;
	}
	
	ostream = g_mime_stream_mem_new ();
	istream = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (mime_part->content, istream);
	g_mime_stream_reset (istream);
	
	rv = g_mime_crypto_context_sign (ctx, FALSE, userid, istream, ostream, err);
	g_object_unref (istream);
	g_object_unref (ctx);
	
	if (rv == -1) {
		g_object_unref (ostream);
		return FALSE;
	}
	
	g_mime_stream_reset (ostream);
	
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_data_wrapper_set_stream (mime_part->content, ostream);
	mime_part->encoding = GMIME_CONTENT_ENCODING_7BIT;
	mime_part->openpgp = GMIME_OPENPGP_DATA_SIGNED;
	g_object_unref (ostream);
	
	return TRUE;
}


/**
 * g_mime_part_openpgp_verify:
 * @mime_part: a #GMimePart
 * @flags: a set of #GMimeVerifyFlags
 * @err: a #GError
 *
 * Verifies the OpenPGP signature of the @mime_part and then replaces the content
 * with the original, raw, content.
 *
 * Returns: (nullable) (transfer full): a #GMimeSignatureList on success or %NULL on error.
 **/
GMimeSignatureList *
g_mime_part_openpgp_verify (GMimePart *mime_part, GMimeVerifyFlags flags, GError **err)
{
	GMimeStream *istream, *extracted;
	GMimeSignatureList *signatures;
	GMimeCryptoContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	
	if (mime_part->content == NULL) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
				     _("No content set on the MIME part."));
		return NULL;
	}
	
	if (!(ctx = g_mime_crypto_context_new ("application/pgp-signature"))) {
		g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
				     _("No crypto context registered for application/pgp-signature."));
		return NULL;
	}
	
	extracted = g_mime_stream_mem_new ();
	istream = g_mime_stream_mem_new ();
	g_mime_data_wrapper_write_to_stream (mime_part->content, istream);
	g_mime_stream_reset (istream);
	
	signatures = g_mime_crypto_context_verify (ctx, flags, istream, NULL, extracted, err);
	g_object_unref (istream);
	g_object_unref (ctx);
	
	if (signatures == NULL) {
		g_object_unref (extracted);
		return NULL;
	}
	
	g_mime_stream_reset (extracted);
	
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_CONTENT_ENCODING_DEFAULT);
	g_mime_data_wrapper_set_stream (mime_part->content, extracted);
	mime_part->openpgp = GMIME_OPENPGP_DATA_NONE;
	g_object_unref (extracted);
	
	return signatures;
}
