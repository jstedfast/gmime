/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000-2002 Ximain, Inc. (www.ximian.com)
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

#include "strlib.h"
#include <ctype.h>

#include "gmime-parser.h"

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

#define d(x)

typedef struct _GMimeParser GMimeParser;

static GMimeParser *parser_new (GMimeStream *stream);
static void parser_destroy (GMimeParser *parser);

static void parser_init (GMimeParser *parser, GMimeStream *stream);
static void parser_close (GMimeParser *parser);

static GMimePart *parser_construct_leaf_part (GMimeParser *parser, GMimeContentType *content_type,
					      int *found);
static GMimePart *parser_construct_multipart (GMimeParser *parser, GMimeContentType *content_type,
					      int *found);


#define SCAN_BUF 4096		/* size of read buffer */
#define SCAN_HEAD 128		/* headroom guaranteed to be before each read buffer */

enum {
	GMIME_PARSER_STATE_INIT,
	GMIME_PARSER_STATE_FROM,
	GMIME_PARSER_STATE_HEADERS,
	GMIME_PARSER_STATE_HEADERS_END,
	GMIME_PARSER_STATE_CONTENT,
};

struct _GMimeParser {
	int state;
	
	GMimeStream *stream;
	
	off_t offset;
	
	/* i/o buffers */
	unsigned char realbuf[SCAN_HEAD + SCAN_BUF + 1];
	unsigned char *inbuf;
	unsigned char *inptr;
	unsigned char *inend;
	
	/* header buffer */
	unsigned char *headerbuf;
	unsigned char *headerptr;
	unsigned int headerleft;
	
	off_t headers_start;
	off_t header_start;
	
	unsigned int unstep:31;
	unsigned int midline:1;
	
	GMimeContentType *content_type;
	struct _header_raw *headers;
	
	struct _boundary_stack *bounds;
};

struct _boundary_stack {
	struct _boundary_stack *parent;
	unsigned char *boundary;
	unsigned int boundarylen;
	unsigned int boundarylenfinal;
	unsigned int boundarylenmax;
};

static void
parser_push_boundary (GMimeParser *parser, const char *boundary)
{
	struct _boundary_stack *s;
	unsigned int max;
	
	max = parser->bounds ? parser->bounds->boundarylenmax : 0;
	
	s = g_new (struct _boundary_stack, 1);
	s->parent = parser->bounds;
	parser->bounds = s;
	
	s->boundary = g_strdup_printf ("--%s--", boundary);
	s->boundarylen = strlen (boundary) + 2;
	s->boundarylenfinal = strlen (s->boundary);
	
	s->boundarylenmax = MAX (s->boundarylenfinal, max);
}

static void
parser_pop_boundary (GMimeParser *parser)
{
	struct _boundary_stack *s;
	
	if (!parser->bounds) {
		g_warning ("boundary stack underflow");
		return;
	}
	
	s = parser->bounds;
	parser->bounds = parser->bounds->parent;
	
	g_free (s->boundary);
	g_free (s);
}

struct _header_raw {
	struct _header_raw *next;
	char *name;
	char *value;
	off_t offset;
};

static const char *
header_raw_find (struct _header_raw *headers, const char *name, off_t *offset)
{
	struct _header_raw *h;
	
	h = headers;
	while (h) {
		if (!strcasecmp (h->name, name)) {
			if (offset)
				*offset = h->offset;
			return h->value;
		}
		
		h = h->next;
	}
	
	return NULL;
}

static void
header_raw_clear (struct _header_raw **headers)
{
	struct _header_raw *h, *n;
	
	h = *headers;
	while (h) {
		n = h->next;
		g_free (h->name);
		g_free (h->value);
		g_free (h);
		h = n;
	}
	
	*headers = NULL;
}

static GMimeParser *
parser_new (GMimeStream *stream)
{
	GMimeParser *parser;
	
	parser = g_new (GMimeParser, 1);
	parser_init (parser, stream);
	
	return parser;
}

static void
parser_destroy (GMimeParser *parser)
{
	if (parser) {
		parser_close (parser);
		g_free (parser);
	}
}


