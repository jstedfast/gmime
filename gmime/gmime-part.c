/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000-2004 Ximian, Inc. (www.ximian.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "gmime-part.h"
#include "gmime-utils.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-null.h"
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "gmime-filter-crlf.h"
#include "gmime-filter-md5.h"

#define d(x)

/* GObject class methods */
static void g_mime_part_class_init (GMimePartClass *klass);
static void g_mime_part_init (GMimePart *mime_part, GMimePartClass *klass);
static void g_mime_part_finalize (GObject *object);

/* GMimeObject class methods */
static void mime_part_init (GMimeObject *object);
static void mime_part_add_header (GMimeObject *object, const char *header, const char *value);
static void mime_part_set_header (GMimeObject *object, const char *header, const char *value);
static const char *mime_part_get_header (GMimeObject *object, const char *header);
static void mime_part_remove_header (GMimeObject *object, const char *header);
static char *mime_part_get_headers (GMimeObject *object);
static ssize_t mime_part_write_to_stream (GMimeObject *object, GMimeStream *stream);

static ssize_t write_disposition (GMimeStream *stream, const char *name, const char *value);

#define NEEDS_DECODING(encoding) (((GMimePartEncodingType) encoding) == GMIME_PART_ENCODING_BASE64 ||   \
				  ((GMimePartEncodingType) encoding) == GMIME_PART_ENCODING_UUENCODE || \
				  ((GMimePartEncodingType) encoding) == GMIME_PART_ENCODING_QUOTEDPRINTABLE)


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
			16,   /* n_preallocs */
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
	
	object_class->init = mime_part_init;
	object_class->add_header = mime_part_add_header;
	object_class->set_header = mime_part_set_header;
	object_class->get_header = mime_part_get_header;
	object_class->remove_header = mime_part_remove_header;
	object_class->get_headers = mime_part_get_headers;
	object_class->write_to_stream = mime_part_write_to_stream;
}

static void
g_mime_part_init (GMimePart *mime_part, GMimePartClass *klass)
{
	mime_part->encoding = GMIME_PART_ENCODING_DEFAULT;
	mime_part->disposition = NULL;
	mime_part->content_description = NULL;
	mime_part->content_location = NULL;
	mime_part->content_md5 = NULL;
	mime_part->content = NULL;
	
	g_mime_header_register_writer (((GMimeObject *) mime_part)->headers, "Content-Disposition", write_disposition);
}

static void
g_mime_part_finalize (GObject *object)
{
	GMimePart *mime_part = (GMimePart *) object;
	
	if (mime_part->disposition)
		g_mime_disposition_destroy (mime_part->disposition);
	
	g_free (mime_part->content_description);
	g_free (mime_part->content_location);
	g_free (mime_part->content_md5);
	
	if (mime_part->content)
		g_object_unref (mime_part->content);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
mime_part_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}

enum {
	HEADER_CONTENT_TRANSFER_ENCODING,
	HEADER_CONTENT_DISPOSITION,
	HEADER_CONTENT_DESCRIPTION,
	HEADER_CONTENT_LOCATION,
	HEADER_CONTENT_MD5,
	HEADER_UNKNOWN
};

static char *headers[] = {
	"Content-Transfer-Encoding",
	"Content-Disposition",
	"Content-Description",
	"Content-Location",
	"Content-Md5",
	NULL
};

static ssize_t
write_disposition (GMimeStream *stream, const char *name, const char *value)
{
	GMimeDisposition *disposition;
	ssize_t nwritten;
	GString *out;
	
	out = g_string_new ("");
	g_string_printf (out, "%s: ", name);
	
	disposition = g_mime_disposition_new (value);
	g_string_append (out, disposition->disposition);
	
	g_mime_param_write_to_string (disposition->params, TRUE, out);
	g_mime_disposition_destroy (disposition);
	
	nwritten = g_mime_stream_write (stream, out->str, out->len);
	g_string_free (out, TRUE);
	
	return nwritten;
}

static void
set_disposition (GMimePart *mime_part, const char *disposition)
{
	if (mime_part->disposition)
		g_mime_disposition_destroy (mime_part->disposition);
	
	mime_part->disposition = g_mime_disposition_new (disposition);
}

