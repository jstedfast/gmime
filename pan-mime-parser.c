/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000, 2001 Helix Code, Inc. (www.helixcode.com)
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

#include "gmime-parser.h"
#include "gmime-utils.h"
#include "gmime-header.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-buffer.h"
#include <string.h>
#include <ctype.h>

#define d(x) x

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

static void
header_unfold (char *header)
{
	/* strip all \n's and replace tabs with spaces - this should
           undo any header folding */
	char *src, *dst;
	
	for (src = dst = header; *src; src++) {
		if (*src != '\n')
			*dst++ = *src != '\t' ? *src : ' ';
	}
	*dst = '\0';
}

enum {
	CONTENT_TYPE = 0,
	CONTENT_TRANSFER_ENCODING,
	CONTENT_DISPOSITION,
	CONTENT_DESCRIPTION,
	CONTENT_LOCATION,
	CONTENT_MD5,
	CONTENT_ID
};

static char *content_headers[] = {
	"Content-Type:",
	"Content-Transfer-Encoding:",
	"Content-Disposition:",
	"Content-Description:",
	"Content-Location:",
	"Content-Md5:",
	"Content-Id:",
	NULL
};

static int
content_header (const char *field)
{
	int i;
	
	for (i = 0; content_headers[i]; i++)
		if (!g_strncasecmp (field, content_headers[i], strlen (content_headers[i])))
			return i;
	
	return -1;
}

enum {
	HEADER_FROM = 0,
	HEADER_REPLY_TO,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_SUBJECT,
	HEADER_DATE,
	HEADER_MESSAGE_ID,
	HEADER_UNKNOWN
};

static char *fields[] = {
	"From:",
	"Reply-To:",
	"To:",
	"Cc:",
	"Bcc:",
	"Subject:",
	"Date:",
	"Message-Id:",
	NULL
};

static gboolean
special_header (const char *field)
{
	return (!g_strcasecmp (field, "MIME-Version:") || content_header (field) != -1);
}

static void
parser_read_headers (GMimeStream *stream, GByteArray *buffer)
{
	guint offset;
	
	do {
		offset = buffer->len;
		g_mime_stream_buffer_readln (stream, buffer);
	} while (!g_mime_stream_eos (stream) && *(buffer->data + offset) != '\n');
	
	/* strip off the empty line */
	g_byte_array_set_size (buffer, MAX (buffer->len - 1, 0));
}

enum {
	FOUND_BOUNDARY,
	FOUND_END_BOUNDARY,
	FOUND_EOS,
};

static guint
parser_read_until_boundary (GMimeStream *stream, GByteArray *buffer,
			    const char *boundary, const char *end_boundary, int *found)
{
	guint offset, len, total = 0;
	gboolean internal;
	
	*found = FOUND_EOS;
	
	if (!buffer) {
		buffer = g_byte_array_new ();
		internal = TRUE;
	} else
		internal = FALSE;
	
	do {
		offset = buffer->len;
		g_mime_stream_buffer_readln (stream, buffer);
		len = buffer->len - offset;
		
		if (boundary && len == strlen (boundary) &&
		    !memcmp (buffer->data + offset, boundary, len)) {
			g_byte_array_set_size (buffer, offset);
			*found = FOUND_BOUNDARY;
			break;
		}
		
		if (end_boundary && len == strlen (end_boundary) &&
		    !memcmp (buffer->data + offset, end_boundary, len)) {
			g_byte_array_set_size (buffer, offset);
			*found = FOUND_END_BOUNDARY;
			break;
		}
		
		total += len;
		
		if (internal)
			g_byte_array_set_size (buffer, 0);
		
	} while (!g_mime_stream_eos (stream));
	
	if (internal)
		g_byte_array_free (buffer, TRUE);
	
	return total;
}


/**
 * parse_content_heaaders:
 * @headers: content header string
 * @inlen: length of the header block.
 * @mime_part: mime part to populate with the information we get from the Content-* headers.
 * @is_multipart: set to TRUE if the part is a multipart, FALSE otherwise (to be set by function)
 * @boundary: multipart boundary string (to be set by function)
 * @end_boundary: multipart end boundary string (to be set by function)
 *
 * Parse a header block for content information.
 */