static void
parser_init (GMimeParser *parser, GMimeStream *stream)
{
	off_t offset = -1;
	
	if (stream) {
		g_mime_stream_ref (stream);
		offset = g_mime_stream_tell (stream);
	}
	
	parser->state = GMIME_PARSER_STATE_INIT;
	
	parser->stream = stream;
	
	parser->offset = offset;
	
	parser->inbuf = parser->realbuf + SCAN_HEAD;
	parser->inptr = parser->inbuf;
	parser->inend = parser->inbuf;
	
	parser->headerbuf = g_malloc (SCAN_HEAD + 1);
	parser->headerptr = parser->headerbuf;
	parser->headerleft = SCAN_HEAD;
	
	parser->headers_start = -1;
	parser->header_start = -1;
	
	parser->unstep = 0;
	parser->midline = FALSE;
	
	parser->headers = NULL;
	
	parser->bounds = NULL;
}

static void
parser_close (GMimeParser *parser)
{
	if (parser->stream)
		g_mime_stream_unref (parser->stream);
	
	if (parser->headerbuf)
		g_free (parser->headerbuf);
	
	header_raw_clear (&parser->headers);
	
	while (parser->bounds)
		parser_pop_boundary (parser);
}

static void
parser_unstep (GMimeParser *parser)
{
	parser->unstep++;
}

static ssize_t
parser_fill (GMimeParser *parser)
{
	unsigned char *inbuf, *inptr, *inend;
	size_t inlen, atleast = SCAN_HEAD;
	ssize_t nread;
	
	inbuf = parser->inbuf;
	inptr = parser->inptr;
	inend = parser->inend;
	inlen = inend - inptr;
	
	atleast = MAX (atleast, parser->bounds ? parser->bounds->boundarylenmax : 0);
	
	if (inlen > atleast)
		return inlen;
	
	inbuf = parser->realbuf;
	
	memmove (inbuf, inptr, inlen);
	parser->inptr = inbuf;
	
	/* start reading into inbuf */
	inbuf += inlen;
	
	parser->inend = inbuf;
	inend = parser->realbuf + SCAN_HEAD + SCAN_BUF - 1;
	
	nread = g_mime_stream_read (parser->stream, inbuf, inend - inbuf);
	if (nread > 0)
		parser->inend += nread;
	
	parser->offset = g_mime_stream_tell (parser->stream);
	
	return parser->inend - parser->inptr;
}

static off_t
parser_offset (GMimeParser *parser, unsigned char *cur)
{
	unsigned char *inptr = cur;
	
	if (!inptr)
		inptr = parser->inptr;
	
	return (parser->offset - (parser->inend - inptr));
}

#define header_backup(parser, start, len) G_STMT_START {                    \
	if (parser->headerleft <= len) {                                    \
		unsigned int hlen, hoff;                                  \
		                                                          \
		hlen = hoff = parser->headerptr - parser->headerbuf;          \
		hlen = hlen ? hlen : 1;                                   \
		                                                          \
		while (hlen < hoff + len)                                 \
			hlen <<= 1;                                       \
		                                                          \
		parser->headerbuf = g_realloc (parser->headerbuf, hlen + 1);  \
		parser->headerptr = parser->headerbuf + hoff;                 \
		parser->headerleft = hlen - hoff;                           \
	}                                                                 \
	                                                                  \
	memcpy (parser->headerptr, start, len);                             \
	parser->headerptr += len;                                           \
	parser->headerleft -= len;                                          \
} G_STMT_END

#define header_parse(parser, hend) G_STMT_START {                           \
	register unsigned char *colon;                                    \
	struct _header_raw *header;                                       \
	unsigned int hlen;                                                \
	                                                                  \
	header = g_new (struct _header_raw, 1);                           \
	header->next = NULL;                                              \
	                                                                  \
	*parser->headerptr = '\0';                                          \
	colon = parser->headerbuf;                                          \
	while (*colon && *colon != ':')                                   \
		colon++;                                                  \
	                                                                  \
	hlen = colon - parser->headerbuf;                                   \
	                                                                  \
	header->name = g_strstrip (g_strndup (parser->headerbuf, hlen));    \
	header->value = g_strstrip (g_strdup (colon + 1));                \
	header->offset = parser->header_start;                              \
	                                                                  \
	hend->next = header;                                              \
	hend = header;                                                    \
	                                                                  \
	parser->headerleft += parser->headerptr - parser->headerbuf;            \
	parser->headerptr = parser->headerbuf;                                \
} G_STMT_END