static void
sync_content_disposition (GMimePart *mime_part)
{
	char *str;
	
	if (mime_part->disposition) {
		str = g_mime_disposition_header (mime_part->disposition, FALSE);
		g_mime_header_set (GMIME_OBJECT (mime_part)->headers, "Content-Disposition", str);
		g_free (str);
	}
}

static gboolean
process_header (GMimeObject *object, const char *header, const char *value)
{
	GMimePart *mime_part = (GMimePart *) object;
	char *text;
	int i;
	
	for (i = 0; headers[i]; i++) {
		if (!strcasecmp (headers[i], header))
			break;
	}
	
	switch (i) {
	case HEADER_CONTENT_TRANSFER_ENCODING:
		text = g_alloca (strlen (value) + 1);
		strcpy (text, value);
		g_strstrip (text);
		mime_part->encoding = g_mime_part_encoding_from_string (text);
		break;
	case HEADER_CONTENT_DISPOSITION:
		set_disposition (mime_part, value);
		break;
	case HEADER_CONTENT_DESCRIPTION:
		/* FIXME: we should decode this */
		g_free (mime_part->content_description);
		mime_part->content_description = g_strstrip (g_strdup (value));
		break;
	case HEADER_CONTENT_LOCATION:
		g_free (mime_part->content_location);
		mime_part->content_location = g_strstrip (g_strdup (value));
		break;
	case HEADER_CONTENT_MD5:
		g_free (mime_part->content_md5);
		mime_part->content_md5 = g_strstrip (g_strdup (value));
		break;
	default:
		return FALSE;
		break;
	}
	
	return TRUE;
}

static void
mime_part_add_header (GMimeObject *object, const char *header, const char *value)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a mime part */
	
	if (!strncasecmp ("Content-", header, 8)) {
		if (process_header (object, header, value))
			g_mime_header_add (object->headers, header, value);
		else
			GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
	}
}

static void
mime_part_set_header (GMimeObject *object, const char *header, const char *value)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a mime part */
	
	if (!strncasecmp ("Content-", header, 8)) {
		if (process_header (object, header, value))
			g_mime_header_set (object->headers, header, value);
		else
			GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
	}
}

static const char *
mime_part_get_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a mime part */
	
	if (!strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
	else
		return NULL;
}

static void
mime_part_remove_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
	   doesn't belong on a mime part */
	
	if (!strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static char *
mime_part_get_headers (GMimeObject *object)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_headers (object);
}

static ssize_t
write_content (GMimePart *part, GMimeStream *stream)
{
	ssize_t nwritten, total = 0;
	
	if (!part->content)
		return 0;
	
	/* Evil Genius's "slight" optimization: Since GMimeDataWrapper::write_to_stream()
	 * decodes its content stream to the raw format, we can cheat by requesting its
	 * content stream and not doing any encoding on the data if the source and
	 * destination encodings are identical.
	 */
	
	if (part->encoding != g_mime_data_wrapper_get_encoding (part->content)) {
		GMimeStream *filtered_stream;
		const char *filename;
		GMimeFilter *filter;
		
		filtered_stream = g_mime_stream_filter_new_with_stream (stream);
		switch (part->encoding) {
		case GMIME_PART_ENCODING_BASE64:
			filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_BASE64_ENC);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
			g_object_unref (filter);
			break;
		case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
			filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_QP_ENC);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
			g_object_unref (filter);
			break;
		case GMIME_PART_ENCODING_UUENCODE:
			filename = g_mime_part_get_filename (part);
			nwritten = g_mime_stream_printf (stream, "begin 0644 %s\n",
							 filename ? filename : "unknown");
			if (nwritten == -1) {
				g_object_unref (filtered_stream);
				return -1;
			}
			
			total += nwritten;
			
			filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_UU_ENC);
			g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
			g_object_unref (filter);
			break;
		default:
			break;
		}
		
		nwritten = g_mime_data_wrapper_write_to_stream (part->content, filtered_stream);
		g_mime_stream_flush (filtered_stream);
		g_object_unref (filtered_stream);
		
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
		
		if (part->encoding == GMIME_PART_ENCODING_UUENCODE) {
			/* FIXME: get rid of this special-case x-uuencode crap */
			nwritten = g_mime_stream_write (stream, "end\n", 4);
			if (nwritten == -1)
				return -1;
			
			total += nwritten;
		}
	} else {
		GMimeStream *content_stream;
		
		content_stream = g_mime_data_wrapper_get_stream (part->content);
		g_mime_stream_reset (content_stream);
		nwritten = g_mime_stream_write_to_stream (content_stream, stream);
		g_object_unref (content_stream);
		
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
	}
	
	return total;
}

