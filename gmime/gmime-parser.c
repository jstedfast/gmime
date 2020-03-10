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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "gmime-parser.h"

#include "gmime-table-private.h"
#include "gmime-message-part.h"
#include "gmime-parse-utils.h"
#include "gmime-stream-null.h"
#include "gmime-stream-mem.h"
#include "gmime-multipart.h"
#include "gmime-internal.h"
#include "gmime-common.h"
#include "gmime-part.h"

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)

#define MAX_LEVEL 1024

/**
 * SECTION: gmime-parser
 * @title: GMimeParser
 * @short_description: Message and MIME part parser
 * @see_also:
 *
 * A #GMimeParser parses a stream into a #GMimeMessage or other
 * #GMimeObject and can also handle parsing MBox formatted streams
 * into multiple #GMimeMessage objects.
 **/

extern const GMimeOpenPGPMarker g_mime_openpgp_markers[9];

typedef enum {
	BOUNDARY_NONE,
	BOUNDARY_EOS,
	BOUNDARY_IMMEDIATE,
	BOUNDARY_IMMEDIATE_END,
	BOUNDARY_PARENT,
	BOUNDARY_PARENT_END
} BoundaryType;

typedef struct _boundary_stack {
	struct _boundary_stack *parent;
	char *boundary;
	size_t boundarylen;
	size_t boundarylenfinal;
	size_t boundarylenmax;
} BoundaryStack;

typedef struct {
	char *raw_name, *name;
	char *raw_value;
	gint64 offset;
} Header;

typedef struct _content_type {
	char *type, *subtype;
	gboolean exists;
} ContentType;

static void g_mime_parser_class_init (GMimeParserClass *klass);
static void g_mime_parser_init (GMimeParser *parser, GMimeParserClass *klass);
static void g_mime_parser_finalize (GObject *object);

static void parser_init (GMimeParser *parser, GMimeStream *stream);
static void parser_close (GMimeParser *parser);

static GMimeObject *parser_construct_leaf_part (GMimeParser *parser, GMimeParserOptions *options, ContentType *content_type,
						gboolean toplevel, BoundaryType *found, int depth);
static GMimeObject *parser_construct_multipart (GMimeParser *parser, GMimeParserOptions *options, ContentType *content_type,
						gboolean toplevel, BoundaryType *found, int depth);

static GObjectClass *parent_class = NULL;

/* size of read buffer */
#define SCAN_BUF 4096

/* headroom guaranteed to be before each read buffer */
#define SCAN_HEAD 128

/* conservative growth sizes */
#define HEADER_INIT_SIZE 256

typedef enum {
	GMIME_PARSER_STATE_ERROR = -1,
	GMIME_PARSER_STATE_INIT,
	GMIME_PARSER_STATE_FROM,
	GMIME_PARSER_STATE_AAAA,
	GMIME_PARSER_STATE_MESSAGE_HEADERS,
	GMIME_PARSER_STATE_HEADERS,
	GMIME_PARSER_STATE_HEADERS_END,
	GMIME_PARSER_STATE_CONTENT,
	GMIME_PARSER_STATE_COMPLETE,
} GMimeParserState;

struct _GMimeParserPrivate {
	GMimeStream *stream;
	GMimeFormat format;
	
	gint64 content_end;
	gint64 offset;
	
	/* i/o buffers */
	char realbuf[SCAN_HEAD + SCAN_BUF + 4];
	char *inbuf;
	char *inptr;
	char *inend;
	
	GMimeParserHeaderRegexFunc header_cb;
	gpointer user_data;
	GRegex *regex;
	
	GByteArray *marker;
	gint64 marker_offset;
	
	char *preheader;
	
	/* current message headerblock offsets */
	gint64 message_headers_begin;
	gint64 message_headers_end;
	
	/* current mime-part headerblock offsets */
	gint64 headers_begin;
	gint64 headers_end;
	
	/* current header field offset */
	gint64 header_offset;
	
	GPtrArray *headers;
	
	/* header buffer */
	char *headerbuf;
	char *headerptr;
	size_t headerleft;
	
	BoundaryStack *bounds;
	
	GMimeOpenPGPState openpgp;
	short int state;
	
	unsigned short int toplevel:1;
	unsigned short int seekable:1;
	unsigned short int have_regex:1;
	unsigned short int persist_stream:1;
	unsigned short int respect_content_length:1;
	unsigned short int unused:11;
};

static const char MBOX_BOUNDARY[6] = "From ";
#define MBOX_BOUNDARY_LEN 5

static const char MMDF_BOUNDARY[6] = "\1\1\1\1";
#define MMDF_BOUNDARY_LEN 4

static void
parser_push_boundary (GMimeParser *parser, const char *boundary)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	BoundaryStack *s;
	size_t max;
	
	max = priv->bounds ? priv->bounds->boundarylenmax : 0;
	
	s = g_slice_new (BoundaryStack);
	s->parent = priv->bounds;
	priv->bounds = s;
	
	if (boundary == MBOX_BOUNDARY) {
		s->boundary = g_strdup (boundary);
		s->boundarylen = MBOX_BOUNDARY_LEN;
		s->boundarylenfinal = MBOX_BOUNDARY_LEN;
	} else if (boundary == MMDF_BOUNDARY) {
		s->boundary = g_strdup (boundary);
		s->boundarylen = MMDF_BOUNDARY_LEN;
		s->boundarylenfinal = MMDF_BOUNDARY_LEN;
	} else {
		s->boundary = g_strdup_printf ("--%s--", boundary);
		s->boundarylen = strlen (boundary) + 2;
		s->boundarylenfinal = s->boundarylen + 2;
	}
	
	s->boundarylenmax = MAX (s->boundarylenfinal, max);
}

static void
parser_pop_boundary (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	BoundaryStack *s;
	
	if (!priv->bounds) {
		d(g_warning ("boundary stack underflow"));
		return;
	}
	
	s = priv->bounds;
	priv->bounds = priv->bounds->parent;
	
	g_free (s->boundary);
	
	g_slice_free (BoundaryStack, s);
}

static const char *
parser_find_header (GMimeParser *parser, const char *name, gint64 *offset)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	Header *header;
	guint i;
	
	for (i = priv->headers->len; i > 0; i--) {
		header = priv->headers->pdata[i - 1];
		
		if (g_ascii_strcasecmp (header->name, name) != 0)
			continue;
		
		if (offset)
			*offset = header->offset;
		
		return header->raw_value;
	}
	
	return NULL;
}

static void
parser_free_headers (struct _GMimeParserPrivate *priv)
{
	Header *header;
	guint i;
	
	g_free (priv->preheader);
	priv->preheader = NULL;
	
	for (i = 0; i < priv->headers->len; i++) {
		header = priv->headers->pdata[i];
		
		g_free (header->name);
		g_free (header->raw_name);
		g_free (header->raw_value);
		g_slice_free (Header, header);
	}
	
	g_ptr_array_set_size (priv->headers, 0);
}

GType
g_mime_parser_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeParserClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_parser_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeParser),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_parser_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeParser", &info, 0);
	}
	
	return type;
}


static void
g_mime_parser_class_init (GMimeParserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_parser_finalize;
}

static void
g_mime_parser_init (GMimeParser *parser, GMimeParserClass *klass)
{
	parser->priv = g_new (struct _GMimeParserPrivate, 1);
	parser->priv->respect_content_length = FALSE;
	parser->priv->format = GMIME_FORMAT_MESSAGE;
	parser->priv->persist_stream = TRUE;
	parser->priv->have_regex = FALSE;
	parser->priv->regex = NULL;
	
	parser_init (parser, NULL);
}

