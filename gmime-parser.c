/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *           Charles Kerr <charles@rebelbase.com>
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
#include "strlib.h"
#include <ctype.h>


#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

#define d(x) x

enum {
	CONTENT_TYPE = 0,
	CONTENT_TRANSFER_ENCODING,
	CONTENT_DISPOSITION,
	CONTENT_DESCRIPTION,
	CONTENT_LOCATION,
	CONTENT_MD5,
	CONTENT_ID,
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

static int
content_header (const char *field)
{
	int i;
	
	for (i = 0; content_headers[i]; i++)
		if (!strncasecmp (field, content_headers[i], strlen (content_headers[i])))
			return i;
	
	return -1;
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
parse_content_headers (const char *headers, int inlen,
                       GMimePart *mime_part, gboolean *is_multipart,
                       char **boundary, char **end_boundary)
{
	const char *inptr = headers;
	const char *inend = inptr + inlen;
	
	*boundary = NULL;
	*end_boundary = NULL;
	
	while (inptr < inend) {
		const int type = content_header (inptr);
		const char *hvalptr;
		const char *hvalend;
		char *header = NULL;
		char *value;
		
		if (type == -1) {
			if (!(hvalptr = memchr (inptr, ':', inend - inptr)))
				break;
			header = g_strndup (inptr, hvalptr - inptr);
			g_strstrip (header);
			hvalptr++;
		} else {
			hvalptr = inptr + strlen (content_headers[type]);
		}
		
		for (hvalend = hvalptr; hvalend < inend; hvalend++)
			if (*hvalend == '\n' && !isblank (*(hvalend + 1)))
				break;
		
		value = g_strndup (hvalptr, (int) (hvalend - hvalptr));
		
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
			GMimeDisposition *disposition;
			
			disposition = g_mime_disposition_new (value);
			g_mime_part_set_content_disposition_object (mime_part, disposition);
			
			break;
		}
		default:
			/* possibly save the raw header */
			if (!strncasecmp (header, "Content-", 8))
				g_mime_part_set_content_header (mime_part, header, value);
			g_free (header);
			break;
		}
		
		g_free (value);
		inptr = hvalend + 1;
	}
}

static GMimePart *
g_mime_parser_construct_part_internal (GMimeStream *stream, GMimeStreamMem *mem)
{
	GMimePart *mime_part;
	char *boundary;
	char *end_boundary;
	gboolean is_multipart;
	const char *inptr;
	const char *inend;
	const char *hdr_end;
	const char *in;
	int inlen;
	
	in = mem->buffer->data + g_mime_stream_tell (GMIME_STREAM (mem));
	inlen = g_mime_stream_length (GMIME_STREAM (mem));
	inend = in + inlen;
	
	/* Headers */
	/* if the beginning of the input is a '\n' then there are no content headers */
	hdr_end = *in == '\n' ? in : strnstr (in, "\n\n", inlen);
	if (!hdr_end)
		return NULL;
	
	mime_part = g_mime_part_new ();
	is_multipart = FALSE;
	parse_content_headers (in, hdr_end - in, mime_part,
			       &is_multipart, &boundary, &end_boundary);
	
	/* Body */
	inptr = hdr_end;
	
	if (is_multipart && boundary && end_boundary) {
		/* get all the subparts */
		GMimePart *subpart;
		const char *part_begin;
		const char *part_end;
		off_t start, end, pos;
		
		pos = g_mime_stream_tell (GMIME_STREAM (mem));
		start = GMIME_STREAM (mem)->bound_start;
		end = GMIME_STREAM (mem)->bound_end;
		
		part_begin = strnstr (inptr, boundary, inend - inptr);
		while (part_begin && part_begin < inend) {
			/* make sure we're not looking at the end boundary */
			if (!strncmp (part_begin, end_boundary, strlen (end_boundary)))
				break;
			
			/* find the end of this part */
			part_begin += strlen (boundary);
			
			part_end = strnstr (part_begin, boundary, inend - part_begin);
			if (!part_end) {
				part_end = strnstr (part_begin, end_boundary, inend - part_begin);
				if (!part_end)
					part_end = inend;
			}
			
			/* get the subpart */
			g_mime_stream_set_bounds (GMIME_STREAM (mem), pos + (part_begin - in),
						  pos + (part_end - in));
			subpart = g_mime_parser_construct_part_internal (stream, mem);
			g_mime_part_add_subpart (mime_part, subpart);
			g_mime_object_unref (GMIME_OBJECT (subpart));
			
			/* the next part begins where the last one left off */
			part_begin = part_end;
		}
		
		g_mime_stream_set_bounds (GMIME_STREAM (mem), start, end);
		g_mime_stream_seek (GMIME_STREAM (mem), pos, GMIME_STREAM_SEEK_SET);
		
		/* free our temp boundary strings */
		g_free (boundary);
		g_free (end_boundary);
	} else {
		GMimePartEncodingType encoding;
		const char *content = NULL;
		size_t len = 0;
		
		/* from here to the end is the content */
		if (inptr < inend) {
			content = inptr + 1;
			len = inend - content;
			
			/* trim off excess trailing \n's */
			while (len > 2 && *(inend - 1) == '\n' && *(inend - 2) == '\n') {
				inend--;
				len--;
			}
		}
		
		encoding = g_mime_part_get_encoding (mime_part);
		
		if (len > 0) {
			if (GMIME_IS_STREAM_MEM (stream)) {
				/* if we've already got it in memory, we use less memory if we
				 * use individual mem streams per part after parsing... */
				g_mime_part_set_pre_encoded_content (mime_part, content, len, encoding);
			} else {
				GMimeDataWrapper *wrapper;
				GMimeStream *substream;
				off_t offset, start, end;
				
				/* mime part offset into memory stream */
				offset = g_mime_stream_tell (GMIME_STREAM (mem));
				
				/* add message offset into original stream (if different streams) */
				if (stream != GMIME_STREAM (mem))
					offset += g_mime_stream_tell (stream);
				
				start = offset + (content - in);
				end = start + len;
				
				substream = g_mime_stream_substream (stream, start, end);
				wrapper = g_mime_data_wrapper_new_with_stream (substream, encoding);
				g_mime_part_set_content_object (mime_part, wrapper);
				g_mime_stream_unref (substream);
			}
		}
	}
	
	return mime_part;
}