static ssize_t
mime_part_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimePart *mime_part = (GMimePart *) object;
	ssize_t nwritten, total = 0;
	
	/* write the content headers */
	if ((nwritten = g_mime_header_write_to_stream (object->headers, stream)) == -1)
		return -1;
	
	total += nwritten;
	
	/* terminate the headers */
	if (g_mime_stream_write (stream, "\n", 1) == -1)
		return -1;
	
	total++;
	
	if ((nwritten = write_content (mime_part, stream)) == -1)
		return -1;
	
	total += nwritten;
	
	return total;
}


/**
 * g_mime_part_new:
 *
 * Creates a new MIME Part object with a default content-type of
 * text/plain.
 *
 * Returns an empty MIME Part object with a default content-type of
 * text/plain.
 **/
GMimePart *
g_mime_part_new ()
{
	GMimePart *mime_part;
	GMimeContentType *type;
	
	mime_part = g_object_new (GMIME_TYPE_PART, NULL, NULL);
	
	type = g_mime_content_type_new ("text", "plain");
	g_mime_object_set_content_type (GMIME_OBJECT (mime_part), type);
	
	return mime_part;
}


/**
 * g_mime_part_new_with_type:
 * @type: content-type
 * @subtype: content-subtype
 *
 * Creates a new MIME Part with a sepcified type.
 *
 * Returns an empty MIME Part object with the specified content-type.
 **/
GMimePart *
g_mime_part_new_with_type (const char *type, const char *subtype)
{
	GMimePart *mime_part;
	GMimeContentType *content_type;
	
	mime_part = g_object_new (GMIME_TYPE_PART, NULL, NULL);
	
	content_type = g_mime_content_type_new (type, subtype);
	g_mime_object_set_content_type (GMIME_OBJECT (mime_part), content_type);
	
	return mime_part;
}


/**
 * g_mime_part_set_content_header:
 * @mime_part: mime part
 * @header: header name
 * @value: header value
 *
 * Set an arbitrary MIME content header.
 **/
void
g_mime_part_set_content_header (GMimePart *mime_part, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (header != NULL);
	
	g_mime_object_set_header (GMIME_OBJECT (mime_part), header, value);
}


/**
 * g_mime_part_get_content_header:
 * @mime_part: mime part
 * @header: header name
 *
 * Gets the value of the requested header if it exists, or %NULL
 * otherwise.
 *
 * Returns the value of the content header @header.
 **/
const char *
g_mime_part_get_content_header (GMimePart *mime_part, const char *header)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	g_return_val_if_fail (header != NULL, NULL);
	
	return g_mime_object_get_header (GMIME_OBJECT (mime_part), header);
}


/**
 * g_mime_part_set_content_description:
 * @mime_part: Mime part
 * @description: content description
 *
 * Set the content description for the specified mime part.
 **/
void
g_mime_part_set_content_description (GMimePart *mime_part, const char *description)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->content_description)
		g_free (mime_part->content_description);
	
	mime_part->content_description = g_strdup (description);
	g_mime_header_set (GMIME_OBJECT (mime_part)->headers, "Content-Description", description);
}


/**
 * g_mime_part_get_content_description:
 * @mime_part: Mime part
 *
 * Gets the value of the Content-Description for the specified mime
 * part if it exists or %NULL otherwise.
 *
 * Returns the content description for the specified mime part.
 **/
const char *
g_mime_part_get_content_description (const GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content_description;
}


/**
 * g_mime_part_set_content_id:
 * @mime_part: Mime part
 * @content_id: content id
 *
 * Set the content id for the specified mime part.
 **/
void
g_mime_part_set_content_id (GMimePart *mime_part, const char *content_id)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	g_mime_object_set_content_id (GMIME_OBJECT (mime_part), content_id);
}


/**
 * g_mime_part_get_content_id:
 * @mime_part: Mime part
 *
 * Gets the content-id of the specified mime part if it exists, or
 * %NULL otherwise.
 *
 * Returns the content id for the specified mime part.
 **/
