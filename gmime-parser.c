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

#include <string.h>
#include <ctype.h>

#define d(x) (x)

enum {
	CONTENT_TYPE = 0,
	CONTENT_TRANSFER_ENCODING,
	CONTENT_DISPOSITION,
	CONTENT_DESCRIPTION,
	CONTENT_LOCATION,
	CONTENT_MD5,
	CONTENT_ID
};

static gchar *content_headers[] = {
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
header_unfold (gchar *header)
{
	/* strip all \n's and replace tabs with spaces - this should
           undo any header folding */
	gchar *src, *dst;
	
	for (src = dst = header; *src; src++) {
		if (*src != '\n')
			*dst++ = *src != '\t' ? *src : ' ';
	}
	*dst = '\0';
}

static int
content_header (const gchar *field)
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

static const gchar *
g_strstrbound (const gchar *haystack, const gchar *needle, const gchar *end)
{
	gboolean matches = FALSE;
	const gchar * ptr;
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

#define GMIME_PARSER_MAX_LINE_WIDTH 1024

/**
 * get_header_block_from_file: Get the header block from a message.
 * @fp file pointer pointing to the beginning of the file block.
 *
 * This will read all of the headers into an unparsed GArray and
 * leave fp_in pointing at the message body that comes after the
 * headers.
 **/
static GArray *
get_header_block_from_file (FILE *fp)
{
	GArray *a;
	gchar buf [GMIME_PARSER_MAX_LINE_WIDTH];
	
	g_return_val_if_fail (fp != NULL, NULL);
	
	a = g_array_new (TRUE, FALSE, 1);
	for (;;) {
		gchar *pch;
		
		pch = fgets (buf, sizeof (buf), fp);
		if (pch == NULL) {
			/* eof reached before end-of-headers! */
			g_array_free (a, TRUE);
			a = NULL;
			break;
		}
		
		/* empty line -- end of headers */
		if (!strcmp (buf,"\n"))
			break;
		
		g_array_append_vals (a, buf, strlen(buf));
	}
	
	return a;
}

/**
 * get_header_block: Get the header block from a message.
 * @message: the message text; that is, header + body.
 *
 * This will read all of the headers into an unparsed GArray.
 **/
static GArray *
get_header_block (const gchar *pch)
{
	GArray *a = NULL;
	const gchar *header_end;
	
	g_return_val_if_fail (pch != NULL, NULL);
	
	header_end = strstr (pch, "\n\n");
	if (header_end != NULL) {
		a = g_array_new (TRUE, FALSE, 1);
		g_array_append_vals (a, pch, header_end - pch);
	}
	
	return a;
}


/**
 * find_header_end: Given the beginning of a header, will return a pointer to its end.
 * @header: the beginning of the header
 * @boundary: maximum end boundary for the header.  Typically points to the end of the header block.
 *
 * Returns a pointer to the end of a header.
 **/
static const gchar *
find_header_end (const gchar *header, const gchar *boundary)
{
	const gchar * eptr;
	
	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (boundary != NULL, NULL);
	
	for (eptr = header; eptr < boundary; eptr++)
		if (*eptr == '\n' && !isblank (eptr[1]))
			break;
	
	return eptr;
}

/**
 * parse_content_heaaders: picks the Content-* headers from a header block
 *                         and extracts info from them.
 * @headers: string containing the zero-terminated header block.
 * @headers_len: length of the header block.
 * @mime_part: mime part to populate with the information we get from the Content-* headers.
 * @is_multipart: set to TRUE if the part is a multipart, FALSE otherwise (to be set by function)
 * @boundary: multipart boundary string (to be set by function)
 * @end_boundary: multipart end boundary string (to be set by function)
 *
 * Parse a header block for content information.
 */
static void
parse_content_headers (const gchar *headers, gint headers_len,
                       GMimePart *mime_part, gboolean *is_multipart,
                       gchar **boundary, gchar **end_boundary)
{
	const gchar *headers_ptr = headers;
	const gchar *headers_end = headers + headers_len;
	
	while (headers_ptr < headers_end) {
		const gint type = content_header (headers_ptr);
		
		switch (type) {
		case CONTENT_DESCRIPTION: {
			const gchar *pch = headers_ptr + strlen (content_headers[type]);
			const gchar *end = find_header_end (pch, headers_end);
			gchar *desc_enc = g_strndup (pch, (gint)(end - pch));
			gchar *desc_dec = g_mime_utils_8bit_header_decode (desc_enc);
			
			g_strstrip (desc_dec);
			g_mime_part_set_content_description (mime_part, desc_dec);
			g_free (desc_enc);
			g_free (desc_dec);
			
			headers_ptr = end + 1;
			break;
		}
		case CONTENT_LOCATION: {
			const gchar *pch = headers_ptr + strlen (content_headers[type]);
			const gchar *end = find_header_end (pch, headers_end);
			gchar * loc = g_strndup (pch, (gint)(end - pch));
			
			g_strstrip (loc);
			g_mime_part_set_content_location (mime_part, loc);
			g_free (loc);
			
			headers_ptr = end + 1;
			break;
		}
		case CONTENT_MD5: {
			const gchar *pch = headers_ptr + strlen (content_headers[type]);
			const gchar *end = find_header_end (pch, headers_end);
			gchar *md5 = g_strndup (pch, (gint) (end - pch));
			
			g_strstrip (md5);
			g_mime_part_set_content_md5 (mime_part, md5);
			g_free (md5);
			
			headers_ptr = end + 1;
			break;
		}
		case CONTENT_ID: {
			const gchar* pch = headers_ptr + strlen (content_headers[type]);
			const gchar* end = find_header_end (pch, headers_end);
			gchar *id = g_strndup (pch, (gint) (end - pch));
			
			g_strstrip (id);
			g_mime_part_set_content_id (mime_part, id);
			g_free (id);
			
			headers_ptr = end + 1;
			break;
		}
		case CONTENT_TRANSFER_ENCODING: {
			const gchar *pch = headers_ptr + strlen (content_headers[type]);
			const gchar *end = find_header_end (pch, headers_end);
			gchar *encoding = g_strndup (pch, (gint) (end - pch));
			
			g_strstrip (encoding);
			g_mime_part_set_encoding (mime_part, g_mime_part_encoding_from_string (encoding));
			g_free (encoding);
			
			headers_ptr = end + 1;
			break;
		}
		case CONTENT_TYPE: {
			const gchar *pch = headers_ptr + strlen (content_headers[type]);
			const gchar *end = find_header_end (pch, headers_end);
			gchar *type = g_strndup (pch, (gint) (end - pch));
			GMimeContentType *mime_type;
			
			g_strstrip (type);
			mime_type = g_mime_content_type_new_from_string (type);
			g_free (type);
			
			*is_multipart = g_mime_content_type_is_type (mime_type, "multipart", "*");
			if (*is_multipart) {
				const gchar *b;
				
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
			
			headers_ptr = end + 1;
			break;
		}
		case CONTENT_DISPOSITION: {
			const gchar *pch = headers_ptr + strlen (content_headers[type]);
			const gchar *end = find_header_end (pch, headers_end);
			gchar *disposition = g_strndup (pch, (gint) (end - pch));
			gchar *ptr;
			gchar *disp;
			
			/* get content disposition part */
			for (ptr = disposition; *ptr && *ptr != ';'; ptr++); /* find ; or \0 */
			disp = g_strndup (disposition, (gint)(ptr - disposition));
			g_strstrip (disp);
			g_mime_part_set_content_disposition (mime_part, disp);
			g_free (disp);
			
			/* parse the parameters, if any */
			while (*ptr == ';') {
				gchar *name;
				gchar *value;
				
				/* get the param name */
				for (name = ptr + 1; *name && !isspace ((int)*name); name++);
				for (ptr = name; *ptr && *ptr != '='; ptr++);
				name = g_strndup (name, (gint) (ptr - name));
				g_strstrip (name);
				
				/* convert param name to lowercase */
				g_strdown (name);
				
				/* skip any whitespace */
				for (value = ptr + 1; *value && isspace ((int)*value); value++);
				
				if (*value == '"') {
					/* value is in quotes */
					value++;
					for (ptr = value; *ptr; ptr++)
						if (*ptr == '"' && *(ptr - 1) != '\\')
							break;
					value = g_strndup (value, (gint) (ptr - value));
					g_strstrip (value);
					g_mime_utils_unquote_string (value);
						
					for ( ; *ptr && *ptr != ';'; ptr++);
				} else {
					/* value is not in quotes */
					for (ptr = value; *ptr && *ptr != ';'; ptr++);
					value = g_strndup (value, (gint) (ptr - value));
					g_strstrip (value);
				}
				
				g_mime_part_add_content_disposition_parameter (mime_part, name, value);
				
				g_free (name);
				g_free (value);
			}
			
			g_free (disposition);
			
			headers_ptr = end + 1;
			break;
		}
		default: { /* ignore this header */
			const gchar *pch = headers_ptr;
			const gchar *end = find_header_end (pch, headers_end);
			
			headers_ptr = end + 1;
			break;
		}
		}
	}
}

typedef enum {
	PARSER_EOF,
	PARSER_BOUNDARY,
	PARSER_END_BOUNDARY,
	PARSER_LINE
} ParserState;

static ParserState
get_next_line (gchar *buf, guint buf_len, FILE *fp, const gchar *boundary, const gchar *end_boundary)
{
	ParserState state;
	
	*buf = '\0';
	if (fgets (buf, buf_len, fp) == NULL)
		state = PARSER_EOF;
	else if (boundary != NULL && !strcmp (buf, boundary))
		state = PARSER_BOUNDARY;
	else if (end_boundary != NULL && !strcmp (buf, end_boundary))
		state = PARSER_END_BOUNDARY;
	else
		state = PARSER_LINE;
	
	return state;
}

/**
 * g_mime_parser_construct_part_from_file: Construct a GMimePart object
 * @in: raw MIME Part data
 *
 * Returns a GMimePart object based on the data.
 **/
static GMimePart *
g_mime_parser_construct_part_from_file (const gchar   *headers,
                                        guint          headers_len,
                                        FILE          *fp,
                                        const gchar   *parent_boundary,
                                        const gchar   *parent_end_boundary,
                                        ParserState   *setme_state)
{
	gchar *boundary;
	gchar *end_boundary;
	gboolean is_multipart;
	GMimePart *mime_part;
	
	g_return_val_if_fail (headers != NULL, NULL);
	g_return_val_if_fail (headers_len > 0, NULL);
	g_return_val_if_fail (fp != NULL, NULL);
	g_return_val_if_fail (setme_state != NULL, NULL);
	
	/* Headers */
	boundary = NULL;
	end_boundary = NULL;
	is_multipart = FALSE;
	mime_part = g_mime_part_new ();
	parse_content_headers (headers, headers_len, mime_part, &is_multipart, &boundary, &end_boundary);
	
	/* Body */
	if (is_multipart && boundary!=NULL && end_boundary!=NULL) { /* is a multipart */
		/* get all the subparts */
		gchar buf[GMIME_PARSER_MAX_LINE_WIDTH];
		
		for (;;) {
			/* get the next line, we're looking for the beginning of a subpart */
			ParserState ps = get_next_line (buf, sizeof (buf), fp, parent_boundary,
							parent_end_boundary);
			if (ps != PARSER_LINE) {
				*setme_state = ps;
				break;
			}
			
			/* is the beginning of a subpart? */
			if (strcmp (buf, boundary))
				continue;
			
			/* add subparts as long as we keep getting boundaries */
			for (;;) {
				GArray *h = get_header_block_from_file (fp);
				if (h != NULL) {
					ParserState ps = 0;
					GMimePart *part;
					
					part = g_mime_parser_construct_part_from_file (h->data, h->len,
										       fp, boundary,
										       end_boundary, &ps);
					g_array_free (h, TRUE);
					if (part != NULL)
						g_mime_part_add_subpart (mime_part, part);
					if (ps != PARSER_BOUNDARY)
						break;
				}
			}
		}
	} else {
		/* not a multipart */
		GMimePartEncodingType encoding = g_mime_part_get_encoding (mime_part);
		GArray *a = g_array_new (TRUE, FALSE, 1);
		gchar buf [GMIME_PARSER_MAX_LINE_WIDTH];
		
		/* keep reading lines until we reach a boundary or EOF, we're populating a part */
		for (;;) {
			ParserState ps = get_next_line (buf, sizeof (buf), fp,
							parent_boundary,
							parent_end_boundary);
			if (ps == PARSER_LINE) {
				g_array_append_vals (a, buf, strlen (buf));
			} else {
				*setme_state = ps;
				break;
			}
		}
		
		/* trim off excess trailing \n's */
		while (a->len > 2  && a->data[a->len-1] == '\n' && a->data[a->len - 2] == '\n')
			g_array_set_size (a, a->len - 1);
		
		if (a->len > 0)
			g_mime_part_set_pre_encoded_content (mime_part, a->data, a->len, encoding);
		
		g_array_free (a, TRUE);
	}
	
	g_free (boundary);	
	g_free (end_boundary);	
	
	return mime_part;
}

/* we pass the length here so that we can avoid dup'ing in the caller
   as mime parts can be BIG (tm) */
/**
 * g_mime_parser_construct_part: Construct a GMimePart object
 * @in: raw MIME Part data
 * @inlen: raw MIME Part data length
 *
 * Returns a GMimePart object based on the data.
 **/
GMimePart *
g_mime_parser_construct_part (const gchar *in, guint inlen)
{
	GMimePart *mime_part;
	GArray *headers;
	gchar *boundary;
	gchar *end_boundary;
	gboolean is_multipart;
	const gchar *inptr;
	const gchar *inend = in + inlen;
	
	g_return_val_if_fail (in != NULL, NULL);
	g_return_val_if_fail (inlen != 0, NULL);
	
	/* Headers */
	headers = get_header_block (in);
	mime_part = g_mime_part_new ();
	is_multipart = FALSE;
	boundary = NULL;
	end_boundary = NULL;
	parse_content_headers (headers->data, headers->len, mime_part,
			       &is_multipart, &boundary, &end_boundary);
	
	/* Body */
	inptr = in + headers->len;
	
	if (is_multipart && boundary && end_boundary) {
		/* get all the subparts */
		GMimePart *subpart;
		const gchar *part_begin;
		const gchar *part_end;
		
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
			subpart = g_mime_parser_construct_part (part_begin, (guint) (part_end - part_begin));
			g_mime_part_add_subpart (mime_part, subpart);
			
			/* the next part begins where the last one left off */
			part_begin = part_end;
		}
		
		/* free our temp boundary strings */
		g_free (boundary);
		g_free (end_boundary);
	} else {
		GMimePartEncodingType encoding;
		const gchar *content = NULL;
		guint len = 0;
		
		/* from here to the end is the content */
		if (inptr < inend) {
			for (inptr++; inptr < inend && isspace ((int)*inptr); inptr++);
			len = inend - inptr;
			content = inptr;
			
			/* trim off excess trailing \n's */
			inend = content + len;
			while (len > 2 && *inend == '\n' && *(inend - 1) == '\n') {
				inend--;
				len--;
			}
		}
		
		encoding = g_mime_part_get_encoding (mime_part);
		
		if (len > 0)
			g_mime_part_set_pre_encoded_content (mime_part, content, len, encoding);
	}
	
	g_array_free (headers, TRUE);	
	
	return mime_part;
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
special_header (const gchar *field)
{
	return (!g_strcasecmp (field, "MIME-Version:") || content_header (field) != -1);
}

static void
construct_headers (GMimeMessage *message, const gchar *headers, gint inlen, gboolean save_extra_headers)
{
	gchar *field, *value, *raw, *q;
	gchar *inptr, *inend;
	time_t date;
	int offset = 0;
	int i;
	
	inptr = (gchar *) headers;
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
			if (save_extra_headers && !special_header (field)) {
				field[strlen (field) - 1] = '\0'; /* kill the ':' */
				g_mime_message_add_arbitrary_header (message, field, value);
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
 * @data: an rfc0822 message
 * @save_extra_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message (const gchar *data, gboolean save_extra_headers)
{
	GMimeMessage *message = NULL;
	GArray *headers;
	
	g_return_val_if_fail (data != NULL, NULL);
	
	headers = get_header_block (data);
	if (headers != NULL) {
		GMimePart * part;
		
		message = g_mime_message_new ();
		construct_headers (message, headers->data, headers->len, save_extra_headers);
		part = g_mime_parser_construct_part (data, strlen(data));
		g_mime_message_set_mime_part (message, part);
		
		g_array_free (headers, TRUE);
	}
	
	return message;
}

/**
 * g_mime_parser_construct_message_from_file: Construct a GMimeMessage object
 * @fp: a file pointer pointing to an rfc0822 message
 * @save_extra_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message_from_file (FILE *fp, gboolean save_extra_headers)
{
	GMimeMessage *message = NULL;
	GArray *headers;
	
	g_return_val_if_fail (fp != NULL, NULL);
	
	headers = get_header_block_from_file (fp);
	if (headers != NULL) {
		GMimePart *part;
		ParserState state = -1;
		
		message = g_mime_message_new ();
		construct_headers (message, headers->data, headers->len, save_extra_headers);
		part = g_mime_parser_construct_part_from_file (headers->data, headers->len, fp,
							       NULL, NULL, &state);
		g_mime_message_set_mime_part (message, part);
		if (state != PARSER_EOF)
			g_warning ("Didn't reach end of file - parser error?");
		
		g_array_free (headers, TRUE);
	}
	
	return message;
}