static int
parser_step_headers (GMimeParser *parser)
{
	register unsigned char *inptr;
	unsigned char *start, *inend;
	struct _header_raw *hend;
	size_t len;
	
	parser->midline = FALSE;
	hend = (struct _header_raw *) &parser->headers;
	parser->headers_start = parser_offset (parser, NULL);
	parser->header_start = parser_offset (parser, NULL);
	
	inptr = parser->inptr;
	
	do {
	refill:
		if (parser_fill (parser) <= 0)
			break;
		
		inptr = parser->inptr;
		inend = parser->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		g_assert (inptr <= inend);
		
		while (inptr < inend) {
			start = inptr;
			/* Note: see optimization comment [1] */
			while (*inptr != '\n')
				inptr++;
			
			if (inptr + 1 >= inend) {
				/* we don't have enough data to tell if we
				   got all of the header or not... */
				parser->inptr = start;
				goto refill;
			}
			
			/* check to see if we've reached the end of the headers */
			if (!parser->midline && inptr == start)
				goto headers_end;
			
			len = inptr - start;
			header_backup (parser, start, len);
			
			if (inptr < inend) {
				/* inptr has to be less than inend - 1 */
				inptr++;
				if (*inptr == ' ' || *inptr == '\t') {
					parser->midline = TRUE;
				} else {
					parser->midline = FALSE;
					header_parse (parser, hend);
					parser->header_start = parser_offset (parser, inptr);
				}
			} else {
				parser->midline = TRUE;
			}
		}
		
		parser->inptr = inptr;
	} while (1);
	
	inptr = parser->inptr;
	inend = parser->inend;
	
	header_backup (parser, inptr, inend - inptr);
	header_parse (parser, hend);
	
 headers_end:
	
	parser->state = GMIME_PARSER_STATE_HEADERS_END;
	
	g_assert (inptr <= parser->inend);
	
	parser->inptr = inptr;
	
	return 0;
}

static GMimeContentType *
parser_content_type (GMimeParser *parser)
{
	const char *content_type;
	
	content_type = header_raw_find (parser->headers, "Content-Type", NULL);
	if (content_type)
		return g_mime_content_type_new_from_string (content_type);
	
	return NULL;
}