const char *
g_mime_part_get_content_id (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return g_mime_object_get_content_id (GMIME_OBJECT (mime_part));
}


/**
 * g_mime_part_set_content_md5:
 * @mime_part: Mime part
 * @content_md5: content md5 or NULL to generate the md5 digest.
 *
 * Set the content md5 for the specified mime part.
 **/
void
g_mime_part_set_content_md5 (GMimePart *mime_part, const char *content_md5)
{
	unsigned char digest[16], b64digest[32];
	const GMimeContentType *content_type;
	GMimeStreamFilter *filtered_stream;
	GMimeFilter *md5_filter;
	GMimeStream *stream;
	int state, save;
	size_t len;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->content_md5)
		g_free (mime_part->content_md5);
	
	if (!content_md5) {
		/* compute a md5sum */
		stream = g_mime_stream_null_new ();
		filtered_stream = (GMimeStreamFilter *) g_mime_stream_filter_new_with_stream (stream);
		g_object_unref (stream);
		
		content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
		if (g_mime_content_type_is_type (content_type, "text", "*")) {
			GMimeFilter *crlf_filter;
			
			crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
							      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
			
			g_mime_stream_filter_add (filtered_stream, crlf_filter);
			g_object_unref (crlf_filter);
		}
		
		md5_filter = g_mime_filter_md5_new ();
		g_mime_stream_filter_add (filtered_stream, md5_filter);
		
		stream = (GMimeStream *) filtered_stream;
		g_mime_data_wrapper_write_to_stream (mime_part->content, stream);
		g_object_unref (stream);
		
		memset (digest, 0, 16);
		g_mime_filter_md5_get_digest ((GMimeFilterMd5 *) md5_filter, digest);
		g_object_unref (md5_filter);
		
		state = save = 0;
		len = g_mime_utils_base64_encode_close (digest, 16, b64digest, &state, &save);
		b64digest[len] = '\0';
		g_strstrip (b64digest);
		
		content_md5 = (const char *) b64digest;
	}
	
	mime_part->content_md5 = g_strdup (content_md5);
	g_mime_header_set (GMIME_OBJECT (mime_part)->headers, "Content-Md5", content_md5);
}


/**
 * g_mime_part_verify_content_md5:
 * @mime_part: Mime part
 *
 * Verify the content md5 for the specified mime part.
 *
 * Returns TRUE if the md5 is valid or FALSE otherwise. Note: will
 * return FALSE if the mime part does not contain a Content-MD5.
 **/
gboolean
g_mime_part_verify_content_md5 (GMimePart *mime_part)
{
	unsigned char digest[16], b64digest[32];
	const GMimeContentType *content_type;
	GMimeStreamFilter *filtered_stream;
	GMimeFilter *md5_filter;
	GMimeStream *stream;
	int state, save;
	size_t len;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	g_return_val_if_fail (mime_part->content != NULL, FALSE);
	
	if (!mime_part->content_md5)
		return FALSE;
	
	stream = g_mime_stream_null_new ();
	filtered_stream = (GMimeStreamFilter *) g_mime_stream_filter_new_with_stream (stream);
	g_object_unref (stream);
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
	if (g_mime_content_type_is_type (content_type, "text", "*")) {
		GMimeFilter *crlf_filter;
		
		crlf_filter = g_mime_filter_crlf_new (GMIME_FILTER_CRLF_ENCODE,
						      GMIME_FILTER_CRLF_MODE_CRLF_ONLY);
		
		g_mime_stream_filter_add (filtered_stream, crlf_filter);
		g_object_unref (crlf_filter);
	}
	
	md5_filter = g_mime_filter_md5_new ();
	g_mime_stream_filter_add (filtered_stream, md5_filter);
	
	stream = (GMimeStream *) filtered_stream;
	g_mime_data_wrapper_write_to_stream (mime_part->content, stream);
	g_object_unref (stream);
	
	memset (digest, 0, 16);
	g_mime_filter_md5_get_digest ((GMimeFilterMd5 *) md5_filter, digest);
	g_object_unref (md5_filter);
	
	state = save = 0;
	len = g_mime_utils_base64_encode_close (digest, 16, b64digest, &state, &save);
	b64digest[len] = '\0';
	g_strstrip (b64digest);
	
	return !strcmp (b64digest, mime_part->content_md5);
}