static void
g_mime_parser_finalize (GObject *object)
{
	GMimeParser *parser = (GMimeParser *) object;
	
	parser_close (parser);
	
	if (parser->priv->regex)
		g_regex_unref (parser->priv->regex);
	
	g_free (parser->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
parser_init (GMimeParser *parser, GMimeStream *stream)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	gint64 offset = -1;
	
	if (stream) {
		g_object_ref (stream);
		offset = g_mime_stream_tell (stream);
	}
	
	priv->state = GMIME_PARSER_STATE_INIT;
	
	priv->stream = stream;
	
	priv->content_end = 0;
	priv->offset = offset;
	
	priv->inbuf = priv->realbuf + SCAN_HEAD;
	priv->inptr = priv->inbuf;
	priv->inend = priv->inbuf;
	
	priv->marker = g_byte_array_new ();
	priv->marker_offset = -1;
	
	priv->preheader = NULL;
	
	priv->headers = g_ptr_array_new ();
	
	priv->headerbuf = g_malloc (HEADER_INIT_SIZE);
	priv->headerleft = HEADER_INIT_SIZE - 1;
	priv->headerptr = priv->headerbuf;
	
	priv->message_headers_begin = -1;
	priv->message_headers_end = -1;
	
	priv->headers_begin = -1;
	priv->headers_end = -1;
	
	priv->header_offset = -1;
	
	priv->openpgp = GMIME_OPENPGP_NONE;
	
	priv->toplevel = FALSE;
	priv->seekable = offset != -1;
	
	priv->bounds = NULL;
}

static void
parser_close (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	
	if (priv->stream)
		g_object_unref (priv->stream);
	
	g_byte_array_free (priv->marker, TRUE);
	
	g_free (priv->headerbuf);
	
	parser_free_headers (priv);
	g_ptr_array_free (priv->headers, TRUE);
	
	while (priv->bounds)
		parser_pop_boundary (parser);
}


/**
 * g_mime_parser_new:
 *
 * Creates a new parser object.
 *
 * Returns: a new parser object.
 **/
GMimeParser *
g_mime_parser_new (void)
{
	return g_object_new (GMIME_TYPE_PARSER, NULL);
}


/**
 * g_mime_parser_new_with_stream:
 * @stream: raw message or part stream
 *
 * Creates a new parser object preset to parse @stream.
 *
 * Returns: a new parser object.
 **/
GMimeParser *
g_mime_parser_new_with_stream (GMimeStream *stream)
{
	GMimeParser *parser;
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	
	return parser;
}


/**
 * g_mime_parser_init_with_stream:
 * @parser: a #GMimeParser context
 * @stream: raw message or part stream
 *
 * Initializes @parser to use @stream.
 *
 * WARNING: Initializing a parser with a stream is comparable to
 * selling your soul (@stream) to the devil (@parser). You are
 * basically giving the parser complete control of the stream, this
 * means that you had better not touch the stream so long as the
 * parser is still using it. This means no reading, writing, seeking,
 * or resetting of the stream. Anything that will/could change the
 * current stream's offset is PROHIBITED.
 *
 * It is also recommended that you not use g_mime_stream_tell()
 * because it will not necessarily give you the current @parser offset
 * since @parser handles its own internal read-ahead buffer. Instead,
 * it is recommended that you use g_mime_parser_tell() if you have a
 * reason to need the current offset of the @parser.
 **/
void
g_mime_parser_init_with_stream (GMimeParser *parser, GMimeStream *stream)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	g_return_if_fail (GMIME_IS_STREAM (stream));
	
	parser_close (parser);
	parser_init (parser, stream);
}


/**
 * g_mime_parser_get_persist_stream:
 * @parser: a #GMimeParser context
 *
 * Gets whether or not the underlying stream is persistent.
 *
 * Returns: %TRUE if the @parser will leave the content on disk or
 * %FALSE if it will load the content into memory.
 **/
gboolean
g_mime_parser_get_persist_stream (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);
	
	return (parser->priv->persist_stream && parser->priv->seekable);
}


/**
 * g_mime_parser_set_persist_stream:
 * @parser: a #GMimeParser context
 * @persist: persist attribute
 *
 * Sets whether or not the @parser's underlying stream is persistent.
 *
 * If @persist is %TRUE, the @parser will attempt to construct
 * messages/parts whose content will remain on disk rather than being
 * loaded into memory so as to reduce memory usage. This is the default.
 *
 * If @persist is %FALSE, the @parser will always load message content
 * into memory.
 *
 * Note: This attribute only serves as a hint to the @parser. If the
 * underlying stream does not support seeking, then this attribute
 * will be ignored.
 *
 * By default, this feature is enabled if the underlying stream is seekable.
 **/
void
g_mime_parser_set_persist_stream (GMimeParser *parser, gboolean persist)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	parser->priv->persist_stream = persist;
}


/**
 * g_mime_parser_get_format:
 * @parser: a #GMimeParser context
 *
 * Gets the format that the parser is set to parse.
 *
 * Returns: the format that the parser is set to parse.
 **/
GMimeFormat
g_mime_parser_get_format (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), GMIME_FORMAT_MESSAGE);
	
	return parser->priv->format;
}


/**
 * g_mime_parser_set_format:
 * @parser: a #GMimeParser context
 * @format: a #GMimeFormat
 *
 * Sets the format that the parser should expect the stream to be in.
 **/
void
g_mime_parser_set_format (GMimeParser *parser, GMimeFormat format)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	parser->priv->format = format;
}


/**
 * g_mime_parser_get_respect_content_length:
 * @parser: a #GMimeParser context
 *
 * Gets whether or not @parser is set to use Content-Length for
 * determining the offset of the end of the message.
 *
 * Returns: whether or not @parser is set to use Content-Length for
 * determining the offset of the end of the message.
 **/
gboolean
g_mime_parser_get_respect_content_length (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);
	
	return parser->priv->respect_content_length;
}


/**
 * g_mime_parser_set_respect_content_length:
 * @parser: a #GMimeParser context
 * @respect_content_length: %TRUE if the parser should use Content-Length headers or %FALSE otherwise.
 *
 * Sets whether or not @parser should respect Content-Length headers
 * when deciding where to look for the start of the next message. Only
 * used when the parser is also set to scan for From-lines.
 *
 * Most notably useful when parsing broken Solaris mbox files (See
 * http://www.jwz.org/doc/content-length.html for details).
 *
 * By default, this feature is disabled.
 **/
void
g_mime_parser_set_respect_content_length (GMimeParser *parser, gboolean respect_content_length)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	parser->priv->respect_content_length = respect_content_length ? 1 : 0;
}


/**
 * g_mime_parser_set_header_regex: (skip)
 * @parser: a #GMimeParser context
 * @regex: regular expression
 * @header_cb: callback function
 * @user_data: user data
 *
 * Sets the regular expression pattern @regex on @parser. Whenever a
 * header matching the pattern @regex is parsed, @header_cb is called
 * with @user_data as the user_data argument.
 *
 * If @regex is %NULL, then the previously registered regex callback
 * is unregistered and no new callback is set.
 **/
void
g_mime_parser_set_header_regex (GMimeParser *parser, const char *regex,
				GMimeParserHeaderRegexFunc header_cb, gpointer user_data)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	priv = parser->priv;
	
	if (priv->regex) {
		g_regex_unref (priv->regex);
		priv->regex = NULL;
	}
	
	if (!regex || !header_cb)
		return;
	
	priv->header_cb = header_cb;
	priv->user_data = user_data;
	
	priv->regex = g_regex_new (regex, G_REGEX_RAW | G_REGEX_EXTENDED | G_REGEX_CASELESS, 0, NULL);
}


static ssize_t
parser_fill (GMimeParser *parser, size_t atleast)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	char *inbuf, *inptr, *inend;
	ssize_t nread;
	size_t inlen;
	
	inbuf = priv->inbuf;
	inptr = priv->inptr;
	inend = priv->inend;
	inlen = inend - inptr;
	
	g_assert (inptr <= inend);
	
	if (inlen > atleast)
		return inlen;
	
	/* attempt to align 'inend' with realbuf + SCAN_HEAD */
	if (inptr >= inbuf) {
		inbuf -= inlen < SCAN_HEAD ? inlen : SCAN_HEAD;
		memmove (inbuf, inptr, inlen);
		inptr = inbuf;
		inbuf += inlen;
	} else if (inptr > priv->realbuf) {
		size_t shift;
		
		shift = MIN (inptr - priv->realbuf, inend - inbuf);
		memmove (inptr - shift, inptr, inlen);
		inptr -= shift;
		inbuf = inptr + inlen;
	} else {
		/* we can't shift... */
		inbuf = inend;
	}
	
	priv->inptr = inptr;
	priv->inend = inbuf;
	inend = priv->realbuf + SCAN_HEAD + SCAN_BUF;
	
	if ((nread = g_mime_stream_read (priv->stream, inbuf, inend - inbuf)) > 0) {
		priv->offset += nread;
		priv->inend += nread;
	}
	
	return (ssize_t) (priv->inend - priv->inptr);
}


