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
#include <string.h>
#include <ctype.h>

#define d(x) x

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
		if (!g_strncasecmp (field, content_headers[i], strlen (content_headers[i])))
			return i;
	
	return -1;
}

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

static const char *
g_strstrbound (const char *haystack, const char *needle, const char *end)
{
	gboolean matches = FALSE;
	const char *ptr;
	guint nlen;
	
	nlen = strlen (needle);
	ptr = haystack;
	
	while (ptr + nlen <= end) {
		if (!strncmp (ptr, needle, nlen)) {
			matches = TRUE;
			break;
		}
		ptr++;
	}
	
	if (matches)
		return ptr;
	else
		return NULL;
}

static const char *
find_header_part_end (const char *in, guint inlen)
{
	const char *pch;
	const char *hdr_end = NULL;
	
	g_return_val_if_fail (in != NULL, NULL);
	
	if (*in == '\n') /* if the beginning is a '\n' there are no content headers */
		hdr_end = in;
	else if ((pch = g_strstrbound (in, "\n\n", in+inlen)) != NULL)
		hdr_end = pch;
	else if ((pch = g_strstrbound (in, "\n\r\n", in+inlen)) != NULL)
		hdr_end = pch;
	
	return hdr_end;
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
	hdr_end = find_header_part_end (in, inlen);
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
		
		part_begin = g_strstrbound (inptr, boundary, inend);
		while (part_begin && part_begin < inend) {
			/* make sure we're not looking at the end boundary */
			if (!strncmp (part_begin, end_boundary, strlen (end_boundary)))
				break;
			
			/* find the end of this part */
			part_end = g_strstrbound (part_begin + strlen (boundary), boundary, inend);
			if (!part_end || part_end >= inend) {
				part_end = g_strstrbound (part_begin + strlen (boundary),
							  end_boundary, inend);
				if (!part_end || part_end >= inend)
					part_end = inend;
			}
			
			/* get the subpart */
			part_begin += strlen (boundary);
			
			g_mime_stream_set_bounds (GMIME_STREAM (mem), pos + (part_begin - in),
						  pos + (part_end - in));
			subpart = g_mime_parser_construct_part_internal (stream, mem);
			g_mime_part_add_subpart (mime_part, subpart);
			
			/* the next part begins where the last one left off */
			part_begin = part_end;
		}
		
		g_mime_stream_set_bounds (stream, start, end);
		g_mime_stream_seek (GMIME_STREAM (mem), pos, GMIME_STREAM_SEEK_SET);
		
		/* free our temp boundary strings */
		g_free (boundary);
		g_free (end_boundary);
	} else {
		GMimePartEncodingType encoding;
		const char *content = NULL;
		guint len = 0;
		
		/* from here to the end is the content */
		if (inptr < inend) {
			for (inptr++; inptr < inend && isspace ((int) *inptr); inptr++);
			len = inend - inptr;
			content = inptr;
			
			/* trim off excess trailing \n's */
			inend = content + len;
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
				
				offset = g_mime_stream_tell (stream);
				if (stream != GMIME_STREAM (mem))
					offset += g_mime_stream_tell (GMIME_STREAM (mem));
				
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
 * g_mime_parser_construct_part: Construct a GMimePart object
 * @stream: raw MIME Part stream
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
special_header (const char *field)
{
	return (!g_strcasecmp (field, "MIME-Version:") || content_header (field) != -1);
}

static void
construct_headers (GMimeMessage *message, const char *headers, gint inlen, gboolean save_extra_headers)
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
			break;
		}
		
		/* possibly save the raw header */
		if ((save_extra_headers || fields[i]) && !special_header (field)) {
			field[strlen (field) - 1] = '\0'; /* kill the ';' */
			g_strstrip (field);
			g_mime_header_set (message->header->headers, field, value);
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
 * @save_extra_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeStream *stream, gboolean save_extra_headers)
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
	
	hdr_end = find_header_part_end (in, inlen);
	if (hdr_end != NULL) {
		GMimePart *part;
		
		message = g_mime_message_new ();
		construct_headers (message, in, hdr_end - in, save_extra_headers);
		part = g_mime_parser_construct_part_internal (stream, mem);
		g_mime_message_set_mime_part (message, part);
	}
	
	g_mime_stream_unref (GMIME_STREAM (mem));
	
	return message;
}