/**
 * g_mime_part_get_content_md5:
 * @mime_part: Mime part
 *
 * Gets the md5sum contained in the Content-Md5 header of the
 * specified mime part if it exists, or %NULL otherwise.
 *
 * Returns the content md5 for the specified mime part.
 **/
const char *
g_mime_part_get_content_md5 (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content_md5;
}


/**
 * g_mime_part_set_content_location:
 * @mime_part: Mime part
 * @content_location: content location
 *
 * Set the content location for the specified mime part.
 **/
void
g_mime_part_set_content_location (GMimePart *mime_part, const char *content_location)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->content_location)
		g_free (mime_part->content_location);
	
	mime_part->content_location = g_strdup (content_location);
	g_mime_header_set (GMIME_OBJECT (mime_part)->headers, "Content-Location", content_location);
}


/**
 * g_mime_part_get_content_location:
 * @mime_part: Mime part
 *
 * Gets the value of the Content-Location header if it exists, or
 * %NULL otherwise.
 *
 * Returns the content location for the specified mime part.
 **/
const char *
g_mime_part_get_content_location (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return mime_part->content_location;
}


/**
 * g_mime_part_set_content_type:
 * @mime_part: Mime part
 * @mime_type: Mime content-type
 *
 * Set the content type/subtype for the specified mime part.
 **/
void
g_mime_part_set_content_type (GMimePart *mime_part, GMimeContentType *mime_type)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (mime_type != NULL);
	
	g_mime_object_set_content_type (GMIME_OBJECT (mime_part), mime_type);
}


/**
 * g_mime_part_get_content_type: Get the content type/subtype
 * @mime_part: Mime part
 *
 * Gets the Content-Type object for the given mime part or %NULL on
 * error.
 *
 * Returns the content-type object for the specified mime part.
 **/
const GMimeContentType *
g_mime_part_get_content_type (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return g_mime_object_get_content_type (GMIME_OBJECT (mime_part));
}


/**
 * g_mime_part_set_encoding: Set the content encoding
 * @mime_part: Mime part
 * @encoding: Mime encoding
 *
 * Set the content encoding for the specified mime part. Available
 * values for the encoding are: GMIME_PART_ENCODING_DEFAULT,
 * GMIME_PART_ENCODING_7BIT, GMIME_PART_ENCODING_8BIT,
 * GMIME_PART_ENCODING_BINARY, GMIME_PART_ENCODING_BASE64 and
 * GMIME_PART_ENCODING_QUOTEDPRINTABLE.
 **/
void
g_mime_part_set_encoding (GMimePart *mime_part, GMimePartEncodingType encoding)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	mime_part->encoding = encoding;
	g_mime_header_set (GMIME_OBJECT (mime_part)->headers, "Content-Transfer-Encoding",
			   g_mime_part_encoding_to_string (encoding));
}


/**
 * g_mime_part_get_encoding: Get the content encoding
 * @mime_part: Mime part
 *
 * Gets the content encoding of the mime part.
 *
 * Returns the content encoding for the specified mime part. The
 * return value will be one of the following:
 * GMIME_PART_ENCODING_DEFAULT, GMIME_PART_ENCODING_7BIT,
 * GMIME_PART_ENCODING_8BIT, GMIME_PART_ENCODING_BINARY,
 * GMIME_PART_ENCODING_BASE64, GMIME_PART_ENCODING_QUOTEDPRINTABLE
 * or GMIME_PART_ENCODING_UUENCODE.
 **/
GMimePartEncodingType
g_mime_part_get_encoding (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), GMIME_PART_ENCODING_DEFAULT);
	
	return mime_part->encoding;
}


/**
 * g_mime_part_encoding_to_string:
 * @encoding: Mime encoding
 *
 * Gets the string value of the content encoding.
 *
 * Returns the encoding type as a string. Available
 * values for the encoding are: GMIME_PART_ENCODING_DEFAULT,
 * GMIME_PART_ENCODING_7BIT, GMIME_PART_ENCODING_8BIT,
 * GMIME_PART_ENCODING_BINARY, GMIME_PART_ENCODING_BASE64,
 * GMIME_PART_ENCODING_QUOTEDPRINTABLE and
 * GMIME_PART_ENCODING_UUENCODE.
 **/