static gint64
parser_offset (struct _GMimeParserPrivate *priv, const char *inptr)
{
	if (priv->offset == -1)
		return -1;
	
	if (!inptr)
		inptr = priv->inptr;
	
	return (priv->offset - (priv->inend - inptr));
}


/**
 * g_mime_parser_tell:
 * @parser: a #GMimeParser context
 *
 * Gets the current stream offset from the parser's internal stream.
 *
 * Returns: the current stream offset from the parser's internal stream
 * or %-1 on error.
 **/
gint64
g_mime_parser_tell (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (parser->priv->stream), -1);
	
	return parser_offset (parser->priv, NULL);
}


/**
 * g_mime_parser_eos:
 * @parser: a #GMimeParser context
 *
 * Tests the end-of-stream indicator for @parser's internal stream.
 *
 * Returns: %TRUE on EOS or %FALSE otherwise.
 **/
gboolean
g_mime_parser_eos (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_val_if_fail (GMIME_IS_STREAM (parser->priv->stream), TRUE);
	
	priv = parser->priv;
	return g_mime_stream_eos (priv->stream) && priv->inptr == priv->inend;
}

static gboolean
is_mbox_marker (const char *inptr, size_t len, gboolean allow_munged)
{
	if (allow_munged && *inptr == '>') {
		inptr++;
		len--;
	}
	
	return len >= 5 && !strncmp ("From ", inptr, 5);
}

static int
parser_step_marker (GMimeParser *parser, const char *marker, size_t marker_len)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	register char *inptr;
	char *start, *inend;
	ssize_t left = 0;
	size_t len;
	
	g_byte_array_set_size (priv->marker, 0);
	
	inptr = priv->inptr;
	
	g_assert (inptr <= priv->inend);
	
	do {
	refill:
		if (parser_fill (parser, MAX (SCAN_HEAD, left)) <= left) {
			/* failed to find a From line; EOF reached */
			priv->state = GMIME_PARSER_STATE_ERROR;
			priv->inptr = priv->inend;
			return -1;
		}
		
		inptr = priv->inptr;
		inend = priv->inend;
		*inend = '\n';
		
		while (inptr < inend) {
			start = inptr;
			while (*inptr != '\n')
				inptr++;
			
			if (inptr + 1 >= inend) {
				/* we don't have enough data; if we can't get more we have to bail */
				left = (ssize_t) (inend - start);
				priv->inptr = start;
				goto refill;
			}
			
			len = (size_t) (inptr - start);
			inptr++;
			
			if (len >= marker_len && !strncmp (start, marker, marker_len)) {
				priv->marker_offset = parser_offset (priv, start);
				
				if (priv->format == GMIME_FORMAT_MBOX)
					g_byte_array_append (priv->marker, (unsigned char *) start, len);
				goto got_marker;
			}
		}
		
		priv->inptr = inptr;
		left = 0;
	} while (1);
	
 got_marker:
	
	priv->state = GMIME_PARSER_STATE_MESSAGE_HEADERS;
	
	priv->inptr = inptr;
	
	return 0;
}

static int
parser_step_from (GMimeParser *parser)
{
	return parser_step_marker (parser, MBOX_BOUNDARY, MBOX_BOUNDARY_LEN);
}

static int
parser_step_mmdf (GMimeParser *parser)
{
	return parser_step_marker (parser, MMDF_BOUNDARY, MMDF_BOUNDARY_LEN);
}

static inline size_t
next_alloc_size (size_t n)
{
	return (n + 63) & ~63;
}

static void
header_buffer_reset (struct _GMimeParserPrivate *priv)
{
	priv->headerleft += priv->headerptr - priv->headerbuf;
	priv->headerptr = priv->headerbuf;
}

static void
header_buffer_append (struct _GMimeParserPrivate *priv, const char *start, size_t len)
{
	if (priv->headerleft <= len) {
		size_t hlen, hoff;
		
		hoff = priv->headerptr - priv->headerbuf;
		hlen = next_alloc_size (hoff + len + 1);
		
		priv->headerbuf = g_realloc (priv->headerbuf, hlen);
		priv->headerptr = priv->headerbuf + hoff;
		priv->headerleft = (hlen - 1) - hoff;
	}
	
	memcpy (priv->headerptr, start, len);
	priv->headerptr += len;
	priv->headerleft -= len;
}

static void
header_buffer_parse (GMimeParser *parser, GMimeParserOptions *options)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	struct _GMimeParserPrivate *priv = parser->priv;
	gboolean blank = FALSE;
	register char *inptr;
	Header *header;
	
	if (priv->headerptr == priv->headerbuf)
		return;
	
	*priv->headerptr = ':';
	inptr = priv->headerbuf;
	
	while (*inptr != ':') {
		/* Note: blank spaces are allowed between the field name
		 * and the ':', but field names themselves are not allowed
		 * to contain spaces (or control characters). */
		if (is_blank (*inptr)) {
			blank = TRUE;
		} else if (blank || is_ctrl (*inptr)) {
			break;
		}
		
		inptr++;
	}
	
	*priv->headerptr = '\0';
	
	if (*inptr != ':') {
		/* ignore invalid headers */
		if (can_warn)
			_g_mime_parser_options_warn (options, priv->header_offset, GMIME_CRIT_INVALID_HEADER_NAME, priv->headerbuf);
		w(g_warning ("Invalid header at %lld: '%s'", (long long) priv->header_offset, priv->headerbuf));
		header_buffer_reset (priv);
		return;
	}
	
	header = g_slice_new (Header);
	g_ptr_array_add (priv->headers, header);
	
	header->raw_name = g_strndup (priv->headerbuf, (size_t) (inptr - priv->headerbuf));
	header->raw_value = g_strdup (inptr + 1);
	header->offset = priv->header_offset;
	
	/* now walk backwards over lwsp characters */
	while (inptr > priv->headerbuf && is_blank (inptr[-1]))
		inptr--;
	
	header->name = g_strndup (priv->headerbuf, (size_t) (inptr - priv->headerbuf));
	
	header_buffer_reset (priv);
	
	if (priv->regex && g_regex_match (priv->regex, header->name, 0, NULL))
		priv->header_cb (parser, header->name, header->raw_value,
				 header->offset, priv->user_data);
	
	if (can_warn && (g_mime_utils_text_is_8bit ((const unsigned char *) header->name, strlen (header->name)) || !g_utf8_validate (header->raw_value, -1, NULL)))
		_g_mime_parser_options_warn (options, header->offset, GMIME_WARN_UNENCODED_8BIT_HEADER, header->name);
}

#if 0
enum {
	SUBJECT = 1 << 0,
	FROM    = 1 << 1,
	DATE    = 1 << 2,
	TO      = 1 << 3,
	CC      = 1 << 4
};

static gboolean
has_message_headers (GPtrArray *headers)
{
	unsigned int found = 0;
	Header *header;
	guint i;
	
	for (i = 0; i < headers->len; i++) {
		header = headers->pdata[i];
		
		if (!g_ascii_strcasecmp (header->name, "Subject"))
			found |= SUBJECT;
		else if (!g_ascii_strcasecmp (header->name, "From"))
			found |= FROM;
		else if (!g_ascii_strcasecmp (header->name, "Date"))
			found |= DATE;
		else if (!g_ascii_strcasecmp (header->name, "To"))
			found |= TO;
		else if (!g_ascii_strcasecmp (header->name, "Cc"))
			found |= CC;
	}
	
	return found != 0;
}

static gboolean
has_content_headers (GPtrArray *headers)
{
	Header *header;
	guint i;
	
	for (i = 0; i < headers->len; i++) {
		header = headers->pdata[i];
		
		if (!g_ascii_strcasecmp (header->name, "Content-Type"))
			return TRUE;
	}
	
	return FALSE;
}
#endif

static void
warn_invalid_header (GMimeParser *parser, GMimeParserOptions *options, const char *start, const char *inptr, const char *inend)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	const char *eoln = inptr;
	char *header;
	
	while (eoln < inend && *eoln != '\r' && *eoln != '\n')
		eoln++;
	
	header = g_strndup (start, eoln - start);
	_g_mime_parser_options_warn (options, priv->header_offset, GMIME_CRIT_INVALID_HEADER_NAME, header);
	g_free (header);
}

struct _StepHeadersState {
	gboolean scanning_field_name;
	gboolean check_folded;
	gboolean midline;
	gboolean blank;
	gboolean valid;
	ssize_t left;
};