static void
construct_content_headers (GMimePart *mime_part, GByteArray *headers, gboolean *is_multipart,
			   char **boundary, char **end_boundary)
{
	const char *inptr = headers->data;
	const char *inend = inptr + headers->len;
	
	*boundary = NULL;
	*end_boundary = NULL;
	
	while (inptr < inend) {
		const gint type = content_header (inptr);
		const char *hvalptr;
		const char *hvalend;
		char *value;
		
		if (type == -1) {
			if (!(hvalptr = memchr (inptr, ':', inend - inptr)))
				break;
			hvalptr++;
		} else {
			hvalptr = inptr + strlen (content_headers[type]);
		}
		
		for (hvalend = hvalptr; hvalend < inend; hvalend++)
			if (*hvalend == '\n' && !isblank (*(hvalend + 1)))
				break;
		
		value = g_strndup (hvalptr, (gint) (hvalend - hvalptr));
		
		header_unfold (value);
		g_strstrip (value);
		
		switch (type) {
		case CONTENT_DESCRIPTION: {
			char *description = g_mime_utils_8bit_header_decode (value);
			
			g_strstrip (description);
			g_mime_part_set_content_description (mime_part, description);
			g_free (description);
			
			break;
		}
		case CONTENT_LOCATION:
			g_mime_part_set_content_location (mime_part, value);
			break;
		case CONTENT_MD5:
			g_mime_part_set_content_md5 (mime_part, value);
			break;
		case CONTENT_ID:
			g_mime_part_set_content_id (mime_part, value);
			break;
		case CONTENT_TRANSFER_ENCODING:
			g_mime_part_set_encoding (mime_part, g_mime_part_encoding_from_string (value));
			break;
		case CONTENT_TYPE: {
			GMimeContentType *mime_type;
			
			mime_type = g_mime_content_type_new_from_string (value);
			
			*is_multipart = g_mime_content_type_is_type (mime_type, "multipart", "*");
			if (*is_multipart) {
				const char *b;
				
				b = g_mime_content_type_get_parameter (mime_type, "boundary");
				if (b != NULL) {
					/* create our temp boundary vars */
					*boundary = g_strdup_printf ("--%s\n", b);
					*end_boundary = g_strdup_printf ("--%s--\n", b);
				} else {
					g_warning ("Invalid MIME structure: boundary not found for multipart"
						   " - defaulting to text/plain.");
					
					/* let's continue onward as if this was not a multipart */
					g_mime_content_type_destroy (mime_type);
					mime_type = g_mime_content_type_new ("text", "plain");
					is_multipart = FALSE;
				}
			}
			g_mime_part_set_content_type (mime_part, mime_type);
			
			break;
		}
		case CONTENT_DISPOSITION: {
			char *disposition, *ptr;
			
			/* get content disposition part */
			for (ptr = value; *ptr && *ptr != ';'; ptr++); /* find ; or \0 */
			disposition = g_strndup (value, (gint)(ptr - value));
			g_strstrip (disposition);
			g_mime_part_set_content_disposition (mime_part, disposition);
			g_free (disposition);
			
			/* parse the parameters, if any */
			while (*ptr == ';') {
				char *pname, *pval;
				
				/* get the param name */
				for (pname = ptr + 1; *pname && !isspace ((int)*pname); pname++);
				for (ptr = pname; *ptr && *ptr != '='; ptr++);
				pname = g_strndup (pname, (gint) (ptr - pname));
				g_strstrip (pname);
				
				/* convert param name to lowercase */
				g_strdown (pname);
				
				/* skip any whitespace */
				for (pval = ptr + 1; *pval && isspace ((int) *pval); pval++);
				
				if (*pval == '"') {
					/* value is in quotes */
					pval++;
					for (ptr = pval; *ptr; ptr++)
						if (*ptr == '"' && *(ptr - 1) != '\\')
							break;
					pval = g_strndup (pval, (gint) (ptr - pval));
					g_strstrip (pval);
					g_mime_utils_unquote_string (pval);
					
					for ( ; *ptr && *ptr != ';'; ptr++);
				} else {
					/* value is not in quotes */
					for (ptr = pval; *ptr && *ptr != ';'; ptr++);
					pval = g_strndup (pval, (gint) (ptr - pval));
					g_strstrip (pval);
				}
				
				g_mime_part_add_content_disposition_parameter (mime_part, pname, pval);
				
				g_free (pname);
				g_free (pval);
			}
			
			break;
		}
		default:
			/* ignore this header */
			break;
		}
		
		g_free (value);
		inptr = hvalend + 1;
	}
}