const char *
g_mime_part_encoding_to_string (GMimePartEncodingType encoding)
{
	switch (encoding) {
        case GMIME_PART_ENCODING_7BIT:
		return "7bit";
        case GMIME_PART_ENCODING_8BIT:
		return "8bit";
	case GMIME_PART_ENCODING_BINARY:
		return "binary";
        case GMIME_PART_ENCODING_BASE64:
		return "base64";
        case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		return "quoted-printable";
	case GMIME_PART_ENCODING_UUENCODE:
		return "x-uuencode";
	default:
		/* I guess this is a good default... */
		return NULL;
	}
}


/**
 * g_mime_part_encoding_from_string:
 * @encoding: Mime encoding in string format
 *
 * Gets the content encoding enumeration value based on the input
 * string.
 *
 * Returns the encoding string as a GMimePartEncodingType.  Available
 * values for the encoding are: GMIME_PART_ENCODING_DEFAULT,
 * GMIME_PART_ENCODING_7BIT, GMIME_PART_ENCODING_8BIT,
 * GMIME_PART_ENCODING_BINARY, GMIME_PART_ENCODING_BASE64,
 * GMIME_PART_ENCODING_QUOTEDPRINTABLE and
 * GMIME_PART_ENCODING_UUENCODE.
 **/
GMimePartEncodingType
g_mime_part_encoding_from_string (const char *encoding)
{
	if (!strcasecmp (encoding, "7bit"))
		return GMIME_PART_ENCODING_7BIT;
	else if (!strcasecmp (encoding, "8bit"))
		return GMIME_PART_ENCODING_8BIT;
	else if (!strcasecmp (encoding, "binary"))
		return GMIME_PART_ENCODING_BINARY;
	else if (!strcasecmp (encoding, "base64"))
		return GMIME_PART_ENCODING_BASE64;
	else if (!strcasecmp (encoding, "quoted-printable"))
		return GMIME_PART_ENCODING_QUOTEDPRINTABLE;
	else if (!strcasecmp (encoding, "x-uuencode"))
		return GMIME_PART_ENCODING_UUENCODE;
	else return GMIME_PART_ENCODING_DEFAULT;
}


/**
 * g_mime_part_set_content_disposition_object:
 * @mime_part: Mime part
 * @disposition: disposition object
 *
 * Set the content disposition for the specified mime part
 **/
void
g_mime_part_set_content_disposition_object (GMimePart *mime_part, GMimeDisposition *disposition)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (mime_part->disposition)
		g_mime_disposition_destroy (mime_part->disposition);
	
	mime_part->disposition = disposition;
	
	sync_content_disposition (mime_part);
}


/**
 * g_mime_part_set_content_disposition:
 * @mime_part: Mime part
 * @disposition: disposition
 *
 * Set the content disposition for the specified mime part
 **/
void
g_mime_part_set_content_disposition (GMimePart *mime_part, const char *disposition)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	set_disposition (mime_part, disposition);
	sync_content_disposition (mime_part);
}


/**
 * g_mime_part_get_content_disposition:
 * @mime_part: Mime part
 *
 * Gets the content disposition if set or %NULL otherwise.
 *
 * Returns the content disposition for the specified mime part.
 **/
const char *
g_mime_part_get_content_disposition (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if (mime_part->disposition)
		return g_mime_disposition_get (mime_part->disposition);
	
	return NULL;
}


/**
 * g_mime_part_add_content_disposition_parameter:
 * @mime_part: Mime part
 * @attribute: parameter name
 * @value: parameter value
 *
 * Add a content-disposition parameter to the specified mime part.
 **/
void
g_mime_part_add_content_disposition_parameter (GMimePart *mime_part, const char *attribute, const char *value)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (attribute != NULL);
	
	if (!mime_part->disposition)
		mime_part->disposition = g_mime_disposition_new (GMIME_DISPOSITION_ATTACHMENT);
	
	g_mime_disposition_add_parameter (mime_part->disposition, attribute, value);
	
	sync_content_disposition (mime_part);
}


/**
 * g_mime_part_get_content_disposition_parameter:
 * @mime_part: Mime part
 * @attribute: parameter name
 *
 * Gets the value of the Content-Disposition parameter specified by
 * @attribute, or %NULL if the parameter does not exist.
 *
 * Returns the value of a previously defined content-disposition
 * parameter specified by #name.
 **/