static gboolean
step_headers (GMimeParser *parser, struct _StepHeadersState *state, GMimeParserOptions *options)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	struct _GMimeParserPrivate *priv = parser->priv;
	register char *inptr = priv->inptr;
	register char *inend = priv->inend;
	gboolean need_input = FALSE;
	gboolean eoln;
	size_t len;
	
	*inend = '\n';
	
	while (inptr < inend) {
		char *start = inptr;
		
		/* if we are scanning a new line, check for a folded header */
		if (!state->midline && state->check_folded && !is_blank (*inptr)) {
			header_buffer_parse (parser, options);
			
			/* set the next header's stream offset */
			priv->header_offset = parser_offset (priv, inptr);
			
			state->scanning_field_name = TRUE;
			state->check_folded = FALSE;
			state->blank = FALSE;
			state->valid = TRUE;
		}
		
		eoln = inptr[0] == '\n' || (inptr[0] == '\r' && inptr[1] == '\n');
		if (state->scanning_field_name && !eoln) {
			/* scan and validate the field name */
			if (*inptr != ':') {
				*inend = ':';
				
				while (*inptr != ':') {
					/* Note: blank spaces are allowed between the field name
					 * and the ':', but field names themselves are not allowed
					 * to contain spaces (or control characters). */
					if (is_blank (*inptr)) {
						state->blank = TRUE;
					} else if (state->blank || is_ctrl (*inptr)) {
						state->valid = FALSE;
						break;
					}
					
					inptr++;
				}
				
				if (inptr == inend) {
					/* we don't have enough input data; restore state back to the beginning of the line */
					state->left = (size_t) (inend - start);
					priv->inptr = start;
					need_input = TRUE;
					break;
				}
				
				*inend = '\n';
			} else {
				state->valid = FALSE;
			}
			
			if (!state->valid) {
				gint64 offset = parser_offset (priv, inptr);
				
				len = (size_t) (inptr - start);
				
				if (priv->format == GMIME_FORMAT_MBOX && offset >= priv->content_end && len >= 5 && is_mbox_marker (start, len, FALSE)) {
					/* we've found the start of the next message... */
					priv->headers_end = parser_offset (priv, start);
					priv->state = GMIME_PARSER_STATE_COMPLETE;
					header_buffer_reset (priv);
					priv->inptr = start;
					return FALSE;
				}
				
				if (priv->headers->len == 0) {
					if (priv->state == GMIME_PARSER_STATE_MESSAGE_HEADERS) {
						/* ignore From-lines that might appear at the start of a message */
						if (priv->toplevel && (len < 5 || !is_mbox_marker (start, len, TRUE))) {
							/* not a From-line... */
							if (can_warn)
								warn_invalid_header (parser, options, start, inptr, inend);
							priv->state = GMIME_PARSER_STATE_ERROR;
							header_buffer_reset (priv);
							priv->inptr = start;
							return FALSE;
						}
					} else if (priv->toplevel) {
						if (can_warn)
							warn_invalid_header (parser, options, start, inptr, inend);
						priv->state = GMIME_PARSER_STATE_ERROR;
						header_buffer_reset (priv);
						priv->inptr = start;
						return FALSE;
					} else if (can_warn) {
						warn_invalid_header (parser, options, start, inptr, inend);
					}
				}
			}
		}
		
		state->scanning_field_name = FALSE;
		
		while (*inptr != '\n')
			inptr++;
		
		if (inptr == inend) {
			/* we didn't manage to slurp up a full line, save what we have and refill our input buffer */
			len = (size_t) (inptr - start);
			
			/* Note: if the last byte we got was a '\r', rewind a byte */
			if (inptr > start && inptr[-1] == '\r') {
				inptr--;
				len--;
			}
			
			if (len > 0) {
				header_buffer_append (priv, start, len);
				state->midline = TRUE;
			}
			
			state->left = (ssize_t) (inend - inptr);
			priv->inptr = inptr;
			need_input = TRUE;
			break;
		}
		
		/* check to see if we've reached the end of the headers */
		if (!state->midline && (start[0] == '\n' || (start[0] == '\r' && start[1] == '\n'))) {
			priv->headers_end = parser_offset (priv, start);
			priv->state = GMIME_PARSER_STATE_CONTENT;
			header_buffer_parse (parser, options);
			priv->inptr = inptr + 1;
			return FALSE;
		}
		
		len = (inptr + 1) - start;
		
		if (!state->valid && priv->headers->len == 0) {
			if (len > 0 && priv->preheader == NULL) {
				if (inptr[-1] == '\r')
					len--;
				len--;
				
				priv->preheader = g_strndup (start, len);
			}
			
			state->scanning_field_name = TRUE;
			state->check_folded = FALSE;
			state->blank = FALSE;
			state->valid = TRUE;
		} else {
			header_buffer_append (priv, start, len);
			state->check_folded = TRUE;
		}
		
		state->midline = FALSE;
		inptr++;
	}
	
	if (!need_input) {
		state->left = (ssize_t) (inend - inptr);
		priv->inptr = inptr;
	}
	
	return TRUE;
}

static void
parser_step_headers (GMimeParser *parser, GMimeParserOptions *options)
{
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	struct _GMimeParserPrivate *priv = parser->priv;
	struct _StepHeadersState state;
	ssize_t available;
	
	state.scanning_field_name = TRUE;
	state.check_folded = FALSE;
	state.midline = FALSE;
	state.blank = FALSE;
	state.valid = TRUE;
	state.left = 0;
	
	parser_free_headers (priv);
	priv->headers_begin = parser_offset (priv, NULL);
	priv->header_offset = priv->headers_begin;
	
	parser_fill (parser, SCAN_HEAD/*, 0*/);
	
	do {
		if (!step_headers (parser, &state, options))
			return;
		
		available = parser_fill (parser, state.left + 1/*, 0*/);
		
		if (available == state.left) {
			/* EOF reached before we reached the end of the headers... */
			if (state.scanning_field_name && state.left > 0) {
				/* EOF reached right in the middle of a header field name. Throw an error.
				 *
				 * See private email from Feb 8, 2018 which contained a sample message w/o
				 * any breaks between the header and message body. The file also did not
				 * end with a newline sequence. */
				priv->state = GMIME_PARSER_STATE_ERROR;
			} else {
				/* EOF reached somewhere in the middle of the value.
				 *
				 * Append whatever data we've got left and pretend we found the end
				 * of the header value (and the header block).
				 *
				 * For more details, see https://github.com/jstedfast/MimeKit/pull/51
				 * and https://github.com/jstedfast/MimeKit/issues/348 */
				if (state.left > 0) {
					header_buffer_append (priv, priv->inptr, state.left);
					priv->inptr = priv->inend;
				}
				
				header_buffer_parse (parser, options);
				
				priv->state = GMIME_PARSER_STATE_CONTENT;
			}
			
			if (can_warn)
				_g_mime_parser_options_warn (options, -1, GMIME_WARN_TRUNCATED_MESSAGE, NULL);
			
			return;
		}
	} while (TRUE);
}

static void
content_type_destroy (ContentType *content_type)
{
	g_free (content_type->subtype);
	g_free (content_type->type);
	
	g_slice_free (ContentType, content_type);
}

static gboolean
content_type_is_type (ContentType *content_type, const char *type, const char *subtype)
{
	if (!strcmp (type, "*") || !g_ascii_strcasecmp (content_type->type, type)) {
		if (!strcmp (subtype, "*")) {
			/* special case */
			return TRUE;
		}
		
		if (!g_ascii_strcasecmp (content_type->subtype, subtype))
			return TRUE;
	}
	
	return FALSE;
}

static ContentType *
parser_content_type (GMimeParser *parser, GMimeContentType *parent)
{
	ContentType *content_type;
	const char *value;
	
	content_type = g_slice_new (ContentType);
	
	if (!(value = parser_find_header (parser, "Content-Type", NULL)) ||
	    !g_mime_parse_content_type (&value, &content_type->type, &content_type->subtype)) {
		if (parent != NULL && g_mime_content_type_is_type (parent, "multipart", "digest")) {
			content_type->type = g_strdup ("message");
			content_type->subtype = g_strdup ("rfc822");
		} else {
			content_type->type = g_strdup ("text");
			content_type->subtype = g_strdup ("plain");
		}
	}
	
	content_type->exists = value != NULL;
	
	return content_type;
}

