/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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
#include "gmime-stream-filter.h"
#include "gmime-filter-basic.h"
#include "md5-utils.h"


static GMimeObject object_template = {
	0, 0, g_mime_part_destroy
};


#define NEEDS_DECODING(encoding) (((GMimePartEncodingType) encoding) == GMIME_PART_ENCODING_BASE64 ||  \
				  ((GMimePartEncodingType) encoding) == GMIME_PART_ENCODING_QUOTEDPRINTABLE)



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
	
	mime_part = g_new0 (GMimePart, 1);
	g_mime_object_construct (GMIME_OBJECT (mime_part),
				 &object_template,
				 GMIME_PART_TYPE);
	
	mime_part->headers = g_mime_header_new ();
	
	/* lets set the default content type just in case the
	 * programmer forgets to set it ;-) */
	mime_part->mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_header_set (mime_part->headers, "Content-Type", "text/plain");
	
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
	char *content_type;
	
	mime_part = g_new0 (GMimePart, 1);
	g_mime_object_construct (GMIME_OBJECT (mime_part),
				 &object_template,
				 GMIME_PART_TYPE);
	
	mime_part->headers = g_mime_header_new ();
	
	mime_part->mime_type = g_mime_content_type_new (type, subtype);
	content_type = g_strdup_printf ("%s/%s", type, subtype);
	g_mime_header_set (mime_part->headers, "Content-Type", content_type);
	g_free (content_type);
	
	return mime_part;
}


/**
 * g_mime_part_destroy:
 * @mime_part: Mime part to destroy
 *
 * Releases all memory used by this mime part and all child mime parts.
 **/
void
g_mime_part_destroy (GMimePart *mime_part)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	g_mime_header_destroy (mime_part->headers);
	
	g_free (mime_part->description);
	g_free (mime_part->content_id);
	g_free (mime_part->content_md5);
	g_free (mime_part->content_location);
	
	if (mime_part->mime_type)
		g_mime_content_type_destroy (mime_part->mime_type);
	
	if (mime_part->disposition)
		g_mime_disposition_destroy (mime_part->disposition);
	
	if (mime_part->children) {
		GList *child;
		
		child = mime_part->children;
		while (child) {
			g_mime_object_unref (GMIME_OBJECT (child->data));
			child = child->next;
		}
		
		g_list_free (mime_part->children);
	}
	
	if (mime_part->content)
		g_mime_data_wrapper_destroy (mime_part->content);
	
	g_free (mime_part);
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
	
	g_mime_header_set (mime_part->headers, header, value);
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
	
	return g_mime_header_get (mime_part->headers, header);
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
	
	if (mime_part->description)
		g_free (mime_part->description);
	
	mime_part->description = g_strdup (description);
	g_mime_header_set (mime_part->headers, "Content-Description", description);
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
	
	return mime_part->description;
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
	
	if (mime_part->content_id)
		g_free (mime_part->content_id);
	
	mime_part->content_id = g_strdup (content_id);
	g_mime_header_set (mime_part->headers, "Content-Id", content_id);
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
	
	return mime_part->content_id;
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
	const GMimeContentType *type;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	/* RFC 1864 states that you cannot set a Content-MD5 for these types */
	type = g_mime_part_get_content_type (mime_part);
	if (g_mime_content_type_is_type (type, "multipart", "*") ||
	    g_mime_content_type_is_type (type, "message", "rfc822"))
		return;
	
	if (mime_part->content_md5)
		g_free (mime_part->content_md5);
	
	if (content_md5) {
		mime_part->content_md5 = g_strdup (content_md5);
	} else if (mime_part->content && mime_part->content->stream) {
		char digest[16], b64digest[32];
		int len, state, save;
		GMimeStream *stream;
		GByteArray *buf;
		
		stream = mime_part->content->stream;
		if (!GMIME_IS_STREAM_MEM (stream) || NEEDS_DECODING (mime_part->content->encoding)) {
			stream = g_mime_stream_mem_new ();
			g_mime_data_wrapper_write_to_stream (mime_part->content, stream);
		} else {
			stream = mime_part->content->stream;
			g_mime_stream_ref (stream);
		}
		
		buf = GMIME_STREAM_MEM (stream)->buffer;
		
		md5_get_digest (buf->data + stream->bound_start,
				g_mime_stream_length (stream),
				digest);
		
		g_mime_stream_unref (stream);
		
		state = save = 0;
		len = g_mime_utils_base64_encode_close (digest, 16, b64digest, &state, &save);
		b64digest[len] = '\0';
		
		mime_part->content_md5 = g_strdup (b64digest);
		
		g_mime_header_set (mime_part->headers, "Content-Md5", b64digest);
	}
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
	char digest[16], b64digest[32];
	int len, state, save;
	GMimeStream *stream;
	GByteArray *buf;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), FALSE);
	g_return_val_if_fail (mime_part->content != NULL, FALSE);
	g_return_val_if_fail (mime_part->content_md5 != NULL, FALSE);
	
	stream = mime_part->content->stream;
	if (!stream)
		return FALSE;
	
	if (!GMIME_IS_STREAM_MEM (stream) || NEEDS_DECODING (mime_part->content->encoding)) {
		stream = g_mime_stream_mem_new ();
		g_mime_data_wrapper_write_to_stream (mime_part->content, stream);
	} else {
		stream = mime_part->content->stream;
		g_mime_stream_ref (stream);
	}
	
	buf = GMIME_STREAM_MEM (stream)->buffer;
	
	md5_get_digest (buf->data + stream->bound_start,
			g_mime_stream_length (stream),
			digest);
	
	g_mime_stream_unref (GMIME_STREAM (stream));
	
	state = save = 0;
	len = g_mime_utils_base64_encode_close (digest, 16, b64digest, &state, &save);
	b64digest[len] = '\0';
	
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
	g_mime_header_set (mime_part->headers, "Content-Location", content_location);
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