static int
parser_step (GMimeParser *parser)
{
	if (!parser->unstep) {
		switch (parser->state) {
		case GMIME_PARSER_STATE_INIT:
			parser->state = GMIME_PARSER_STATE_HEADERS;
		case GMIME_PARSER_STATE_HEADERS:
			parser_step_headers (parser);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else {
		parser->unstep--;
	}
	
	return parser->state;
}

static void
parser_skip_line (GMimeParser *parser)
{
	register unsigned char *inptr;
	unsigned char *inend;
	
	inptr = parser->inptr;
	
	do {
		if (parser_fill (parser) <= 0)
			break;
		
		inptr = parser->inptr;
		inend = parser->inend;
		
		while (inptr < inend && *inptr != '\n')
			inptr++;
		
		if (inptr < inend)
			break;
		
		parser->inptr = inptr;
	} while (1);
	
	parser->midline = FALSE;
	
	parser->inptr = inptr + 1;
}

enum {
	FOUND_EOS,
	FOUND_BOUNDARY,
	FOUND_END_BOUNDARY
};

#define content_save(content, start, len) G_STMT_START {   \
	if (content)                                       \
		g_byte_array_append (content, start, len); \
} G_STMT_END

#define possible_boundary(start, len)  (len >= 3 && (start[0] == '-' && start[1] == '-'))

/* Optimization Notes:
 *
 * 1. By making the parser->realbuf char array 1 extra char longer, we
 * can safely set '*inend' to '\n' and not fear an ABW. Setting *inend
 * to '\n' means that we can eliminate having to check that inptr <
 * inend every trip through our inner while-loop. This cuts the number
 * of instructions down from ~7 to ~4, assuming the compiler does its
 * job correctly ;-)
 **/

static int
parser_scan_content (GMimeParser *parser, GByteArray *content)
{
	register unsigned char *inptr;
	unsigned char *start, *inend;
	gboolean found_eos = FALSE;
	size_t nleft, len;
	int found;
	
	d(printf ("scan-content\n"));
	
	parser->midline = FALSE;
	
	g_assert (parser->inptr <= parser->inend);
	
	start = inptr = parser->inptr;
	
	do {
	refill:
		nleft = parser->inend - inptr;
		if (parser_fill (parser) <= 0) {
			found = FOUND_EOS;
			break;
		}
		
		inptr = parser->inptr;
		inend = parser->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		if (inend - inptr == nleft)
			found_eos = TRUE;
		
		while (inptr < inend) {
			start = inptr;
			/* Note: see optimization comment [1] */
			while (*inptr != '\n')
				inptr++;
			
			len = inptr - start;
			
			if (inptr < inend) {
				inptr++;
				if (possible_boundary (start, len)) {
					struct _boundary_stack *s;
					
					d(printf ("checking boundary '%.*s'\n", len, start));
					
					s = parser->bounds;
					while (s) {
						if (len == s->boundarylenfinal &&
						    !strncmp (s->boundary, start,
							      s->boundarylenfinal)) {
							found = FOUND_END_BOUNDARY;
							goto boundary;
						}
						
						if (len == s->boundarylen &&
						    !strncmp (s->boundary, start,
							      s->boundarylen)) {
							found = FOUND_BOUNDARY;
							goto boundary;
						}
						
						s = s->parent;
					}
					
					d(printf ("'%.*s' not a boundary\n", len, start));
				}
				len++;
			} else if (!found_eos) {
				/* not enough to tell if we found a boundary */
				parser->inptr = start;
				goto refill;
			}
			
			content_save (content, start, len);
		}
		
		parser->inptr = inptr;
	} while (1);
	
 boundary:
	
	/* don't chew up the boundary */
	parser->inptr = start;
	
	return found;
}

static void
parser_scan_mime_part_content (GMimeParser *parser, GMimePart *mime_part, int *found)
{
	GMimePartEncodingType encoding;
	GMimeDataWrapper *wrapper;
	GMimeStream *stream;
	off_t start, end;
	
	start = parser_offset (parser, NULL);
	*found = parser_scan_content (parser, NULL);
	end = parser_offset (parser, NULL) - 1;  /* last '\n' belongs to the boundary */
	
	encoding = g_mime_part_get_encoding (mime_part);
	stream = g_mime_stream_substream (parser->stream, start, end);
	wrapper = g_mime_data_wrapper_new_with_stream (stream, encoding);
	g_mime_part_set_content_object (mime_part, wrapper);
	g_mime_stream_unref (stream);
}



enum {
	CONTENT_TYPE = 0,
	CONTENT_TRANSFER_ENCODING,
	CONTENT_DISPOSITION,
	CONTENT_DESCRIPTION,
	CONTENT_LOCATION,
	CONTENT_MD5,
	CONTENT_ID,
	CONTENT_UNKNOWN
};

static char *content_headers[] = {
	"Content-Type",
	"Content-Transfer-Encoding",
	"Content-Disposition",
	"Content-Description",
	"Content-Location",
	"Content-Md5",
	"Content-Id",
	NULL
};


static void
mime_part_set_content_headers (GMimePart *mime_part, struct _header_raw *headers)
{
	struct _header_raw *header;
	
	header = headers;
	while (header) {
		GMimePartEncodingType encoding;
		GMimeDisposition *disposition;
		char *text;
		int i;
		
		for (i = 0; content_headers[i]; i++)
			if (!strcasecmp (content_headers[i], header->name))
				break;
		
		g_strstrip (header->value);
		
		switch (i) {
		case CONTENT_DESCRIPTION:
			text = g_mime_utils_8bit_header_decode (header->value);
			g_strstrip (text);
			g_mime_part_set_content_description (mime_part, text);
			g_free (text);
			break;
		case CONTENT_LOCATION:
			g_mime_part_set_content_location (mime_part, header->value);
			break;
		case CONTENT_MD5:
			g_mime_part_set_content_md5 (mime_part, header->value);
			break;
		case CONTENT_ID:
			g_mime_part_set_content_id (mime_part, header->value);
			break;
		case CONTENT_TRANSFER_ENCODING:
			encoding = g_mime_part_encoding_from_string (header->value);
			g_mime_part_set_encoding (mime_part, encoding);
			break;
		case CONTENT_TYPE:
			/* no-op, this has already been decoded */
			break;
		case CONTENT_DISPOSITION:
			disposition = g_mime_disposition_new (header->value);
			g_mime_part_set_content_disposition_object (mime_part, disposition);
			break;
		default:
			/* possibly save the header */
			if (!strncasecmp ("Content-", header->name, 8))
				g_mime_part_set_content_header (mime_part, header->name, header->value);
			break;
		}
		
		header = header->next;
	}
}

static GMimePart *
parser_construct_leaf_part (GMimeParser *parser, GMimeContentType *content_type, int *found)
{
	GMimePart *mime_part;
	
	/* get the headers */
	while (parser_step (parser) != GMIME_PARSER_STATE_HEADERS_END)
		;
	
	if (!content_type) {
		content_type = parser_content_type (parser);
		if (!content_type)
			content_type = g_mime_content_type_new ("text", "plain");
	}
	
	mime_part = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	mime_part_set_content_headers (mime_part, parser->headers);
	header_raw_clear (&parser->headers);
	
	g_mime_part_set_content_type (mime_part, content_type);
	
	/* skip empty line */
	parser_skip_line (parser);
	
	parser_scan_mime_part_content (parser, mime_part, found);
	
	return mime_part;
}

static int
parser_scan_multipart_face (GMimeParser *parser, GMimePart *multipart, gboolean preface)
{
	GByteArray *buffer;
	const char *face;
	int len, found;
	
	buffer = g_byte_array_new ();
	found = parser_scan_content (parser, buffer);
	
	/* last '\n' belongs to the boundary */
	if (buffer->len) {
		buffer->data[buffer->len - 1] = '\0';
		face = buffer->data;
	} else
		face = NULL;
	
#if 0
	/* FIXME: allow setting of these? */
	if (preface)
		g_mime_part_set_preface (multipart, face);
	else
		g_mime_part_set_postface (multipart, face);
#endif
	
	g_byte_array_free (buffer, TRUE);
	
	return found;
}

#define parser_scan_multipart_preface(parser, multipart) parser_scan_multipart_face (parser, multipart, TRUE)
#define parser_scan_multipart_postface(parser, multipart) parser_scan_multipart_face (parser, multipart, FALSE)

static int
parser_scan_multipart_subparts (GMimeParser *parser, GMimePart *multipart)
{
	GMimeContentType *content_type;
	struct _header_raw *header;
	GMimePart *subpart;
	int found;
	
	do {
		/* skip over the boundary marker */
		parser_skip_line (parser);
		
		/* get the headers */
		parser_step_headers (parser);
		
		content_type = parser_content_type (parser);
		if (!content_type)
			content_type = g_mime_content_type_new ("text", "plain");
		
		parser_unstep (parser);
		if (g_mime_content_type_is_type (content_type, "multipart", "*")) {
			subpart = parser_construct_multipart (parser, content_type, &found);
		} else {
			subpart = parser_construct_leaf_part (parser, content_type, &found);
		}
		
		g_mime_part_add_subpart (multipart, subpart);
		g_mime_object_unref (GMIME_OBJECT (subpart));
	} while (found == FOUND_BOUNDARY);
	
	return found;
}

static GMimePart *
parser_construct_multipart (GMimeParser *parser, GMimeContentType *content_type, int *found)
{
	struct _header_raw *header;
	GMimePart *multipart;
	const char *boundary;
	
	/* get the headers */
	while (parser_step (parser) != GMIME_PARSER_STATE_HEADERS_END)
		;
	
	multipart = g_mime_part_new_with_type (content_type->type, content_type->subtype);
	mime_part_set_content_headers (multipart, parser->headers);
	header_raw_clear (&parser->headers);
	
	g_mime_part_set_content_type (multipart, content_type);
	
	/* skip empty line */
	parser_skip_line (parser);
	
	boundary = g_mime_content_type_get_parameter (content_type, "boundary");
	if (boundary) {
		parser_push_boundary (parser, boundary);
		
		*found = parser_scan_multipart_preface (parser, multipart);
		
		if (*found == FOUND_BOUNDARY)
			*found = parser_scan_multipart_subparts (parser, multipart);
		parser_pop_boundary (parser);
		
		/* eat end boundary */
		parser_skip_line (parser);
		
		if (*found == FOUND_END_BOUNDARY)
			*found = parser_scan_multipart_postface (parser, multipart);
	} else {
		g_warning ("multipart without boundary encountered");
		/* this will scan everything into the preface */
		*found = parser_scan_multipart_preface (parser, multipart);
	}
	
	return multipart;
}

static GMimePart *
parser_construct_part (GMimeParser *parser)
{
	GMimeContentType *content_type;
	GMimePart *part;
	int found;
	
	/* get the headers */
	while (parser_step (parser) != GMIME_PARSER_STATE_HEADERS_END)
		;
	
	content_type = parser_content_type (parser);
	if (!content_type)
		content_type = g_mime_content_type_new ("text", "plain");
	
	parser_unstep (parser);
	if (g_mime_content_type_is_type (content_type, "multipart", "*")) {
		part = parser_construct_multipart (parser, content_type, &found);
	} else {
		part = parser_construct_leaf_part (parser, content_type, &found);
	}
	
	return part;
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
	GMimeParser *parser;
	GMimePart *part;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	parser = parser_new (stream);
	part = parser_construct_part (parser);
	parser_destroy (parser);
	
	return part;
}



static int
content_header (const char *field)
{
	int i;
	
	for (i = 0; content_headers[i]; i++)
		if (!strcasecmp (field, content_headers[i]))
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

static char *message_headers[] = {
	"From",
	"Reply-To",
	"To",
	"Cc",
	"Bcc",
	"Subject",
	"Date",
	"Message-Id",
	NULL
};

static gboolean
special_header (const char *header)
{
	return (!strcasecmp (header, "MIME-Version") || content_header (header) != -1);
}


static GMimeMessage *
parser_construct_message (GMimeParser *parser)
{
	GMimeContentType *content_type;
	struct _header_raw *header;
	GMimeMessage *message;
	int i, offset, found;
	GMimePart *part;
	time_t date;
	char *raw;
	
	/* get the headers */
	while (parser_step (parser) != GMIME_PARSER_STATE_HEADERS_END)
		;
	
	message = g_mime_message_new (FALSE);
	header = parser->headers;
	while (header) {
		for (i = 0; message_headers[i]; i++)
			if (!strcasecmp (message_headers[i], header->name))
				break;
		
		g_strstrip (header->value);
		
		switch (i) {
		case HEADER_FROM:
			raw = g_mime_utils_8bit_header_decode (header->value);
			g_mime_message_set_sender (message, raw);
			g_free (raw);
			break;
		case HEADER_REPLY_TO:
			raw = g_mime_utils_8bit_header_decode (header->value);
			g_mime_message_set_reply_to (message, raw);
			g_free (raw);
			break;
		case HEADER_TO:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_TO,
								   header->value);
			break;
		case HEADER_CC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_CC,
								   header->value);
			break;
		case HEADER_BCC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_BCC,
								   header->value);
			break;
		case HEADER_SUBJECT:
			raw = g_mime_utils_8bit_header_decode (header->value);
			g_mime_message_set_subject (message, raw);
			g_free (raw);
			break;
		case HEADER_DATE:
			date = g_mime_utils_header_decode_date (header->value, &offset);
			g_mime_message_set_date (message, date, offset);
			break;
		case HEADER_MESSAGE_ID:
			raw = g_mime_utils_8bit_header_decode (header->value);
			g_mime_message_set_message_id (message, raw);
			g_free (raw);
			break;
		case HEADER_UNKNOWN:
		default:
			/* save the raw header if it's not a mime-part header */
			if (!special_header (header->name)) {
				g_mime_message_add_header (message, header->name, header->value);
			}
			break;
		}
		
		header = header->next;
	}
	
	content_type = parser_content_type (parser);
	if (!content_type)
		content_type = g_mime_content_type_new ("text", "plain");
	
	parser_unstep (parser);
	if (content_type && g_mime_content_type_is_type (content_type, "multipart", "*")) {
		part = parser_construct_multipart (parser, content_type, &found);
	} else {
		part = parser_construct_leaf_part (parser, content_type, &found);
	}
	
	g_mime_message_set_mime_part (message, part);
	g_mime_object_unref (GMIME_OBJECT (part));
	
	return message;
}


/**
 * g_mime_parser_construct_message:
 * @stream: an rfc0822 message stream
 *
 * Constructs a GMimeMessage object based on @stream.
 *
 * Returns a GMimeMessage object based on the rfc0822 message stream.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeStream *stream)
{
	GMimeMessage *message;
	GMimeParser *parser;
	
	g_return_val_if_fail (stream != NULL, NULL);
	
	parser = parser_new (stream);
	message = parser_construct_message (parser);
	parser_destroy (parser);
	
	return message;
}