static GMimePart *
g_mime_parser_construct_part_internal (GMimeStream *stream, GByteArray *headers,
				       const char *parent_boundary,
				       const char *parent_end_boundary,
				       int *parent_boundary_found)
{
	GMimePart *mime_part;
	char *boundary;
	char *end_boundary;
	gboolean is_multipart;
	
	mime_part = g_mime_part_new ();
	is_multipart = FALSE;
	construct_content_headers (mime_part, headers, &is_multipart,
				   &boundary, &end_boundary);
	
	/* Content */
	if (is_multipart && boundary && end_boundary) {
		/* get all the subparts */
		GMimePart *subpart;
		GByteArray *preface;
		off_t start, end, pos;
		int found;
		
		pos = g_mime_stream_tell (stream);
		start = stream->bound_start;
		end = stream->bound_end;
		
		preface = g_byte_array_new ();
		parser_read_until_boundary (stream, preface, boundary, end_boundary, &found);
		/* FIXME: save the preface? */
		g_byte_array_free (preface, TRUE);
		
		while (found == FOUND_BOUNDARY) {
			GByteArray *content_headers;
			
			content_headers = g_byte_array_new ();
			parser_read_headers (stream, content_headers);
			
			g_mime_stream_set_bounds (stream, g_mime_stream_tell (stream), end);
			subpart = g_mime_parser_construct_part_internal (stream, content_headers,
									 boundary,
									 end_boundary,
									 &found);
			g_mime_part_add_subpart (mime_part, subpart);
			g_byte_array_free (content_headers, TRUE);
		}
		
		g_mime_stream_set_bounds (stream, start, end);
		
		if (parent_boundary) {
			parser_read_until_boundary (stream, NULL, parent_boundary,
						    parent_end_boundary, parent_boundary_found);
		}
		
		/* free our temp boundary strings */
		g_free (boundary);
		g_free (end_boundary);
	} else {
		GMimePartEncodingType encoding;
		GMimeDataWrapper *wrapper;
		GMimeStream *substream;
		off_t start, end;
		guint len;
		
		start = g_mime_stream_tell (stream);		
		
		len = parser_read_until_boundary (stream, NULL, parent_boundary,
						  parent_end_boundary,
						  parent_boundary_found);
		
		if (*parent_boundary_found != FOUND_EOS)
			end = start + len;
		else
			end = g_mime_stream_tell (stream);
		
		encoding = g_mime_part_get_encoding (mime_part);
		
		substream = g_mime_stream_substream (stream, start, end);
		wrapper = g_mime_data_wrapper_new_with_stream (substream, encoding);
		g_mime_part_set_content_object (mime_part, wrapper);
		g_mime_stream_unref (substream);
	}
	
	return mime_part;
}


/**
 * g_mime_parser_construct_part: Construct a GMimePart object
 * @stream: raw MIME Part stream
 *
 * Returns a GMimePart object based on the data.
 **/
GMimePart *
g_mime_parser_construct_part (GMimeStream *stream)
{
	GMimePart *part = NULL;
	GByteArray *headers;
	int found;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	headers = g_byte_array_new ();
	parser_read_headers (stream, headers);
	
	if (headers->len)
		part = g_mime_parser_construct_part_internal (stream, headers, NULL, NULL, &found);
	
	g_byte_array_free (headers, TRUE);
	
	return part;
}

static void
construct_message_headers (GMimeMessage *message, GByteArray *headers, gboolean preserve_headers)
{
	char *field, *value, *raw, *q;
	char *inptr, *inend;
	time_t date;
	int offset = 0;
	int i;
	
	inptr = (char *) headers->data;
	inend = inptr + headers->len;
	
	for ( ; inptr < inend; inptr++) {
		for (i = 0; fields[i]; i++)
			if (!g_strncasecmp (fields[i], inptr, strlen (fields[i])))
				break;
		
		if (!fields[i]) {
			field = inptr;
			for (q = field; q < inend && *q != ':'; q++);
			field = g_strndup (field, (gint) (q - field + 1));
			g_strstrip (field);
		} else {
			field = g_strdup (fields[i]);
		}
		
		value = inptr + strlen (field);
		for (q = value; q < inend; q++)
			if (*q == '\n' && !isblank (*(q + 1)))
				break;
		
		value = g_strndup (value, (gint) (q - value));
		g_strstrip (value);
		header_unfold (value);
		
		switch (i) {
		case HEADER_FROM:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_sender (message, raw);
			g_free (raw);
			break;
		case HEADER_REPLY_TO:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_reply_to (message, raw);
			g_free (raw);
			break;
		case HEADER_TO:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_TO, value);
			break;
		case HEADER_CC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_CC, value);
			break;
		case HEADER_BCC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_BCC, value);
			break;
		case HEADER_SUBJECT:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_subject (message, raw);
			g_free (raw);
			break;
		case HEADER_DATE:
			date = g_mime_utils_header_decode_date (value, &offset);
			g_mime_message_set_date (message, date, offset);
			break;
		case HEADER_MESSAGE_ID:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_message_id (message, raw);
			g_free (raw);
			break;
		case HEADER_UNKNOWN:
		default:
			/* possibly save the raw header */
			if ((preserve_headers || fields[i]) && !special_header (field)) {
				field[strlen (field) - 1] = '\0'; /* kill the ';' */
				g_strstrip (field);
				g_mime_header_add (message->header->headers, field, value);
			}
			break;
		}
		
		g_free (field);
		g_free (value);
		
		if (q >= inend)
			break;
		else
			inptr = q;
	}
}


/**
 * g_mime_parser_construct_message: Construct a GMimeMessage object
 * @stream: an rfc0822 message stream
 * @preserve_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeStream *stream, gboolean preserve_headers)
{
	GMimeMessage *message = NULL;
	GByteArray *headers;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	headers = g_byte_array_new ();
	parser_read_headers (stream, headers);
	
	if (headers->len) {
		GMimePart *part;
		int found;
		
		message = g_mime_message_new ();
		construct_message_headers (message, headers, preserve_headers);
		part = g_mime_parser_construct_part_internal (stream, headers, NULL, NULL, &found);
		g_mime_message_set_mime_part (message, part);
	}
	
	g_byte_array_free (headers, TRUE);
	
	return message;
}