static void
unfold (char *str)
{
	register char *s, *d;
	
	for (d = s = str; *s; s++) {
		if (*s != '\n') {
			if (*s == '\t')
				*d++ = ' ';
			else
				*d++ = *s;
		}
	}
	
	*d = '\0';
}

static void
sync_content_type (GMimePart *mime_part)
{
	GMimeContentType *mime_type;
	GMimeParam *params;
	GString *string;
	char *type, *p;
	
	mime_type = mime_part->mime_type;
	
	string = g_string_new ("Content-Type: ");
	
	type = g_mime_content_type_to_string (mime_type);
	g_string_append (string, type);
	g_free (type);
	
	params = mime_type->params;
	if (params)
		g_mime_param_write_to_string (params, FALSE, string);
	
	p = string->str;
	g_string_free (string, FALSE);
	
	type = p + strlen ("Content-Type: ");
	g_mime_header_set (mime_part->headers, "Content-Type", type);
	g_free (p);
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
	
	if (mime_part->mime_type)
		g_mime_content_type_destroy (mime_part->mime_type);
	
	mime_part->mime_type = mime_type;
	
	sync_content_type (mime_part);
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
	
	return mime_part->mime_type;
}


/**
 * g_mime_part_set_encoding: Set the content encoding
 * @mime_part: Mime part
 * @encoding: Mime encoding
 *
 * Set the content encoding for the specified mime part. Available
 * values for the encoding are: GMIME_PART_ENCODING_DEFAULT,
 * GMIME_PART_ENCODING_7BIT, GMIME_PART_ENCODING_8BIT,
 * GMIME_PART_ENCODING_BASE64 and GMIME_PART_ENCODING_QUOTEDPRINTABLE.
 **/