static int
parser_skip_line (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	register char *inptr;
	char *inend;
	int rv = 0;
	
	do {
		inptr = priv->inptr;
		inend = priv->inend;
		*inend = '\n';
		
		while (*inptr != '\n')
			inptr++;
		
		if (inptr < inend)
			break;
		
		priv->inptr = inptr;
		
		if (parser_fill (parser, SCAN_HEAD) <= 0) {
			inptr = priv->inptr;
			rv = -1;
			break;
		}
	} while (1);
	
	priv->inptr = MIN (inptr + 1, priv->inend);
	
	return rv;
}

static int
parser_step (GMimeParser *parser, GMimeParserOptions *options)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	
	switch (priv->state) {
	case GMIME_PARSER_STATE_ERROR:
		break;
	case GMIME_PARSER_STATE_INIT:
		priv->message_headers_begin = -1;
		priv->message_headers_end = -1;
		if (priv->format == GMIME_FORMAT_MBOX)
			priv->state = GMIME_PARSER_STATE_FROM;
		else if (priv->format == GMIME_FORMAT_MMDF)
			priv->state = GMIME_PARSER_STATE_AAAA;
		else
			priv->state = GMIME_PARSER_STATE_MESSAGE_HEADERS;
		break;
	case GMIME_PARSER_STATE_FROM:
		priv->message_headers_begin = -1;
		priv->message_headers_end = -1;
		parser_step_from (parser);
		break;
	case GMIME_PARSER_STATE_AAAA:
		priv->message_headers_begin = -1;
		priv->message_headers_end = -1;
		parser_step_mmdf (parser);
		break;
	case GMIME_PARSER_STATE_MESSAGE_HEADERS:
	case GMIME_PARSER_STATE_HEADERS:
		parser_step_headers (parser, options);
		priv->toplevel = FALSE;
		
		if (priv->message_headers_begin == -1) {
			priv->message_headers_begin = priv->headers_begin;
			priv->message_headers_end = priv->headers_end;
		}
		break;
	case GMIME_PARSER_STATE_HEADERS_END:
		if (parser_skip_line (parser) == -1)
			priv->state = GMIME_PARSER_STATE_ERROR;
		else
			priv->state = GMIME_PARSER_STATE_CONTENT;
		break;
	case GMIME_PARSER_STATE_CONTENT:
		break;
	case GMIME_PARSER_STATE_COMPLETE:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	return priv->state;
}

#define possible_boundary(marker, mlen, start, len)				        \
                         ((marker && len >= mlen && !strncmp (start, marker, mlen)) ||  \
			  (len >= 2 && (start[0] == '-' && start[1] == '-')))

static gboolean
is_boundary (struct _GMimeParserPrivate *priv, const char *text, size_t len, const char *boundary, size_t boundary_len)
{
	const char *inptr = text + boundary_len;
	const char *inend = text + len;
	
	if (boundary_len > len)
		return FALSE;
	
	/* make sure that the text matches the boundary */
	if (strncmp (text, boundary, boundary_len) != 0)
		return FALSE;
	
	if (priv->format == GMIME_FORMAT_MBOX) {
		if (!strncmp (text, MBOX_BOUNDARY, MBOX_BOUNDARY_LEN))
			return TRUE;
	} else if (priv->format == GMIME_FORMAT_MMDF) {
		if (!strncmp (text, MMDF_BOUNDARY, MMDF_BOUNDARY_LEN))
			return TRUE;
	}
	
	/* the boundary may be optionally followed by linear whitespace */
	while (inptr < inend) {
		if (!is_lwsp (*inptr))
			return FALSE;
		
		inptr++;
	}
	
	return TRUE;
}

static BoundaryType
check_boundary (struct _GMimeParserPrivate *priv, const char *start, size_t len)
{
	gint64 offset = parser_offset (priv, start);
	BoundaryStack *bounds;
	const char *marker;
	size_t mlen;
	guint i;
	
	switch (priv->format) {
	case GMIME_FORMAT_MBOX: marker = MBOX_BOUNDARY; mlen = MBOX_BOUNDARY_LEN; break;
	case GMIME_FORMAT_MMDF: marker = MMDF_BOUNDARY; mlen = MMDF_BOUNDARY_LEN; break;
	default: marker = NULL; mlen = 0; break;
	}
	
	if (len > 0 && start[len - 1] == '\r')
		len--;
	
	if (!possible_boundary (marker, mlen, start, len))
		return BOUNDARY_NONE;
	
	d(printf ("checking boundary '%.*s'\n", len, start));
	
	bounds = priv->bounds;
	while (bounds != NULL && (priv->content_end > 0 ? bounds->parent != NULL : TRUE)) {
		if (is_boundary (priv, start, len, bounds->boundary, bounds->boundarylenfinal)) {
			d(printf ("found end boundary\n"));
			return bounds == priv->bounds ? BOUNDARY_IMMEDIATE_END : BOUNDARY_PARENT_END;
		}
		
		if (is_boundary (priv, start, len, bounds->boundary, bounds->boundarylen)) {
			d(printf ("found boundary\n"));
			return bounds == priv->bounds ? BOUNDARY_IMMEDIATE : BOUNDARY_PARENT;
		}
		
		bounds = bounds->parent;
	}

	if (priv->content_end > 0 && bounds != NULL) {
		/* now it is time to check the mbox From-marker for the Content-Length case */
		if (offset >= priv->content_end && is_boundary (priv, start, len, bounds->boundary, bounds->boundarylenfinal)) {
			d(printf ("found end of content\n"));
			return BOUNDARY_IMMEDIATE_END;
		}
	}
	
	d(printf ("'%.*s' not a boundary\n", len, start));
	
	if (!strncmp (start, "--", 2)) {
		start += 2;
		len -= 2;
		
		/* check for OpenPGP markers... */
		for (i = 0; i < G_N_ELEMENTS (g_mime_openpgp_markers); i++) {
			const char *pgp_marker = g_mime_openpgp_markers[i].marker + 2;
			GMimeOpenPGPState state = g_mime_openpgp_markers[i].before;
			size_t n = g_mime_openpgp_markers[i].len - 2;
			
			if (len == n && priv->openpgp == state && !strncmp (pgp_marker, start, len))
				priv->openpgp = g_mime_openpgp_markers[i].after;
		}
	}
	
	return BOUNDARY_NONE;
}

static gboolean
found_immediate_boundary (struct _GMimeParserPrivate *priv, gboolean end)
{
	BoundaryStack *s = priv->bounds;
	size_t boundary_len = end ? s->boundarylenfinal : s->boundarylen;
	register char *inptr = priv->inptr;
	char *inend = priv->inend;
	
	/* Note: see optimization comment [1] */
	*inend = '\n';
	while (*inptr != '\n')
		inptr++;
	
	return is_boundary (priv, priv->inptr, inptr - priv->inptr, s->boundary, boundary_len);
}

/* Optimization Notes:
 *
 * 1. By making the priv->realbuf char array 1 extra char longer, we
 * can safely set '*inend' to '\n' and not fear an ABW. Setting *inend
 * to '\n' means that we can eliminate having to check that inptr <
 * inend every trip through our inner while-loop. This cuts the number
 * of instructions down from ~7 to ~4, assuming the compiler does its
 * job correctly ;-)
 **/


/* we add 2 for \r\n */
#define MAX_BOUNDARY_LEN(bounds) (bounds ? bounds->boundarylenmax + 2 : 0)