const char *
g_mime_part_get_content_disposition_parameter (GMimePart *mime_part, const char *attribute)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	g_return_val_if_fail (attribute != NULL, NULL);
	
	if (!mime_part->disposition)
		return NULL;
	
	return g_mime_disposition_get_parameter (mime_part->disposition, attribute);
}


/**
 * g_mime_part_set_filename:
 * @mime_part: Mime part
 * @filename: the filename of the Mime Part's content
 *
 * Sets the "filename" parameter on the Content-Disposition and also sets the
 * "name" parameter on the Content-Type.
 **/
void
g_mime_part_set_filename (GMimePart *mime_part, const char *filename)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (!mime_part->disposition)
		mime_part->disposition = g_mime_disposition_new (GMIME_DISPOSITION_ATTACHMENT);
	
	g_mime_disposition_add_parameter (mime_part->disposition, "filename", filename);
	
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (mime_part), "name", filename);
	
	sync_content_disposition (mime_part);
}


/**
 * g_mime_part_get_filename:
 * @mime_part: Mime part
 *
 * Gets the filename of the specificed mime part, or %NULL if the mime
 * part does not have the filename or name parameter set.
 *
 * Returns the filename of the specified MIME Part. It first checks to
 * see if the "filename" parameter was set on the Content-Disposition
 * and if not then checks the "name" parameter in the Content-Type.
 **/
const char *
g_mime_part_get_filename (const GMimePart *mime_part)
{
	const char *filename = NULL;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if (mime_part->disposition)
		filename = g_mime_disposition_get_parameter (mime_part->disposition, "filename");
	
	if (!filename) {
		/* check the "name" param in the content-type */
		return g_mime_object_get_content_type_parameter (GMIME_OBJECT (mime_part), "name");
	}
	
	return filename;
}


/**
 * g_mime_part_set_content:
 * @mime_part: Mime part
 * @content: raw mime part content
 * @len: raw content length
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_part_set_content_object() instead.
 *
 * Sets the content of the Mime Part (only non-multiparts)
 **/
void
g_mime_part_set_content (GMimePart *mime_part, const char *content, size_t len)
{
	GMimeStream *stream;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (!mime_part->content)
		mime_part->content = g_mime_data_wrapper_new ();
	
	stream = g_mime_stream_mem_new_with_buffer (content, len);
	g_mime_data_wrapper_set_stream (mime_part->content, stream);
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_PART_ENCODING_DEFAULT);
	g_object_unref (stream);
}


/**
 * g_mime_part_set_content_byte_array:
 * @mime_part: Mime part
 * @content: raw mime part content.
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_part_set_content_object() instead.
 *
 * Sets the content of the Mime Part (only non-multiparts)
 **/
void
g_mime_part_set_content_byte_array (GMimePart *mime_part, GByteArray *content)
{
	GMimeStream *stream;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (!mime_part->content)
		mime_part->content = g_mime_data_wrapper_new ();
	
	stream = g_mime_stream_mem_new_with_byte_array (content);
	g_mime_data_wrapper_set_stream (mime_part->content, stream);
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_PART_ENCODING_DEFAULT);
	g_object_unref (stream);
}


/**
 * g_mime_part_set_pre_encoded_content:
 * @mime_part: Mime part
 * @content: encoded mime part content
 * @len: length of the content
 * @encoding: content encoding
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_part_set_content_object() instead.
 *
 * Sets the encoding type and raw content on the mime part after
 * decoding the content.
 **/
void
g_mime_part_set_pre_encoded_content (GMimePart *mime_part, const char *content,
				     size_t len, GMimePartEncodingType encoding)
{
	GMimeStream *stream, *filtered_stream;
	GMimeFilter *filter;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (content != NULL);
	
	if (!mime_part->content)
		mime_part->content = g_mime_data_wrapper_new ();
	
	stream = g_mime_stream_mem_new ();
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	switch (encoding) {
	case GMIME_PART_ENCODING_BASE64:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_BASE64_DEC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		g_object_unref (filter);
		break;
	case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_QP_DEC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		g_object_unref (filter);
		break;
	case GMIME_PART_ENCODING_UUENCODE:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_UU_DEC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		g_object_unref (filter);
		break;
	default:
		break;
	}
	
	g_mime_stream_write (filtered_stream, (char *) content, len);
	g_mime_stream_flush (filtered_stream);
	g_object_unref (filtered_stream);
	
	g_mime_stream_reset (stream);
	g_mime_data_wrapper_set_stream (mime_part->content, stream);
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_PART_ENCODING_DEFAULT);
	g_object_unref (stream);
	
	mime_part->encoding = encoding;
}