void
g_mime_part_set_encoding (GMimePart *mime_part, GMimePartEncodingType encoding)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	mime_part->encoding = encoding;
	g_mime_header_set (mime_part->headers, "Content-Transfer-Encoding",
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
 * GMIME_PART_ENCODING_8BIT, GMIME_PART_ENCODING_BASE64 or
 * GMIME_PART_ENCODING_QUOTEDPRINTABLE.
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
 * GMIME_PART_ENCODING_BASE64 and GMIME_PART_ENCODING_QUOTEDPRINTABLE.
 **/
const char *
g_mime_part_encoding_to_string (GMimePartEncodingType encoding)
{
	switch (encoding) {
        case GMIME_PART_ENCODING_7BIT:
		return "7bit";
        case GMIME_PART_ENCODING_8BIT:
		return "8bit";
        case GMIME_PART_ENCODING_BASE64:
		return "base64";
        case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		return "quoted-printable";
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
 * Returns the encoding string as a GMimePartEncodingType.
 * Available values for the encoding are:
 * GMIME_PART_ENCODING_DEFAULT, GMIME_PART_ENCODING_7BIT,
 * GMIME_PART_ENCODING_8BIT, GMIME_PART_ENCODING_BASE64 and
 * GMIME_PART_ENCODING_QUOTEDPRINTABLE.
 **/
GMimePartEncodingType
g_mime_part_encoding_from_string (const char *encoding)
{
	if (!g_strcasecmp (encoding, "7bit"))
		return GMIME_PART_ENCODING_7BIT;
	else if (!g_strcasecmp (encoding, "8bit"))
		return GMIME_PART_ENCODING_8BIT;
	else if (!g_strcasecmp (encoding, "base64"))
		return GMIME_PART_ENCODING_BASE64;
	else if (!g_strcasecmp (encoding, "quoted-printable"))
		return GMIME_PART_ENCODING_QUOTEDPRINTABLE;
	else return GMIME_PART_ENCODING_DEFAULT;
}


static void
sync_content_disposition (GMimePart *mime_part)
{
	char *str, *buf;
	
	str = g_mime_disposition_header (mime_part->disposition, FALSE);
	g_mime_header_set (mime_part->headers, "Content-Disposition", str);
	g_free (str);
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
	
	if (!mime_part->disposition)
		mime_part->disposition = g_mime_disposition_new (NULL);
	
	g_mime_disposition_set (mime_part->disposition, disposition);
	
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
	GMimeParam *param = NULL;
	
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
	GMimeParam *param;
	
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
	GMimeParam *param = NULL;
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	
	if (!mime_part->disposition)
		mime_part->disposition = g_mime_disposition_new (GMIME_DISPOSITION_ATTACHMENT);
	
	g_mime_disposition_add_parameter (mime_part->disposition, "filename", filename);
	
	g_mime_content_type_add_parameter (mime_part->mime_type, "name", filename);
	
	sync_content_type (mime_part);
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
		return g_mime_content_type_get_parameter (mime_part->mime_type, "name");
	}
	
	return filename;
}


static void
read_random_pool (char *buffer, size_t bytes)
{
	int fd;
	
	fd = open ("/dev/urandom", O_RDONLY);
	if (fd == -1) {
		fd = open ("/dev/random", O_RDONLY);
		if (fd == -1)
			return;
	}
	
	read (fd, buffer, bytes);
	close (fd);
}


/**
 * g_mime_part_set_boundary:
 * @mime_part: Mime part
 * @boundary: the boundary for the multi-part or NULL to generate a random one.
 *
 * Sets the boundary on the multipart mime part.
 **/
void
g_mime_part_set_boundary (GMimePart *mime_part, const char *boundary)
{
	char bbuf[27];
	
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (g_mime_content_type_is_type (mime_part->mime_type, "multipart", "*"));
	
	if (!boundary) {
		/* Generate a fairly random boundary string. */
		char digest[16], *p;
		int state, save;
		
		read_random_pool (digest, 16);
		
		strcpy (bbuf, "=-");
		p = bbuf + 2;
		state = save = 0;
		p += g_mime_utils_base64_encode_step (digest, 16, p, &state, &save);
		*p = '\0';
		
		boundary = bbuf;
	}
	
	g_mime_content_type_add_parameter (mime_part->mime_type, "boundary", boundary);
	sync_content_type (mime_part);
}


/**
 * g_mime_part_get_boundary:
 * @mime_part: Mime part
 *
 * Gets the multipart boundary or %NULL on error.
 *
 * Returns the boundary on the mime part.
 **/
const char *
g_mime_part_get_boundary (GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	return g_mime_content_type_get_parameter (mime_part->mime_type, "boundary");
}


/**
 * g_mime_part_set_content:
 * @mime_part: Mime part
 * @content: raw mime part content
 * @len: raw content length
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
	g_mime_stream_unref (stream);
}


/**
 * g_mime_part_set_content_byte_array:
 * @mime_part: Mime part
 * @content: raw mime part content.
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
	g_mime_stream_unref (stream);
}


/**
 * g_mime_part_set_pre_encoded_content:
 * @mime_part: Mime part
 * @content: encoded mime part content
 * @len: length of the content
 * @encoding: content encoding
 *
 * Sets the encoding type and raw content on the mime part after decoding the content.
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
		break;
	case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_QP_DEC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		break;
	default:
		break;
	}
	
	g_mime_stream_write (filtered_stream, (char *) content, len);
	g_mime_stream_flush (filtered_stream);
	g_mime_stream_unref (filtered_stream);
	
	g_mime_stream_reset (stream);
	g_mime_data_wrapper_set_stream (mime_part->content, stream);
	g_mime_data_wrapper_set_encoding (mime_part->content, GMIME_PART_ENCODING_DEFAULT);
	g_mime_stream_unref (stream);
	
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
	
	if (mime_part->content)
		g_mime_data_wrapper_destroy (mime_part->content);
	
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
const GMimeDataWrapper *
g_mime_part_get_content_object (const GMimePart *mime_part)
{
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
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
 * Returns a const char * pointer to the raw contents of the MIME Part
 * and sets %len to the length of the buffer.
 **/
const char *
g_mime_part_get_content (const GMimePart *mime_part, size_t *len)
{
	const char *retval = NULL;
	GMimeStream *stream;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	if (!mime_part->content || !mime_part->content->stream) {
		g_warning ("no content set on this mime part");
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
		g_mime_stream_unref (cache);
		
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
 * g_mime_part_add_subpart:
 * @mime_part: Parent Mime part
 * @subpart: Child Mime part
 *
 * Adds a subpart to the parent mime part which *must* be a
 * multipart.
 **/
void
g_mime_part_add_subpart (GMimePart *mime_part, GMimePart *subpart)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (GMIME_IS_PART (subpart));
	
	if (g_mime_content_type_is_type (mime_part->mime_type, "multipart", "*")) {
		mime_part->children = g_list_append (mime_part->children, subpart);
		g_mime_object_ref (GMIME_OBJECT (subpart));
	}
}


static void
write_content (GMimePart *part, GMimeStream *stream)
{
	GMimeStream *filtered_stream;
	GMimeFilter *filter;
	ssize_t written;
	
	if (!part->content || !part->content->stream)
		return;
	
	filtered_stream = g_mime_stream_filter_new_with_stream (stream);
	switch (part->encoding) {
	case GMIME_PART_ENCODING_BASE64:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_BASE64_ENC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		break;
	case GMIME_PART_ENCODING_QUOTEDPRINTABLE:
		filter = g_mime_filter_basic_new_type (GMIME_FILTER_BASIC_QP_ENC);
		g_mime_stream_filter_add (GMIME_STREAM_FILTER (filtered_stream), filter);
		break;
	default:
		break;
	}
	
	written = g_mime_data_wrapper_write_to_stream (part->content, filtered_stream);
	g_mime_stream_flush (filtered_stream);
	g_mime_stream_unref (filtered_stream);
	
	/* this is just so that I get a warning on fail... */
	g_return_if_fail (written != -1);
}


/**
 * g_mime_part_write_to_stream:
 * @mime_part: MIME Part
 * @stream: output stream
 *
 * Writes the contents of the MIME Part to @stream.
 **/
void
g_mime_part_write_to_stream (GMimePart *mime_part, GMimeStream *stream)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (stream != NULL);
	
	if (g_mime_content_type_is_type (mime_part->mime_type, "multipart", "*")) {
		const char *boundary;
		GList *child;
		
		/* make sure there's a boundary, else force a random boundary */
		boundary = g_mime_part_get_boundary (mime_part);
		if (!boundary) {
			g_mime_part_set_boundary (mime_part, NULL);
			boundary = g_mime_part_get_boundary (mime_part);
		}
		
		/* write the mime headers */
		g_mime_header_write_to_stream (mime_part->headers, stream);
		g_mime_stream_write (stream, "\n", 1);
		
		child = mime_part->children;
		while (child) {
			g_mime_stream_printf (stream, "--%s\n", boundary);
			g_mime_part_write_to_stream (child->data, stream);
			g_mime_stream_write (stream, "\n", 1);
			
			child = child->next;
		}
		
		g_mime_stream_printf (stream, "\n--%s--\n", boundary);
	} else {
		/* write the mime headers */
		g_mime_header_write_to_stream (mime_part->headers, stream);
		
		g_mime_stream_write (stream, "\n", 1);
		write_content (mime_part, stream);
		g_mime_stream_write (stream, "\n", 1);
	}
}


/**
 * g_mime_part_to_string:
 * @mime_part: MIME Part
 *
 * Allocates a string buffer containing the MIME Part.
 *
 * Returns an allocated string containing the MIME Part.
 **/
char *
g_mime_part_to_string (GMimePart *mime_part)
{
	GMimeStream *stream;
	GByteArray *buf;
	char *str;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	
	buf = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), buf);
	g_mime_part_write_to_stream (mime_part, stream);
	g_mime_stream_unref (stream);
	g_byte_array_append (buf, "", 1);
	str = buf->data;
	g_byte_array_free (buf, FALSE);
	
	return str;
}