static BoundaryType
parser_scan_content (GMimeParser *parser, GMimeStream *content, gboolean *empty)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	BoundaryType found = BOUNDARY_NONE;
	char *aligned, *start, *inend;
	register unsigned int *dword;
	gboolean midline = FALSE;
	register char *inptr;
	unsigned int mask;
	size_t nleft, len;
	size_t atleast;
	gint64 pos;
	char c;
	
	d(printf ("scan-content\n"));
	
	priv->openpgp = GMIME_OPENPGP_NONE;
	
	g_assert (priv->inptr <= priv->inend);
	
	start = inptr = priv->inptr;
	
	/* figure out minimum amount of data we need */
	atleast = MAX (SCAN_HEAD, MAX_BOUNDARY_LEN (priv->bounds));
	
	do {
	refill:
		nleft = priv->inend - inptr;
		if (parser_fill (parser, atleast) <= 0) {
			start = priv->inptr;
			found = BOUNDARY_EOS;
			break;
		}
		
		inptr = priv->inptr;
		inend = priv->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		len = (size_t) (inend - inptr);
		if (midline && len == nleft)
			found = BOUNDARY_EOS;
		
		midline = FALSE;
		
		while (inptr < inend) {
			aligned = (char *) (((size_t) (inptr + 3)) & ~3);
			start = inptr;
			
			/* Note: see optimization comment [1] */
			c = *aligned;
			*aligned = '\n';
			
			while (*inptr != '\n')
				inptr++;
			
			*aligned = c;
			
			if (inptr == aligned && c != '\n') {
				dword = (unsigned int *) inptr;
				
				do {
					mask = *dword++ ^ 0x0A0A0A0AU;
					mask = ((mask - 0x01010101U) & (~mask & 0x80808080U));
				} while (mask == 0);
				
				inptr = (char *) (dword - 1);
				while (*inptr != '\n')
					inptr++;
			}
			
			len = (size_t) (inptr - start);
			
			if (inptr < inend) {
				if ((found = check_boundary (priv, start, len)))
					goto boundary;
				
				inptr++;
				len++;
			} else {
				/* didn't find an end-of-line */
				midline = TRUE;
				
				if (!found) {
					/* not enough to tell if we found a boundary */
					priv->inptr = start;
					inptr = start;
					goto refill;
				}
				
				/* check for a boundary not ending in a \n (EOF) */
				if ((found = check_boundary (priv, start, len)))
					goto boundary;
			}
			
			g_mime_stream_write (content, start, len);
		}
		
		priv->inptr = inptr;
	} while (!found);
	
 boundary:
	
	/* don't chew up the boundary */
	priv->inptr = start;
	
	pos = g_mime_stream_tell (content);
	*empty = pos == 0;
	
	if (found != BOUNDARY_EOS && pos > 0) {
		/* the last \r\n belongs to the boundary */
		if (inptr[-1] == '\r')
			g_mime_stream_seek (content, -2, GMIME_STREAM_SEEK_CUR);
		else
			g_mime_stream_seek (content, -1, GMIME_STREAM_SEEK_CUR);
	}
	
	return found;
}

static void
parser_scan_mime_part_content (GMimeParser *parser, GMimePart *mime_part, BoundaryType *found)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	GMimeContentType *content_type;
	GMimeContentEncoding encoding;
	GMimeDataWrapper *content;
	GMimeStream *stream;
	GByteArray *buffer;
	gint64 start, len;
	gboolean empty;
	
	g_assert (priv->state >= GMIME_PARSER_STATE_HEADERS_END);
	
	if (priv->persist_stream && priv->seekable) {
		stream = g_mime_stream_null_new ();
		start = parser_offset (priv, NULL);
	} else {
		stream = g_mime_stream_mem_new ();
		start = 0;
	}
	
	*found = parser_scan_content (parser, stream, &empty);
	len = g_mime_stream_tell (stream);
	
	if (priv->persist_stream && priv->seekable) {
		g_object_unref (stream);
		
		stream = g_mime_stream_substream (priv->stream, start, start + len);
	} else {
		buffer = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
		g_byte_array_set_size (buffer, (guint) len);
		g_mime_stream_reset (stream);
	}
	
	encoding = g_mime_part_get_content_encoding (mime_part);
	content = g_mime_data_wrapper_new_with_stream (stream, encoding);
	g_object_unref (stream);
	
	g_mime_part_set_content (mime_part, content);
	g_object_unref (content);
	
	content_type = g_mime_object_get_content_type ((GMimeObject *) mime_part);
	
	switch (priv->openpgp) {
	case GMIME_OPENPGP_END_PGP_SIGNATURE:
		g_mime_part_set_openpgp_data (mime_part, GMIME_OPENPGP_DATA_SIGNED);
		break;
	case GMIME_OPENPGP_END_PGP_MESSAGE:
		g_mime_part_set_openpgp_data (mime_part, GMIME_OPENPGP_DATA_ENCRYPTED);
		break;
	case GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK:
		g_mime_part_set_openpgp_data (mime_part, GMIME_OPENPGP_DATA_PUBLIC_KEY);
		break;
	case GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK:
		g_mime_part_set_openpgp_data (mime_part, GMIME_OPENPGP_DATA_PRIVATE_KEY);
		break;
	default:
		if (encoding != GMIME_CONTENT_ENCODING_BASE64 && encoding != GMIME_CONTENT_ENCODING_UUENCODE) {
			/* If the Content-Transfer-Encoding is not obfuscated by base64 or uuencode,
			   then we can safely assume that no OpenPGP blocks exist. */
			g_mime_part_set_openpgp_data (mime_part, GMIME_OPENPGP_DATA_NONE);
		}
		break;
	}
}

static void
check_header_conflict (GMimeParserOptions *options, GMimeObject *object, const Header *header)
{
	const GMimeHeader *existing;

	if ((existing = g_mime_header_list_get_header (object->headers, header->name)) != NULL) {
		if (strcmp (existing->raw_value, header->raw_value) != 0)
			_g_mime_parser_options_warn (options, header->offset, GMIME_CRIT_CONFLICTING_HEADER, header->name);
		else
			_g_mime_parser_options_warn (options, header->offset, GMIME_WARN_DUPLICATED_HEADER, header->name);
	}
}

static int
compare_header (const void *a, const void *b)
{
	return g_ascii_strcasecmp ((const gchar *) a, * (const gchar **) b);
}

/* headers which may exist only once according to RFC 5322, Sect. 3.6 (keep the list sorted) */
static const char *rfc5322_single_hdr[] = {
	"bcc", "cc", "date", "from", "in-reply-to", "message-id", "references", "reply-to", "sender", "subject", "to"
};

static void
check_repeated_header (GMimeParserOptions *options, GMimeObject *object, const Header *header)
{
	if (bsearch (header->name, rfc5322_single_hdr, G_N_ELEMENTS (rfc5322_single_hdr), sizeof (char *), compare_header))
		check_header_conflict (options, object, header);
}

static void
parser_scan_message_part (GMimeParser *parser, GMimeParserOptions *options, GMimeMessagePart *mpart, BoundaryType *found, int depth)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	GMimeMessage *message;
	GMimeObject *object;
	gboolean can_warn;
	Header *header;
	guint i;
	
	g_assert (priv->state == GMIME_PARSER_STATE_CONTENT);
	
	if (priv->bounds != NULL) {
		/* Check for the possibility of an empty message/rfc822 part. */
		register char *inptr;
		size_t atleast;
		char *inend;
		
		/* figure out minimum amount of data we need */
		atleast = MAX (SCAN_HEAD, MAX_BOUNDARY_LEN (priv->bounds));
		
		if (parser_fill (parser, atleast) <= 0) {
			*found = BOUNDARY_EOS;
			return;
		}
		
		inptr = priv->inptr;
		inend = priv->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		while (*inptr != '\n')
			inptr++;
		
		*found = check_boundary (priv, priv->inptr, inptr - priv->inptr);
		switch (*found) {
		case BOUNDARY_IMMEDIATE_END:
		case BOUNDARY_IMMEDIATE:
		case BOUNDARY_PARENT:
			return;
		case BOUNDARY_PARENT_END:
			/* ignore "From " boundaries, boken mailers tend to include these lines... */
			if (strncmp (priv->inptr, "From ", 5) != 0)
				return;
			break;
		case BOUNDARY_NONE:
		case BOUNDARY_EOS:
			break;
		}
	}
	
	/* get the headers */
	priv->state = GMIME_PARSER_STATE_HEADERS;
	if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR) {
		*found = BOUNDARY_EOS;
		return;
	}
	
	message = g_mime_message_new (FALSE);
	((GMimeObject *) message)->ensure_newline = FALSE;
	_g_mime_header_list_set_options (((GMimeObject *) message)->headers, options);
	message->marker = priv->preheader;
	priv->preheader = NULL;
	
	can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	for (i = 0; i < priv->headers->len; i++) {
		header = priv->headers->pdata[i];
		
		if (g_ascii_strncasecmp (header->name, "Content-", 8) != 0) {
			if (can_warn)
				check_repeated_header (options, (GMimeObject *) message, header);
			_g_mime_object_append_header ((GMimeObject *) message, header->name, header->raw_name,
						      header->raw_value, header->offset);
		}
	}
	
	content_type = parser_content_type (parser, NULL);
	if (content_type_is_type (content_type, "multipart", "*"))
		object = parser_construct_multipart (parser, options, content_type, TRUE, found, depth + 1);
	else
		object = parser_construct_leaf_part (parser, options, content_type, TRUE, found, depth + 1);
	
	content_type_destroy (content_type);
	message->mime_part = object;
	
	g_mime_message_part_set_message (mpart, message);
	g_object_unref (message);
}