/**
 * g_mime_parser_construct_part:
 * @stream: raw MIME Part stream
 *
 * Constructs a GMimePart object based on @stream.
 *
 * Returns a GMimePart object based on the data.
 **/
GMimePart *
g_mime_parser_construct_part (GMimeStream *stream)
{
	GMimeStreamMem *mem;
	GMimePart *part;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	if (GMIME_IS_STREAM_MEM (stream)) {
		mem = GMIME_STREAM_MEM (stream);
		g_mime_stream_ref (stream);
	} else {
		off_t offset;
		
		mem = (GMimeStreamMem *) g_mime_stream_mem_new ();
		offset = g_mime_stream_tell (stream);
		g_mime_stream_write_to_stream (stream, GMIME_STREAM (mem));
		g_mime_stream_seek (stream, offset, GMIME_STREAM_SEEK_SET);
		g_mime_stream_reset (GMIME_STREAM (mem));
	}
	
	part = g_mime_parser_construct_part_internal (stream, mem);
	g_mime_stream_unref (GMIME_STREAM (mem));
	
	return part;
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
special_header (const char *header)
{
	return (!strcasecmp (header, "MIME-Version:") || content_header (header) != -1);
}

static void
construct_message_headers (GMimeMessage *message, const char *headers, int inlen, gboolean preserve_headers)
{
	char *field, *value, *raw, *q;
	char *inptr, *inend;
	time_t date;
	int offset = 0;
	int i;
	
	inptr = (char *) headers;
	inend = inptr + inlen;
	
	for ( ; inptr < inend; inptr++) {
		for (i = 0; fields[i]; i++)
			if (!strncasecmp (fields[i], inptr, strlen (fields[i])))
				break;
		
		if (!fields[i]) {
			field = inptr;
			for (q = field; q < inend && *q != ':'; q++);
			field = g_strndup (field, (int) (q - field + 1));
			g_strstrip (field);
		} else {
			field = g_strdup (fields[i]);
		}
		
		value = inptr + strlen (field);
		for (q = value; q < inend; q++)
			if (*q == '\n' && !isblank (*(q + 1)))
				break;
		
		value = g_strndup (value, (int) (q - value));
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
				field[strlen (field) - 1] = '\0'; /* kill the ':' */
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
 * g_mime_parser_construct_message:
 * @stream: an rfc0822 message stream
 * @preserve_headers: if TRUE, then store the arbitrary headers
 *
 * Constructs a GMimeMessage object based on @stream.
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeStream *stream, gboolean preserve_headers)
{
	GMimeMessage *message = NULL;
	GMimeStreamMem *mem;
	const char *hdr_end;
	const char *in;
	off_t offset;
	int inlen;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	if (GMIME_IS_STREAM_MEM (stream)) {
		mem = GMIME_STREAM_MEM (stream);
		g_mime_stream_ref (stream);
	} else {
		mem = (GMimeStreamMem *) g_mime_stream_mem_new ();
		offset = g_mime_stream_tell (stream);
		g_mime_stream_write_to_stream (stream, GMIME_STREAM (mem));
		g_mime_stream_seek (stream, offset, GMIME_STREAM_SEEK_SET);
		g_mime_stream_reset (GMIME_STREAM (mem));
	}
	
	offset = g_mime_stream_tell (GMIME_STREAM (mem));
	in = mem->buffer->data + offset;
	inlen = g_mime_stream_seek (GMIME_STREAM (mem), 0, GMIME_STREAM_SEEK_END) - offset;
	g_mime_stream_seek (GMIME_STREAM (mem), offset, GMIME_STREAM_SEEK_SET);
	
	hdr_end = strnstr (in, "\n\n", inlen);
	if (hdr_end != NULL) {
		GMimePart *part;
		
		message = g_mime_message_new (!preserve_headers);
		construct_message_headers (message, in, hdr_end - in, preserve_headers);
		part = g_mime_parser_construct_part_internal (stream, mem);
		g_mime_message_set_mime_part (message, part);
		g_mime_object_unref (GMIME_OBJECT (part));
	}
	
	g_mime_stream_unref (GMIME_STREAM (mem));
	
	return message;
}