/**
 * g_mime_part_set_content_object:
 * @mime_part: MIME Part
 * @content: content object
 *
 * Sets the content object on the mime part.
 **/
void
g_mime_part_set_content_object (GMimePart *mime_part, GMimeDataWrapper *content)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (content)
		g_object_ref (content);
	
	if (mime_part->content)
		g_object_unref (mime_part->content);
	
	mime_part->content = content;
}


/**
 * g_mime_part_get_content_object:
 * @mime_part: MIME part object
 *
 * Gets the internal data-wrapper of the specified mime part, or %NULL
 * on error.
 *
 * Returns the data-wrapper for the mime part's contents.
 **/
GMimeDataWrapper *
g_mime_part_get_content_object (const GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if (mime_part->content)
		g_object_ref (mime_part->content);
	
	return mime_part->content;
}


/**
 * g_mime_part_get_content: 
 * @mime_part: MIME part object
 * @len: pointer to the content length
 *
 * Gets the raw contents of the mime part and sets @len to the length
 * of the raw data buffer.
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_part_get_content_object() instead.
 * 
 * Returns a const char * pointer to the raw contents of the MIME Part
 * and sets @len to the length of the buffer. Note: textual content
 * will not be converted to UTF-8. Also note that this buffer will not
 * be nul-terminated and may in fact contain nul bytes mid-buffer so
 * you MUST treat the data returned as raw binary data even if the
 * content type is text.
 **/
const char *
g_mime_part_get_content (const GMimePart *mime_part, size_t *len)
{
	const char *retval = NULL;
	GMimeStream *stream;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if (!mime_part->content || !mime_part->content->stream) {
		d(g_warning ("no content set on this mime part"));
		return NULL;
	}
	
	stream = mime_part->content->stream;
	if (!GMIME_IS_STREAM_MEM (stream) || NEEDS_DECODING (mime_part->content->encoding)) {
		/* Decode and cache this mime part's contents... */
		GMimeStream *cache;
		GByteArray *buf;
		
		buf = g_byte_array_new ();
		cache = g_mime_stream_mem_new_with_byte_array (buf);
		
		g_mime_data_wrapper_write_to_stream (mime_part->content, cache);
		
		g_mime_data_wrapper_set_stream (mime_part->content, cache);
		g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_PART_ENCODING_DEFAULT);
		g_object_unref (cache);
		
		*len = buf->len;
		retval = buf->data;
	} else {
		GByteArray *buf = GMIME_STREAM_MEM (stream)->buffer;
		int start_index = 0;
		int end_index = buf->len;
		
		/* check boundaries */
		if (stream->bound_start >= 0)
			start_index = CLAMP (stream->bound_start, 0, buf->len);
		if (stream->bound_end >= 0)
			end_index = CLAMP (stream->bound_end, 0, buf->len);
		if (end_index < start_index)
			end_index = start_index;
		
		*len = end_index - start_index;
		retval = buf->data + start_index;
	}
	
	return retval;
}


/**
 * g_mime_part_write_to_stream:
 * @mime_part: MIME Part
 * @stream: output stream
 *
 * Writes the contents of the MIME Part to @stream.
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_object_write_to_stream() instead.
 *
 * Returns the number of bytes written or -1 on fail.
 **/
ssize_t
g_mime_part_write_to_stream (GMimePart *mime_part, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return g_mime_object_write_to_stream ((GMimeObject *) mime_part, stream);
}


/**
 * g_mime_part_to_string:
 * @mime_part: MIME Part
 *
 * Allocates a string buffer containing the MIME Part.
 *
 * WARNING: This interface is deprecated. Use
 * g_mime_object_to_string() instead.
 *
 * Returns an allocated string containing the MIME Part.
 **/
char *
g_mime_part_to_string (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return g_mime_object_to_string ((GMimeObject *) mime_part);
}