static gboolean
is_rfc822 (const char *subtype)
{
	if (!g_ascii_strcasecmp (subtype, "rfc2822"))
		return TRUE;
	if (!g_ascii_strcasecmp (subtype, "rfc822"))
		return TRUE;
	if (!g_ascii_strcasecmp (subtype, "global"))
		return TRUE;
	if (!g_ascii_strcasecmp (subtype, "news"))
		return TRUE;
	return FALSE;
}

static GMimeObject *
parser_construct_leaf_part (GMimeParser *parser, GMimeParserOptions *options, ContentType *content_type, gboolean toplevel, BoundaryType *found, int depth)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	const char *subtype = content_type->subtype;
	const char *type = content_type->type;
	GMimeObject *object;
	Header *header;
	guint i;
	
	g_assert (priv->state >= GMIME_PARSER_STATE_HEADERS_END);
	
	if (!g_ascii_strcasecmp (type, "message") && is_rfc822 (subtype)) {
		gboolean is_encoded = FALSE;
		
		if (depth >= MAX_LEVEL) {
			/* The maximum MIME nesting level has been exceeded. Treat this message/rfc822
			 * part as if it was a leaf-node MIME part (i.e. don't recursively parse the
			 * message content). */
			_g_mime_parser_options_warn (options, priv->headers_begin, GMIME_CRIT_NESTING_OVERFLOW, NULL);
			w(g_warning ("maximum nesting level exceeded"));
			is_encoded = TRUE;
		}
		
		for (i = 0; i < priv->headers->len && !is_encoded; i++) {
			header = priv->headers->pdata[i];
			
			if (g_ascii_strcasecmp (header->name, "Content-Transfer-Encoding") != 0)
				continue;
			
			switch (g_mime_content_encoding_from_string (header->raw_value)) {
			case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
			case GMIME_CONTENT_ENCODING_UUENCODE:
			case GMIME_CONTENT_ENCODING_BASE64:
				is_encoded = TRUE;
				break;
			default:
				break;
			}
			
			break;
		}
		
		if (is_encoded) {
			subtype = "octet-stream";
			type = "application";
		}
	}
	
	object = g_mime_object_new_type (options, type, subtype);
	
	if (!content_type->exists) {
		GMimeContentType *mime_type;
		
		mime_type = g_mime_content_type_new (content_type->type, content_type->subtype);
		_g_mime_object_set_content_type (object, mime_type);
		g_object_unref (mime_type);
	}
	
	for (i = 0; i < priv->headers->len; i++) {
		header = priv->headers->pdata[i];
		
		if (!toplevel || !g_ascii_strncasecmp (header->name, "Content-", 8)) {
			check_header_conflict (options, object, header);
			_g_mime_object_append_header (object, header->name, header->raw_name,
						      header->raw_value, header->offset);
		}
	}
	
	parser_free_headers (priv);
	
	if (priv->state == GMIME_PARSER_STATE_HEADERS_END) {
		/* skip empty line after headers */
		if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR) {
			*found = BOUNDARY_EOS;
			return object;
		}
	}
	
	if (GMIME_IS_MESSAGE_PART (object))
		parser_scan_message_part (parser, options, (GMimeMessagePart *) object, found, depth + 1);
	else
		parser_scan_mime_part_content (parser, (GMimePart *) object, found);

	return object;
}

static void
crlf2lf (char *in)
{
	register char *inptr = in;
	register char *outptr;
	
	while (*inptr != '\0' && !(inptr[0] == '\r' && inptr[1] == '\n'))
		inptr++;
	
	if (*inptr == '\0')
		return;
	
	outptr = inptr++;
	
	while (*inptr != '\0') {
		while (*inptr != '\0' && !(inptr[0] == '\r' && inptr[1] == '\n'))
			*outptr++ = *inptr++;
		
		if (*inptr == '\r')
			inptr++;
	}
	
	*outptr = '\0';
}

static BoundaryType
parser_scan_multipart_face (GMimeParser *parser, GMimeMultipart *multipart, gboolean prologue)
{
	GMimeStream *stream;
	BoundaryType found;
	GByteArray *buffer;
	gboolean empty;
	gint64 len;
	char *face;
	
	stream = g_mime_stream_mem_new ();
	found = parser_scan_content (parser, stream, &empty);
	
	if (!empty) {
		buffer = g_mime_stream_mem_get_byte_array ((GMimeStreamMem *) stream);
		len = g_mime_stream_tell (stream);
		g_byte_array_set_size (buffer, len + 1);
		buffer->data[buffer->len - 1] = '\0';
		face = (char *) buffer->data;
		crlf2lf (face);
		
		if (prologue)
			g_mime_multipart_set_prologue (multipart, face);
		else
			g_mime_multipart_set_epilogue (multipart, face);
	}
	
	g_object_unref (stream);
	
	return found;
}

#define parser_scan_multipart_prologue(parser, multipart) parser_scan_multipart_face (parser, multipart, TRUE)
#define parser_scan_multipart_epilogue(parser, multipart) parser_scan_multipart_face (parser, multipart, FALSE)

static BoundaryType
parser_scan_multipart_subparts (GMimeParser *parser, GMimeParserOptions *options, GMimeMultipart *multipart, int depth)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	GMimeObject *subpart;
	BoundaryType found;
	
	do {
		/* skip over the boundary marker */
		if (parser_skip_line (parser) == -1) {
			found = BOUNDARY_EOS;
			break;
		}
		
		/* get the headers */
		priv->state = GMIME_PARSER_STATE_HEADERS;
		if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR) {
			found = BOUNDARY_EOS;
			break;
		}
		
		if (priv->state == GMIME_PARSER_STATE_COMPLETE && priv->headers->len == 0) {
			found = BOUNDARY_IMMEDIATE_END;
			break;
		}
		
		content_type = parser_content_type (parser, ((GMimeObject *) multipart)->content_type);
		if (content_type_is_type (content_type, "multipart", "*"))
			subpart = parser_construct_multipart (parser, options, content_type, FALSE, &found, depth + 1);
		else
			subpart = parser_construct_leaf_part (parser, options, content_type, FALSE, &found, depth + 1);
		
		g_mime_multipart_add (multipart, subpart);
		content_type_destroy (content_type);
		g_object_unref (subpart);
	} while (found == BOUNDARY_IMMEDIATE);
	
	return found;
}