/**
 * g_mime_part_foreach: 
 * @mime_part: the MIME part
 * @callback: function to call for #mime_part and all it's subparts
 * @data: extra data to pass to the callback
 * 
 * Calls @callback on @mime_part and each of it's subparts.
 **/
void
g_mime_part_foreach (GMimePart *mime_part, GMimePartFunc callback, gpointer data)
{
	g_return_if_fail (GMIME_IS_PART (mime_part));
	g_return_if_fail (callback != NULL);
	
	callback (mime_part, data);
	
	if (mime_part->children) {
		GList *child;
		
		child = mime_part->children;
		while (child) {
			g_mime_part_foreach ((GMimePart *) child->data, callback, data);
			child = child->next;
		}
	}
}


/**
 * g_mime_part_get_subpart_from_content_id: 
 * @mime_part: the MIME part
 * @content_id: the content id of the part to look for
 *
 * Gets the mime part with the content-id @content_id from the
 * multipart @mime_part.
 *
 * Returns the GMimePart whose content-id matches the search string,
 * or %NULL if a match cannot be found.
 **/
const GMimePart *
g_mime_part_get_subpart_from_content_id (GMimePart *mime_part, const char *content_id)
{
	GList *child;
	
	g_return_val_if_fail (GMIME_IS_PART (mime_part), NULL);
	g_return_val_if_fail (content_id != NULL, NULL);
	
	if (mime_part->content_id && !strcmp (mime_part->content_id, content_id))
		return mime_part;
	
	child = mime_part->children;
	while (child) {
		const GMimeContentType *type;
		const GMimePart *part = NULL;
		GMimePart *subpart;
		
		subpart = (GMimePart *) child->data;
		type = g_mime_part_get_content_type (subpart);
		
		if (g_mime_content_type_is_type (type, "multipart", "*"))
			part = g_mime_part_get_subpart_from_content_id (subpart, content_id);
		else if (subpart->content_id && !strcmp (subpart->content_id, content_id))
			part = subpart;
		
		if (part)
			return part;
		
		child = child->next;
	}
	
	return NULL;
}