static GMimeObject *
parser_construct_multipart (GMimeParser *parser, GMimeParserOptions *options, ContentType *content_type, gboolean toplevel, BoundaryType *found, int depth)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	GMimeMultipart *multipart;
	gint64 ctype_offset = -1;
	const char *boundary;
	GMimeObject *object;
	Header *header;
	guint i;
	
	g_assert (priv->state >= GMIME_PARSER_STATE_HEADERS_END);
	
	object = g_mime_object_new_type (options, content_type->type, content_type->subtype);
	
	for (i = 0; i < priv->headers->len; i++) {
		header = priv->headers->pdata[i];
		
		if (!toplevel || !g_ascii_strncasecmp (header->name, "Content-", 8)) {
			check_header_conflict (options, object, header);
			
			if (!g_ascii_strcasecmp (header->name, "Content-Type"))
				ctype_offset = header->offset;
			
			_g_mime_object_append_header (object, header->name, header->raw_name,
						      header->raw_value, header->offset);
		}
	}
	
	parser_free_headers (priv);
	
	multipart = (GMimeMultipart *) object;
	
	if (priv->state == GMIME_PARSER_STATE_HEADERS_END) {
		/* skip empty line after headers */
		if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR) {
			*found = BOUNDARY_EOS;
			return object;
		}
	}
	
	if ((boundary = g_mime_object_get_content_type_parameter (object, "boundary")) && depth < MAX_LEVEL) {
		parser_push_boundary (parser, boundary);
		
		*found = parser_scan_multipart_prologue (parser, multipart);
		
		if (*found == BOUNDARY_IMMEDIATE)
			*found = parser_scan_multipart_subparts (parser, options, multipart, depth);
		
		if (*found == BOUNDARY_IMMEDIATE_END) {
			/* eat end boundary */
			multipart->write_end_boundary = TRUE;
			parser_skip_line (parser);
			parser_pop_boundary (parser);
			*found = parser_scan_multipart_epilogue (parser, multipart);
			return object;
		}
		
		if ((*found == BOUNDARY_PARENT) || (*found == BOUNDARY_PARENT_END))
			_g_mime_parser_options_warn (options, ctype_offset, GMIME_WARN_MALFORMED_MULTIPART, content_type->subtype);
		
		if (*found == BOUNDARY_EOS)
			_g_mime_parser_options_warn (options, -1, GMIME_WARN_TRUNCATED_MESSAGE, NULL);
		
		multipart->write_end_boundary = FALSE;
		parser_pop_boundary (parser);
		
		if (*found == BOUNDARY_PARENT_END && found_immediate_boundary (priv, TRUE))
			*found = BOUNDARY_IMMEDIATE_END;
		else if (*found == BOUNDARY_PARENT && found_immediate_boundary (priv, FALSE))
			*found = BOUNDARY_IMMEDIATE;
	} else {
		if (depth >= MAX_LEVEL) {
			_g_mime_parser_options_warn (options, priv->headers_begin, GMIME_CRIT_NESTING_OVERFLOW, NULL);
			w(g_warning ("maximum nesting level exceeded @ boundary = %s", boundary));
		} else {
			_g_mime_parser_options_warn (options, ctype_offset, GMIME_CRIT_MULTIPART_WITHOUT_BOUNDARY, content_type->subtype);
			w(g_warning ("multipart without boundary encountered"));
		}
		
		/* this will scan everything into the prologue */
		*found = parser_scan_multipart_prologue (parser, multipart);
	}
	
	return object;
}

static GMimeObject *
parser_construct_part (GMimeParser *parser, GMimeParserOptions *options)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	GMimeObject *object;
	BoundaryType found;
	
	/* get the headers */
	priv->state = GMIME_PARSER_STATE_HEADERS;
	priv->toplevel = TRUE;
	
	while (priv->state < GMIME_PARSER_STATE_HEADERS_END) {
		if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR)
			return NULL;
	}
	
	content_type = parser_content_type (parser, NULL);
	if (content_type_is_type (content_type, "multipart", "*"))
		object = parser_construct_multipart (parser, options, content_type, FALSE, &found, 0);
	else
		object = parser_construct_leaf_part (parser, options, content_type, FALSE, &found, 0);
	
	content_type_destroy (content_type);
	
	return object;
}


/**
 * g_mime_parser_construct_part:
 * @parser: a #GMimeParser context
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Constructs a MIME part from @parser.
 *
 * Returns: (nullable) (transfer full): a MIME part based on @parser or %NULL on
 * fail.
 **/
GMimeObject *
g_mime_parser_construct_part (GMimeParser *parser, GMimeParserOptions *options)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	
	return parser_construct_part (parser, options);
}


static GMimeMessage *
parser_construct_message (GMimeParser *parser, GMimeParserOptions *options)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	unsigned long content_length = ULONG_MAX;
	ContentType *content_type;
	GMimeMessage *message;
	GMimeObject *object;
	BoundaryType found;
	const char *inptr;
	gboolean can_warn;
	Header *header;
	char *endptr;
	guint i;
	
	/* scan the from-line if we are parsing an mbox */
	while (priv->state != GMIME_PARSER_STATE_MESSAGE_HEADERS) {
		if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR)
			return NULL;
	}
	
	/* parse the headers */
	priv->toplevel = TRUE;
	while (priv->state < GMIME_PARSER_STATE_HEADERS_END) {
		if (parser_step (parser, options) == GMIME_PARSER_STATE_ERROR)
			return NULL;
	}
	
	message = g_mime_message_new (FALSE);
	((GMimeObject *) message)->ensure_newline = FALSE;
	_g_mime_header_list_set_options (((GMimeObject *) message)->headers, options);
	
	can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	for (i = 0; i < priv->headers->len; i++) {
		header = priv->headers->pdata[i];
		
		if (priv->respect_content_length && !g_ascii_strcasecmp (header->name, "Content-Length")) {
			inptr = header->raw_value;
			while (is_lwsp (*inptr))
				inptr++;
			
			content_length = strtoul (inptr, &endptr, 10);
			if (endptr == inptr)
				content_length = ULONG_MAX;
		}
		
		if (g_ascii_strncasecmp (header->name, "Content-", 8) != 0) {
			if (can_warn)
				check_repeated_header (options, (GMimeObject *) message, header);
			_g_mime_object_append_header ((GMimeObject *) message, header->name, header->raw_name,
						      header->raw_value, header->offset);
		}
	}
	
	if (priv->format == GMIME_FORMAT_MBOX) {
		parser_push_boundary (parser, MBOX_BOUNDARY);
		priv->content_end = 0;
		
		if (priv->respect_content_length && content_length < ULONG_MAX)
			priv->content_end = parser_offset (priv, NULL) + content_length;
	} else if (priv->format == GMIME_FORMAT_MMDF) {
		parser_push_boundary (parser, MMDF_BOUNDARY);
	}
	
	content_type = parser_content_type (parser, NULL);
	if (content_type_is_type (content_type, "multipart", "*"))
		object = parser_construct_multipart (parser, options, content_type, TRUE, &found, 0);
	else
		object = parser_construct_leaf_part (parser, options, content_type, TRUE, &found, 0);
	
	content_type_destroy (content_type);
	message->mime_part = object;
	
	if (priv->state == GMIME_PARSER_STATE_ERROR)
		_g_mime_parser_options_warn (options, -1, GMIME_WARN_MALFORMED_MESSAGE, NULL);
	
	if (priv->format == GMIME_FORMAT_MBOX) {
		priv->state = GMIME_PARSER_STATE_FROM;
		parser_pop_boundary (parser);
	}
	
	return message;
}


/**
 * g_mime_parser_construct_message:
 * @parser: a #GMimeParser context
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Constructs a MIME message from @parser.
 *
 * Returns: (nullable) (transfer full): a MIME message or %NULL on fail.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeParser *parser, GMimeParserOptions *options)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	
	return parser_construct_message (parser, options);
}


/**
 * g_mime_parser_get_mbox_marker:
 * @parser: a #GMimeParser context
 *
 * Gets the mbox-style From-line of the most recently parsed message
 * (gotten from g_mime_parser_construct_message()).
 *
 * Returns: (nullable): the mbox-style From-line of the most recently parsed
 * message or %NULL on error.
 **/
char *
g_mime_parser_get_mbox_marker (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	
	priv = parser->priv;
	if (priv->format != GMIME_FORMAT_MBOX)
		return NULL;
	
	if (priv->marker->len)
		return g_strndup ((char *) priv->marker->data, priv->marker->len);
	
	return NULL;
}


/**
 * g_mime_parser_get_mbox_marker_offset:
 * @parser: a #GMimeParser context
 *
 * Gets the offset of the most recently parsed mbox-style From-line
 * (gotten from g_mime_parser_construct_message()).
 *
 * Returns: the offset of the most recently parsed mbox-style From-line
 * or %-1 on error.
 **/
gint64
g_mime_parser_get_mbox_marker_offset (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	
	priv = parser->priv;
	if (priv->format != GMIME_FORMAT_MBOX)
		return -1;
	
	return priv->marker_offset;
}


/**
 * g_mime_parser_get_headers_begin:
 * @parser: a #GMimeParser context
 *
 * Gets the stream offset of the beginning of the headers of the most
 * recently parsed message.
 *
 * Returns: the offset of the beginning of the headers of the most
 * recently parsed message or %-1 on error.
 **/
gint64
g_mime_parser_get_headers_begin (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	
	return parser->priv->message_headers_begin;
}


/**
 * g_mime_parser_get_headers_end:
 * @parser: a #GMimeParser context
 *
 * Gets the stream offset of the end of the headers of the most
 * recently parsed message.
 *
 * Returns: the offset of the end of the headers of the most recently
 * parsed message or %-1 on error.
 **/
gint64
g_mime_parser_get_headers_end (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	
	return parser->priv->message_headers_end;
}
